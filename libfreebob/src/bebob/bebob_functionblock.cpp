/* bebob_functionblock.cpp
 * Copyright (C) 2006,07 by Daniel Wagner
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

#include "bebob/bebob_functionblock.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/bebob_avdevice.h"
#include "configrom.h"

namespace BeBoB {

IMPL_DEBUG_MODULE( FunctionBlock, FunctionBlock, DEBUG_LEVEL_NORMAL );

FunctionBlock::FunctionBlock(
    AvDeviceSubunit& subunit,
    function_block_type_t type,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : m_subunit( &subunit )
    , m_type( type )
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
    , m_id( rhs.m_id )
    , m_purpose( rhs.m_purpose )
    , m_nrOfInputPlugs( rhs.m_nrOfInputPlugs )
    , m_nrOfOutputPlugs( rhs.m_nrOfOutputPlugs )
    , m_verbose( rhs.m_verbose )
{
}

FunctionBlock::~FunctionBlock()
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }

}

bool
FunctionBlock::discover()
{
    debugOutput( DEBUG_LEVEL_VERBOSE,
                 "discover function block %s (nr of input plugs = %d, "
                 "nr of output plugs = %d)\n",
                 getName(),
                 m_nrOfInputPlugs,
                 m_nrOfOutputPlugs );

    if ( !discoverPlugs( AvPlug::eAPD_Input, m_nrOfInputPlugs ) ) {
        debugError( "Could not discover input plug for '%s'\n",
                    getName() );
        return false;
    }

    if ( !discoverPlugs( AvPlug::eAPD_Output, m_nrOfOutputPlugs ) ) {
        debugError( "Could not discover output plugs for '%s'\n",
                    getName() );
        return false;
    }

    return true;
}

bool
FunctionBlock::discoverPlugs( AvPlug::EAvPlugDirection plugDirection,
                              plug_id_t plugMaxId )
{
    for ( int plugId = 0; plugId < plugMaxId; ++plugId ) {
        AvPlug* plug = new AvPlug(
            m_subunit->getAvDevice().get1394Service(),
            m_subunit->getAvDevice().getConfigRom(),
            m_subunit->getAvDevice().getPlugManager(),
            m_subunit->getSubunitType(),
            m_subunit->getSubunitId(),
            m_type,
            m_id,
            AvPlug::eAPA_FunctionBlockPlug,
            plugDirection,
            plugId,
            m_verbose );

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

    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        AvPlug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "Could not discover plug connections\n" );
            return false;
        }
    }
    return true;
}

///////////////////////

FunctionBlockSelector::FunctionBlockSelector(
    AvDeviceSubunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitSelector,
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

FunctionBlockSelector::~FunctionBlockSelector()
{
}

const char*
FunctionBlockSelector::getName()
{
    return "Selector";
}

///////////////////////

FunctionBlockFeature::FunctionBlockFeature(
    AvDeviceSubunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitFeature,
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

FunctionBlockFeature::~FunctionBlockFeature()
{
}

const char*
FunctionBlockFeature::getName()
{
    return "Feature";
}

///////////////////////

FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer(
    AvDeviceSubunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitProcessing,
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

FunctionBlockEnhancedMixer::~FunctionBlockEnhancedMixer()
{
}

const char*
FunctionBlockEnhancedMixer::getName()
{
    return "EnhancedMixer";
}

///////////////////////

FunctionBlockProcessing::FunctionBlockProcessing(
    AvDeviceSubunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitProcessing,
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

FunctionBlockProcessing::~FunctionBlockProcessing()
{
}

const char*
FunctionBlockProcessing::getName()
{
    return "Dummy Processing";
}

///////////////////////

FunctionBlockCodec::FunctionBlockCodec(
    AvDeviceSubunit& subunit,
    function_block_id_t id,
    ESpecialPurpose purpose,
    no_of_input_plugs_t nrOfInputPlugs,
    no_of_output_plugs_t nrOfOutputPlugs,
    int verbose )
    : FunctionBlock( subunit,
                     eFBT_AudioSubunitCodec,
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

FunctionBlockCodec::~FunctionBlockCodec()
{
}

const char*
FunctionBlockCodec::getName()
{
    return "Dummy Codec";
}

}
