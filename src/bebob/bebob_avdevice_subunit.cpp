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

#include "bebob/bebob_functionblock.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avplug.h"
#include "libieee1394/configrom.h"

#include "libavc/avc_plug_info.h"
#include "libavc/avc_extended_stream_format.h"
#include "libavc/avc_serialize.h"

#include <sstream>

IMPL_DEBUG_MODULE( BeBoB::AvDeviceSubunit, BeBoB::AvDeviceSubunit, DEBUG_LEVEL_VERBOSE );

////////////////////////////////////////////

BeBoB::AvDeviceSubunit::AvDeviceSubunit( AvDevice& avDevice,
                                         AVCCommand::ESubunitType type,
                                         subunit_t id,
                                         int verboseLevel )
    : m_avDevice( &avDevice )
    , m_sbType( type )
    , m_sbId( id )
    , m_verboseLevel( verboseLevel )
{
    setDebugLevel( m_verboseLevel );
}

BeBoB::AvDeviceSubunit::AvDeviceSubunit()
{
}

BeBoB::AvDeviceSubunit::~AvDeviceSubunit()
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }
}

bool
BeBoB::AvDeviceSubunit::discover()
{
    if ( !discoverPlugs() ) {
        debugError( "plug discovering failed\n" );
        return false;
    }

    return true;
}

bool
BeBoB::AvDeviceSubunit::discoverPlugs()
{
    PlugInfoCmd plugInfoCmd( m_avDevice->get1394Service(),
                             PlugInfoCmd::eSF_SerialBusIsochronousAndExternalPlug );
    plugInfoCmd.setNodeId( m_avDevice->getConfigRom().getNodeId() );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setSubunitType( m_sbType );
    plugInfoCmd.setSubunitId( m_sbId );
    plugInfoCmd.setVerbose( m_verboseLevel );

    if ( !plugInfoCmd.fire() ) {
        debugError( "plug info command failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_NORMAL, "number of source plugs = %d\n",
                 plugInfoCmd.m_sourcePlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of destination output "
                 "plugs = %d\n", plugInfoCmd.m_destinationPlugs );

    if ( !discoverPlugs( AvPlug::eAPD_Input,
                         plugInfoCmd.m_destinationPlugs ) )
    {
        debugError( "destination plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugs(  AvPlug::eAPD_Output,
                          plugInfoCmd.m_sourcePlugs ) )
    {
        debugError( "source plug discovering failed\n" );
        return false;
    }

    return true;
}

bool
BeBoB::AvDeviceSubunit::discoverConnections()
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "plug connection discovering failed ('%s')\n",
                        plug->getName() );
            return false;
        }
    }

    return true;
}

bool
BeBoB::AvDeviceSubunit::discoverPlugs(AvPlug::EAvPlugDirection plugDirection,
                                      plug_id_t plugMaxId )
{
    for ( int plugIdx = 0;
          plugIdx < plugMaxId;
          ++plugIdx )
    {
        AVCCommand::ESubunitType subunitType =
            static_cast<AVCCommand::ESubunitType>( getSubunitType() );
        AvPlug* plug = new AvPlug( m_avDevice->get1394Service(),
                                   m_avDevice->getConfigRom(),
                                   m_avDevice->getPlugManager(),
                                   subunitType,
                                   getSubunitId(),
                                   0xff,
                                   0xff,
                                   AvPlug::eAPA_SubunitPlug,
                                   plugDirection,
                                   plugIdx,
                                   m_verboseLevel );
        if ( !plug || !plug->discover() ) {
            debugError( "plug discover failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "plug '%s' found\n",
                     plug->getName() );
        m_plugs.push_back( plug );
    }
    return true;
}

bool
BeBoB::AvDeviceSubunit::addPlug( AvPlug& plug )
{
    m_plugs.push_back( &plug );
    return true;
}


BeBoB::AvPlug*
BeBoB::AvDeviceSubunit::getPlug(AvPlug::EAvPlugDirection direction, plug_id_t plugId)
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( ( plug->getPlugId() == plugId )
            && ( plug->getDirection() == direction ) )
        {
            return plug;
        }
    }
    return 0;
}


bool
BeBoB::AvDeviceSubunit::serialize( Glib::ustring basePath,
                                   Util::IOSerialize& ser ) const
{
    bool result;

    result  = ser.write( basePath + "m_sbType", m_sbType );
    result &= ser.write( basePath + "m_sbId", m_sbId );
    result &= ser.write( basePath + "m_verboseLevel", m_verboseLevel );
    result &= serializeChild( basePath, ser );

    return result;
}

BeBoB::AvDeviceSubunit*
BeBoB::AvDeviceSubunit::deserialize( Glib::ustring basePath,
                                     Util::IODeserialize& deser,
                                     AvDevice& avDevice )
{
    bool result;
    AVCCommand::ESubunitType sbType;
    result  = deser.read( basePath + "m_sbType", sbType );

    AvDeviceSubunit* pSubunit = 0;
    switch( sbType ) {
    case AVCCommand::eST_Audio:
        pSubunit = new AvDeviceSubunitAudio;
        break;
    case AVCCommand::eST_Music:
        pSubunit = new AvDeviceSubunitMusic;
        break;
    default:
        pSubunit = 0;
    }

    if ( !pSubunit ) {
        return 0;
    }

    pSubunit->m_avDevice = &avDevice;
    pSubunit->m_sbType = sbType;
    result &= deser.read( basePath + "m_sbId", pSubunit->m_sbId );
    result &= deser.read( basePath + "m_verboseLevel", pSubunit->m_verboseLevel );
    result &= pSubunit->deserializeChild( basePath, deser, avDevice );

    if ( !result ) {
        delete pSubunit;
        return 0;
    }

    return pSubunit;
}

////////////////////////////////////////////

BeBoB::AvDeviceSubunitAudio::AvDeviceSubunitAudio( AvDevice& avDevice,
                                                   subunit_t id,
                                                   int verboseLevel )
    : AvDeviceSubunit( avDevice, AVCCommand::eST_Audio, id, verboseLevel )
{
}

BeBoB::AvDeviceSubunitAudio::AvDeviceSubunitAudio()
    : AvDeviceSubunit()
{
}

BeBoB::AvDeviceSubunitAudio::~AvDeviceSubunitAudio()
{
    for ( FunctionBlockVector::iterator it = m_functions.begin();
          it != m_functions.end();
          ++it )
    {
        delete *it;
    }
}

bool
BeBoB::AvDeviceSubunitAudio::discover()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering Audio Subunit...\n");
    
    if ( !AvDeviceSubunit::discover() ) {
        return false;
    }

    if ( !discoverFunctionBlocks() ) {
        debugError( "function block discovering failed\n" );
        return false;
    }

    return true;
}

bool
BeBoB::AvDeviceSubunitAudio::discoverConnections()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering connections...\n");
    if ( !AvDeviceSubunit::discoverConnections() ) {
        return false;
    }

    for ( FunctionBlockVector::iterator it = m_functions.begin();
          it != m_functions.end();
          ++it )
    {
        FunctionBlock* function = *it;
        if ( !function->discoverConnections() ) {
            debugError( "functionblock connection discovering failed ('%s')\n",
                        function->getName() );
            return false;
        }
    }

    return true;
}

const char*
BeBoB::AvDeviceSubunitAudio::getName()
{
    return "AudioSubunit";
}

bool
BeBoB::AvDeviceSubunitAudio::discoverFunctionBlocks()
{
    debugOutput( DEBUG_LEVEL_NORMAL,
                 "Discovering function blocks...\n");

    if ( !discoverFunctionBlocksDo(
             ExtendedSubunitInfoCmd::eFBT_AudioSubunitSelector) )
    {
        debugError( "Could not discover function block selector\n" );
        return false;
    }
    if ( !discoverFunctionBlocksDo(
             ExtendedSubunitInfoCmd::eFBT_AudioSubunitFeature) )
    {
        debugError( "Could not discover function block feature\n" );
        return false;
    }
    if ( !discoverFunctionBlocksDo(
             ExtendedSubunitInfoCmd::eFBT_AudioSubunitProcessing) )
    {
        debugError( "Could not discover function block processing\n" );
        return false;
    }
    if ( !discoverFunctionBlocksDo(
             ExtendedSubunitInfoCmd::eFBT_AudioSubunitCodec) )
    {
        debugError( "Could not discover function block codec\n" );
        return false;
    }

    // print a function block list
#ifdef DEBUG
    if (getDebugLevel() >= DEBUG_LEVEL_NORMAL) {
    
        for ( FunctionBlockVector::iterator it = m_functions.begin();
            it != m_functions.end();
            ++it )
        {
            debugOutput(DEBUG_LEVEL_NORMAL, "%20s FB, type 0x%X, id=%d\n",
                (*it)->getName(),
                (*it)->getType(),
                (*it)->getId());
        }
    }
#endif

    return true;
}

bool
BeBoB::AvDeviceSubunitAudio::discoverFunctionBlocksDo(
    ExtendedSubunitInfoCmd::EFunctionBlockType fbType )
{
    int page = 0;
    bool cmdSuccess = false;
    bool finished = false;

    do {
        ExtendedSubunitInfoCmd
            extSubunitInfoCmd( m_avDevice->get1394Service() );
        extSubunitInfoCmd.setNodeId( m_avDevice->getConfigRom().getNodeId() );
        extSubunitInfoCmd.setCommandType( AVCCommand::eCT_Status );
        extSubunitInfoCmd.setSubunitId( getSubunitId() );
        extSubunitInfoCmd.setSubunitType( getSubunitType() );
        extSubunitInfoCmd.setVerbose( m_verboseLevel );

        extSubunitInfoCmd.m_fbType = fbType;
        extSubunitInfoCmd.m_page = page;

        cmdSuccess = extSubunitInfoCmd.fire();
        if ( cmdSuccess
             && ( extSubunitInfoCmd.getResponse()
                  == AVCCommand::eR_Implemented ) )
        {
            for ( ExtendedSubunitInfoPageDataVector::iterator it =
                      extSubunitInfoCmd.m_infoPageDatas.begin();
                  cmdSuccess
                      && ( it != extSubunitInfoCmd.m_infoPageDatas.end() );
                  ++it )
            {
                cmdSuccess = createFunctionBlock( fbType, **it );
            }
            if ( ( extSubunitInfoCmd.m_infoPageDatas.size() != 0 )
                 && ( extSubunitInfoCmd.m_infoPageDatas.size() == 5 ) )
            {
                page++;
            } else {
                finished = true;
            }
        } else {
            finished = true;
        }
    } while ( cmdSuccess && !finished );

    return cmdSuccess;
}

bool
BeBoB::AvDeviceSubunitAudio::createFunctionBlock(
    ExtendedSubunitInfoCmd::EFunctionBlockType fbType,
    ExtendedSubunitInfoPageData& data )
{
    FunctionBlock::ESpecialPurpose purpose
        = convertSpecialPurpose(  data.m_functionBlockSpecialPupose );

    FunctionBlock* fb = 0;

    switch ( fbType ) {
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitSelector:
    {
        fb = new FunctionBlockSelector( *this,
                                        data.m_functionBlockId,
                                        purpose,
                                        data.m_noOfInputPlugs,
                                        data.m_noOfOutputPlugs,
                                        m_verboseLevel );
    }
    break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitFeature:
    {
        fb = new FunctionBlockFeature( *this,
                                       data.m_functionBlockId,
                                       purpose,
                                       data.m_noOfInputPlugs,
                                       data.m_noOfOutputPlugs,
                                       m_verboseLevel );
    }
    break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitProcessing:
    {
        switch ( data.m_functionBlockType ) {
        case ExtendedSubunitInfoCmd::ePT_EnhancedMixer:
        {
            fb = new FunctionBlockEnhancedMixer( *this,
                                                 data.m_functionBlockId,
                                                 purpose,
                                                 data.m_noOfInputPlugs,
                                                 data.m_noOfOutputPlugs,
                                                 m_verboseLevel );
        }
        break;
        case ExtendedSubunitInfoCmd::ePT_Mixer:
        case ExtendedSubunitInfoCmd::ePT_Generic:
        case ExtendedSubunitInfoCmd::ePT_UpDown:
        case ExtendedSubunitInfoCmd::ePT_DolbyProLogic:
        case ExtendedSubunitInfoCmd::ePT_3DStereoExtender:
        case ExtendedSubunitInfoCmd::ePT_Reverberation:
        case ExtendedSubunitInfoCmd::ePT_Chorus:
        case ExtendedSubunitInfoCmd::ePT_DynamicRangeCompression:
        default:
            fb = new FunctionBlockProcessing( *this,
                                              data.m_functionBlockId,
                                              purpose,
                                              data.m_noOfInputPlugs,
                                              data.m_noOfOutputPlugs,
                                              m_verboseLevel );
            debugWarning( "Dummy function block processing created. "
                          "Implementation is missing\n" );
        }
    }
    break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitCodec:
    {
        fb = new FunctionBlockCodec( *this,
                                     data.m_functionBlockId,
                                     purpose,
                                     data.m_noOfInputPlugs,
                                     data.m_noOfOutputPlugs,
                                     m_verboseLevel );
        debugWarning( "Dummy function block codec created. "
                      "Implementation is missing\n" );
    }
    break;
    default:
        debugError( "Unhandled function block type found\n" );
        return false;
    }

    if ( !fb ) {
        debugError( "Could create function block\n" );
        return false;
    }
    if ( !fb->discover() ) {
        debugError( "Could not discover function block %s\n",
                    fb->getName() );
        delete fb;
        return false;
    }
    m_functions.push_back( fb );

    return true;
}

BeBoB::FunctionBlock::ESpecialPurpose
BeBoB::AvDeviceSubunitAudio::convertSpecialPurpose(
    function_block_special_purpose_t specialPurpose )
{
    FunctionBlock::ESpecialPurpose p;
    switch ( specialPurpose ) {
    case ExtendedSubunitInfoPageData::eSP_InputGain:
        p  = FunctionBlock::eSP_InputGain;
        break;
    case ExtendedSubunitInfoPageData::eSP_OutputVolume:
        p = FunctionBlock::eSP_OutputVolume;
    break;
    default:
        p = FunctionBlock::eSP_NoSpecialPurpose;
    }
    return p;
}

bool
BeBoB::AvDeviceSubunitAudio::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    bool result = true;
    int i = 0;

    for ( FunctionBlockVector::const_iterator it = m_functions.begin();
          it != m_functions.end();
          ++it )
    {
        FunctionBlock* pFB = *it;
        std::ostringstream strstrm;
        strstrm << basePath << "FunctionBlock" << i << "/";

        result &= pFB->serialize( strstrm.str() , ser );

        i++;
    }

    return result;
}

bool
BeBoB::AvDeviceSubunitAudio::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               AvDevice& avDevice )
{
    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << "FunctionBlock" << i << "/";
        FunctionBlock* pFB = FunctionBlock::deserialize( strstrm.str(),
                                                         deser,
                                                         avDevice,
                                                         *this );
        if ( pFB ) {
            m_functions.push_back( pFB );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

////////////////////////////////////////////

BeBoB::AvDeviceSubunitMusic::AvDeviceSubunitMusic( AvDevice& avDevice,
                                                   subunit_t id,
                                                   int verboseLevel )
    : AvDeviceSubunit( avDevice, AVCCommand::eST_Music, id, verboseLevel )
{
}

BeBoB::AvDeviceSubunitMusic::AvDeviceSubunitMusic()
    : AvDeviceSubunit()
{
}

BeBoB::AvDeviceSubunitMusic::~AvDeviceSubunitMusic()
{
}

const char*
BeBoB::AvDeviceSubunitMusic::getName()
{
    return "MusicSubunit";
}

bool
BeBoB::AvDeviceSubunitMusic::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::AvDeviceSubunitMusic::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               AvDevice& avDevice )
{
    return true;
}
