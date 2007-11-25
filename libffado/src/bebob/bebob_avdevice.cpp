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

#include "bebob/focusrite/focusrite_saffire.h"
#include "bebob/focusrite/focusrite_saffirepro.h"
#include "bebob/terratec/terratec_device.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "genericavc/avc_vendormodel.h"

#include "libavc/general/avc_plug_info.h"
#include "libavc/general/avc_extended_plug_info.h"
#include "libavc/general/avc_subunit_info.h"
#include "libavc/streamformat/avc_extended_stream_format.h"
#include "libutil/cmd_serialize.h"
#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

using namespace AVC;

namespace BeBoB {

AvDevice::AvDevice( Ieee1394Service& ieee1394service,
                    std::auto_ptr< ConfigRom >( configRom ) )
    : GenericAVC::AvDevice( ieee1394service, configRom )
    , m_Mixer ( 0 )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::AvDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    // DM1500 based devices seem to upset the linux1394 stack when commands are
    // sent too fast.
    if (AVC::AVCCommand::getSleepAfterAVCCommand() < 200) {
        AVC::AVCCommand::setSleepAfterAVCCommand( 200 );
    }

}

AvDevice::~AvDevice()
{
    destroyMixer();
}

bool
AvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_bebob.txt" );
    if ( vendorModel.parse() ) {
        return vendorModel.isPresent( vendorId, modelId );
    }

    return false;
}

FFADODevice *
AvDevice::createDevice( Ieee1394Service& ieee1394Service,
                        std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();
    unsigned int modelId = configRom->getModelId();

    switch (vendorId) {
        case FW_VENDORID_TERRATEC:
            return new Terratec::PhaseSeriesDevice(ieee1394Service, configRom);
        case FW_VENDORID_FOCUSRITE:
            switch(modelId) {
                case 0x00000003:
                case 0x00000006:
                    return new Focusrite::SaffireProDevice(ieee1394Service, configRom);
                case 0x00000000:
                    return new Focusrite::SaffireDevice(ieee1394Service, configRom);
                default: // return a plain BeBoB device
                    return new AvDevice(ieee1394Service, configRom);
           }
        default:
            return new AvDevice(ieee1394Service, configRom);
    }
    return NULL;
}

bool
AvDevice::discover()
{
    unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();

    GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_bebob.txt" );
    if ( vendorModel.parse() ) {
        m_model = vendorModel.find( vendorId, modelId );
    }

    if (GenericAVC::VendorModel::isValid(m_model)) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     m_model.vendor_name.c_str(),
                     m_model.model_name.c_str());
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

    if(!buildMixer()) {
        debugWarning("Could not build mixer\n");
    }
    return true;
}

bool
AvDevice::buildMixer()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a generic BeBoB mixer...\n");
    // create a Mixer
    // this removes the mixer if it already exists
    // note: a mixer self-registers to it's parent
    delete m_Mixer;

    // create the mixer & register it
    if(getAudioSubunit(0) == NULL) {
        debugWarning("Could not find audio subunit, mixer not available.\n");
        m_Mixer = NULL;
    } else {
        m_Mixer = new Mixer(*this);
    }
    return m_Mixer != NULL;
}

bool
AvDevice::destroyMixer()
{
    delete m_Mixer;
    return true;
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
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, 0 );
    extStreamFormatCmd.setPlugAddress( PlugAddress( PlugAddress::ePD_Input,
                                                    PlugAddress::ePAM_Unit,
                                                    unitPlugAddress ) );

    extStreamFormatCmd.setNodeId( getNodeId() );
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
                                     getNodeId() );
    extPlugInfoCmd.setPlugAddress( PlugAddress( ePlugDirection,
                                                PlugAddress::ePAM_Unit,
                                                unitPlugAddress ) );
    extPlugInfoCmd.setNodeId( getNodeId() );
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
    signalSourceCmd.setNodeId( getNodeId() );
    signalSourceCmd.setSubunitType( eST_Unit  );
    signalSourceCmd.setSubunitId( 0xff );

    signalSourceCmd.setCommandType( AVCCommand::eCT_Status );

    if ( !signalSourceCmd.fire() ) {
        debugError( "Signal source command failed\n" );
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

bool
AvDevice::serialize( Glib::ustring basePath,
                     Util::IOSerialize& ser ) const
{
    bool result;
    result  = GenericAVC::AvDevice::serialize( basePath, ser );
    return result;
}

bool
AvDevice::deserialize( Glib::ustring basePath,
                       Util::IODeserialize& deser )
{
    bool result;
    result  = GenericAVC::AvDevice::deserialize( basePath, deser );
    return result;
}

Glib::ustring
AvDevice::getCachePath()
{
    Glib::ustring cachePath;
    char* pCachePath;

    string path = CACHEDIR;
    if ( path.size() && path[0] == '~' ) {
        path.erase( 0, 1 ); // remove ~
        path.insert( 0, getenv( "HOME" ) ); // prepend the home path
    }

    if ( asprintf( &pCachePath, "%s/cache/",  path.c_str() ) < 0 ) {
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
    Glib::ustring sDevicePath = getCachePath() + m_pConfigRom->getGuidString();

    char* configId;
    asprintf(&configId, "%08x", getConfigurationId() );
    if ( !configId ) {
        debugError( "could not create id string\n" );
        return false;
    }

    Glib::ustring sFileName = sDevicePath + "/" + configId + ".xml";
    free( configId );
    debugOutput( DEBUG_LEVEL_NORMAL, "filename %s\n", sFileName.c_str() );

    struct stat buf;
    if ( stat( sFileName.c_str(), &buf ) != 0 ) {
        debugOutput( DEBUG_LEVEL_NORMAL,  "\"%s\" does not exist\n",  sFileName.c_str() );
        return false;
    } else {
        if ( !S_ISREG( buf.st_mode ) ) {
            debugOutput( DEBUG_LEVEL_NORMAL,  "\"%s\" is not a regular file\n",  sFileName.c_str() );
            return false;
        }
    }

    Util::XMLDeserialize deser( sFileName, getDebugLevel() );

    bool result = deserialize( "", deser );
    if ( result ) {
        debugOutput( DEBUG_LEVEL_NORMAL, "could create valid bebob driver from %s\n",
                     sFileName.c_str() );
    }

    return result;
}

bool
AvDevice::saveCache()
{
    // the path looks like this:
    // PATH_TO_CACHE + GUID + CONFIGURATION_ID
    string tmp_path = getCachePath() + m_pConfigRom->getGuidString();

    // the following piece should do something like
    // 'mkdir -p some/path/with/some/dirs/which/do/not/exist'
    vector<string> tokens;
    tokenize( tmp_path, tokens, "/" );
    string path;
    for ( vector<string>::const_iterator it = tokens.begin();
          it != tokens.end();
          ++it )
    {
        path +=  "/" + *it;

        struct stat buf;
        if ( stat( path.c_str(), &buf ) == 0 ) {
            if ( !S_ISDIR( buf.st_mode ) ) {
                debugError( "\"%s\" is not a directory\n",  path.c_str() );
                return false;
            }
        } else {
            if (  mkdir( path.c_str(), S_IRWXU | S_IRWXG ) != 0 ) {
                debugError( "Could not create \"%s\" directory\n", path.c_str() );
                return false;
            }
        }
    }

    // come up with an unique file name for the current settings
    char* configId;
    asprintf(&configId, "%08x", BeBoB::AvDevice::getConfigurationId() );
    if ( !configId ) {
        debugError( "Could not create id string\n" );
        return false;
    }
    string filename = path + "/" + configId + ".xml";
    free( configId );
    debugOutput( DEBUG_LEVEL_NORMAL, "filename %s\n", filename.c_str() );

    Util::XMLSerialize ser( filename );
    return serialize( "", ser );
}

} // end of namespace
