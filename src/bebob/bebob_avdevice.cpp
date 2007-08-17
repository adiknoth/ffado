/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/GenericMixer.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/general/avc_plug_info.h"
#include "libavc/general/avc_extended_plug_info.h"
#include "libavc/general/avc_subunit_info.h"
#include "libavc/streamformat/avc_extended_stream_format.h"
#include "libavc/util/avc_serialize.h"
#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>

using namespace AVC;

namespace BeBoB {

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_VERBOSE );

static VendorModelEntry supportedDeviceList[] =
{
    {0x00000f, 0x00010065, "Mackie", "Onyx Firewire"},

    {0x0003db, 0x00010048, "Apogee Electronics", "Rosetta 200"},

    {0x0007f5, 0x00010048, "BridgeCo", "RD Audio1"},

    {0x000a92, 0x00010000, "PreSonus", "FIREBOX"},
    {0x000a92, 0x00010066, "PreSonus", "FirePOD"},

    {0x000aac, 0x00000003, "TerraTec Electronic GmbH", "Phase 88 FW"},
    {0x000aac, 0x00000004, "TerraTec Electronic GmbH", "Phase X24 FW (model version 4)"},
    {0x000aac, 0x00000007, "TerraTec Electronic GmbH", "Phase X24 FW (model version 7)"},

    {0x000f1b, 0x00010064, "ESI", "Quatafire 610"},

    {0x00130e, 0x00000003, "Focusrite", "Saffire Pro26IO"},
    {0x00130e, 0x00000006, "Focusrite", "Saffire Pro10IO"}, 

    {0x0040ab, 0x00010048, "EDIROL", "FA-101"},
    {0x0040ab, 0x00010049, "EDIROL", "FA-66"},
};

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    : FFADODevice( configRom, ieee1394service, nodeId )
    , AVC::Unit( )
    , m_model ( NULL )
    , m_Mixer ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::AvDevice (NodeID %d)\n",
                 nodeId );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

AvDevice::~AvDevice()
{
    if(m_Mixer != NULL) {
        if (!removeChildOscNode(m_Mixer)) {
            debugWarning("failed to unregister mixer from OSC namespace\n");
        }
        delete m_Mixer;
    }
}

AVC::Subunit* 
BeBoB::AvDevice::createSubunit(AVC::Unit& unit,
                               AVC::ESubunitType type,
                               AVC::subunit_t id ) 
{
    switch (type) {
        case AVC::eST_Audio:
            return new BeBoB::SubunitAudio(unit, id );
        case AVC::eST_Music:
            return new BeBoB::SubunitMusic(unit, id );
        default:
            return NULL;
    }
}

AVC::Plug *
BeBoB::AvDevice::createPlug( AVC::Unit* unit,
                     AVC::Subunit* subunit,
                     AVC::function_block_type_t functionBlockType,
                     AVC::function_block_type_t functionBlockId,
                     AVC::Plug::EPlugAddressType plugAddressType,
                     AVC::Plug::EPlugDirection plugDirection,
                     AVC::plug_id_t plugId )
{

    return new BeBoB::Plug( unit,
                     subunit,
                     functionBlockType,
                     functionBlockId,
                     plugAddressType,
                     plugDirection,
                     plugId );
}

void
AvDevice::setVerboseLevel(int l)
{
    m_pPlugManager->setVerboseLevel(l);

    FFADODevice::setVerboseLevel(l);
    AVC::Unit::setVerboseLevel(l);
}

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId ) )
        {
            return true;
        }
    }
    return false;
}

bool
AvDevice::discover()
{
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            m_model = &(supportedDeviceList[i]);
            break;
        }
    }

    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
    } else return false;

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

// replaced by the previous Unit discovery
//     if ( !enumerateSubUnits() ) {
//         debugError( "Could not enumarate sub units\n" );
//         return false;
//     }

    // create a GenericMixer and add it as an OSC child node
    //  remove if already there
    if(m_Mixer != NULL) {
        if (!removeChildOscNode(m_Mixer)) {
            debugWarning("failed to unregister mixer from OSC namespace\n");
        }
        delete m_Mixer;
    }
    
    //  create the mixer & register it
    if(getAudioSubunit(0) == NULL) {
        debugWarning("Could not find audio subunit, mixer not available.\n");
        m_Mixer = NULL;
    } else {
        m_Mixer = new GenericMixer(*m_p1394Service , *this);
        if (!addChildOscNode(m_Mixer)) {
            debugWarning("failed to register mixer in OSC namespace\n");
        }
    }
    return true;
}

bool
AvDevice::setSamplingFrequency( int s )
{
    ESamplingFrequency samplingFrequency=parseSampleRate( s );
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if(snoopMode) {
        int current_sr=getSamplingFrequency();
        if (current_sr != convertESamplingFrequency( samplingFrequency ) ) {
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

        if ( !setSamplingFrequencyPlug( *plug,
                                        Plug::eAPD_Input,
                                        samplingFrequency ) )
        {
            debugError( "setSampleRate: Setting sample rate failed\n" );
            return false;
        }

        plug = getPlugById( m_pcrPlugs, Plug::eAPD_Output,  0 );
        if ( !plug ) {
            debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
            return false;
        }

        if ( !setSamplingFrequencyPlug( *plug,
                                        Plug::eAPD_Output,
                                        samplingFrequency ) )
        {
            debugError( "setSampleRate: Setting sample rate failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "setSampleRate: Set sample rate to %d\n",
                     convertESamplingFrequency( samplingFrequency ) );
        return true;
    }
    // not executable
    return false;
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
AvDevice::setSamplingFrequencyPlug( AVC::Plug& plug,
                                    Plug::EPlugDirection direction,
                                    ESamplingFrequency samplingFrequency )
{

    ExtendedStreamFormatCmd extStreamFormatCmd(
        *m_p1394Service,
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommandList );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     plug.getPlugId() );

    extStreamFormatCmd.setPlugAddress(
        PlugAddress(
            Plug::convertPlugDirection(direction ),
            PlugAddress::ePAM_Unit,
            unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_pConfigRom->getNodeId() );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );

    int i = 0;
    bool cmdSuccess = false;
    bool correctFormatFound = false;

    do {
        extStreamFormatCmd.setIndexInStreamFormat( i );
        extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
        extStreamFormatCmd.setVerbose( m_verboseLevel );

        cmdSuccess = extStreamFormatCmd.fire();

        if ( cmdSuccess
             && ( extStreamFormatCmd.getResponse() ==
                  AVCCommand::eR_Implemented ) )
        {
            ESamplingFrequency foundFreq = eSF_DontCare;

            FormatInformation* formatInfo =
                extStreamFormatCmd.getFormatInformation();
            FormatInformationStreamsCompound* compoundStream
                = dynamic_cast< FormatInformationStreamsCompound* > (
                    formatInfo->m_streams );
            if ( compoundStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        compoundStream->m_samplingFrequency );
            }

            FormatInformationStreamsSync* syncStream
                = dynamic_cast< FormatInformationStreamsSync* > (
                    formatInfo->m_streams );
            if ( syncStream ) {
                foundFreq =
                    static_cast< ESamplingFrequency >(
                        syncStream->m_samplingFrequency );
            }

            if ( foundFreq == samplingFrequency )
            {
                correctFormatFound = true;
                break;
            }
        }

        ++i;
    } while ( cmdSuccess
              && ( extStreamFormatCmd.getResponse() ==
                   ExtendedStreamFormatCmd::eR_Implemented ) );

    if ( !cmdSuccess ) {
        debugError( "setSampleRatePlug: Failed to retrieve format info\n" );
        return false;
    }

    if ( !correctFormatFound ) {
        debugError( "setSampleRatePlug: %s plug %d does not support "
                    "sample rate %d\n",
                    plug.getName(),
                    plug.getPlugId(),
                    convertESamplingFrequency( samplingFrequency ) );
        return false;
    }

    extStreamFormatCmd.setSubFunction(
        ExtendedStreamFormatCmd::eSF_ExtendedStreamFormatInformationCommand );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Control );
    extStreamFormatCmd.setVerbose( m_verboseLevel );

    if ( !extStreamFormatCmd.fire() ) {
        debugError( "setSampleRate: Could not set sample rate %d "
                    "to %s plug %d\n",
                    convertESamplingFrequency( samplingFrequency ),
                    plug.getName(),
                    plug.getPlugId() );
        return false;
    }

    return true;
}

void
AvDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        m_nodeId);

    m_pPlugManager->showPlugs();
    flushDebugOutput();
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

    int samplerate=outputPlug->getSampleRate();

    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing receive processor...\n");
    // create & add streamprocessors
    Streaming::StreamProcessor *p;

    p=new Streaming::AmdtpReceiveStreamProcessor(
                             m_p1394Service->getPort(),
                             samplerate,
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
        p=new Streaming::AmdtpReceiveStreamProcessor(
                                  m_p1394Service->getPort(),
                                  samplerate,
                                  inputPlug->getNrOfChannels());
    } else {
        p=new Streaming::AmdtpTransmitStreamProcessor(
                                m_p1394Service->getPort(),
                                samplerate,
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
                p=new Streaming::AmdtpAudioPort(
                        portname.str(),
                        direction,
                        // \todo: streaming backend expects indexing starting from 0
                        // but bebob reports it starting from 1. Decide where
                        // and how to handle this (pp: here)
                        // note that the descriptor mechanism specifies indexing from 0
                        channelInfo->m_streamPosition - 1,
                        channelInfo->m_location - 1,
                        Streaming::AmdtpPortInfo::E_MBLA
                );
                break;

            case ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
                p=new Streaming::AmdtpMidiPort(
                        portname.str(),
                        direction,
                        channelInfo->m_streamPosition,
                        // Workaround for out-of-spec hardware
                        // should be:
                        // channelInfo->m_location-1,
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
                    m_p1394Service->getHandle(),
                    m_pConfigRom->getNodeId() | 0xffc0,
                    (quadlet_t *)&opcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n",i);
                return false;
            }

            iso_channel=opcr.channel;
        } else {
            iso_channel=m_p1394Service->allocateIsoChannelCMP(
                m_pConfigRom->getNodeId() | 0xffc0, n,
                m_p1394Service->getLocalNodeId()| 0xffc0, -1);
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
                    m_p1394Service->getHandle(),
                    m_pConfigRom->getNodeId() | 0xffc0,
                    (quadlet_t *)&ipcr,
                    n)) {

                debugWarning("Error getting the channel for SP %d\n",i);
                return false;
            }

            iso_channel=ipcr.channel;

        } else {
            iso_channel=m_p1394Service->allocateIsoChannelCMP(
                m_p1394Service->getLocalNodeId()| 0xffc0, -1,
                m_pConfigRom->getNodeId() | 0xffc0, n);
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
            if(!m_p1394Service->freeIsoChannel(p->getChannel())) {
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
            if(!m_p1394Service->freeIsoChannel(p->getChannel())) {
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

template <typename T> bool serializeVector( Glib::ustring path,
                                            Util::IOSerialize& ser,
                                            const T& vec )
{
    bool result = true; // if vec.size() == 0
    int i = 0;
    for ( typename T::const_iterator it = vec.begin(); it != vec.end(); ++it ) {
        std::ostringstream strstrm;
        strstrm << path << i;
        result &= ( *it )->serialize( strstrm.str() + "/", ser );
        i++;
    }
    return result;
}

template <typename T, typename VT> bool deserializeVector( Glib::ustring path,
                                                           Util::IODeserialize& deser,
                                                           Unit& avDevice,
                                                           VT& vec )
{
    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << path << i << "/";
        T* ptr = T::deserialize( strstrm.str(),
                                 deser,
                                 avDevice );
        if ( ptr ) {
            vec.push_back( ptr );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

bool
AvDevice::serialize( Glib::ustring basePath,
                     Util::IOSerialize& ser ) const
{

    bool result;
    result  = m_pConfigRom->serialize( basePath + "m_pConfigRom/", ser );
    result &= ser.write( basePath + "m_verboseLevel", getDebugLevel() );
    result &= m_pPlugManager->serialize( basePath + "Plug", ser ); // serialize all av plugs
    result &= serializeVector( basePath + "PlugConnection", ser, m_plugConnections );
    result &= serializeVector( basePath + "Subunit", ser, m_subunits );
    result &= serializeSyncInfoVector( basePath + "SyncInfo", ser, m_syncInfos );

    int i = 0;
    for ( SyncInfoVector::const_iterator it = m_syncInfos.begin();
          it != m_syncInfos.end();
          ++it )
    {
        const SyncInfo& info = *it;
        if ( m_activeSyncInfo == &info ) {
            result &= ser.write( basePath + "m_activeSyncInfo",  i );
            break;
        }
        i++;
    }

    result &= serializeOptions( basePath + "Options", ser );

//     result &= ser.write( basePath + "m_id", id );

    return result;
}

AvDevice*
AvDevice::deserialize( Glib::ustring basePath,
                       Util::IODeserialize& deser,
                       Ieee1394Service& ieee1394Service )
{

    ConfigRom *configRom =
        ConfigRom::deserialize( basePath + "m_pConfigRom/", deser, ieee1394Service );

    if ( !configRom ) {
        return NULL;
    }

    AvDevice* pDev = new AvDevice(
        std::auto_ptr<ConfigRom>(configRom),
        ieee1394Service, configRom->getNodeId());

    if ( pDev ) {
        bool result;
        int verboseLevel;
        result  = deser.read( basePath + "m_verboseLevel", verboseLevel );
        setDebugLevel( verboseLevel );
        
        result &= AVC::Unit::deserialize(basePath, pDev, deser, ieee1394Service);

        result &= deserializeOptions( basePath + "Options", deser, *pDev );
    }

    return pDev;
}

} // end of namespace
