/* avc_feature_function_block.cpp
 * Copyright (C) 2006 by Daniel Wagner
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

#include "avc_feature_function_block.h"
#include "serialize.h"
#include "ieee1394service.h"

FeatureFunctionBlockVolume::FeatureFunctionBlockVolume()
    : IBusData()
    , m_selectorLength( 6 )
    , m_audioChannelNumber( 0 )
    , m_controlSelector( eFCS_Volume )
    , m_controlDataLength( 2 )
    , m_volume( 0 )
{
}

FeatureFunctionBlockVolume::FeatureFunctionBlockVolume( const FeatureFunctionBlockVolume& rhs )
    : m_selectorLength( rhs.m_selectorLength )
    , m_audioChannelNumber( rhs.m_audioChannelNumber )
    , m_controlSelector( rhs.m_controlSelector )
    , m_controlDataLength( rhs.m_controlDataLength )
    , m_volume( rhs.m_volume )
{
}

FeatureFunctionBlockVolume::~FeatureFunctionBlockVolume()
{
}

bool
FeatureFunctionBlockVolume::serialize( IOSSerialize& se )
{
    bool bStatus;
    byte_t val;
    bStatus = se.write( m_selectorLength,      "FeatureFunctionBlockVolume selectorLength" );
    bStatus &= se.write( m_audioChannelNumber, "FeatureFunctionBlockVolume audioChannelNumber" );
    bStatus &= se.write( m_controlSelector,    "FeatureFunctionBlockVolume controlSelector" );
    bStatus &= se.write( m_controlDataLength,  "FeatureFunctionBlockVolume controlDataLength" );
    val = (byte_t)(m_volume >> 8);
    bStatus &= se.write( val,                  "FeatureFunctionBlockVolume volume high" );
    val = m_volume & 0xff;
    bStatus &= se.write( val,                  "FeatureFunctionBlockVolume volume low" );

    return bStatus;
}

bool
FeatureFunctionBlockVolume::deserialize( IISDeserialize& de )
{
    bool bStatus;
    byte_t val;
    bStatus = de.read( &m_selectorLength );
    bStatus = de.read( &m_audioChannelNumber );
    bStatus = de.read( &m_controlSelector );
    bStatus = de.read( &m_controlDataLength );
    bStatus = de.read( &val );
    m_volume = val << 8;
    bStatus = de.read( &val );
    m_volume |= val;

    return bStatus;
}

FeatureFunctionBlockVolume*
FeatureFunctionBlockVolume::clone() const
{
    return new FeatureFunctionBlockVolume( *this );
}

/////////////////////////////////


FeatureFunctionBlockCmd::FeatureFunctionBlockCmd( Ieee1394Service* ieee1394service )
    : AVCCommand( ieee1394service, AVC1394_FUNCTION_BLOCK_CMD )
    , m_functionBlockType( 0x81 ) // feature function block
    , m_functionBlockId( 0xff )
    , m_controlAttribute( eCA_Current )
{
    setSubunitType( eST_Audio );
}

FeatureFunctionBlockCmd::FeatureFunctionBlockCmd( const FeatureFunctionBlockCmd& rhs )
    : AVCCommand( rhs )
    , m_functionBlockType( rhs.m_functionBlockType )
    , m_functionBlockId( rhs.m_functionBlockId )
    , m_controlAttribute( rhs.m_controlAttribute )
    , m_volumeControl( rhs.m_volumeControl )
{
}

FeatureFunctionBlockCmd::~FeatureFunctionBlockCmd()
{
}

bool FeatureFunctionBlockCmd::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus = AVCCommand::serialize( se );
    bStatus &= se.write( m_functionBlockType, "FeatureFunctionBlockCmd functionBlockType" );
    bStatus &= se.write( m_functionBlockId,   "FeatureFunctionBlockCmd functionBlockId" );
    bStatus &= se.write( m_controlAttribute,  "FeatureFunctionBlockCmd controlAttribute" );
    bStatus &= m_volumeControl.serialize( se );

    return bStatus;
}

bool FeatureFunctionBlockCmd::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus = AVCCommand::deserialize( de );

    bStatus &= de.read( &m_functionBlockType );
    bStatus &= de.read( &m_functionBlockId );
    bStatus &= de.read( &m_controlAttribute );
    bStatus &= m_volumeControl.deserialize( de );

    return bStatus;
}

FeatureFunctionBlockCmd*
FeatureFunctionBlockCmd::clone() const
{
    FeatureFunctionBlockCmd* pFeatureFunctionBlockCmd
        = new FeatureFunctionBlockCmd( *this );
    return pFeatureFunctionBlockCmd;
}
