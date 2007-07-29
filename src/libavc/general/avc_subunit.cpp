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


#include "avc_subunit.h"
#include "avc_unit.h"
#include "bebob/bebob_avplug.h"
#include "libieee1394/configrom.h"

#include "../general/avc_plug_info.h"
#include "../streamformat/avc_extended_stream_format.h"
#include "../util/avc_serialize.h"

#include <sstream>

namespace AVC {

IMPL_DEBUG_MODULE( Subunit, Subunit, DEBUG_LEVEL_VERBOSE );

////////////////////////////////////////////

Subunit::Subunit( Unit& unit,
                  ESubunitType type,
                  subunit_t id )
    : m_unit( &unit )
    , m_sbType( type )
    , m_sbId( id )
{

}

Subunit::Subunit()
{
}

Subunit::~Subunit()
{
    for ( PlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }
}

bool
Subunit::discover()
{
    // There is nothing we can discover for a generic subunit
    // Maybe its plugs, but there are multiple ways of doing that.
    return true;
}

bool
Subunit::addPlug( Plug& plug )
{
    m_plugs.push_back( &plug );
    return true;
}


Plug*
Subunit::getPlug(Plug::EPlugDirection direction, plug_id_t plugId)
{
    for ( PlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( ( plug->getPlugId() == plugId )
            && ( plug->getDirection() == direction ) )
        {
            return plug;
        }
    }
    return 0;
}

bool
Subunit::serialize( Glib::ustring basePath,
                                   Util::IOSerialize& ser ) const
{
    bool result;

    result  = ser.write( basePath + "m_sbType", m_sbType );
    result &= ser.write( basePath + "m_sbId", m_sbId );
    result &= ser.write( basePath + "m_verboseLevel", getDebugLevel() );
    result &= serializeChild( basePath, ser );

    return result;
}

Subunit*
Subunit::deserialize( Glib::ustring basePath,
                                     Util::IODeserialize& deser,
                                     Unit& unit )
{
    bool result;
    ESubunitType sbType;
    result  = deser.read( basePath + "m_sbType", sbType );

    Subunit* pSubunit = 0;
    switch( sbType ) {
    case eST_Audio:
        pSubunit = new SubunitAudio;
        break;
    case eST_Music:
        pSubunit = new SubunitMusic;
        break;
    default:
        pSubunit = 0;
    }

    if ( !pSubunit ) {
        return 0;
    }

    pSubunit->m_unit = &unit;
    pSubunit->m_sbType = sbType;
    result &= deser.read( basePath + "m_sbId", pSubunit->m_sbId );
    int verboseLevel;
    result &= deser.read( basePath + "m_verboseLevel", verboseLevel );
    setDebugLevel(verboseLevel);
    result &= pSubunit->deserializeChild( basePath, deser, unit );

    if ( !result ) {
        delete pSubunit;
        return 0;
    }

    return pSubunit;
}

////////////////////////////////////////////

SubunitAudio::SubunitAudio( Unit& unit, subunit_t id )
    : Subunit( unit, eST_Audio, id )
{
}

SubunitAudio::SubunitAudio()
    : Subunit()
{
}

SubunitAudio::~SubunitAudio()
{
    for ( BeBoB::FunctionBlockVector::iterator it = m_functions.begin();
          it != m_functions.end();
          ++it )
    {
        delete *it;
    }
}

bool
SubunitAudio::discover()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering Audio Subunit...\n");
    
    if ( !Subunit::discover() ) {
        return false;
    }

//     if ( !discoverBeBoB::FunctionBlocks() ) {
//         debugError( "function block discovering failed\n" );
//         return false;
//     }

    return true;
}


const char*
SubunitAudio::getName()
{
    return "AudioSubunit";
}

// bool
// SubunitAudio::discoverConnections()
// {
//     debugOutput(DEBUG_LEVEL_NORMAL, "Discovering connections...\n");
//     if ( !Subunit::discoverConnections() ) {
//         return false;
//     }
// 
//     for ( BeBoB::FunctionBlockVector::iterator it = m_functions.begin();
//           it != m_functions.end();
//           ++it )
//     {
//         BeBoB::FunctionBlock* function = *it;
//         if ( !function->discoverConnections() ) {
//             debugError( "functionblock connection discovering failed ('%s')\n",
//                         function->getName() );
//             return false;
//         }
//     }
// 
//     return true;
// }
// bool
// SubunitAudio::discoverBeBoB::FunctionBlocks()
// {
//     debugOutput( DEBUG_LEVEL_NORMAL,
//                  "Discovering function blocks...\n");
// 
//     if ( !discoverBeBoB::FunctionBlocksDo(
//              ExtendedSubunitInfoCmd::eFBT_AudioSubunitSelector) )
//     {
//         debugError( "Could not discover function block selector\n" );
//         return false;
//     }
//     if ( !discoverBeBoB::FunctionBlocksDo(
//              ExtendedSubunitInfoCmd::eFBT_AudioSubunitFeature) )
//     {
//         debugError( "Could not discover function block feature\n" );
//         return false;
//     }
//     if ( !discoverBeBoB::FunctionBlocksDo(
//              ExtendedSubunitInfoCmd::eFBT_AudioSubunitProcessing) )
//     {
//         debugError( "Could not discover function block processing\n" );
//         return false;
//     }
//     if ( !discoverBeBoB::FunctionBlocksDo(
//              ExtendedSubunitInfoCmd::eFBT_AudioSubunitCodec) )
//     {
//         debugError( "Could not discover function block codec\n" );
//         return false;
//     }
// 
//     // print a function block list
// #ifdef DEBUG
//     if (getDebugLevel() >= DEBUG_LEVEL_NORMAL) {
//     
//         for ( BeBoB::FunctionBlockVector::iterator it = m_functions.begin();
//             it != m_functions.end();
//             ++it )
//         {
//             debugOutput(DEBUG_LEVEL_NORMAL, "%20s FB, type 0x%X, id=%d\n",
//                 (*it)->getName(),
//                 (*it)->getType(),
//                 (*it)->getId());
//         }
//     }
// #endif
// 
//     return true;
// }
// 
// bool
// SubunitAudio::discoverBeBoB::FunctionBlocksDo(
//     ExtendedSubunitInfoCmd::EBeBoB::FunctionBlockType fbType )
// {
//     int page = 0;
//     bool cmdSuccess = false;
//     bool finished = false;
// 
//     do {
//         ExtendedSubunitInfoCmd
//             extSubunitInfoCmd( m_unit->get1394Service() );
//         extSubunitInfoCmd.setNodeId( m_unit->getConfigRom().getNodeId() );
//         extSubunitInfoCmd.setCommandType( AVCCommand::eCT_Status );
//         extSubunitInfoCmd.setSubunitId( getSubunitId() );
//         extSubunitInfoCmd.setSubunitType( getSubunitType() );
//         extSubunitInfoCmd.setVerbose( m_verboseLevel );
// 
//         extSubunitInfoCmd.m_fbType = fbType;
//         extSubunitInfoCmd.m_page = page;
// 
//         cmdSuccess = extSubunitInfoCmd.fire();
//         if ( cmdSuccess
//              && ( extSubunitInfoCmd.getResponse()
//                   == AVCCommand::eR_Implemented ) )
//         {
//             for ( ExtendedSubunitInfoPageDataVector::iterator it =
//                       extSubunitInfoCmd.m_infoPageDatas.begin();
//                   cmdSuccess
//                       && ( it != extSubunitInfoCmd.m_infoPageDatas.end() );
//                   ++it )
//             {
//                 cmdSuccess = createBeBoB::FunctionBlock( fbType, **it );
//             }
//             if ( ( extSubunitInfoCmd.m_infoPageDatas.size() != 0 )
//                  && ( extSubunitInfoCmd.m_infoPageDatas.size() == 5 ) )
//             {
//                 page++;
//             } else {
//                 finished = true;
//             }
//         } else {
//             finished = true;
//         }
//     } while ( cmdSuccess && !finished );
// 
//     return cmdSuccess;
// }
// 
// bool
// SubunitAudio::createBeBoB::FunctionBlock(
//     ExtendedSubunitInfoCmd::EBeBoB::FunctionBlockType fbType,
//     ExtendedSubunitInfoPageData& data )
// {
//     BeBoB::FunctionBlock::ESpecialPurpose purpose
//         = convertSpecialPurpose(  data.m_functionBlockSpecialPupose );
// 
//     BeBoB::FunctionBlock* fb = 0;
// 
//     switch ( fbType ) {
//     case ExtendedSubunitInfoCmd::eFBT_AudioSubunitSelector:
//     {
//         fb = new BeBoB::FunctionBlockSelector( *this,
//                                         data.m_functionBlockId,
//                                         purpose,
//                                         data.m_noOfInputPlugs,
//                                         data.m_noOfOutputPlugs,
//                                         m_verboseLevel );
//     }
//     break;
//     case ExtendedSubunitInfoCmd::eFBT_AudioSubunitFeature:
//     {
//         fb = new BeBoB::FunctionBlockFeature( *this,
//                                        data.m_functionBlockId,
//                                        purpose,
//                                        data.m_noOfInputPlugs,
//                                        data.m_noOfOutputPlugs,
//                                        m_verboseLevel );
//     }
//     break;
//     case ExtendedSubunitInfoCmd::eFBT_AudioSubunitProcessing:
//     {
//         switch ( data.m_functionBlockType ) {
//         case ExtendedSubunitInfoCmd::ePT_EnhancedMixer:
//         {
//             fb = new BeBoB::FunctionBlockEnhancedMixer( *this,
//                                                  data.m_functionBlockId,
//                                                  purpose,
//                                                  data.m_noOfInputPlugs,
//                                                  data.m_noOfOutputPlugs,
//                                                  m_verboseLevel );
//         }
//         break;
//         case ExtendedSubunitInfoCmd::ePT_Mixer:
//         case ExtendedSubunitInfoCmd::ePT_Generic:
//         case ExtendedSubunitInfoCmd::ePT_UpDown:
//         case ExtendedSubunitInfoCmd::ePT_DolbyProLogic:
//         case ExtendedSubunitInfoCmd::ePT_3DStereoExtender:
//         case ExtendedSubunitInfoCmd::ePT_Reverberation:
//         case ExtendedSubunitInfoCmd::ePT_Chorus:
//         case ExtendedSubunitInfoCmd::ePT_DynamicRangeCompression:
//         default:
//             fb = new BeBoB::FunctionBlockProcessing( *this,
//                                               data.m_functionBlockId,
//                                               purpose,
//                                               data.m_noOfInputPlugs,
//                                               data.m_noOfOutputPlugs,
//                                               m_verboseLevel );
//             debugWarning( "Dummy function block processing created. "
//                           "Implementation is missing\n" );
//         }
//     }
//     break;
//     case ExtendedSubunitInfoCmd::eFBT_AudioSubunitCodec:
//     {
//         fb = new BeBoB::FunctionBlockCodec( *this,
//                                      data.m_functionBlockId,
//                                      purpose,
//                                      data.m_noOfInputPlugs,
//                                      data.m_noOfOutputPlugs,
//                                      m_verboseLevel );
//         debugWarning( "Dummy function block codec created. "
//                       "Implementation is missing\n" );
//     }
//     break;
//     default:
//         debugError( "Unhandled function block type found\n" );
//         return false;
//     }
// 
//     if ( !fb ) {
//         debugError( "Could create function block\n" );
//         return false;
//     }
//     if ( !fb->discover() ) {
//         debugError( "Could not discover function block %s\n",
//                     fb->getName() );
//         delete fb;
//         return false;
//     }
//     m_functions.push_back( fb );
// 
//     return true;
// }
// 
// BeBoB::FunctionBlock::ESpecialPurpose
// SubunitAudio::convertSpecialPurpose(
//     function_block_special_purpose_t specialPurpose )
// {
//     BeBoB::FunctionBlock::ESpecialPurpose p;
//     switch ( specialPurpose ) {
//     case ExtendedSubunitInfoPageData::eSP_InputGain:
//         p  = BeBoB::FunctionBlock::eSP_InputGain;
//         break;
//     case ExtendedSubunitInfoPageData::eSP_OutputVolume:
//         p = BeBoB::FunctionBlock::eSP_OutputVolume;
//     break;
//     default:
//         p = BeBoB::FunctionBlock::eSP_NoSpecialPurpose;
//     }
//     return p;
// }
// 
bool
SubunitAudio::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    bool result = true;
    int i = 0;

    for ( BeBoB::FunctionBlockVector::const_iterator it = m_functions.begin();
          it != m_functions.end();
          ++it )
    {
        BeBoB::FunctionBlock* pFB = *it;
        std::ostringstream strstrm;
        strstrm << basePath << "BeBoB::FunctionBlock" << i << "/";

        result &= pFB->serialize( strstrm.str() , ser );

        i++;
    }

    return result;
}

bool
SubunitAudio::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               Unit& unit )
{
    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << "BeBoB::FunctionBlock" << i << "/";
        BeBoB::FunctionBlock* pFB = BeBoB::FunctionBlock::deserialize( strstrm.str(),
                                                         deser,
                                                         unit,
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

SubunitMusic::SubunitMusic( Unit& unit, subunit_t id )
    : Subunit( unit, eST_Music, id )
{
}

SubunitMusic::SubunitMusic()
    : Subunit()
{
}

SubunitMusic::~SubunitMusic()
{
}

const char*
SubunitMusic::getName()
{
    return "MusicSubunit";
}

bool
SubunitMusic::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    return true;
}

bool
SubunitMusic::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               Unit& unit )
{
    return true;
}

}
