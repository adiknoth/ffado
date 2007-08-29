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

#include "config.h"

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/bebob_mixer.h"

#include "bebob/focusrite/focusrite.h"
#include "bebob/terratec/terratec.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "genericavc/avc_vendormodel.h"

#include "libavc/general/avc_plug_info.h"
#include "libavc/general/avc_extended_plug_info.h"
#include "libavc/general/avc_subunit_info.h"
#include "libavc/streamformat/avc_extended_stream_format.h"
#include "libavc/util/avc_serialize.h"
#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

using namespace AVC;

namespace BeBoB {

static GenericAVC::VendorModelEntry supportedDeviceList[] =
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

    {0x00130e, 0x00000000, "Focusrite", "Saffire (LE)"},
    {0x00130e, 0x00000003, "Focusrite", "Saffire Pro26IO"},
    {0x00130e, 0x00000006, "Focusrite", "Saffire Pro10IO"},

    {0x0040ab, 0x00010048, "EDIROL", "FA-101"},
    {0x0040ab, 0x00010049, "EDIROL", "FA-66"},

    {0x000d6c, 0x00010062, "M-Audio", "FW Solo"},
    {0x000d6c, 0x00010081, "M-Audio", "NRV10"},
};

AvDevice::AvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    : GenericAVC::AvDevice( configRom, ieee1394service, nodeId )
    , m_Mixer ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::AvDevice (NodeID %d)\n",
                 nodeId );
}

AvDevice::~AvDevice()
{
    if(m_Mixer != NULL) {
        delete m_Mixer;
    }
}

void
AvDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Device is a BeBoB device\n");
    GenericAVC::AvDevice::showDevice();
    flushDebugOutput();
}

AVC::Subunit*
AvDevice::createSubunit(AVC::Unit& unit,
                        AVC::ESubunitType type,
                        AVC::subunit_t id )
{
    AVC::Subunit* s=NULL;
    switch (type) {
        case eST_Audio:
            s=new BeBoB::SubunitAudio(unit, id );
            break;
        case eST_Music:
            s=new BeBoB::SubunitMusic(unit, id );
            break;
        default:
            s=NULL;
            break;
    }
    if(s) s->setVerboseLevel(getDebugLevel());
    return s;
}


AVC::Plug *
AvDevice::createPlug( AVC::Unit* unit,
                      AVC::Subunit* subunit,
                      AVC::function_block_type_t functionBlockType,
                      AVC::function_block_type_t functionBlockId,
                      AVC::Plug::EPlugAddressType plugAddressType,
                      AVC::Plug::EPlugDirection plugDirection,
                      AVC::plug_id_t plugId )
{

    Plug *p= new BeBoB::Plug( unit,
                              subunit,
                              functionBlockType,
                              functionBlockId,
                              plugAddressType,
                              plugDirection,
                              plugId );
    if (p) p->setVerboseLevel(getDebugLevel());
    return p;
}

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    //GenericAVC::VendorModel vendorModel( "/home/wagi/src/libffado/src/bebob/ffado_driver_bebob.txt" );

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( GenericAVC::VendorModelEntry ) );
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
          i < ( sizeof( supportedDeviceList )/sizeof( GenericAVC::VendorModelEntry ) );
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

    // create a Mixer
    // this removes the mixer if it already exists
    // note: a mixer self-registers to it's parent
    if(m_Mixer != NULL) {
        delete m_Mixer;
    }

//      create the mixer & register it
    if(getAudioSubunit(0) == NULL) {
        debugWarning("Could not find audio subunit, mixer not available.\n");
        m_Mixer = NULL;
    } else {
        m_Mixer = new Mixer(*this);
    }
    return true;
}

bool
AvDevice::propagatePlugInfo() {
    // we don't have to propagate since we discover things
    // another way
    debugOutput(DEBUG_LEVEL_VERBOSE, "Skip plug info propagation\n");
    return true;
}


int
AvDevice::getConfigurationIdSampleRate()
{
    ExtendedStreamFormatCmd extStreamFormatCmd( *m_p1394Service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     m_nodeId );
    extStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( m_nodeId );
    extStreamFormatCmd.setCommandType( AVCCommand::eCT_Status );
    extStreamFormatCmd.setVerbose( true );

    if ( !extStreamFormatCmd.fire() ) {
        debugError( "Stream format command failed\n" );
        return false;
    }

    FormatInformation* formatInfo =
        extStreamFormatCmd.getFormatInformation();
    FormatInformationStreamsCompound* compoundStream
        = dynamic_cast< FormatInformationStreamsCompound* > (
            formatInfo->m_streams );
    if ( compoundStream ) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sample rate 0x%02x\n",
                    compoundStream->m_samplingFrequency );
        return compoundStream->m_samplingFrequency;
    }

    debugError( "Could not retrieve sample rate\n" );
    return 0;
}

int
AvDevice::getConfigurationIdNumberOfChannel( PlugAddress::EPlugDirection ePlugDirection )
{
    ExtendedPlugInfoCmd extPlugInfoCmd( *m_p1394Service );
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR,
                                     m_nodeId );
    extPlugInfoCmd.setPlugAddress( PlugAddress( ePlugDirection,
                                                PlugAddress::ePAM_Unit,
                                                unitPlugAddress ) );
    extPlugInfoCmd.setNodeId( m_nodeId );
    extPlugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    extPlugInfoCmd.setVerbose( true );
    ExtendedPlugInfoInfoType extendedPlugInfoInfoType(
        ExtendedPlugInfoInfoType::eIT_NoOfChannels );
    extendedPlugInfoInfoType.initialize();
    extPlugInfoCmd.setInfoType( extendedPlugInfoInfoType );

    if ( !extPlugInfoCmd.fire() ) {
        debugError( "Number of channels command failed\n" );
        return false;
    }

    ExtendedPlugInfoInfoType* infoType = extPlugInfoCmd.getInfoType();
    if ( infoType
         && infoType->m_plugNrOfChns )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Number of channels 0x%02x\n",
                    infoType->m_plugNrOfChns->m_nrOfChannels );
        return infoType->m_plugNrOfChns->m_nrOfChannels;
    }

    debugError( "Could not retrieve number of channels\n" );
    return 0;
}

int
AvDevice::getConfigurationIdSyncMode()
{
    SignalSourceCmd signalSourceCmd( *m_p1394Service );
    SignalUnitAddress signalUnitAddr;
    signalUnitAddr.m_plugId = 0x01;
    signalSourceCmd.setSignalDestination( signalUnitAddr );
    signalSourceCmd.setNodeId( m_nodeId );
    signalSourceCmd.setSubunitType( eST_Unit  );
    signalSourceCmd.setSubunitId( 0xff );

    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Number of channels command failed\n" );
        return false;
    }

    SignalAddress* pSyncPlugSignalAddress = signalSourceCmd.getSignalSource();
    SignalSubunitAddress* pSyncPlugSubunitAddress
        = dynamic_cast<SignalSubunitAddress*>( pSyncPlugSignalAddress );
    if ( pSyncPlugSubunitAddress ) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sync mode 0x%02x\n",
                    ( pSyncPlugSubunitAddress->m_subunitType << 3
                      | pSyncPlugSubunitAddress->m_subunitId ) << 8
                    | pSyncPlugSubunitAddress->m_plugId );

        return ( pSyncPlugSubunitAddress->m_subunitType << 3
                 | pSyncPlugSubunitAddress->m_subunitId ) << 8
            | pSyncPlugSubunitAddress->m_plugId;
    }

    debugError( "Could not retrieve sync mode\n" );
    return 0;
}

int
AvDevice::getConfigurationId()
{
    // create a unique configuration id.
    int id = 0;
    id = getConfigurationIdSampleRate();
    id |= ( getConfigurationIdNumberOfChannel( PlugAddress::ePD_Input )
            + getConfigurationIdNumberOfChannel( PlugAddress::ePD_Output ) ) << 8;
    id |= getConfigurationIdSyncMode() << 16;

    return id;
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
    #warning broken at echoaudio merge
    //result &= serializeSyncInfoVector( basePath + "SyncInfo", ser, m_syncInfos );

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

//     ConfigRom *configRom =
//         ConfigRom::deserialize( basePath + "m_pConfigRom/", deser, ieee1394Service );
//
//     if ( !configRom ) {
//         return NULL;
//     }
//
//     AvDevice* pDev = new AvDevice(
//         std::auto_ptr<ConfigRom>(configRom),
//         ieee1394Service, configRom->getNodeId());
//
//     if ( pDev ) {
//         bool result;
//         int verboseLevel;
//         result  = deser.read( basePath + "m_verboseLevel", verboseLevel );
//         setDebugLevel( verboseLevel );
//
//         result &= AVC::Unit::deserialize(basePath, pDev, deser, ieee1394Service);
//
//         result &= deserializeOptions( basePath + "Options", deser, *pDev );
//     }
//
//     return pDev;
    return NULL;
}

Glib::ustring
AvDevice::getCachePath()
{
    Glib::ustring cachePath;
    char* pCachePath;
    if ( asprintf( &pCachePath, "%s/cache/libffado/",  CACHEDIR ) < 0 ) {
        debugError( "Could not create path string for cache pool (trying '/var/cache/libffado' instead)\n" );
        cachePath == "/var/cache/libffado/";
    } else {
        cachePath = pCachePath;
        free( pCachePath );
    }
    return cachePath;
}

bool
AvDevice::loadFromCache()
{
/*    Glib::ustring sDevicePath = getCachePath() + m_pConfigRom->getGuidString();

    char* configId;
    asprintf(&configId, "%08x", getConfigurationId() );
    if ( !configId ) {
        debugError( "could not create id string\n" );
        return false;
    }

    Glib::ustring sFileName = sDevicePath + "/" + configId + ".xml";
    free( configId );
    debugOutput( DEBUG_LEVEL_NORMAL, "filename %s\n", sFileName.c_str() );

    Util::XMLDeserialize deser( sFileName, m_verboseLevel );

    bool result = deserialize( "", deser );
    if ( result ) {
        debugOutput( DEBUG_LEVEL_NORMAL, "could create valid bebob driver from %s\n",
                     sFileName.c_str() );
    }

    return result;*/
    return false;
}

bool
AvDevice::saveCache()
{
//     // the path looks like this:
//     // PATH_TO_CACHE + GUID + CONFIGURATION_ID
//
//     Glib::ustring sDevicePath = getCachePath() + m_pConfigRom->getGuidString();
//     struct stat buf;
//     if ( stat( sDevicePath.c_str(), &buf ) == 0 ) {
//         if ( !S_ISDIR( buf.st_mode ) ) {
//             debugError( "\"%s\" is not a directory\n",  sDevicePath.c_str() );
//             return false;
//         }
//     } else {
//         if (  mkdir( sDevicePath.c_str(), S_IRWXU | S_IRWXG ) != 0 ) {
//             debugError( "Could not create \"%s\" directory\n", sDevicePath.c_str() );
//             return false;
//         }
//     }
//
//     char* configId;
//     asprintf(&configId, "%08x", BeBoB::AvDevice::getConfigurationId() );
//     if ( !configId ) {
//         debugError( "Could not create id string\n" );
//         return false;
//     }
//     Glib::ustring sFileName = sDevicePath + "/" + configId + ".xml";
//     free( configId );
//     debugOutput( DEBUG_LEVEL_NORMAL, "filename %s\n", sFileName.c_str() );
//
//     Util::XMLSerialize ser( sFileName );
//     return serialize( "", ser );
    return false;
}

} // end of namespace
