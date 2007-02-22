/* avc_function_block.cpp
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

#include "avc_function_block.h"
#include "avc_serialize.h"
#include "libieee1394/ieee1394service.h"

/////////////////////////////////

FunctionBlockFeatureVolume::FunctionBlockFeatureVolume()
    : IBusData()
    , m_controlSelector( FunctionBlockFeature::eCSE_Feature_Volume )
    , m_controlDataLength( 2 )
    , m_volume( 0 )
{
}

FunctionBlockFeatureVolume::FunctionBlockFeatureVolume( const FunctionBlockFeatureVolume& rhs )
    : m_controlSelector( rhs.m_controlSelector )
    , m_controlDataLength( rhs.m_controlDataLength )
    , m_volume( rhs.m_volume )
{
}

FunctionBlockFeatureVolume::~FunctionBlockFeatureVolume()
{
}

bool
FunctionBlockFeatureVolume::serialize( IOSSerialize& se )
{
    bool bStatus;
    byte_t val;
    bStatus = se.write( m_controlSelector,    "FunctionBlockFeatureVolume controlSelector" );
    bStatus &= se.write( m_controlDataLength,  "FunctionBlockFeatureVolume controlDataLength" );
    val = (byte_t)(m_volume >> 8);
    bStatus &= se.write( val,                  "FunctionBlockFeatureVolume volume high" );
    val = m_volume & 0xff;
    bStatus &= se.write( val,                  "FunctionBlockFeatureVolume volume low" );

    return bStatus;
}

bool
FunctionBlockFeatureVolume::deserialize( IISDeserialize& de )
{
    bool bStatus;
    byte_t val;
    bStatus = de.read( &m_controlSelector );
    bStatus &= de.read( &m_controlDataLength );
    bStatus &= de.read( &val );
    m_volume = val << 8;
    bStatus &= de.read( &val );
    m_volume |= val;

    return bStatus;
}

FunctionBlockFeatureVolume*
FunctionBlockFeatureVolume::clone() const
{
    return new FunctionBlockFeatureVolume( *this );
}

/////////////////////////////////

FunctionBlockProcessingMixer::FunctionBlockProcessingMixer()
    : IBusData()
    , m_controlSelector( FunctionBlockProcessing::eCSE_Processing_Mixer )
{
}

FunctionBlockProcessingMixer::FunctionBlockProcessingMixer( const FunctionBlockProcessingMixer& rhs )
    : m_controlSelector( rhs.m_controlSelector )
{
}

FunctionBlockProcessingMixer::~FunctionBlockProcessingMixer()
{
}

bool
FunctionBlockProcessingMixer::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus = se.write( m_controlSelector,    "FunctionBlockProcessingMixer controlSelector" );

    return bStatus;
}

bool
FunctionBlockProcessingMixer::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus = de.read( &m_controlSelector );

    return bStatus;
}

FunctionBlockProcessingMixer*
FunctionBlockProcessingMixer::clone() const
{
    return new FunctionBlockProcessingMixer( *this );
}

/////////////////////////////////

FunctionBlockProcessingEnhancedMixer::FunctionBlockProcessingEnhancedMixer()
    : IBusData()
    , m_controlSelector( FunctionBlockProcessing::eCSE_Processing_EnhancedMixer )
    , m_statusSelector( eSS_ProgramableState )
{
}

FunctionBlockProcessingEnhancedMixer::FunctionBlockProcessingEnhancedMixer(
    const FunctionBlockProcessingEnhancedMixer& rhs )
    : m_controlSelector( rhs.m_controlSelector )
    , m_statusSelector( rhs.m_statusSelector )
{
}

FunctionBlockProcessingEnhancedMixer::~FunctionBlockProcessingEnhancedMixer()
{
}

bool
FunctionBlockProcessingEnhancedMixer::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus  = se.write( m_controlSelector, "FunctionBlockProcessingEnhancedMixer controlSelector" );
    bStatus &= se.write( m_statusSelector,  "FunctionBlockProcessingEnhancedMixer statusSelector" );

    return bStatus;
}

bool
FunctionBlockProcessingEnhancedMixer::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus  = de.read( &m_controlSelector );
    bStatus &= de.read( &m_statusSelector );

    return bStatus;
}

FunctionBlockProcessingEnhancedMixer*
FunctionBlockProcessingEnhancedMixer::clone() const
{
    return new FunctionBlockProcessingEnhancedMixer( *this );
}


/////////////////////////////////
/////////////////////////////////

FunctionBlockSelector::FunctionBlockSelector()
    : IBusData()
    , m_selectorLength( 0x02 )
    , m_inputFbPlugNumber( 0x00 )
    , m_controlSelector( eCSE_Selector_Selector )
{
}

FunctionBlockSelector::FunctionBlockSelector( const FunctionBlockSelector& rhs )
    : IBusData()
    , m_selectorLength( rhs.m_selectorLength )
    , m_inputFbPlugNumber( rhs.m_inputFbPlugNumber )
    , m_controlSelector( rhs.m_controlSelector )
{
}

FunctionBlockSelector::~FunctionBlockSelector()
{
}

bool
FunctionBlockSelector::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus  = se.write( m_selectorLength,    "FunctionBlockSelector selectorLength" );
    bStatus &= se.write( m_inputFbPlugNumber, "FunctionBlockSelector inputFbPlugNumber" );
    bStatus &= se.write( m_controlSelector,   "FunctionBlockSelector controlSelector" );

    return bStatus;
}

bool
FunctionBlockSelector::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus  = de.read( &m_selectorLength );
    bStatus &= de.read( &m_inputFbPlugNumber );
    bStatus &= de.read( &m_controlSelector );

    return bStatus;
}

FunctionBlockSelector*
FunctionBlockSelector::clone() const
{
    return new FunctionBlockSelector( *this );
}

/////////////////////////////////

FunctionBlockFeature::FunctionBlockFeature()
    : IBusData()
    , m_selectorLength( 0x02 )
    , m_audioChannelNumber( 0x00 )
    , m_pVolume( 0 )
{
}

FunctionBlockFeature::FunctionBlockFeature( const FunctionBlockFeature& rhs )
    : IBusData()
    , m_selectorLength( rhs.m_selectorLength )
    , m_audioChannelNumber( rhs.m_audioChannelNumber )
{
    if ( rhs.m_pVolume ) {
        m_pVolume = new FunctionBlockFeatureVolume( *rhs.m_pVolume );
    }
}

FunctionBlockFeature::~FunctionBlockFeature()
{
    delete m_pVolume;
    m_pVolume = 0;
}

bool
FunctionBlockFeature::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus  = se.write( m_selectorLength,     "FunctionBlockFeature selectorLength" );
    bStatus &= se.write( m_audioChannelNumber, "FunctionBlockFeature audioChannelNumber" );

    if ( m_pVolume ) {
        bStatus &= m_pVolume->serialize( se );
    } else {
        bStatus = false;
    }

    return bStatus;
}

bool
FunctionBlockFeature::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus  = de.read( &m_selectorLength );
    bStatus &= de.read( &m_audioChannelNumber );

    byte_t controlSelector;
    bStatus &= de.peek( &controlSelector );
    switch( controlSelector ) {
    case eCSE_Feature_Volume:
        if ( !m_pVolume ) {
            m_pVolume = new FunctionBlockFeatureVolume;
        }
        bStatus &= m_pVolume->deserialize( de );
        break;
    case eCSE_Feature_Mute:
    case eCSE_Feature_LRBalance:
    case eCSE_Feature_FRBalance:
    case eCSE_Feature_Bass:
    case eCSE_Feature_Mid:
    case eCSE_Feature_Treble:
    case eCSE_Feature_GEQ:
    case eCSE_Feature_AGC:
    case eCSE_Feature_Delay:
    case eCSE_Feature_BassBoost:
    case eCSE_Feature_Loudness:
    default:
        bStatus = false;
    }

    return bStatus;
}

FunctionBlockFeature*
FunctionBlockFeature::clone() const
{
    return new FunctionBlockFeature( *this );
}

/////////////////////////////////

FunctionBlockProcessing::FunctionBlockProcessing()
    : IBusData()
    , m_selectorLength( 0x04 )
    , m_fbInputPlugNumber( 0x00 )
    , m_inputAudioChannelNumber( 0x00 )
    , m_outputAudioChannelNumber( 0x00 )
    , m_pMixer( 0 )
    , m_pEnhancedMixer( 0 )
{
}

FunctionBlockProcessing::FunctionBlockProcessing( const FunctionBlockProcessing& rhs )
    : m_selectorLength( rhs.m_selectorLength )
    , m_fbInputPlugNumber( rhs.m_fbInputPlugNumber )
    , m_inputAudioChannelNumber( rhs.m_inputAudioChannelNumber )
    , m_outputAudioChannelNumber( rhs.m_outputAudioChannelNumber )
{
    if ( rhs.m_pMixer ) {
        m_pMixer = new FunctionBlockProcessingMixer( *rhs.m_pMixer );
    } else if ( rhs.m_pEnhancedMixer ) {
        m_pEnhancedMixer = new FunctionBlockProcessingEnhancedMixer( *rhs.m_pEnhancedMixer );
    }
}

FunctionBlockProcessing::~FunctionBlockProcessing()
{
    delete m_pMixer;
    m_pMixer = 0;
    delete m_pEnhancedMixer;
    m_pEnhancedMixer = 0;
}

bool
FunctionBlockProcessing::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus  = se.write( m_selectorLength,     "FunctionBlockProcessing selectorLength" );
    bStatus &= se.write( m_fbInputPlugNumber,  "FunctionBlockProcessing fbInputPlugNumber" );
    bStatus &= se.write( m_inputAudioChannelNumber,  "FunctionBlockProcessing inputAudioChannelNumber" );
    bStatus &= se.write( m_outputAudioChannelNumber, "FunctionBlockProcessing outputAudioChannelNumber" );

    if ( m_pMixer ) {
        bStatus &= m_pMixer->serialize( se );
    } else if ( m_pEnhancedMixer ) {
        bStatus &= m_pEnhancedMixer->serialize( se );
    } else {
        bStatus = false;
    }

    return bStatus;
}

bool
FunctionBlockProcessing::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus  = de.read( &m_selectorLength );
    bStatus &= de.read( &m_fbInputPlugNumber );
    bStatus &= de.read( &m_inputAudioChannelNumber );
    bStatus &= de.read( &m_outputAudioChannelNumber );

    byte_t controlSelector;
    bStatus &= de.peek( &controlSelector );
    switch( controlSelector ) {
    case eCSE_Processing_Mixer:
        if ( !m_pMixer ) {
            m_pMixer = new FunctionBlockProcessingMixer;
        }
        bStatus &= m_pMixer->deserialize( de );
        break;
    case eCSE_Processing_EnhancedMixer:
        if ( !m_pEnhancedMixer ) {
            m_pEnhancedMixer = new FunctionBlockProcessingEnhancedMixer;
        }
        bStatus &= m_pEnhancedMixer->deserialize( de );
        break;
    case eCSE_Processing_Enable:
    case eCSE_Processing_Mode:
    default:
        bStatus = false;
    }

    return bStatus;
}

FunctionBlockProcessing*
FunctionBlockProcessing::clone() const
{
    return new FunctionBlockProcessing( *this );
}

/////////////////////////////////

FunctionBlockCodec::FunctionBlockCodec()
    : IBusData()
{
}

FunctionBlockCodec::FunctionBlockCodec( const FunctionBlockCodec& rhs )
    : IBusData()
{
}

FunctionBlockCodec::~FunctionBlockCodec()
{
}

bool
FunctionBlockCodec::serialize( IOSSerialize& se )
{
    return false;
}

bool
FunctionBlockCodec::deserialize( IISDeserialize& de )
{
    return false;
}

FunctionBlockCodec*
FunctionBlockCodec::clone() const
{
    return new FunctionBlockCodec( *this );
}

/////////////////////////////////
/////////////////////////////////

FunctionBlockCmd::FunctionBlockCmd( Ieee1394Service& ieee1394service,
                                    EFunctionBlockType eType,
                                    function_block_id_t id,
                                    EControlAttribute eCtrlAttrib )
    : AVCCommand( ieee1394service, AVC1394_FUNCTION_BLOCK_CMD )
    , m_functionBlockType( eType )
    , m_functionBlockId( id )
    , m_controlAttribute( eCtrlAttrib )
    , m_pFBSelector( 0 )
    , m_pFBFeature( 0 )
    , m_pFBProcessing( 0 )
    , m_pFBCodec( 0 )
{
    setSubunitType( eST_Audio );

    switch( m_functionBlockType ) {
        case eFBT_Selector:
            m_pFBSelector = new FunctionBlockSelector;
            break;
        case eFBT_Feature:
            m_pFBFeature = new FunctionBlockFeature;
            break;
        case eFBT_Processing:
            m_pFBProcessing = new FunctionBlockProcessing;
            break;
        case eFBT_Codec:
            m_pFBCodec = new FunctionBlockCodec;
            break;
    }
}

FunctionBlockCmd::FunctionBlockCmd( const FunctionBlockCmd& rhs )
    : AVCCommand( rhs )
    , m_functionBlockType( rhs.m_functionBlockType )
    , m_functionBlockId( rhs.m_functionBlockId )
    , m_controlAttribute( rhs.m_controlAttribute )
    , m_pFBSelector( new FunctionBlockSelector( *rhs.m_pFBSelector ) )
    , m_pFBFeature( new FunctionBlockFeature( *rhs.m_pFBFeature ) )
    , m_pFBProcessing( new FunctionBlockProcessing( *rhs.m_pFBProcessing ) )
    , m_pFBCodec( new FunctionBlockCodec( *rhs.m_pFBCodec ) )
{
}

FunctionBlockCmd::~FunctionBlockCmd()
{
    delete m_pFBSelector;
    m_pFBSelector = 0;
    delete m_pFBFeature;
    m_pFBFeature = 0;
    delete m_pFBProcessing;
    m_pFBProcessing = 0;
    delete m_pFBCodec;
    m_pFBCodec = 0;
}

bool
FunctionBlockCmd::serialize( IOSSerialize& se )
{
    bool bStatus;
    bStatus  = AVCCommand::serialize( se );
    bStatus &= se.write( m_functionBlockType, "FunctionBlockCmd functionBlockType" );
    bStatus &= se.write( m_functionBlockId,   "FunctionBlockCmd functionBlockId" );
    bStatus &= se.write( m_controlAttribute,  "FunctionBlockCmd controlAttribute" );

    switch( m_functionBlockType ) {
        case eFBT_Selector:
            if ( m_pFBSelector ) {
                bStatus &= m_pFBSelector->serialize( se );
            } else {
                bStatus = false;
            }
            break;
        case eFBT_Feature:
            if ( m_pFBFeature ) {
                bStatus &= m_pFBFeature->serialize( se );
            } else {
                bStatus = false;
            }
            break;
        case eFBT_Processing:
            if ( m_pFBProcessing ) {
                bStatus &= m_pFBProcessing->serialize( se );
            } else {
                bStatus = false;
            }
            break;
        case eFBT_Codec:
            if ( m_pFBCodec ) {
                bStatus &= m_pFBCodec->serialize( se );
            } else {
                bStatus = false;
            }
            break;
        default:
            bStatus = false;
    }

    return bStatus;
}

bool
FunctionBlockCmd::deserialize( IISDeserialize& de )
{
    bool bStatus;
    bStatus  = AVCCommand::deserialize( de );

    bStatus &= de.read( &m_functionBlockType );
    bStatus &= de.read( &m_functionBlockId );
    bStatus &= de.read( &m_controlAttribute );

    switch( m_functionBlockType ) {
        case eFBT_Selector:
            if ( !m_pFBSelector ) {
                m_pFBSelector = new FunctionBlockSelector;
            }
            bStatus &= m_pFBSelector->deserialize( de );
            break;
        case eFBT_Feature:
            if ( !m_pFBFeature ) {
                m_pFBFeature = new FunctionBlockFeature;
            }
            bStatus &= m_pFBFeature->deserialize( de );
            break;
        case eFBT_Processing:
            if ( !m_pFBProcessing ) {
                m_pFBProcessing = new FunctionBlockProcessing;
            }
            bStatus &= m_pFBProcessing->deserialize( de );
            break;
        case eFBT_Codec:
            if ( !m_pFBCodec ) {
                m_pFBCodec = new FunctionBlockCodec;
            }
            bStatus &= m_pFBCodec->deserialize( de );
            break;
        default:
            bStatus = false;
    }

    return bStatus;
}

FunctionBlockCmd*
FunctionBlockCmd::clone() const
{
    return new FunctionBlockCmd( *this );
}
