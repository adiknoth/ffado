/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "avc_extended_cmd_generic.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

namespace AVC {

UnitPlugAddress::UnitPlugAddress( EPlugType plugType,  plug_type_t plugId )
    : m_plugType( plugType )
    , m_plugId( plugId )
    , m_reserved( 0xff )
{
}

UnitPlugAddress::~UnitPlugAddress()
{
}

bool
UnitPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_plugType, "UnitPlugAddress plugType" );
    se.write( m_plugId, "UnitPlugAddress plugId" );
    se.write( m_reserved, "UnitPlugAddress reserved" );
    return true;
}

bool
UnitPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_plugType );
    de.read( &m_plugId );
    de.read( &m_reserved );
    return true;
}

UnitPlugAddress*
UnitPlugAddress::clone() const
{
    return new UnitPlugAddress( *this );
}

////////////////////////////////////////////////////////////

SubunitPlugAddress::SubunitPlugAddress( plug_id_t plugId )
    : m_plugId( plugId )
    , m_reserved0( 0xff )
    , m_reserved1( 0xff )
{
}

SubunitPlugAddress::~SubunitPlugAddress()
{
}

bool
SubunitPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_plugId, "SubunitPlugAddress plugId" );
    se.write( m_reserved0, "SubunitPlugAddress reserved0" );
    se.write( m_reserved1, "SubunitPlugAddress reserved1" );
    return true;
}

bool
SubunitPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_plugId );
    de.read( &m_reserved0 );
    de.read( &m_reserved1 );
    return true;
}

SubunitPlugAddress*
SubunitPlugAddress::clone() const
{
    return new SubunitPlugAddress( *this );
}

////////////////////////////////////////////////////////////

FunctionBlockPlugAddress::FunctionBlockPlugAddress(  function_block_type_t functionBlockType,
                                                     function_block_id_t functionBlockId,
                                                     plug_id_t plugId )
    : m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_plugId( plugId )
{
}

FunctionBlockPlugAddress::~FunctionBlockPlugAddress()
{
}

bool
FunctionBlockPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_functionBlockType, "FunctionBlockPlugAddress functionBlockType" );
    se.write( m_functionBlockId, "FunctionBlockPlugAddress functionBlockId" );
    se.write( m_plugId, "FunctionBlockPlugAddress plugId" );
    return true;
}

bool
FunctionBlockPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_functionBlockType );
    de.read( &m_functionBlockId );
    de.read( &m_plugId );
    return true;
}

FunctionBlockPlugAddress*
FunctionBlockPlugAddress:: clone() const
{
    return new FunctionBlockPlugAddress( *this );
}

////////////////////////////////////////////////////////////

UndefinedPlugAddress::UndefinedPlugAddress()
    : m_reserved0( 0xff )
    , m_reserved1( 0xff )
    , m_reserved2( 0xff )
{
}

UndefinedPlugAddress::~UndefinedPlugAddress()
{
}

bool
UndefinedPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_reserved0, "UndefinedPlugAddress reserved0" );
    se.write( m_reserved1, "UndefinedPlugAddress reserved1" );
    se.write( m_reserved2, "UndefinedPlugAddress reserved2" );
    return true;
}

bool
UndefinedPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_reserved0 );
    de.read( &m_reserved1 );
    de.read( &m_reserved2 );
    return true;
}

UndefinedPlugAddress*
UndefinedPlugAddress:: clone() const
{
    return new UndefinedPlugAddress( *this );
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

UnitPlugSpecificDataPlugAddress::UnitPlugSpecificDataPlugAddress( EPlugType plugType,  plug_type_t plugId )
    : m_plugType( plugType )
    , m_plugId( plugId )
    , m_reserved0( 0xff )
    , m_reserved1( 0xff )
    , m_reserved2( 0xff )
{
}

UnitPlugSpecificDataPlugAddress::~UnitPlugSpecificDataPlugAddress()
{
}

bool
UnitPlugSpecificDataPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_plugType,  "UnitPlugSpecificDataPlugAddress plugType" );
    se.write( m_plugId,    "UnitPlugSpecificDataPlugAddress plugId" );
    se.write( m_reserved0, "UnitPlugSpecificDataPlugAddress reserved0" );
    se.write( m_reserved1, "UnitPlugSpecificDataPlugAddress reserved1" );
    se.write( m_reserved2, "UnitPlugSpecificDataPlugAddress reserved2" );
    return true;
}

bool
UnitPlugSpecificDataPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_plugType );
    de.read( &m_plugId );
    de.read( &m_reserved0 );
    de.read( &m_reserved1 );
    de.read( &m_reserved2 );
    return true;
}

UnitPlugSpecificDataPlugAddress*
UnitPlugSpecificDataPlugAddress::clone() const
{
    return new UnitPlugSpecificDataPlugAddress( *this );
}

////////////////////////////////////////////////////////////

SubunitPlugSpecificDataPlugAddress::SubunitPlugSpecificDataPlugAddress(
    ESubunitType subunitType,
    subunit_id_t subunitId,
    plug_id_t plugId )
    : m_subunitType( subunitType )
    , m_subunitId( subunitId )
    , m_plugId( plugId )
    , m_reserved0( 0xff )
    , m_reserved1( 0xff )
{
}

SubunitPlugSpecificDataPlugAddress::~SubunitPlugSpecificDataPlugAddress()
{
}

bool
SubunitPlugSpecificDataPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_subunitType,  "SubunitPlugSpecificDataPlugAddress subunitType" );
    se.write( m_subunitId,    "SubunitPlugSpecificDataPlugAddress subunitId" );
    se.write( m_plugId,       "SubunitPlugSpecificDataPlugAddress plugId" );
    se.write( m_reserved0,    "SubunitPlugSpecificDataPlugAddress reserved0" );
    se.write( m_reserved1,    "SubunitPlugSpecificDataPlugAddress reserved1" );
    return true;
}

bool
SubunitPlugSpecificDataPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_subunitType );
    de.read( &m_subunitId );
    de.read( &m_plugId );
    de.read( &m_reserved0 );
    de.read( &m_reserved1 );
    return true;
}

SubunitPlugSpecificDataPlugAddress*
SubunitPlugSpecificDataPlugAddress::clone() const
{
    return new SubunitPlugSpecificDataPlugAddress( *this );
}

////////////////////////////////////////////////////////////

FunctionBlockPlugSpecificDataPlugAddress::FunctionBlockPlugSpecificDataPlugAddress(
    ESubunitType subunitType,
    subunit_id_t subunitId,
    function_block_type_t functionBlockType,
    function_block_id_t functionBlockId,
    plug_id_t plugId )
    : m_subunitType( subunitType )
    , m_subunitId( subunitId )
    , m_functionBlockType( functionBlockType )
    , m_functionBlockId( functionBlockId )
    , m_plugId( plugId )
{
}

FunctionBlockPlugSpecificDataPlugAddress::~FunctionBlockPlugSpecificDataPlugAddress()
{
}

bool
FunctionBlockPlugSpecificDataPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_subunitType,       "FunctionPlugSpecificDataBlockPlugAddress subunitType" );
    se.write( m_subunitId,         "FunctionPlugSpecificDataBlockPlugAddress subunitId" );
    se.write( m_functionBlockType, "FunctionBlockPlugSpecificDataPlugAddress functionBlockType" );
    se.write( m_functionBlockId,   "FunctionBlockPlugSpecificDataPlugAddress functionBlockId" );
    se.write( m_plugId,            "FunctionBlockPlugSpecificDataPlugAddress plugId" );
    return true;
}

bool
FunctionBlockPlugSpecificDataPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_subunitType );
    de.read( &m_subunitId );
    de.read( &m_functionBlockType );
    de.read( &m_functionBlockId );
    de.read( &m_plugId );
    return true;
}

FunctionBlockPlugSpecificDataPlugAddress*
FunctionBlockPlugSpecificDataPlugAddress:: clone() const
{
    return new FunctionBlockPlugSpecificDataPlugAddress( *this );
}

////////////////////////////////////////////////////////////

UndefinedPlugSpecificDataPlugAddress::UndefinedPlugSpecificDataPlugAddress()
    : m_reserved0( 0xff )
    , m_reserved1( 0xff )
    , m_reserved2( 0xff )
    , m_reserved3( 0xff )
    , m_reserved4( 0xff )
{
}

UndefinedPlugSpecificDataPlugAddress::~UndefinedPlugSpecificDataPlugAddress()
{
}

bool
UndefinedPlugSpecificDataPlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_reserved0, "UndefinedPlugAddress reserved0" );
    se.write( m_reserved1, "UndefinedPlugAddress reserved1" );
    se.write( m_reserved2, "UndefinedPlugAddress reserved2" );
    se.write( m_reserved3, "UndefinedPlugAddress reserved3" );
    se.write( m_reserved4, "UndefinedPlugAddress reserved4" );
    return true;
}

bool
UndefinedPlugSpecificDataPlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_reserved0 );
    de.read( &m_reserved1 );
    de.read( &m_reserved2 );
    de.read( &m_reserved3 );
    de.read( &m_reserved4 );
    return true;
}

UndefinedPlugSpecificDataPlugAddress*
UndefinedPlugSpecificDataPlugAddress:: clone() const
{
    return new UndefinedPlugSpecificDataPlugAddress( *this );
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

PlugAddress::PlugAddress( EPlugDirection plugDirection,
                          EPlugAddressMode plugAddressMode,
                          UnitPlugAddress& unitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new UnitPlugAddress( unitPlugAddress ) )
{
}

PlugAddress::PlugAddress( EPlugDirection plugDirection,
                          EPlugAddressMode plugAddressMode,
                          SubunitPlugAddress& subUnitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new SubunitPlugAddress( subUnitPlugAddress ) )
{
}

PlugAddress::PlugAddress( EPlugDirection plugDirection,
                          EPlugAddressMode plugAddressMode,
                          FunctionBlockPlugAddress& functionBlockPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new FunctionBlockPlugAddress( functionBlockPlugAddress ) )
{
}

PlugAddress::PlugAddress()
    : m_plugDirection( ePD_Undefined )
    , m_addressMode( ePAM_Undefined )
    , m_plugAddressData( new UndefinedPlugAddress() )
{
}

PlugAddress::PlugAddress( const PlugAddress& pa )
    : m_plugDirection( pa.m_plugDirection )
    , m_addressMode( pa.m_addressMode )
    , m_plugAddressData( dynamic_cast<PlugAddressData*>( pa.m_plugAddressData->clone() ) )
{
}

PlugAddress::~PlugAddress()
{
    delete m_plugAddressData;
    m_plugAddressData = 0;
}

bool
PlugAddress::serialize( Util::IOSSerialize& se )
{
    se.write( m_plugDirection, "PlugAddress plugDirection" );
    se.write( m_addressMode, "PlugAddress addressMode" );
    return m_plugAddressData->serialize( se );
}

bool
PlugAddress::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_plugDirection );
    de.read( &m_addressMode );
    return m_plugAddressData->deserialize( de );
}

PlugAddress*
PlugAddress::clone() const
{
    return new PlugAddress( *this );
}

const char* plugAddressDirectionStrings[] =
{
    "Input",
    "Output",
    "Undefined",
};

const char* plugAddressAddressModeStrings[] =
{
    "Unit",
    "Subunit",
    "FunctionBlock",
    "Undefined",
};

const char*
plugAddressPlugDirectionToString( PlugAddress::EPlugDirection direction  )
{
    if ( direction > PlugAddress::ePD_Output ) {
        direction = PlugAddress::ePD_Undefined;
    }
    return plugAddressDirectionStrings[direction];
}

const char*
plugAddressAddressModeToString( PlugAddress::EPlugAddressMode mode )
{
    if ( mode > PlugAddress::ePAM_FunctionBlock ) {
        mode = PlugAddress::ePAM_FunctionBlock;
    }
    return plugAddressAddressModeStrings[mode];
}


////////////////////////////////////////////////////////////

PlugAddressSpecificData::PlugAddressSpecificData( EPlugDirection plugDirection,
                          EPlugAddressMode plugAddressMode,
                          UnitPlugSpecificDataPlugAddress& unitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new UnitPlugSpecificDataPlugAddress( unitPlugAddress ) )
{
}

PlugAddressSpecificData::PlugAddressSpecificData( EPlugDirection plugDirection,
                          EPlugAddressMode plugAddressMode,
                          SubunitPlugSpecificDataPlugAddress& subUnitPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new SubunitPlugSpecificDataPlugAddress( subUnitPlugAddress ) )
{
}

PlugAddressSpecificData::PlugAddressSpecificData( EPlugDirection plugDirection,
                          EPlugAddressMode plugAddressMode,
                          FunctionBlockPlugSpecificDataPlugAddress& functionBlockPlugAddress )
    : m_plugDirection( plugDirection )
    , m_addressMode( plugAddressMode )
    , m_plugAddressData( new FunctionBlockPlugSpecificDataPlugAddress( functionBlockPlugAddress ) )
{
}

PlugAddressSpecificData::PlugAddressSpecificData( const PlugAddressSpecificData& pa )
    : m_plugDirection( pa.m_plugDirection )
    , m_addressMode( pa.m_addressMode )
    , m_plugAddressData( dynamic_cast<PlugAddressData*>( pa.m_plugAddressData->clone() ) )
{
}

PlugAddressSpecificData::~PlugAddressSpecificData()
{
    delete m_plugAddressData;
    m_plugAddressData = 0;
}

bool
PlugAddressSpecificData::serialize( Util::IOSSerialize& se )
{
    se.write( m_plugDirection, "PlugAddressSpecificData plugDirection" );
    se.write( m_addressMode, "PlugAddressSpecificData addressMode" );
    return m_plugAddressData->serialize( se );
}

bool
PlugAddressSpecificData::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_plugDirection );
    de.read( &m_addressMode );
    if ( m_plugAddressData ) {
        delete m_plugAddressData;
        m_plugAddressData = 0;
    }
    switch ( m_addressMode ) {
    case ePAM_Unit:
        m_plugAddressData =
            new UnitPlugSpecificDataPlugAddress(
                UnitPlugSpecificDataPlugAddress::ePT_PCR,
                0xff );
        break;
    case ePAM_Subunit:
        m_plugAddressData =
            new SubunitPlugSpecificDataPlugAddress(
                eST_Reserved,
                0xff,
                0xff );
        break;
    case ePAM_FunctionBlock:
        m_plugAddressData =
            new FunctionBlockPlugSpecificDataPlugAddress(
                eST_Reserved,
                0xff,
                0xff,
                0xff,
                0xff);
        break;
    default:
        m_plugAddressData =
            new UndefinedPlugSpecificDataPlugAddress();
    }

    return m_plugAddressData->deserialize( de );
}

PlugAddressSpecificData*
PlugAddressSpecificData::clone() const
{
    return new PlugAddressSpecificData( *this );
}

}
