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

//#include "config.h"
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

namespace GenericAVC {

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_NORMAL );

AvDevice::AvDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created GenericAVC::AvDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

AvDevice::~AvDevice()
{
    for ( StreamProcessorVectorIterator it = m_receiveProcessors.begin();
          it != m_receiveProcessors.end();
          ++it )
    {
        delete *it;
    }
    for ( StreamProcessorVectorIterator it = m_transmitProcessors.begin();
          it != m_transmitProcessors.end();
          ++it )
    {
        delete *it;
    }
}

bool
AvDevice::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
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
AvDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new AvDevice(d, configRom );
}

bool
AvDevice::discover()
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
AvDevice::discoverGeneric()
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
AvDevice::setVerboseLevel(int l)
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
AvDevice::getStreamingState()
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
AvDevice::getSamplingFrequency( ) {
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

    int samplerate_playback=inputPlug->getSampleRate();
    int samplerate_capture=outputPlug->getSampleRate();

    if (samplerate_playback != samplerate_capture) {
        debugWarning("Samplerates for capture and playback differ!\n");
    }
    return samplerate_capture;
}

bool
AvDevice::setSamplingFrequency( int s )
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
AvDevice::supportsSamplingFrequency( int s )
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
AvDevice::getSupportedSamplingFrequencies()
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
AvDevice::getSupportedClockSources() {
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
AvDevice::setActiveClockSource(ClockSource s) {
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
AvDevice::getActiveClockSource() {
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
AvDevice::syncInfoToClockSource(const SyncInfo& si) {
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
AvDevice::lock() {
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
AvDevice::unlock() {
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
AvDevice::showDevice()
{
    FFADODevice::showDevice();

    AVC::Unit::show();
    flushDebugOutput();
}

bool
AvDevice::prepare() {
    bool snoopMode=false;
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
    // base value is the config.h value
    float recv_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;
    float xmit_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;

    int xmit_max_cycles_early_transmit = AMDTP_MAX_CYCLES_TO_TRANSMIT_EARLY;
    int xmit_transfer_delay = AMDTP_TRANSMIT_TRANSFER_DELAY;
    int xmit_min_cycles_before_presentation = AMDTP_MIN_CYCLES_BEFORE_PRESENTATION;

    // we can override that globally
    config.getValueForSetting("streaming.common.recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForSetting("streaming.common.xmit_sp_dll_bw", xmit_sp_dll_bw);
    config.getValueForSetting("streaming.amdtp.xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForSetting("streaming.amdtp.xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForSetting("streaming.amdtp.xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    // or override in the device section
    uint32_t vendorid = getConfigRom().getNodeVendorId();
    uint32_t modelid = getConfigRom().getModelId();
    config.getValueForDeviceSetting(vendorid, modelid, "recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_sp_dll_bw", xmit_sp_dll_bw);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    // initialize the SP's
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing receive processor...\n");
    // create & add streamprocessors
    Streaming::StreamProcessor *p;

    if ( outputPlug->getNrOfChannels() == 0 ) {
        debugError("Receive plug has no channels\n");
        return false;
    }
    p = new Streaming::AmdtpReceiveStreamProcessor(*this,
                             outputPlug->getNrOfChannels());

    if(!p->init()) {
        debugFatal("Could not initialize receive processor!\n");
        delete p;
        return false;
    }

    if (!addPlugToProcessor(*outputPlug, p,
        Streaming::Port::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        delete p;
        return false;
    }

    if(!p->setDllBandwidth(recv_sp_dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete p;
        return false;
    }

    m_receiveProcessors.push_back(p);

    // do the transmit processor
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing transmit processor%s...\n",
            (snoopMode?" in snoop mode":""));
    if (snoopMode) {
        // we are snooping, so this is receive too.
        p=new Streaming::AmdtpReceiveStreamProcessor(*this,
                                  inputPlug->getNrOfChannels());
    } else {
        Streaming::AmdtpTransmitStreamProcessor * t;
        t=new Streaming::AmdtpTransmitStreamProcessor(*this,
                                inputPlug->getNrOfChannels());
        #if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
            // FIXME: it seems that some BeBoB devices can't handle NO-DATA without payload
            // FIXME: make this a config value too
            t->sendPayloadForNoDataPackets(true);
        #endif

        // transmit control parameters
        t->setMaxCyclesToTransmitEarly(xmit_max_cycles_early_transmit);
        t->setTransferDelay(xmit_transfer_delay);
        t->setMinCyclesBeforePresentation(xmit_min_cycles_before_presentation);

        p=t;
    }

    if(!p->init()) {
        debugFatal("Could not initialize transmit processor %s!\n",
            (snoopMode?" in snoop mode":""));
        delete p;
        return false;
    }

    if (snoopMode) {
        if (!addPlugToProcessor(*inputPlug, p,
            Streaming::Port::E_Capture)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
        if(!p->setDllBandwidth(recv_sp_dll_bw)) {
            debugFatal("Could not set DLL bandwidth\n");
            delete p;
            return false;
        }
    } else {
        if (!addPlugToProcessor(*inputPlug, p,
            Streaming::Port::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
        if(!p->setDllBandwidth(xmit_sp_dll_bw)) {
            debugFatal("Could not set DLL bandwidth\n");
            delete p;
            return false;
        }
    }

    // we put this SP into the transmit SP vector,
    // no matter if we are in snoop mode or not
    // this allows us to find out what direction
    // a certain stream should have.
    m_transmitProcessors.push_back(p);

    return true;
}

bool
AvDevice::addPlugToProcessor(
    AVC::Plug& plug,
    Streaming::StreamProcessor *processor,
    Streaming::AmdtpAudioPort::E_Direction direction) {

    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
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

            portname << id << "_" << channelInfo->m_name;

            Streaming::Port *p=NULL;
            switch(clusterInfo->m_portType) {
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Speaker:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Headphone:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Microphone:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Line:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Analog:
                debugOutput(DEBUG_LEVEL_VERBOSE, " Adding audio channel %s (pos=0x%02X, loc=0x%02X)\n",
                    channelInfo->m_name.c_str(), channelInfo->m_streamPosition, channelInfo->m_location);
                p=new Streaming::AmdtpAudioPort(
                        *processor,
                        portname.str(),
                        direction,
                        channelInfo->m_streamPosition,
                        channelInfo->m_location,
                        Streaming::AmdtpPortInfo::E_MBLA
                );
                break;

            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
                debugOutput(DEBUG_LEVEL_VERBOSE, " Adding MIDI channel %s (pos=0x%02X, loc=0x%02X)\n",
                    channelInfo->m_name.c_str(), channelInfo->m_streamPosition, processor->getPortCount(Streaming::Port::E_Midi));
                p=new Streaming::AmdtpMidiPort(
                        *processor,
                        portname.str(),
                        direction,
                        channelInfo->m_streamPosition,
                        // Workaround for out-of-spec hardware
                        // should be:
                        // channelInfo->m_location,
                        // but now we renumber the midi channels' location as they
                        // are discovered
                        processor->getPortCount(Streaming::Port::E_Midi),
                        Streaming::AmdtpPortInfo::E_Midi
                );

                break;
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_SPDIF:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_ADAT:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_TDIF:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_MADI:
            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_Digital:
                debugOutput(DEBUG_LEVEL_VERBOSE, " Adding digital audio channel %s (pos=0x%02X, loc=0x%02X)\n",
                    channelInfo->m_name.c_str(), channelInfo->m_streamPosition, channelInfo->m_location);
                p=new Streaming::AmdtpAudioPort(
                        *processor,
                        portname.str(),
                        direction,
                        channelInfo->m_streamPosition,
                        channelInfo->m_location,
                        Streaming::AmdtpPortInfo::E_MBLA
                );
                break;

            case AVC::ExtendedPlugInfoClusterInfoSpecificData::ePT_NoType:
            default:
            // unsupported
                break;
            }

            if (!p) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", channelInfo->m_name.c_str());
            }
         }
    }
    return true;
}

int
AvDevice::getStreamCount() {
    int retval;
    Util::MutexLockHelper lock(m_DeviceMutex);
    retval = m_receiveProcessors.size() + m_transmitProcessors.size();
    return retval;
}

Streaming::StreamProcessor *
AvDevice::getStreamProcessorByIndex(int i) {

    if (i<(int)m_receiveProcessors.size()) {
        return m_receiveProcessors.at(i);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        return m_transmitProcessors.at(i-m_receiveProcessors.size());
    }

    return NULL;
}

bool
AvDevice::startStreamByIndex(int i) {
    int iso_channel=-1;
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);

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

            iso_channel=opcr.channel;
        } else {
            iso_channel=get1394Service().allocateIsoChannelCMP(
                getConfigRom().getNodeId() | 0xffc0, n,
                get1394Service().getLocalNodeId()| 0xffc0, -1);
        }
        if (iso_channel<0) {
            debugError("Could not allocate ISO channel for SP %d\n",i);
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Started SP %d on channel %d\n",i,iso_channel);

        p->setChannel(iso_channel);
        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        if(snoopMode) { // a stream from another host to the device
            // FIXME: put this into a decent framework!
            // we should check the iPCR[n] on the device
            struct iec61883_iPCR ipcr;
            if (iec61883_get_iPCRX(
                    get1394Service().getHandle(),
                    getConfigRom().getNodeId() | 0xffc0,
                    (quadlet_t *)&ipcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n",i);
                return false;
            }

            iso_channel=ipcr.channel;

        } else {
            iso_channel=get1394Service().allocateIsoChannelCMP(
                get1394Service().getLocalNodeId()| 0xffc0, -1,
                getConfigRom().getNodeId() | 0xffc0, n);
        }

        if (iso_channel<0) {
            debugError("Could not allocate ISO channel for SP %d\n",i);
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Started SP %d on channel %d\n",i,iso_channel);

        p->setChannel(iso_channel);
        return true;
    }

    debugError("SP index %d out of range!\n",i);
    return false;
}

bool
AvDevice::stopStreamByIndex(int i) {
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);

        // can't stop it if it's not running
        if(p->getChannel() == -1) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %d not running\n",i);
            return true;
        }

        if(snoopMode) {

        } else {
            // deallocate ISO channel
            if(!get1394Service().freeIsoChannel(p->getChannel())) {
                debugError("Could not deallocate iso channel for SP %d\n",i);
                return false;
            }
        }
        p->setChannel(-1);

        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        // can't stop it if it's not running
        if(p->getChannel() == -1) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "SP %d not running\n",i);
            return true;
        }

        if(snoopMode) {

        } else {
            // deallocate ISO channel
            if(!get1394Service().freeIsoChannel(p->getChannel())) {
                debugError("Could not deallocate iso channel for SP %d\n",i);
                return false;
            }
        }
        p->setChannel(-1);

        return true;
    }

    debugError("SP index %d out of range!\n",i);
    return false;
}

bool
AvDevice::serialize( std::string basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = AVC::Unit::serialize( basePath, ser );
    result &= serializeOptions( basePath + "Options", ser );
    return result;
}

bool
AvDevice::deserialize( std::string basePath, Util::IODeserialize& deser )
{
    bool result;
    result = AVC::Unit::deserialize( basePath, deser );
    return result;
}

}
