/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#include "avc_function_block.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"


namespace AVC {


/////////////////////////////////

FunctionBlockFeatureVolume::FunctionBlockFeatureVolume()
    : IBusData()
    , m_controlDataLength( 2 )
    , m_volume( 0 )
{
}

FunctionBlockFeatureVolume::FunctionBlockFeatureVolume( const FunctionBlockFeatureVolume& rhs )
    : m_controlDataLength( rhs.m_controlDataLength )
    , m_volume( rhs.m_volume )
{
}

FunctionBlockFeatureVolume::~FunctionBlockFeatureVolume()
{
}

bool
FunctionBlockFeatureVolume::serialize( Util::Cmd::IOSSerialize& se )
{
    bool bStatus;
    byte_t val;
    bStatus = se.write( m_controlDataLength,  "FunctionBlockFeatureVolume controlDataLength" );
    val = (byte_t)(m_volume >> 8);
    bStatus &= se.write( val,                  "FunctionBlockFeatureVolume volume high" );
    val = m_volume & 0xff;
    bStatus &= se.write( val,                  "FunctionBlockFeatureVolume volume low" );

    return bStatus;
}

bool
FunctionBlockFeatureVolume::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool bStatus;
    byte_t val;
    bStatus = de.read( &m_controlDataLength );
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
FunctionBlockProcessingMixer::serialize( Util::Cmd::IOSSerialize& se )
{
    bool bStatus;
    bStatus = se.write( m_controlSelector,    "FunctionBlockProcessingMixer controlSelector" );

    return bStatus;
}

bool
FunctionBlockProcessingMixer::deserialize( Util::Cmd::IISDeserialize& de )
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
    , m_controlDataLength( 0 )
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
FunctionBlockProcessingEnhancedMixer::serialize( Util::Cmd::IOSSerialize& se )
{
    int todo,done;
    bool bStatus;
    byte_t data_length_hi, data_length_lo;
    
    bStatus  = se.write( m_controlSelector, "FunctionBlockProcessingEnhancedMixer controlSelector" );
    bStatus &= se.write( m_statusSelector,  "FunctionBlockProcessingEnhancedMixer statusSelector" );
    
    switch (m_statusSelector) {
        case eSS_ProgramableState:
            m_controlDataLength=m_LevelData.size();
            data_length_hi=(m_controlDataLength >> 8);
            data_length_lo=(m_controlDataLength & 0xFF);
            bStatus &= se.write( data_length_hi,  "FunctionBlockProcessingEnhancedMixer controlDataLengthHi" );
            bStatus &= se.write( data_length_lo,  "FunctionBlockProcessingEnhancedMixer controlDataLengthLo" );

            for (int i=0;i<m_controlDataLength/8;i++) {
                byte_t value=0;
                
                for (int j=0;j<8;j++) {
                    control_data_ext_length_t bit_value=m_ProgramableStateData.at(i*8+j);
                    value |= bit_value << (7-j);
                }
                
                bStatus &= se.write( value,  "FunctionBlockProcessingEnhancedMixer data" );
            }
            
            todo=m_controlDataLength%8;
            done=m_controlDataLength-todo;
            if (todo) {
                byte_t value=0;
                for (int j=0;j<todo;j++) {
                    control_data_ext_length_t bit_value=m_ProgramableStateData.at(done*8+j);
                    value |= bit_value << (7-j);
                }
                bStatus &= se.write( value,  "FunctionBlockProcessingEnhancedMixer data" );
            }
            break;
        case eSS_Level:
            m_controlDataLength=m_LevelData.size()/2;
            data_length_hi=(m_controlDataLength >> 8);
            data_length_lo=(m_controlDataLength & 0xFF);
            bStatus &= se.write( data_length_hi,  "FunctionBlockProcessingEnhancedMixer controlDataLengthHi" );
            bStatus &= se.write( data_length_lo,  "FunctionBlockProcessingEnhancedMixer controlDataLengthLo" );

            for (int i=0;i<m_controlDataLength/2;i++) {
                mixer_level_t value=m_LevelData.at(i);
                byte_t value_hi=value >> 8;
                byte_t value_lo=value & 0xFF;
                
                bStatus &= se.write( value_hi,  "FunctionBlockProcessingEnhancedMixer data" );
                bStatus &= se.write( value_lo,  "FunctionBlockProcessingEnhancedMixer data" );
            }
            break;
    }
    return bStatus;
}

bool
FunctionBlockProcessingEnhancedMixer::deserialize( Util::Cmd::IISDeserialize& de )
{
    int todo;
    bool bStatus=true;
    bStatus  = de.read( &m_controlSelector );

    // NOTE: the returned value is currently bogus, so overwrite it
    m_controlSelector=FunctionBlockProcessing::eCSE_Processing_EnhancedMixer;

    bStatus &= de.read( &m_statusSelector );

    // same here
    //m_statusSelector = eSS_Level; 
    m_statusSelector = eSS_ProgramableState; 

    byte_t data_length_hi;
    byte_t data_length_lo;
    bStatus &= de.read( &data_length_hi );
    bStatus &= de.read( &data_length_lo );

    m_controlDataLength = (data_length_hi << 8) + data_length_lo;
    switch (m_statusSelector) {
        case eSS_ProgramableState:
            m_ProgramableStateData.clear();
            for (int i=0;i<m_controlDataLength/8;i++) {
                byte_t value;
                bStatus &= de.read( &value);

                for (int j=7;j>=0;j--) {
                    byte_t bit_value;
                    bit_value=(((1<<j) & value) ? 1 : 0);
                    m_ProgramableStateData.push_back(bit_value);
                }
            }

            todo=m_controlDataLength%8;
            if (todo) {
                byte_t value;
                bStatus &= de.read( &value);

                for (int j=7;j>7-todo;j--) {
                    byte_t bit_value;
                    bit_value=(((1<<j) & value) ? 1 : 0);
                    m_ProgramableStateData.push_back(bit_value);
                }
            }
            break;
        case eSS_Level:
            m_LevelData.clear();
            for (int i=0;i<m_controlDataLength/2;i++) {
                byte_t mixer_value_hi=0, mixer_value_lo=0;
                bStatus &= de.read( &mixer_value_hi);
                bStatus &= de.read( &mixer_value_lo);
                mixer_level_t value = (mixer_value_hi << 8) + mixer_value_lo;
                m_LevelData.push_back(value);
            }
            break;
    }

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
FunctionBlockSelector::serialize( Util::Cmd::IOSSerialize& se )
{
    bool bStatus;
    bStatus  = se.write( m_selectorLength,    "FunctionBlockSelector selectorLength" );
    bStatus &= se.write( m_inputFbPlugNumber, "FunctionBlockSelector inputFbPlugNumber" );
    bStatus &= se.write( m_controlSelector,   "FunctionBlockSelector controlSelector" );

    return bStatus;
}

bool
FunctionBlockSelector::deserialize( Util::Cmd::IISDeserialize& de )
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
    , m_controlSelector( eCSE_Feature_Unknown )
    , m_pVolume( new FunctionBlockFeatureVolume )
{
}

FunctionBlockFeature::FunctionBlockFeature( const FunctionBlockFeature& rhs )
    : IBusData()
    , m_selectorLength( rhs.m_selectorLength )
    , m_audioChannelNumber( rhs.m_audioChannelNumber )
    , m_controlSelector( rhs.m_controlSelector )
{
    if ( rhs.m_pVolume ) {
        m_pVolume = new FunctionBlockFeatureVolume( *rhs.m_pVolume );
    }
}

FunctionBlockFeature::~FunctionBlockFeature()
{
    delete m_pVolume;
    m_pVolume = NULL;
}

bool
FunctionBlockFeature::serialize( Util::Cmd::IOSSerialize& se )
{
    bool bStatus;
    bStatus  = se.write( m_selectorLength,     "FunctionBlockFeature selectorLength" );
    bStatus &= se.write( m_audioChannelNumber, "FunctionBlockFeature audioChannelNumber" );
    bStatus &= se.write( m_controlSelector,    "FunctionBlockFeature controlSelector" );

    if ( m_controlSelector == eCSE_Feature_Volume ) {
        bStatus &= m_pVolume->serialize( se );
    } else {
        bStatus = false;
    }

    return bStatus;
}

bool
FunctionBlockFeature::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool bStatus;
    bStatus  = de.read( &m_selectorLength );
    bStatus &= de.read( &m_audioChannelNumber );
    bStatus &= de.read( &m_controlSelector );

    switch( m_controlSelector ) {
    case eCSE_Feature_Volume:
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
FunctionBlockProcessing::serialize( Util::Cmd::IOSSerialize& se )
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
FunctionBlockProcessing::deserialize( Util::Cmd::IISDeserialize& de )
{
    // NOTE: apparently the fbCmd of the STATUS type,
    // with EnhancedMixer controlSelector returns with this
    // controlSelector type changed to Mixer, making it
    // impossible to choose the correct response handler
    // based upon the response only.
    
    // HACK: we assume that it is the same as the sent one
    // we also look at our data structure to figure out what we sent
    byte_t controlSelector=eCSE_Processing_Unknown;
    if(m_pMixer) {
        controlSelector=eCSE_Processing_Mixer;
    } else if(m_pEnhancedMixer) {
        controlSelector=eCSE_Processing_EnhancedMixer;
    }
    
    bool bStatus;
    bStatus  = de.read( &m_selectorLength );
    bStatus &= de.read( &m_fbInputPlugNumber );
    bStatus &= de.read( &m_inputAudioChannelNumber );
    bStatus &= de.read( &m_outputAudioChannelNumber );

    byte_t controlSelector_response;
    bStatus &= de.peek( &controlSelector_response );
/*    debugOutput(DEBUG_LEVEL_VERBOSE,"ctrlsel: orig = %02X, resp = %02X\n",
        controlSelector, controlSelector_response);*/
    
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

    byte_t tmp;
    if (de.peek(&tmp)) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Unprocessed bytes:\n");
        while (de.read(&tmp)) {
            debugOutput(DEBUG_LEVEL_VERBOSE," %02X\n",tmp);
        }
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
FunctionBlockCodec::serialize( Util::Cmd::IOSSerialize& se )
{
    return false;
}

bool
FunctionBlockCodec::deserialize( Util::Cmd::IISDeserialize& de )
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
FunctionBlockCmd::serialize( Util::Cmd::IOSSerialize& se )
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
FunctionBlockCmd::deserialize( Util::Cmd::IISDeserialize& de )
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

}
