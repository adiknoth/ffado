/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#ifndef AVCFUNCTIONBLOCK_H
#define AVCFUNCTIONBLOCK_H

#include "../general/avc_extended_cmd_generic.h"
#include "../general/avc_generic.h"

#include <libavc1394/avc1394.h>
#include <vector>
using namespace std;

namespace AVC {


class FunctionBlockFeatureVolume: public IBusData
{
public:
    FunctionBlockFeatureVolume();
    FunctionBlockFeatureVolume( const FunctionBlockFeatureVolume& rhs );
    virtual ~FunctionBlockFeatureVolume();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockFeatureVolume* clone() const;

    control_data_length_t  m_controlDataLength;
    u_int16_t              m_volume;
};

///////////////////////////////////////////

class FunctionBlockProcessingMixer: public IBusData
{
public:
    FunctionBlockProcessingMixer();
    FunctionBlockProcessingMixer( const FunctionBlockProcessingMixer& rhs );
    virtual ~FunctionBlockProcessingMixer();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockProcessingMixer* clone() const;

    control_selector_t m_controlSelector;
};

///////////////////////////////////////////

/* 
 * The enhanced mixer feature function block is not
 * working on all current BeBoB devices.  This code
 * is there for not really tested or even working.
 */
class FunctionBlockProcessingEnhancedMixer: public IBusData
{
public:
    enum EStatusSelector {
        eSS_ProgramableState = 0x00,
        eSS_Level            = 0x01,
    };

    FunctionBlockProcessingEnhancedMixer();
    FunctionBlockProcessingEnhancedMixer(
        const FunctionBlockProcessingEnhancedMixer& rhs );
    virtual ~FunctionBlockProcessingEnhancedMixer();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockProcessingEnhancedMixer* clone() const;

    control_selector_t        m_controlSelector;
    status_selector_t         m_statusSelector;
    control_data_ext_length_t m_controlDataLength;
    vector<mixer_programmable_state_t> m_ProgramableStateData;
    vector<mixer_level_t>              m_LevelData;
};

///////////////////////////////////////////
///////////////////////////////////////////

class FunctionBlockSelector: public IBusData
{
// untested
public:
    // Control selector encoding
    enum EControlSelectorEncoding {
        eCSE_Selector_Unknown           = 0x00,
        eCSE_Selector_Selector          = 0x01,
    };

    FunctionBlockSelector();
    FunctionBlockSelector( const FunctionBlockSelector& rhs );
    virtual ~FunctionBlockSelector();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockSelector* clone() const;

    selector_length_t      m_selectorLength;
    input_fb_plug_number_t m_inputFbPlugNumber;
    control_selector_t     m_controlSelector;
};

///////////////////////////////////////////

class FunctionBlockFeature: public IBusData
{
// no complete implementation
public:
    // Control selector encoding
    enum EControlSelectorEncoding {
        eCSE_Feature_Unknown            = 0x00,
        eCSE_Feature_Mute               = 0x01,
        eCSE_Feature_Volume             = 0x02,
        eCSE_Feature_LRBalance          = 0x03,
        eCSE_Feature_FRBalance          = 0x04,
        eCSE_Feature_Bass               = 0x05,
        eCSE_Feature_Mid                = 0x06,
        eCSE_Feature_Treble             = 0x07,
        eCSE_Feature_GEQ                = 0x08,
        eCSE_Feature_AGC                = 0x09,
        eCSE_Feature_Delay              = 0x0a,
        eCSE_Feature_BassBoost          = 0x0b,
        eCSE_Feature_Loudness           = 0x0c,
    };

    FunctionBlockFeature();
    FunctionBlockFeature( const FunctionBlockFeature& rhs );
    virtual ~FunctionBlockFeature();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockFeature* clone() const;

    selector_length_t           m_selectorLength;
    audio_channel_number_t      m_audioChannelNumber;
    control_selector_t          m_controlSelector;

    FunctionBlockFeatureVolume*     m_pVolume;
};

///////////////////////////////////////////

class FunctionBlockProcessing: public IBusData
{
// no complete implementation
public:
    // Function block selector
    enum EProcessingTypeEncoding {
        ePTE_Mixer                      = 0x01,
        ePTE_Generic                    = 0x02,
        ePTE_UpDown                     = 0x03,
        ePTE_DolbyProLogic              = 0x04,
        ePTE_3dStereoExtender           = 0x05,
        ePTE_Reverberation              = 0x06,
        ePTE_Chorus                     = 0x07,
        ePTE_DynamicRangeCompression    = 0x08,
    };

    // Control selector encoding
    enum EControlSelectorEncoding {
        eCSE_Processing_Unknown         = 0x00,
        eCSE_Processing_Enable          = 0x01,
        eCSE_Processing_Mode            = 0x02,
        eCSE_Processing_Mixer           = 0x03,
        eCSE_Processing_EnhancedMixer   = 0xf1,

        // lots of definitions missing

    };

    FunctionBlockProcessing();
    FunctionBlockProcessing( const FunctionBlockProcessing& rhs );
    virtual ~FunctionBlockProcessing();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockProcessing* clone() const;

    selector_length_t             m_selectorLength;
    input_fb_plug_number_t        m_fbInputPlugNumber;
    input_audio_channel_number_t  m_inputAudioChannelNumber;
    output_audio_channel_number_t m_outputAudioChannelNumber;

    FunctionBlockProcessingMixer*         m_pMixer;
    FunctionBlockProcessingEnhancedMixer* m_pEnhancedMixer;
};

///////////////////////////////////////////

class FunctionBlockCodec: public IBusData
{
// dummy implementation
public:
    // CODEC type endcoding
    enum ECodecTypeEncoding {
        eCTE_Unknown                    = 0x00,
        eCTE_Ac3Decoder                 = 0x01,
        eCTE_MpegDecoder                = 0x02,
        eCTE_DtsDecoder                 = 0x03,
    };

    FunctionBlockCodec();
    FunctionBlockCodec( const FunctionBlockCodec& rhs );
    virtual ~FunctionBlockCodec();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockCodec* clone() const;
};

///////////////////////////////////////////
///////////////////////////////////////////

#define AVC1394_FUNCTION_BLOCK_CMD 0xB8

class FunctionBlockCmd: public AVCCommand
{
public:
    enum EFunctionBlockType {
        eFBT_Selector   = 0x80,
        eFBT_Feature    = 0x81,
        eFBT_Processing = 0x82,
        eFBT_Codec      = 0x83,
    };

    enum EControlAttribute {
        eCA_Resolution = 0x01,
        eCA_Minimum    = 0x02,
        eCA_Maximum    = 0x03,
        eCA_Default    = 0x04,
        eCA_Duration   = 0x08,
        eCA_Current    = 0x10,
        eCA_Move       = 0x18,
        eCA_Delta      = 0x19,
    };

    FunctionBlockCmd( Ieee1394Service& ieee1394service,
                      EFunctionBlockType eType,
                      function_block_id_t id,
                      EControlAttribute eCtrlAttrib );
    FunctionBlockCmd( const FunctionBlockCmd& rhs );
    virtual ~FunctionBlockCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    virtual FunctionBlockCmd* clone() const;

    virtual const char* getCmdName() const
        { return "FunctionBlockCmd"; }

    function_block_type_t m_functionBlockType;
    function_block_id_t   m_functionBlockId;
    control_attribute_t   m_controlAttribute;

    FunctionBlockSelector*      m_pFBSelector;
    FunctionBlockFeature*       m_pFBFeature;
    FunctionBlockProcessing*    m_pFBProcessing;
    FunctionBlockCodec*         m_pFBCodec;
};

}

#endif
