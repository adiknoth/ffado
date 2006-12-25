/* maudio_avdevice.cpp
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "maudio/maudio_avdevice.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"
#include "libfreebobavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <string>
#include <stdint.h>

namespace MAudio {

IMPL_DEBUG_MODULE( AvDevice, AvDevice, DEBUG_LEVEL_NORMAL );

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int iNodeId,
                    int iVerboseLevel )
    :  m_pConfigRom( configRom )
    , m_p1394Service( &ieee1394service )
    , m_iNodeId( iNodeId )
    , m_iVerboseLevel( iVerboseLevel )
    , m_pFilename( 0 )
    , m_id(0)
    , m_receiveProcessor ( 0 )
    , m_receiveProcessorBandwidth ( -1 )
    , m_transmitProcessor ( 0 )
    , m_transmitProcessorBandwidth ( -1 )
{
    setDebugLevel( iVerboseLevel );
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MAudio::AvDevice (NodeID %d)\n",
                 iNodeId );
}

AvDevice::~AvDevice()
{
}

ConfigRom&
AvDevice::getConfigRom() const
{
    return *m_pConfigRom;
}

struct VendorModelEntry {
    unsigned int m_iVendorId;
    unsigned int m_iModelId;
    const char*  m_pFilename;
};

static VendorModelEntry supportedDeviceList[] =
{
    //{0x0007f5, 0x00010048, "refdesign.xml"},  // BridgeCo, RD Audio1

    {0x000d6c, 0x00010046, "fw410.xml"},       // M-Audio, FW 410
    {0x000d6c, 0x00010058, "fw410.xml"},       // M-Audio, FW 410; Version 5.10.0.5036
};

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int iVendorId = configRom.getNodeVendorId();
    unsigned int iModelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].m_iVendorId == iVendorId )
             && ( supportedDeviceList[i].m_iModelId == iModelId ) )
        {
            return true;
        }
    }

    return false;
}

bool
AvDevice::discover()
{
    unsigned int iVendorId = m_pConfigRom->getNodeVendorId();
    unsigned int iModelId = m_pConfigRom->getModelId();
    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].m_iVendorId == iVendorId )
             && ( supportedDeviceList[i].m_iModelId == iModelId ) )
        {
            m_pFilename = supportedDeviceList[i].m_pFilename;
            break;
        }
    }

    return m_pFilename != 0;
}

bool
AvDevice::setSamplingFrequency( ESamplingFrequency eSamplingFrequency )
{
    // not supported
    return false;
}

int AvDevice::getSamplingFrequency( ) {
    return 44100;
}


void
AvDevice::showDevice() const
{
}

bool
AvDevice::addXmlDescription( xmlNodePtr pDeviceNode )
{
    char* pFilename;
    if ( asprintf( &pFilename, "%s/libfreebob/maudio/%s", DATADIR, m_pFilename ) < 0 ) {
        debugError( "addXmlDescription: Could not create filename string\n" );
        return false;
    }

    xmlDocPtr pDoc = xmlParseFile( pFilename );

    if ( !pDoc ) {
        debugError( "addXmlDescription: No file '%s' found'\n", pFilename );
        free( pFilename );
        return false;;
    }

    xmlNodePtr pCur = xmlDocGetRootElement( pDoc );
    if ( !pCur ) {
        debugError( "addXmlDescription: Document '%s' has not root element\n", pFilename );
        xmlFreeDoc( pDoc );
        free( pFilename );
        return false;
    }

    if ( xmlStrcmp( pCur->name, ( const xmlChar * ) "FreeBoBConnectionInfo" ) ) {
        debugError( "addXmlDescription: No node 'FreeBoBConnectionInfo' found\n" );
        xmlFreeDoc( pDoc );
        free( pFilename );
        return false;
    }

    pCur = pCur->xmlChildrenNode;
    while ( pCur ) {
        if ( !xmlStrcmp( pCur->name, ( const xmlChar * ) "Device" ) ) {
            break;
        }
        pCur = pCur->next;
    }

    if ( pCur ) {
        pCur = pCur->xmlChildrenNode;
        while ( pCur ) {
            if ( ( !xmlStrcmp( pCur->name, ( const xmlChar * ) "ConnectionSet" ) ) ) {
                xmlNodePtr pDevDesc = xmlCopyNode( pCur, 1 );
                if ( !pDevDesc ) {
                    debugError( "addXmlDescription: Could not copy node 'ConnectionSet'\n" );
                    xmlFreeDoc( pDoc );
                    free( pFilename );
                    return false;
                }

                // set correct node id
                for ( xmlNodePtr pNode = pDevDesc->xmlChildrenNode; pNode; pNode = pNode->next ) {
                    if ( ( !xmlStrcmp( pNode->name,  ( const xmlChar * ) "Connection" ) ) ) {
                        for ( xmlNodePtr pSubNode = pNode->xmlChildrenNode; pSubNode; pSubNode = pSubNode->next ) {
                            if ( ( !xmlStrcmp( pSubNode->name,  ( const xmlChar * ) "Node" ) ) ) {
                                char* result;
                                asprintf( &result, "%d", m_iNodeId );
                                xmlNodeSetContent( pSubNode, BAD_CAST result );
                                free( result );
                            }
                        }
                    }
                }

                xmlAddChild( pDeviceNode, pDevDesc );
            }
            if ( ( !xmlStrcmp( pCur->name, ( const xmlChar * ) "StreamFormats" ) ) ) {
                xmlNodePtr pDevDesc = xmlCopyNode( pCur, 1 );
                if ( !pDevDesc ) {
                    debugError( "addXmlDescription: Could not copy node 'StreamFormats'\n" );
                    xmlFreeDoc( pDoc );
                    free( pFilename );
                    return false;
                }
                xmlAddChild( pDeviceNode, pDevDesc );
            }

            pCur = pCur->next;
        }
    }

    xmlFreeDoc( pDoc );
    free( pFilename );

    return true;
}

bool AvDevice::setId( unsigned int id)
{
    // FIXME: decent ID system nescessary
    m_id=id;
    return true;
}

bool
AvDevice::prepare() {
/*    ///////////
    // get plugs

    AvPlug* inputplug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Input, 0 );
    if ( !inputplug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AvPlug* outputPlug = getPlugById( m_pcrPlugs, AvPlug::eAPD_Output, 0 );
    if ( !inputplug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    int samplerate=outputPlug->getSampleRate();
    m_receiveProcessor=new FreebobStreaming::AmdtpReceiveStreamProcessor(
                             m_p1394Service->getPort(),
                             samplerate,
                             outputPlug->getNrOfChannels());

    if(!m_receiveProcessor->init()) {
        debugFatal("Could not initialize receive processor!\n");
        return false;
    }

    if (!addPlugToProcessor(*outputPlug,m_receiveProcessor, 
        FreebobStreaming::AmdtpAudioPort::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        return false;
    }

    // do the transmit processor
//     if (m_snoopMode) {
//         // we are snooping, so these are receive too.
//         samplerate=inputPlug->getSampleRate();
//         m_receiveProcessor2=new FreebobStreaming::AmdtpReceiveStreamProcessor(
//                                   channel,
//                                   m_p1394Service->getPort(),
//                                   samplerate,
//                                   inputPlug->getNrOfChannels());
//         
//         if(!m_receiveProcessor2->init()) {
//             debugFatal("Could not initialize snooping receive processor!\n");
//             return false;
//         }
//         if (!addPlugToProcessor(*inputPlug,m_receiveProcessor2, 
//             FreebobStreaming::AmdtpAudioPort::E_Capture)) {
//             debugFatal("Could not add plug to processor!\n");
//             return false;
//         }
//     } else {
        // do the transmit processor
        samplerate=inputPlug->getSampleRate();
        m_transmitProcessor=new FreebobStreaming::AmdtpTransmitStreamProcessor(
                                m_p1394Service->getPort(),
                                samplerate,
                                inputPlug->getNrOfChannels());
                                
        if(!m_transmitProcessor->init()) {
            debugFatal("Could not initialize transmit processor!\n");
            return false;
        
        }
        
        // FIXME: do this the proper way!
        m_transmitProcessor->syncmaster=m_receiveProcessor;
    
        if (!addPlugToProcessor(*inputPlug,m_transmitProcessor, 
            FreebobStreaming::AmdtpAudioPort::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
//     }

    return true;*/
}

// bool
// AvDevice::addPlugToProcessor(
//     AvPlug& plug, 
//     FreebobStreaming::StreamProcessor *processor, 
//     FreebobStreaming::AmdtpAudioPort::E_Direction direction) {
// 
//     AvPlug::ClusterInfoVector& clusterInfos = plug.getClusterInfos();
//     for ( AvPlug::ClusterInfoVector::const_iterator it = clusterInfos.begin();
//           it != clusterInfos.end();
//           ++it )
//     {
//         const AvPlug::ClusterInfo* clusterInfo = &( *it );
// 
//         AvPlug::ChannelInfoVector channelInfos = clusterInfo->m_channelInfos;
//         for ( AvPlug::ChannelInfoVector::const_iterator it = channelInfos.begin();
//               it != channelInfos.end();
//               ++it )
//         {
//             const AvPlug::ChannelInfo* channelInfo = &( *it );
//             std::ostringstream portname;
//             
//             portname << "dev" << m_id << "_" << channelInfo->m_name;
//             
//             FreebobStreaming::Port *p=NULL;
//             switch(clusterInfo->m_portType) {
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_Speaker:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_Headphone:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_Microphone:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_Line:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_Analog:
//                 p=new FreebobStreaming::AmdtpAudioPort(
//                         portname.str(),
//                         direction, 
//                         // \todo: streaming backend expects indexing starting from 0
//                         // but bebob reports it starting from 1. Decide where
//                         // and how to handle this (pp: here)
//                         channelInfo->m_streamPosition - 1, 
//                         channelInfo->m_location, 
//                         FreebobStreaming::AmdtpPortInfo::E_MBLA, 
//                         clusterInfo->m_portType
//                 );
//                 break;
// 
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_MIDI:
//                 p=new FreebobStreaming::AmdtpMidiPort(
//                         portname.str(),
//                         direction, 
//                         // \todo: streaming backend expects indexing starting from 0
//                         // but bebob reports it starting from 1. Decide where
//                         // and how to handle this (pp: here)
//                         channelInfo->m_streamPosition - 1, 
//                         channelInfo->m_location, 
//                         FreebobStreaming::AmdtpPortInfo::E_Midi, 
//                         clusterInfo->m_portType
//                 );
// 
//                 break;
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_SPDIF:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_ADAT:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_TDIF:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_MADI:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_Digital:
//             case ExtendedPlugInfoClusterInfoSpecificData::ePT_NoType:
//             default:
//             // unsupported
//                 break;
//             }
// 
//             if (!p) {
//                 debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",channelInfo->m_name.c_str());
//             } else {
//         
//                 if (!processor->addPort(p)) {
//                     debugWarning("Could not register port with stream processor\n");
//                     return false;
//                 }
//             }
//          }
//     }
//     return true;
// }

int 
AvDevice::getStreamCount() {
    return 0;
//     return 2; // one receive, one transmit
}

FreebobStreaming::StreamProcessor *
AvDevice::getStreamProcessorByIndex(int i) {
    switch (i) {
    case 0:
        return m_receiveProcessor;
    case 1:
//         if (m_snoopMode) {
//             return m_receiveProcessor2;
//         } else {
            return m_transmitProcessor;
//         }
    default:
        return NULL;
    }
    return 0;
}

int
AvDevice::startStreamByIndex(int i) {
    int iso_channel=0;
    int plug=0;
    int hostplug=-1;
    
//     if (m_snoopMode) {
//     
//         switch (i) {
//         case 0:
//             // snooping doesn't use CMP, but obtains the info of the channel
//             // from the target plug
//             
//             // TODO: get isochannel from plug
//             
//             // set the channel obtained by the connection management
//             m_receiveProcessor->setChannel(iso_channel);
//             break;
//         case 1:
//             // snooping doesn't use CMP, but obtains the info of the channel
//             // from the target plug
//             
//             // TODO: get isochannel from plug
//             
//             // set the channel obtained by the connection management
//             m_receiveProcessor2->setChannel(iso_channel);
// 
//             break;
//         default:
//             return 0;
//         }
//     } else {
    
        switch (i) {
        case 0:
            // do connection management: make connection
            iso_channel = iec61883_cmp_connect(
                m_p1394Service->getHandle(), 
                m_iNodeId | 0xffc0, 
                &plug,
                raw1394_get_local_id (m_p1394Service->getHandle()), 
                &hostplug, 
                &m_receiveProcessorBandwidth);
            
            // set the channel obtained by the connection management
            m_receiveProcessor->setChannel(iso_channel);
            break;
        case 1:
            // do connection management: make connection
            iso_channel = iec61883_cmp_connect(
                m_p1394Service->getHandle(), 
                raw1394_get_local_id (m_p1394Service->getHandle()), 
                &hostplug, 
                m_iNodeId | 0xffc0, 
                &plug,
                &m_transmitProcessorBandwidth);
            
            // set the channel obtained by the connection management
            m_transmitProcessor->setChannel(iso_channel);
            break;
        default:
            return 0;
        }
//     }
    
    return 0;

}

int
AvDevice::stopStreamByIndex(int i) {
    // do connection management: break connection

    int plug=0;
    int hostplug=-1;
//     if (m_snoopMode) {
//         switch (i) {
//         case 0:
//             // do connection management: break connection
//     
//             break;
//         case 1:
//             // do connection management: break connection
// 
//             break;
//         default:
//             return 0;
//         }
//     } else {
        switch (i) {
        case 0:
            // do connection management: break connection
            iec61883_cmp_disconnect(
                m_p1394Service->getHandle(), 
                m_iNodeId | 0xffc0, 
                plug,
                raw1394_get_local_id (m_p1394Service->getHandle()), 
                hostplug, 
                m_receiveProcessor->getChannel(),
                m_receiveProcessorBandwidth);
    
            break;
        case 1:
            // do connection management: break connection
            iec61883_cmp_disconnect(
                m_p1394Service->getHandle(), 
                raw1394_get_local_id (m_p1394Service->getHandle()), 
                hostplug, 
                m_iNodeId | 0xffc0, 
                plug,
                m_transmitProcessor->getChannel(),
                m_transmitProcessorBandwidth);

            break;
        default:
            return 0;
        }
//     }
    
    return 0;
}

}
