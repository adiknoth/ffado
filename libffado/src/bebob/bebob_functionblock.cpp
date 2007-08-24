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
#include "libieee1394/configrom.h"

using namespace AVC;

namespace BeBoB {

IMPL_DEBUG_MODULE( FunctionBlock, FunctionBlock, DEBUG_LEVEL_NORMAL );

FunctionBlock::FunctionBlock(
    AVC::Subunit& subunit,
    function_block_type_t type,
    function_block_type_t subtype,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : m_subunit( &subunit )
    , m_type( type )
    , m_subtype( subtype )
    , m_id( id )
    , m_purpose( purpose )
    , m_nrOfInputPlugs( nrOfInputPlugs )
    , m_nrOfOutputPlugs( nrOfOutputPlugs )
    , m_verbose( verbose )
{
    setDebugLevel( verbose );
}

FunctionBlock::FunctionBlock( const FunctionBlock& rhs )
    : m_subunit( rhs.m_subunit )
    , m_type( rhs.m_type )
    , m_subtype( rhs.m_subtype )
    , m_id( rhs.m_id )
    , m_purpose( rhs.m_purpose )
    , m_nrOfInputPlugs( rhs.m_nrOfInputPlugs )
    , m_nrOfOutputPlugs( rhs.m_nrOfOutputPlugs )
    , m_verbose( rhs.m_verbose )
{
}

FunctionBlock::FunctionBlock()
{
}

FunctionBlock::~FunctionBlock()
{
    for ( PlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }

}

bool
FunctionBlock::discover()
{
    debugOutput( DEBUG_LEVEL_NORMAL,
                 "discover function block %s (nr of input plugs = %d, "
                 "nr of output plugs = %d)\n",
                 getName(),
                 m_nrOfInputPlugs,
                 m_nrOfOutputPlugs );

    if ( !discoverPlugs( AVC::Plug::eAPD_Input, m_nrOfInputPlugs ) ) {
        debugError( "Could not discover input plug for '%s'\n",
                    getName() );
        return false;
    }

    if ( !discoverPlugs( AVC::Plug::eAPD_Output, m_nrOfOutputPlugs ) ) {
        debugError( "Could not discover output plugs for '%s'\n",
                    getName() );
        return false;
    }

    return true;
}

bool
FunctionBlock::discoverPlugs( AVC::Plug::EPlugDirection plugDirection,
                                     plug_id_t plugMaxId )
{
    for ( int plugId = 0; plugId < plugMaxId; ++plugId ) {
        AVC::Plug* plug = new BeBoB::Plug(
            &m_subunit->getUnit(),
            m_subunit,
            m_type,
            m_id,
            AVC::Plug::eAPA_FunctionBlockPlug,
            plugDirection,
            plugId);

        if ( !plug || !plug->discover() ) {
            debugError( "plug discovering failed for plug %d\n",
                        plugId );
            delete plug;
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "plug '%s' found\n",
                     plug->getName() );
        m_plugs.push_back( plug );
    }

    return true;
}

bool
FunctionBlock::discoverConnections()
{
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "discover connections function block %s\n",
                 getName() );

    for ( PlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        BeBoB::Plug* plug = dynamic_cast<BeBoB::Plug*>(*it);
        if(!plug) {
            debugError("BUG: not a bebob plug\n");
            return false;
        }
        if ( !plug->discoverConnections() ) {
            debugError( "Could not discover plug connections\n" );
            return false;
        }
    }
    return true;
}

bool
serializePlugVector( Glib::ustring basePath,
                       Util::IOSerialize& ser,
                       const PlugVector& vec )
{
    bool result = true;
    int i = 0;
    for ( PlugVector::const_iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        std::ostringstream strstrm;
        strstrm << basePath << i;
        result &= ser.write( strstrm.str(), ( *it )->getGlobalId() );
        i++;
    }
    return result;
}

bool
deserializePlugVector( Glib::ustring basePath,
                         Util::IODeserialize& deser,
                         AVC::Unit& unit,
                         PlugVector& vec )
{
    int i = 0;
    bool bFinished = false;
    bool result = true;
    do {
        plug_id_t plugId;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        if ( deser.isExisting( strstrm.str() ) ) {
            result &= deser.read( strstrm.str(), plugId );
            AVC::Plug* pPlug = unit.getPlugManager().getPlug( plugId );

            if ( result && pPlug ) {
                vec.push_back( pPlug );
                i++;
            } else {
                bFinished = true;
            }
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}

bool
FunctionBlock::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;

    result  = ser.write( basePath + "m_type", m_type );
    result &= ser.write( basePath + "m_subtype", m_subtype );
    result &= ser.write( basePath + "m_id", m_id );
    result &= ser.write( basePath + "m_purpose", m_purpose );
    result &= ser.write( basePath + "m_nrOfInputPlugs", m_nrOfInputPlugs );
    result &= ser.write( basePath + "m_nrOfOutputPlugs", m_nrOfOutputPlugs );
    result &= ser.write( basePath + "m_verbose", m_verbose );
    result &= serializePlugVector( basePath + "m_plugs", ser, m_plugs );

    return result;
}

FunctionBlock*
FunctionBlock::deserialize( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AVC::Unit& unit,
                                   AVC::Subunit& subunit )
{
    bool result;
    function_block_type_t type;
    function_block_type_t subtype;
    FunctionBlock* pFB = 0;

    result  = deser.read( basePath + "m_type", type );
    result &= deser.read( basePath + "m_subtype", subtype );
    if ( !result ) {
        return 0;
    }

    switch ( type ) {
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitSelector:
        pFB = new FunctionBlockSelector;
        break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitFeature:
        pFB = new FunctionBlockFeature;
        break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitProcessing:
        if ( subtype == ExtendedSubunitInfoCmd::ePT_EnhancedMixer ) {
            pFB = new FunctionBlockEnhancedMixer;
        } else {
            pFB = new FunctionBlockProcessing;
        }
        break;
    case ExtendedSubunitInfoCmd::eFBT_AudioSubunitCodec:
        pFB = new FunctionBlockCodec;
        break;
    default:
        pFB = 0;
    }

    if ( !pFB ) {
        return 0;
    }

    pFB->m_subunit = &subunit;
    pFB->m_type = type;
    pFB->m_subtype = subtype;

    result &= deser.read( basePath + "m_id", pFB->m_id );
    result &= deser.read( basePath + "m_purpose", pFB->m_purpose );
    result &= deser.read( basePath + "m_nrOfInputPlugs", pFB->m_nrOfInputPlugs );
    result &= deser.read( basePath + "m_nrOfOutputPlugs", pFB->m_nrOfOutputPlugs );
    result &= deser.read( basePath + "m_verbose", pFB->m_verbose );
    result &= deserializePlugVector( basePath + "m_plugs", deser, unit, pFB->m_plugs );

    return 0;
}

///////////////////////

FunctionBlockSelector::FunctionBlockSelector(
    AVC::Subunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitSelector,
                     0,
                     id,
                     purpose,
                     nrOfInputPlugs,
                     nrOfOutputPlugs,
                     verbose )
{
}

FunctionBlockSelector::FunctionBlockSelector(
    const FunctionBlockSelector& rhs )
    : FunctionBlock( rhs )
{
}

FunctionBlockSelector::FunctionBlockSelector()
    : FunctionBlock()
{
}

FunctionBlockSelector::~FunctionBlockSelector()
{
}

const char*
FunctionBlockSelector::getName()
{
    return "Selector";
}

bool
FunctionBlockSelector::serializeChild( Glib::ustring basePath,
                                              Util::IOSerialize& ser ) const
{
    return true;
}

bool
FunctionBlockSelector::deserializeChild( Glib::ustring basePath,
                                                Util::IODeserialize& deser,
                                                AvDevice& unit )
{
    return true;
}

///////////////////////

FunctionBlockFeature::FunctionBlockFeature(
    AVC::Subunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitFeature,
                     0,
                     id,
                     purpose,
                     nrOfInputPlugs,
                     nrOfOutputPlugs,
                     verbose )
{
}

FunctionBlockFeature::FunctionBlockFeature(
    const FunctionBlockFeature& rhs )
    : FunctionBlock( rhs )
{
}

FunctionBlockFeature::FunctionBlockFeature()
    : FunctionBlock()
{
}

FunctionBlockFeature::~FunctionBlockFeature()
{
}

const char*
FunctionBlockFeature::getName()
{
    return "Feature";
}

bool
FunctionBlockFeature::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    return true;
}

bool
FunctionBlockFeature::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               AvDevice& unit )
{
    return true;
}

///////////////////////

FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer(
    AVC::Subunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitProcessing,
                     ExtendedSubunitInfoCmd::ePT_EnhancedMixer,
                     id,
                     purpose,
                     nrOfInputPlugs,
                     nrOfOutputPlugs,
                     verbose )
{
}

FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer(
    const FunctionBlockEnhancedMixer& rhs )
    : FunctionBlock( rhs )
{
}

FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer()
    : FunctionBlock()
{
}

FunctionBlockEnhancedMixer::~FunctionBlockEnhancedMixer()
{
}

const char*
FunctionBlockEnhancedMixer::getName()
{
    return "EnhancedMixer";
}

bool
FunctionBlockEnhancedMixer::serializeChild( Glib::ustring basePath,
                                                   Util::IOSerialize& ser ) const
{
    return true;
}

bool
FunctionBlockEnhancedMixer::deserializeChild( Glib::ustring basePath,
                                                     Util::IODeserialize& deser,
                                                     AvDevice& unit )
{
    return true;
}

///////////////////////

FunctionBlockProcessing::FunctionBlockProcessing(
    AVC::Subunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitProcessing,
                     0,
                     id,
                     purpose,
                     nrOfInputPlugs,
                     nrOfOutputPlugs,
                     verbose )
{
}

FunctionBlockProcessing::FunctionBlockProcessing(
    const FunctionBlockProcessing& rhs )
    : FunctionBlock( rhs )
{
}

FunctionBlockProcessing::FunctionBlockProcessing()
    : FunctionBlock()
{
}

FunctionBlockProcessing::~FunctionBlockProcessing()
{
}

const char*
FunctionBlockProcessing::getName()
{
    return "Dummy Processing";
}

bool
FunctionBlockProcessing::serializeChild( Glib::ustring basePath,
                                                Util::IOSerialize& ser ) const
{
    return true;
}

bool
FunctionBlockProcessing::deserializeChild( Glib::ustring basePath,
                                                  Util::IODeserialize& deser,
                                                  AvDevice& unit )
{
    return true;
}

///////////////////////

FunctionBlockCodec::FunctionBlockCodec(
    AVC::Subunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitCodec,
                     0,
                     id,
                     purpose,
                     nrOfInputPlugs,
                     nrOfOutputPlugs,
                     verbose )
{
}

FunctionBlockCodec::FunctionBlockCodec( const FunctionBlockCodec& rhs )
    : FunctionBlock( rhs )
{
}

FunctionBlockCodec::FunctionBlockCodec()
    : FunctionBlock()
{
}

FunctionBlockCodec::~FunctionBlockCodec()
{
}

const char*
FunctionBlockCodec::getName()
{
    return "Dummy Codec";
}

bool
FunctionBlockCodec::serializeChild( Glib::ustring basePath,
                                           Util::IOSerialize& ser ) const
{
    return true;
}

bool
FunctionBlockCodec::deserializeChild( Glib::ustring basePath,
                                             Util::IODeserialize& deser,
                                             AvDevice& unit )
{
    return true;
}

}
