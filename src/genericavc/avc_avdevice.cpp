/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "genericavc/avc_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_plug_info.h"
#include "libavc/general/avc_extended_plug_info.h"

#include "debugmodule/debugmodule.h"

#include "config.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>
#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

using namespace AVC;

namespace GenericAVC {

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_NORMAL );

AvDevice::AvDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created GenericAVC::AvDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_genericavc.txt" );
    if ( vendorModel.parse() ) {
        return vendorModel.isPresent( vendorId, modelId );
    }

    return false;
}

FFADODevice *
AvDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new AvDevice(d, configRom );
}

bool
AvDevice::discover()
{
    // check if we already have a valid VendorModel entry
    // e.g. because a subclass called this function
    if (!GenericAVC::VendorModel::isValid(m_model)) {
        unsigned int vendorId = getConfigRom().getNodeVendorId();
        unsigned int modelId = getConfigRom().getModelId();

        GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_genericavc.txt" );
        if ( vendorModel.parse() ) {
            m_model = vendorModel.find( vendorId, modelId );
        }

        if (!GenericAVC::VendorModel::isValid(m_model)) {
            return false;
        }
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model.vendor_name.c_str(), m_model.model_name.c_str());
    }

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
    setDebugLevel(l);
    m_pPlugManager->setVerboseLevel(l);
    FFADODevice::setVerboseLevel(l);
    AVC::Unit::setVerboseLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

int
AvDevice::getSamplingFrequency( ) {
    AVC::Plug* inputPlug = getPlugById( m_pcrPlugs, Plug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AVC::Plug* outputPlug = getPlugById( m_pcrPlugs, Plug::eAPD_Output, 0 );
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
        AVC::Plug* plug = getPlugById( m_pcrPlugs, Plug::eAPD_Input, 0 );
        if ( !plug ) {
            debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
            return false;
        }

        if ( !plug->setSampleRate( s ) )
        {
            debugError( "setSampleRate: Setting sample rate failed\n" );
            return false;
        }

        plug = getPlugById( m_pcrPlugs, Plug::eAPD_Output,  0 );
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

FFADODevice::ClockSourceVector
AvDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;

    PlugVector syncMSUInputPlugs = m_pPlugManager->getPlugsByType(
        eST_Music,
        0,
        0xff,
        0xff,
        Plug::eAPA_SubunitPlug,
        Plug::eAPD_Input,
        Plug::eAPT_Sync );
    if ( !syncMSUInputPlugs.size() ) {
        debugWarning( "No sync input plug for MSU subunit found\n" );
        return r;
    }

    for ( SyncInfoVector::const_iterator it
              = getSyncInfos().begin();
          it != getSyncInfos().end();
          ++it )
    {
        const SyncInfo si=*it;

        // check if the destination is a MSU input plug
        bool found=false;
        for ( PlugVector::const_iterator it2 = syncMSUInputPlugs.begin();
              it2 != syncMSUInputPlugs.end();
              ++it2 )
        {
            AVC::Plug* msuPlug = *it2;
            found |= (msuPlug == si.m_destination);
        }

        if (found) {
            ClockSource s=syncInfoToClockSource(*it);
            r.push_back(s);
        }
    }

    return r;
}

bool
AvDevice::setActiveClockSource(ClockSource s) {
    Plug *src=m_pPlugManager->getPlug( s.id );
    if (!src) {
        debugError("Could not find plug with id %d\n", s.id);
        return false;
    }

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
        case Plug::eAPT_IsoStream:
            s.type=eCT_SytMatch;
            break;
        case Plug::eAPT_Sync:
            if(si.m_source->getPlugAddressType() == Plug::eAPA_PCR) {
                s.type=eCT_SytStream; // this is logical
            } else if(si.m_source->getPlugAddressType() == Plug::eAPA_SubunitPlug) {
                s.type=eCT_Internal; // this assumes some stuff
            } else if(si.m_source->getPlugAddressType() == Plug::eAPA_ExternalPlug) {
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
        case Plug::eAPT_Digital:
            if(si.m_source->getPlugAddressType() == Plug::eAPA_ExternalPlug) {
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
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    ///////////
    // get plugs

    AVC::Plug* inputPlug = getPlugById( m_pcrPlugs, Plug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AVC::Plug* outputPlug = getPlugById( m_pcrPlugs, Plug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

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

    if (!addPlugToProcessor(*outputPlug,p,
        Streaming::Port::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
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
        p=new Streaming::AmdtpTransmitStreamProcessor(*this,
                                inputPlug->getNrOfChannels());
    }

    if(!p->init()) {
        debugFatal("Could not initialize transmit processor %s!\n",
            (snoopMode?" in snoop mode":""));
        delete p;
        return false;
    }

    if (snoopMode) {
        if (!addPlugToProcessor(*inputPlug,p,
            Streaming::Port::E_Capture)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
    } else {
        if (!addPlugToProcessor(*inputPlug,p,
            Streaming::Port::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
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

    Plug::ClusterInfoVector& clusterInfos = plug.getClusterInfos();
    for ( Plug::ClusterInfoVector::const_iterator it = clusterInfos.begin();
          it != clusterInfos.end();
          ++it )
    {
        const Plug::ClusterInfo* clusterInfo = &( *it );

        Plug::ChannelInfoVector channelInfos = clusterInfo->m_channelInfos;
        for ( Plug::ChannelInfoVector::const_iterator it = channelInfos.begin();
              it != channelInfos.end();
              ++it )
        {
            const Plug::ChannelInfo* channelInfo = &( *it );
            std::ostringstream portname;

            portname << id << "_" << channelInfo->m_name;

            Streaming::Port *p=NULL;
            switch(clusterInfo->m_portType) {
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Speaker:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Headphone:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Microphone:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Line:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Analog:
                debugOutput(DEBUG_LEVEL_VERBOSE, " Adding audio channel %s (pos=0x%02X, loc=0x%02X)\n",
                    channelInfo->m_name.c_str(), channelInfo->m_streamPosition, channelInfo->m_location);
                p=new Streaming::AmdtpAudioPort(
                        portname.str(),
                        direction,
                        channelInfo->m_streamPosition,
                        channelInfo->m_location,
                        Streaming::AmdtpPortInfo::E_MBLA
                );
                break;

            case ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
                debugOutput(DEBUG_LEVEL_VERBOSE, " Adding MIDI channel %s (pos=0x%02X, loc=0x%02X)\n",
                    channelInfo->m_name.c_str(), channelInfo->m_streamPosition, processor->getPortCount(Streaming::Port::E_Midi));
                p=new Streaming::AmdtpMidiPort(
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
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_SPDIF:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_ADAT:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_TDIF:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_MADI:
            case ExtendedPlugInfoClusterInfoSpecificData::ePT_Digital:
                debugOutput(DEBUG_LEVEL_VERBOSE, " Adding digital audio channel %s (pos=0x%02X, loc=0x%02X)\n",
                    channelInfo->m_name.c_str(), channelInfo->m_streamPosition, channelInfo->m_location);
                p=new Streaming::AmdtpAudioPort(
                        portname.str(),
                        direction,
                        channelInfo->m_streamPosition,
                        channelInfo->m_location,
                        Streaming::AmdtpPortInfo::E_MBLA
                );
                break;

            case ExtendedPlugInfoClusterInfoSpecificData::ePT_NoType:
            default:
            // unsupported
                break;
            }

            if (!p) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",channelInfo->m_name.c_str());
            } else {

                if (!processor->addPort(p)) {
                    debugWarning("Could not register port with stream processor\n");
                    return false;
                }
            }
         }
    }
    return true;
}

int
AvDevice::getStreamCount() {
    return m_receiveProcessors.size() + m_transmitProcessors.size();
    //return 1;
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
AvDevice::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = AVC::Unit::serialize( basePath, ser );
    result &= serializeOptions( basePath + "Options", ser );
    return result;
}

bool
AvDevice::deserialize( Glib::ustring basePath, Util::IODeserialize& deser )
{
    bool result;
    result = AVC::Unit::deserialize( basePath, deser );
    return result;
}

}
