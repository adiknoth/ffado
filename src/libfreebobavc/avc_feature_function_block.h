/* avc_feature_function_block.h
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

#ifndef AVCFEATUREFUNCTIONBLOCK_H
#define AVCFEATUREFUNCTIONBLOCK_H

#include "avc_extended_cmd_generic.h"
#include "avc_generic.h"

#include <libavc1394/avc1394.h>


enum EControlSelector {
    eFCS_Mute        = 0x01,
    eFCS_Volume      = 0x02,
    eFCS_LRBalance   = 0x03,
    eFCS_FRBalance   = 0x04,
    eFCS_Bass        = 0x05,
    eFCS_Mid         = 0x06,
    eFCS_Treble      = 0x07,
    eFCS_GEQ         = 0x08,
    eFCS_AGC         = 0x09,
    eFCS_Delay       = 0x0a,
    eFCS_BassBoost   = 0x0b,
    eFCS_Loudness    = 0x0c,
};

class FeatureFunctionBlockVolume: public IBusData
{
public:
    FeatureFunctionBlockVolume();
    FeatureFunctionBlockVolume( const FeatureFunctionBlockVolume& rhs );
    virtual ~FeatureFunctionBlockVolume();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FeatureFunctionBlockVolume* clone() const;


    selector_length_t      m_selectorLength;
    audio_channel_number_t m_audioChannelNumber;
    control_selector_t     m_controlSelector;
    control_data_length_t  m_controlDataLength;
    u_int16_t              m_volume;
};

///////////////////////////////////////////

#define AVC1394_FUNCTION_BLOCK_CMD 0xB8

// audio function block command only
class FeatureFunctionBlockCmd: public AVCCommand
{
public:
    enum EControlAttribute {
        eCA_Resolution = 0x01,
        eCA_Minimum    = 0x02,
        eCA_Maximum    = 0x03,
        eCA_Default    = 0x04,
        eCA_Duration   = 0x08,
        eCA_Current    = 0x10,
        eCA_Move       = 0x18,
        eCA_Delta      = 0x19
    };

    FeatureFunctionBlockCmd( Ieee1394Service* ieee1394service );
    FeatureFunctionBlockCmd( const FeatureFunctionBlockCmd& rhs );
    virtual ~FeatureFunctionBlockCmd();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FeatureFunctionBlockCmd* clone() const;

    virtual const char* getCmdName() const
	{ return "FeatureFunctionBlockCmd"; }


    bool setFunctionBlockId( function_block_type_t functionBlockId )
        { m_functionBlockId = functionBlockId; return true; }
    bool setControlAttribute( EControlAttribute eControlAttribute )
        { m_controlAttribute = eControlAttribute; return true; }

    function_block_type_t m_functionBlockType;
    function_block_id_t   m_functionBlockId;
    control_attribute_t   m_controlAttribute;

    FeatureFunctionBlockVolume m_volumeControl;
    // all other selectors not implemented
};

#endif
