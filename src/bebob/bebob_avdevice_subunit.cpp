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

#include "libavc/general/avc_plug_info.h"
#include "libavc/streamformat/avc_extended_stream_format.h"
#include "libutil/cmd_serialize.h"

#include <sstream>

using namespace AVC;

//////////////////////////

BeBoB::SubunitAudio::SubunitAudio( AVC::Unit& avDevice,
                                   subunit_t id )
    : AVC::SubunitAudio( avDevice, id )
{
}

BeBoB::SubunitAudio::SubunitAudio()
    : AVC::SubunitAudio()
{
}

BeBoB::SubunitAudio::~SubunitAudio()
{
    for ( FunctionBlockVector::iterator it = m_functions.begin();
          it != m_functions.end();
          ++it )
    {
        delete *it;
    }
}

AVC::Plug *
BeBoB::SubunitAudio::createPlug( AVC::Unit* unit,
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

bool
BeBoB::SubunitAudio::discover()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering %s...\n", getName());
    
    // discover the AV/C generic part
    if ( !AVC::SubunitAudio::discover() ) {
        return false;
    }

    // do the remaining BeBoB audio subunit discovery
    if ( !discoverFunctionBlocks() ) {
        debugError( "function block discovering failed\n" );
        return false;
    }

    return true;
}

bool
BeBoB::SubunitAudio::discoverConnections()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering connections...\n");
    if ( !Subunit::discoverConnections() ) {
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
BeBoB::SubunitAudio::getName()
{
    return "BeBoB::AudioSubunit";
}

bool
BeBoB::SubunitAudio::discoverFunctionBlocks()
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
    if ((int)getDebugLevel() >= DEBUG_LEVEL_NORMAL) {
    
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
BeBoB::SubunitAudio::discoverFunctionBlocksDo(
    ExtendedSubunitInfoCmd::EFunctionBlockType fbType )
{
    int page = 0;
    bool cmdSuccess = false;
    bool finished = false;

    do {
        ExtendedSubunitInfoCmd
        extSubunitInfoCmd( m_unit->get1394Service() );
        extSubunitInfoCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
        extSubunitInfoCmd.setCommandType( AVCCommand::eCT_Status );
        extSubunitInfoCmd.setSubunitId( getSubunitId() );
        extSubunitInfoCmd.setSubunitType( getSubunitType() );
        extSubunitInfoCmd.setVerbose( (int)getDebugLevel() );

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
BeBoB::SubunitAudio::createFunctionBlock(
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
                                        (int)getDebugLevel() );
    }
    break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitFeature:
    {
        fb = new FunctionBlockFeature( *this,
                                       data.m_functionBlockId,
                                       purpose,
                                       data.m_noOfInputPlugs,
                                       data.m_noOfOutputPlugs,
                                       (int)getDebugLevel() );
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
                                                 (int)getDebugLevel() );
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
                                              (int)getDebugLevel() );
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
                                     (int)getDebugLevel() );
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
BeBoB::SubunitAudio::convertSpecialPurpose(
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
BeBoB::SubunitAudio::serializeChild( Glib::ustring basePath,
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
BeBoB::SubunitAudio::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               AVC::Unit& avDevice )
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

BeBoB::SubunitMusic::SubunitMusic( AVC::Unit& avDevice,
                                   subunit_t id )
    : AVC::SubunitMusic( avDevice, id )
{
}

BeBoB::SubunitMusic::SubunitMusic()
    : AVC::SubunitMusic()
{
}

BeBoB::SubunitMusic::~SubunitMusic()
{
}

AVC::Plug *
BeBoB::SubunitMusic::createPlug( AVC::Unit* unit,
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

bool
BeBoB::SubunitMusic::discover()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering %s...\n", getName());
    
    // discover the AV/C generic part
    if ( !AVC::SubunitMusic::discover() ) {
        return false;
    }

    // do the remaining BeBoB music subunit discovery
    // which is nothing

    return true;
}

const char*
BeBoB::SubunitMusic::getName()
{
    return "BeBoB::MusicSubunit";
}

bool
BeBoB::SubunitMusic::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::SubunitMusic::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               AVC::Unit& avDevice )
{
    return true;
}
