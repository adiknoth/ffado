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

IMPL_DEBUG_MODULE( BeBoB::FunctionBlock, BeBoB::FunctionBlock, DEBUG_LEVEL_NORMAL );

BeBoB::FunctionBlock::FunctionBlock(
    AvDeviceSubunit& subunit,
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

BeBoB::FunctionBlock::FunctionBlock( const FunctionBlock& rhs )
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

BeBoB::FunctionBlock::FunctionBlock()
{
}

BeBoB::FunctionBlock::~FunctionBlock()
{
    for ( AvPlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }

}

bool
BeBoB::FunctionBlock::discover()
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
BeBoB::FunctionBlock::discoverPlugs( AvPlug::EAvPlugDirection plugDirection,
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
BeBoB::FunctionBlock::discoverConnections()
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

bool
serializeAvPlugVector( Glib::ustring basePath,
                       Util::IOSerialize& ser,
                       const BeBoB::AvPlugVector& vec )
{
    bool result = true;
    int i = 0;
    for ( BeBoB::AvPlugVector::const_iterator it = vec.begin();
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
deserializeAvPlugVector( Glib::ustring basePath,
                         Util::IODeserialize& deser,
                         BeBoB::AvDevice& avDevice,
                         BeBoB::AvPlugVector& vec )
{
    int i = 0;
    bool bFinished = false;
    bool result = true;
    do {
        plug_id_t plugId;
        std::ostringstream strstrm;
        strstrm << basePath << i;

        result &= deser.read( strstrm.str(), plugId );
        BeBoB::AvPlug* pPlug = avDevice.getPlugManager().getPlug( plugId );

        if ( pPlug ) {
            vec.push_back( pPlug );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}

bool
BeBoB::FunctionBlock::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;

    result  = ser.write( basePath + "m_type", m_type );
    result &= ser.write( basePath + "m_subtype", m_subtype );
    result &= ser.write( basePath + "m_id", m_id );
    result &= ser.write( basePath + "m_purpose", m_purpose );
    result &= ser.write( basePath + "m_nrOfInputPlugs", m_nrOfInputPlugs );
    result &= ser.write( basePath + "m_nrOfOutputPlugs", m_nrOfOutputPlugs );
    result &= ser.write( basePath + "m_verbose", m_verbose );
    result &= serializeAvPlugVector( basePath + "m_plugs", ser, m_plugs );

    return result;
}

BeBoB::FunctionBlock*
BeBoB::FunctionBlock::deserialize( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice,
                                   AvDeviceSubunit& subunit )
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
    result &= deserializeAvPlugVector( basePath + "m_plugs", deser, avDevice, pFB->m_plugs );

    return 0;
}

///////////////////////

BeBoB::FunctionBlockSelector::FunctionBlockSelector(
    AvDeviceSubunit& subunit,
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

BeBoB::FunctionBlockSelector::FunctionBlockSelector(
    const FunctionBlockSelector& rhs )
    : FunctionBlock( rhs )
{
}

BeBoB::FunctionBlockSelector::FunctionBlockSelector()
    : FunctionBlock()
{
}

BeBoB::FunctionBlockSelector::~FunctionBlockSelector()
{
}

const char*
BeBoB::FunctionBlockSelector::getName()
{
    return "Selector";
}

bool
BeBoB::FunctionBlockSelector::serializeChild( Glib::ustring basePath,
                                              Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::FunctionBlockSelector::deserializeChild( Glib::ustring basePath,
                                                Util::IODeserialize& deser,
                                                AvDevice& avDevice )
{
    return true;
}

///////////////////////

BeBoB::FunctionBlockFeature::FunctionBlockFeature(
    AvDeviceSubunit& subunit,
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

BeBoB::FunctionBlockFeature::FunctionBlockFeature(
    const FunctionBlockFeature& rhs )
    : FunctionBlock( rhs )
{
}

BeBoB::FunctionBlockFeature::FunctionBlockFeature()
    : FunctionBlock()
{
}

BeBoB::FunctionBlockFeature::~FunctionBlockFeature()
{
}

const char*
BeBoB::FunctionBlockFeature::getName()
{
    return "Feature";
}

bool
BeBoB::FunctionBlockFeature::serializeChild( Glib::ustring basePath,
                                             Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::FunctionBlockFeature::deserializeChild( Glib::ustring basePath,
                                               Util::IODeserialize& deser,
                                               AvDevice& avDevice )
{
    return true;
}

///////////////////////

BeBoB::FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer(
    AvDeviceSubunit& subunit,
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

BeBoB::FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer(
    const FunctionBlockEnhancedMixer& rhs )
    : FunctionBlock( rhs )
{
}

BeBoB::FunctionBlockEnhancedMixer::FunctionBlockEnhancedMixer()
    : FunctionBlock()
{
}

BeBoB::FunctionBlockEnhancedMixer::~FunctionBlockEnhancedMixer()
{
}

const char*
BeBoB::FunctionBlockEnhancedMixer::getName()
{
    return "EnhancedMixer";
}

bool
BeBoB::FunctionBlockEnhancedMixer::serializeChild( Glib::ustring basePath,
                                                   Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::FunctionBlockEnhancedMixer::deserializeChild( Glib::ustring basePath,
                                                     Util::IODeserialize& deser,
                                                     AvDevice& avDevice )
{
    return true;
}

///////////////////////

BeBoB::FunctionBlockProcessing::FunctionBlockProcessing(
    AvDeviceSubunit& subunit,
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

BeBoB::FunctionBlockProcessing::FunctionBlockProcessing(
    const FunctionBlockProcessing& rhs )
    : FunctionBlock( rhs )
{
}

BeBoB::FunctionBlockProcessing::FunctionBlockProcessing()
    : FunctionBlock()
{
}

BeBoB::FunctionBlockProcessing::~FunctionBlockProcessing()
{
}

const char*
BeBoB::FunctionBlockProcessing::getName()
{
    return "Dummy Processing";
}

bool
BeBoB::FunctionBlockProcessing::serializeChild( Glib::ustring basePath,
                                                Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::FunctionBlockProcessing::deserializeChild( Glib::ustring basePath,
                                                  Util::IODeserialize& deser,
                                                  AvDevice& avDevice )
{
    return true;
}

///////////////////////

BeBoB::FunctionBlockCodec::FunctionBlockCodec(
    AvDeviceSubunit& subunit,
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

BeBoB::FunctionBlockCodec::FunctionBlockCodec( const FunctionBlockCodec& rhs )
    : FunctionBlock( rhs )
{
}

BeBoB::FunctionBlockCodec::FunctionBlockCodec()
    : FunctionBlock()
{
}

BeBoB::FunctionBlockCodec::~FunctionBlockCodec()
{
}

const char*
BeBoB::FunctionBlockCodec::getName()
{
    return "Dummy Codec";
}

bool
BeBoB::FunctionBlockCodec::serializeChild( Glib::ustring basePath,
                                           Util::IOSerialize& ser ) const
{
    return true;
}

bool
BeBoB::FunctionBlockCodec::deserializeChild( Glib::ustring basePath,
                                             Util::IODeserialize& deser,
                                             AvDevice& avDevice )
{
    return true;
}
