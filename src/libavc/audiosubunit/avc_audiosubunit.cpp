/*
 * Copyright (C)      2007 by Pieter Palmers
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

#include "avc_audiosubunit.h"
#include "../general/avc_subunit.h"

#warning merge with bebob functionblock
#include "bebob/bebob_functionblock.h"
#include "../audiosubunit/avc_function_block.h"


#include <sstream>

namespace AVC {

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
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering %s...\n", getName());
    
    if ( !Subunit::discover() ) {
        return false;
    }

    return true;
}


const char*
SubunitAudio::getName()
{
    return "AVC::AudioSubunit";
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

}
