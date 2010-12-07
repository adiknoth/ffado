/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "devicemanager.h"
#include "genericavc/avc_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_plug_info.h"
#include "libavc/general/avc_extended_plug_info.h"
#include "libavc/general/avc_subunit_info.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"
#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

#include "stanton/scs.h"

typedef uint32_t __u32; // FIXME
typedef uint64_t __u64; // FIXME

#include "libstreaming/connections.h"
#include "libstreaming/streamer.h"
#include "libstreaming/am824stream.h"

namespace GenericAVC {

IMPL_DEBUG_MODULE( Device, Device, DEBUG_LEVEL_NORMAL );

Device::Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_nb_stream_settings( 0 )
    , m_stream_settings( NULL )
    , m_amdtp_settings( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created GenericAVC::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

Device::~Device()
{
    if(m_stream_settings) {
        free(m_stream_settings);
    }
    if(m_amdtp_settings) {
        free(m_amdtp_settings);
    }
}

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if(generic) {
        // check if we have a music subunit
        AVC::SubUnitInfoCmd subUnitInfoCmd( configRom.get1394Service() );
        subUnitInfoCmd.setCommandType( AVC::AVCCommand::eCT_Status );
        subUnitInfoCmd.m_page = 0;
        subUnitInfoCmd.setNodeId( configRom.getNodeId() );
        subUnitInfoCmd.setVerbose( configRom.getVerboseLevel() );
        if ( !subUnitInfoCmd.fire() ) {
            debugError( "Subunit info command failed\n" );
            return false;
        }
        for ( int i = 0; i < subUnitInfoCmd.getNrOfValidEntries(); ++i ) {
            AVC::subunit_type_t subunit_type
                = subUnitInfoCmd.m_table[i].m_subunit_type;
            if (subunit_type == AVC::eST_Music) return true;
        }

        return false;
    } else {
        // check if device is in supported devices list
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();

        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_GenericAVC;
        return false;
    }
}

FFADODevice *
Device::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();
    unsigned int modelId = configRom->getModelId();

    switch (vendorId) {
        case FW_VENDORID_STANTON:
            if (modelId == 0x00001000 ) {
                return new Stanton::ScsDevice(d, configRom);
            }
        default:
            return new GenericAVC::Device(d, configRom);
    }
    return NULL;
}

bool
Device::discover()
{
    Util::MutexLockHelper lock(m_DeviceMutex);

    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_GenericAVC) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Using generic AV/C support for unsupported device '%s %s'\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }
    return discoverGeneric();
}

bool
Device::discoverGeneric()
{
    if ( !Unit::discover() ) {
        debugError( "Could not discover unit\n" );
        return false;
    }

    if((getAudioSubunit( 0 ) == NULL)) {
        debugError( "Unit doesn't have an Audio subunit.\n");
        return false;
    }
    if((getMusicSubunit( 0 ) == NULL)) {
        debugError( "Unit doesn't have a Music subunit.\n");
        return false;
    }
    return true;
}

void
Device::setVerboseLevel(int l)
{
    Util::MutexLockHelper lock(m_DeviceMutex);
    setDebugLevel(l);
    m_pPlugManager->setVerboseLevel(l);
    FFADODevice::setVerboseLevel(l);
    AVC::Unit::setVerboseLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

#include <libieee1394/IEC61883.h>
enum FFADODevice::eStreamingState
Device::getStreamingState()
{
    // check the IEC plug control registers to see if the device is streaming
    // a bit of a hack, but will do until we come up with something better
    struct iec61883_oPCR oPCR0;
    struct iec61883_iPCR iPCR0;
    
    quadlet_t *oPCR0q = (quadlet_t *)&oPCR0;
    quadlet_t *iPCR0q = (quadlet_t *)&iPCR0;
    
    if(!get1394Service().read(getNodeId() | 0xFFC0, CSR_REGISTER_BASE + CSR_O_PCR_0, 1, oPCR0q)) {
        debugWarning("Could not read oPCR0 register\n");
    }
    if(!get1394Service().read(getNodeId() | 0xFFC0, CSR_REGISTER_BASE + CSR_I_PCR_0, 1, iPCR0q)) {
        debugWarning("Could not read iPCR0 register\n");
    }

    *oPCR0q = CondSwapFromBus32(*oPCR0q);
    *iPCR0q = CondSwapFromBus32(*iPCR0q);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "iPCR0: %08X, oPCR0: %08X\n", *iPCR0q, *oPCR0q);

    if(iPCR0.n_p2p_connections > 0 && oPCR0.n_p2p_connections > 0) {
        return eSS_Both;
    } else if (iPCR0.n_p2p_connections > 0) {
        return eSS_Receiving;
    } else if (oPCR0.n_p2p_connections > 0) {
        return eSS_Sending;
    } else {
        return eSS_Idle;
    }
}

int
Device::getSamplingFrequency( ) {
    AVC::Plug* inputPlug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AVC::Plug* outputPlug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    int samplerate_playback = inputPlug->getSampleRate();
    int samplerate_capture = outputPlug->getSampleRate();

    if (samplerate_playback != samplerate_capture) {
        debugWarning("Samplerates for capture and playback differ!\n");
    }
    return samplerate_capture;
}

bool
Device::setSamplingFrequency( int s )
{
    Util::MutexLockHelper lock(m_DeviceMutex);
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if(snoopMode) {
        int current_sr=getSamplingFrequency();
        if (current_sr != s ) {
            debugError("In snoop mode it is impossible to set the sample rate.\n");
            debugError("Please start the client with the correct setting.\n");
            return false;
        }
        return true;
    } else {
        AVC::Plug* plug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Input, 0 );
        if ( !plug ) {
            debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
            return false;
        }

        if ( !plug->setSampleRate( s ) )
        {
            debugError( "setSampleRate: Setting sample rate failed\n" );
            return false;
        }

        plug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Output,  0 );
        if ( !plug ) {
            debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
            return false;
        }

        if ( !plug->setSampleRate( s ) )
        {
            debugError( "setSampleRate: Setting sample rate failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "setSampleRate: Set sample rate to %d\n",
                     s );
        return true;
    }
    // not executable
    return false;

}

bool
Device::supportsSamplingFrequency( int s )
{
    Util::MutexLockHelper lock(m_DeviceMutex);

    AVC::Plug* plug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Input, 0 );
    if ( !plug ) {
        debugError( "Could not retrieve iso input plug 0\n" );
        return false;
    }

    if ( !plug->supportsSampleRate( s ) )
    {
        debugError( "sample rate not supported by input plug\n" );
        return false;
    }

    plug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Output,  0 );
    if ( !plug ) {
        debugError( "Could not retrieve iso output plug 0\n" );
        return false;
    }

    if ( !plug->supportsSampleRate( s ) )
    {
        debugError( "sample rate not supported by output plug\n" );
        return false;
    }
    return true;
}

#define GENERICAVC_CHECK_AND_ADD_SR(v, x) \
    { if(supportsSamplingFrequency(x)) \
      v.push_back(x); }

std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    if (m_supported_frequencies_cache.size() == 0) {
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 22050);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 24000);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 32000);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 44100);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 48000);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 88200);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 96000);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 176400);
        GENERICAVC_CHECK_AND_ADD_SR(m_supported_frequencies_cache, 192000);
    }
    return m_supported_frequencies_cache;
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;

    Util::MutexLockHelper lock(m_DeviceMutex);

    AVC::PlugVector syncMSUInputPlugs = m_pPlugManager->getPlugsByType(
        AVC::eST_Music,
        0,
        0xff,
        0xff,
        AVC::Plug::eAPA_SubunitPlug,
        AVC::Plug::eAPD_Input,
        AVC::Plug::eAPT_Sync );
    if ( !syncMSUInputPlugs.size() ) {
        // there exist devices which do not have a sync plug
        // or their av/c model is broken. 
        return r;
    }

    for ( SyncInfoVector::const_iterator it
              = getSyncInfos().begin();
          it != getSyncInfos().end();
          ++it )
    {
        const SyncInfo si=*it;

        ClockSource s=syncInfoToClockSource(*it);
        r.push_back(s);
    }
    return r;
}

bool
Device::setActiveClockSource(ClockSource s) {
    AVC::Plug *src = m_pPlugManager->getPlug( s.id );
    if (!src) {
        debugError("Could not find plug with id %d\n", s.id);
        return false;
    }

    Util::MutexLockHelper lock(m_DeviceMutex);
    for ( SyncInfoVector::const_iterator it
              = getSyncInfos().begin();
          it != getSyncInfos().end();
          ++it )
    {
        const SyncInfo si=*it;

        if (si.m_source==src) {
            return setActiveSync(si);
        }
    }
    return false;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    const SyncInfo* si=getActiveSyncInfo();
    if ( !si ) {
        debugError( "Could not retrieve active sync information\n" );
        ClockSource s;
        s.type=eCT_Invalid;
        return s;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "Active Sync mode:  %s\n", si->m_description.c_str() );

    return syncInfoToClockSource(*si);
}

FFADODevice::ClockSource
Device::syncInfoToClockSource(const SyncInfo& si) {
    ClockSource s;

    // the description is easy
    // it can be that we overwrite it later
    s.description=si.m_description;

    // FIXME: always valid at the moment
    s.valid=true;

    assert(si.m_source);
    s.id=si.m_source->getGlobalId();

    // now figure out what type this is
    switch(si.m_source->getPlugType()) {
        case AVC::Plug::eAPT_IsoStream:
            s.type=eCT_SytMatch;
            break;
        case AVC::Plug::eAPT_Sync:
            if(si.m_source->getPlugAddressType() == AVC::Plug::eAPA_PCR) {
                s.type=eCT_SytStream; // this is logical
            } else if(si.m_source->getPlugAddressType() == AVC::Plug::eAPA_SubunitPlug) {
                s.type=eCT_Internal; // this assumes some stuff
            } else if(si.m_source->getPlugAddressType() == AVC::Plug::eAPA_ExternalPlug) {
                std::string plugname=si.m_source->getName();
                s.description=plugname;
                // this is basically due to Focusrites interpretation
                if(plugname.find( "SPDIF", 0 ) != string::npos) {
                    s.type=eCT_SPDIF; // this assumes the name will tell us
                } else {
                    s.type=eCT_WordClock; // this assumes a whole lot more
                }
            } else {
                s.type=eCT_Invalid;
            }
            break;
        case AVC::Plug::eAPT_Digital:
            if(si.m_source->getPlugAddressType() == AVC::Plug::eAPA_ExternalPlug) {
                std::string plugname=si.m_source->getName();
                s.description=plugname;
                // this is basically due to Focusrites interpretation
                if(plugname.find( "ADAT", 0 ) != string::npos) {
                    s.type=eCT_ADAT; // this assumes the name will tell us
                } else if(plugname.find( "SPDIF", 0 ) != string::npos) {
                    s.type=eCT_SPDIF; // this assumes the name will tell us
                } else {
                    s.type=eCT_WordClock; // this assumes a whole lot more
                }
            } else {
                s.type=eCT_Invalid;
            }
            break;
        default:
            s.type=eCT_Invalid; break;
    }

    // is it active?
    const SyncInfo* active=getActiveSyncInfo();
    if (active) {
        if ((active->m_source == si.m_source)
           && (active->m_destination == si.m_destination))
           s.active=true;
        else s.active=false;
    } else s.active=false;

    return s;
}

bool
Device::lock() {
    bool snoopMode=false;
    Util::MutexLockHelper lock(m_DeviceMutex);
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        // don't lock
    } else {
//         return Unit::reserve(4);
    }

    return true;
}

bool
Device::unlock() {
    bool snoopMode=false;
    Util::MutexLockHelper lock(m_DeviceMutex);
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        // don't unlock
    } else {
//         return Unit::reserve(0);
    }
    return true;
}

void
Device::showDevice()
{
    FFADODevice::showDevice();

    AVC::Unit::show();
    flushDebugOutput();
}

bool
Device::prepare() {
    bool snoopMode = false;
    Util::MutexLockHelper lock(m_DeviceMutex);
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    ///////////
    // get plugs

    AVC::Plug* inputPlug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AVC::Plug* outputPlug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    // get the device specific and/or global SP configuration
    Util::Configuration &config = getDeviceManager().getConfiguration();

    int xmit_max_cycles_early_transmit = AMDTP_MAX_CYCLES_TO_TRANSMIT_EARLY;
    int xmit_transfer_delay = AMDTP_TRANSMIT_TRANSFER_DELAY;
    int xmit_min_cycles_before_presentation = AMDTP_MIN_CYCLES_BEFORE_PRESENTATION;

    // we can override that globally
    config.getValueForSetting("streaming.amdtp.xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForSetting("streaming.amdtp.xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForSetting("streaming.amdtp.xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    // or override in the device section
    uint32_t vendorid = getConfigRom().getNodeVendorId();
    uint32_t modelid = getConfigRom().getModelId();
    config.getValueForDeviceSetting(vendorid, modelid,
                                    "xmit_max_cycles_early_transmit",
                                    xmit_max_cycles_early_transmit);
    config.getValueForDeviceSetting(vendorid, modelid,
                                    "xmit_transfer_delay",
                                    xmit_transfer_delay);
    config.getValueForDeviceSetting(vendorid, modelid,
                                    "xmit_min_cycles_before_presentation",
                                    xmit_min_cycles_before_presentation);

    // allocate streaming information
    int m_nb_rcv = 1; // FIXME: only one stream supported
    int m_nb_xmt = 1; // FIXME: only one stream supported

    m_stream_settings = (struct stream_settings *)calloc(m_nb_rcv + m_nb_xmt,
                                                         sizeof(struct stream_settings));
    if(m_stream_settings == NULL) {
        debugError("Failed to allocate memory\n");
        return false;
    }
    m_amdtp_settings = (struct am824_settings *)calloc(m_nb_rcv + m_nb_xmt, 
                                                       sizeof(struct am824_settings));
    if(m_amdtp_settings == NULL) {
        debugError("Failed to allocate memory\n");
        return false;
    }
    m_nb_stream_settings = 0; // increment when successfully initialized

    struct stream_settings *s = NULL;

    // initialize the SP's
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing receive streams...\n");
    s = m_stream_settings;

    // initialize stream settings and add substreams
    if(!addPlugToStreamSettings(*outputPlug, s)) {
        debugError("Failed to initialize stream settings for RX\n");
        return false;
    }
    m_nb_stream_settings++;

    // setup the AMDTP stream information
    struct am824_settings *amdtp_settings = m_amdtp_settings;
    if(am824_init(amdtp_settings, getNodeId(), outputPlug->getNrOfChannels(), getSamplingFrequency()) < 0) {
        debugError("Failed to setup AMDTP stream configuration\n");
        return false;
    }

    s->type = STREAM_TYPE_RECEIVE;
    s->channel = -1; // to be filled in at stream startup
    s->port = get1394Service().getNewStackPort();

    s->max_packet_size = amdtp_settings->packet_length;
    s->packet_size_for_sync = amdtp_settings->syt_interval;

    s->client_data = amdtp_settings;
    s->process_header  = NULL;
    s->process_payload = receive_am824_packet_data;


    // do the transmit processor
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing transmit streams%s...\n",
            (snoopMode?" in snoop mode":""));

    // initialize stream settings and add substreams
    s = m_stream_settings + m_nb_rcv;
    if(!addPlugToStreamSettings(*inputPlug, s)) {
        debugError("Failed to initialize stream settings for TX\n");
        return false;
    }
    m_nb_stream_settings++;

    // setup the AMDTP stream information
    amdtp_settings = m_amdtp_settings + m_nb_rcv;
    if(am824_init(amdtp_settings, getNodeId(), inputPlug->getNrOfChannels(), getSamplingFrequency()) < 0) {
        debugError("Failed to setup AMDTP stream configuration\n");
        return false;
    }

    if(snoopMode) {
        s->type = STREAM_TYPE_RECEIVE;
        s->process_payload = receive_am824_packet_data;
        s->process_header  = NULL;
    } else {
        s->type = STREAM_TYPE_TRANSMIT;
        s->process_payload = transmit_am824_packet_data;
        s->process_header  = NULL;

        #if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
        // FIXME: it seems that some BeBoB devices can't handle NO-DATA without payload
        // FIXME: make this a config value too
        amdtp_settings->send_nodata_payload = 1;
        #endif
        amdtp_settings->transfer_delay = xmit_transfer_delay;
    }

    s->channel = -1; // to be filled at stream startup
    s->port = get1394Service().getNewStackPort();

    s->max_packet_size = amdtp_settings->packet_length;
    s->packet_size_for_sync = amdtp_settings->syt_interval;

    s->client_data = amdtp_settings;

    return true;
}

bool
Device::addPlugToStreamSettings(
    AVC::Plug& plug,
    struct stream_settings *s) {

    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

    // substream settings (FIXME: somewhere we should de-allocate this)
    if(stream_settings_alloc_substreams(s, plug.getNrOfChannels()) < 0) {
        debugError("Failed to allocate substream storage\n");
        return false;
    }

    AVC::Plug::ClusterInfoVector& clusterInfos = plug.getClusterInfos();
    for ( AVC::Plug::ClusterInfoVector::const_iterator it = clusterInfos.begin();
          it != clusterInfos.end();
          ++it )
    {
        const AVC::Plug::ClusterInfo* clusterInfo = &( *it );

        AVC::Plug::ChannelInfoVector channelInfos = clusterInfo->m_channelInfos;
        for ( AVC::Plug::ChannelInfoVector::const_iterator it = channelInfos.begin();
              it != channelInfos.end();
              ++it )
        {
            const AVC::Plug::ChannelInfo* channelInfo = &( *it );
            std::ostringstream portname;
            int index;

            portname << id << "_" << channelInfo->m_name;

            switch(clusterInfo->m_portType) {
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Speaker:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Headphone:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Microphone:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Line:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Analog:
                index = channelInfo->m_streamPosition;
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            " Adding audio channel %s (pos=0x%02X, loc=0x%02X) as index %d\n",
                            channelInfo->m_name.c_str(),
                            channelInfo->m_streamPosition,
                            channelInfo->m_location,
                            index);

                stream_settings_set_substream_name(s, index, portname.str().c_str());
                stream_settings_set_substream_type(s, index, ffado_stream_type_audio);
                stream_settings_set_substream_state(s, index, STREAM_PORT_STATE_ON);

                break;

            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
                // FIXME: midi channels
                index = channelInfo->m_streamPosition;
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            " Adding midi channel %s (pos=0x%02X, loc=0x%02X) as index %d\n",
                            channelInfo->m_name.c_str(),
                            channelInfo->m_streamPosition,
                            channelInfo->m_location,
                            index);

                stream_settings_set_substream_name(s, index, portname.str().c_str());
                stream_settings_set_substream_type(s, index, ffado_stream_type_midi);
                stream_settings_set_substream_state(s, index, STREAM_PORT_STATE_ON);

                break;
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_SPDIF:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_ADAT:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_TDIF:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_MADI:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Digital:
                index = channelInfo->m_streamPosition;
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            " Adding digital channel %s (pos=0x%02X, loc=0x%02X) as index %d\n",
                            channelInfo->m_name.c_str(),
                            channelInfo->m_streamPosition,
                            channelInfo->m_location,
                            index);

                stream_settings_set_substream_name(s, index, portname.str().c_str());
                // FIXME: digital stream type?
//                 stream_settings_set_substream_type(s, index, ffado_stream_type_digital);
                stream_settings_set_substream_type(s, index, ffado_stream_type_audio);
                stream_settings_set_substream_state(s, index, STREAM_PORT_STATE_OFF);

                break;

            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_NoType:
            default:
            // unsupported
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "Unknown port type for %s\n",
                            channelInfo->m_name.c_str());
                return false;
                break;
            }
         }
    }
    return true;
}

int
Device::getStreamCount() {
    return m_nb_stream_settings;
}

struct stream_settings *
Device::getSettingsForStream(unsigned int i) {
    if(i >= m_nb_stream_settings) {
        debugWarning("requested stream %d out of range\n", i);
        return NULL;
    }
    return m_stream_settings + i;
}

bool
Device::startStreamByIndex(int i) {
    if(i >= m_nb_stream_settings) {
        debugError("requested stream %d out of range\n", i);
        return NULL;
    }

    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    struct stream_settings *s = m_stream_settings + i;

    if (i < 1) { // FIXME: only one stream supported per direction
        unsigned int n = i;
        // should be a receive stream
        assert(s->type == STREAM_TYPE_RECEIVE);
        assert(s->channel == -1);

        if(snoopMode) { // a stream from the device to another host
            // FIXME: put this into a decent framework!
            // we should check the oPCR[n] on the device
            struct iec61883_oPCR opcr;
            if (iec61883_get_oPCRX(
                    get1394Service().getHandle(),
                    getConfigRom().getNodeId() | 0xffc0,
                    (quadlet_t *)&opcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n",i);
                return false;
            }

            s->channel = opcr.channel;
        } else {
            s->channel = get1394Service().allocateIsoChannelCMP(
                getConfigRom().getNodeId() | 0xffc0, n,
                get1394Service().getLocalNodeId()| 0xffc0, -1);
        }
        if (s->channel < 0) {
            debugError("Could not allocate ISO channel for SP %d\n", i);
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Started SP %d on channel %d\n", i, s->channel);
        return true;

    } else if (i < 2) { // FIXME: only one stream supported per direction
        unsigned int n = i - 1; // FIXME: only one stream supported per direction
        assert(s->channel == -1);

        if(snoopMode) { // a stream from another host to the device
            // FIXME: put this into a decent framework!
            // we should check the iPCR[n] on the device
            struct iec61883_iPCR ipcr;
            if (iec61883_get_iPCRX(
                    get1394Service().getHandle(),
                    getConfigRom().getNodeId() | 0xffc0,
                    (quadlet_t *)&ipcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n", i);
                return false;
            }

            s->channel = ipcr.channel;

        } else {
            s->channel = get1394Service().allocateIsoChannelCMP(
                get1394Service().getLocalNodeId()| 0xffc0, -1,
                getConfigRom().getNodeId() | 0xffc0, n);
        }

        if (s->channel < 0) {
            debugError("Could not allocate ISO channel for SP %d\n", i);
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Started SP %d on channel %d\n", i, s->channel);
        return true;
    }

    debugError("SP index %d out of range!\n", i);
    return false;
}

bool
Device::stopStreamByIndex(int i) {
    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    struct stream_settings *s = m_stream_settings + i;
    if (i < 1) { // FIXME: only one stream supported per direction
        // can't stop it if it's not running
        if(s->channel == -1) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %d not running\n",i);
            return true;
        }

        if(snoopMode) {
            // nothing to do
        } else {
            // deallocate ISO channel
            if(!get1394Service().freeIsoChannel(s->channel)) {
                debugError("Could not deallocate iso channel for SP %d\n", i);
                return false;
            }
        }
        s->channel = -1;

        return true;

    } else if (i < 2) {
        // can't stop it if it's not running
        if(s->channel == -1) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %d not running\n", i);
            return true;
        }

        if(snoopMode) {
            // nothing to do
        } else {
            // deallocate ISO channel
            if(!get1394Service().freeIsoChannel(s->channel)) {
                debugError("Could not deallocate iso channel for SP %d\n", i);
                return false;
            }
        }
        s->channel = -1;

        return true;
    }

    debugError("SP index %d out of range!\n",i);
    return false;
}

bool
Device::serialize( std::string basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = AVC::Unit::serialize( basePath, ser );
    result &= serializeOptions( basePath + "Options", ser );
    return result;
}

bool
Device::deserialize( std::string basePath, Util::IODeserialize& deser )
{
    bool result;
    result = AVC::Unit::deserialize( basePath, deser );
    return result;
}

}
