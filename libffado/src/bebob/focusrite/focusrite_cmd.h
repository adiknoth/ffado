/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#ifndef FOCUSRITEVENDORDEPENDENT_H
#define FOCUSRITEVENDORDEPENDENT_H

#include "libavc/general/avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

#include <libavc1394/avc1394.h>

#define FOCUSRITE_CMD_ID_SAMPLERATE         84

#define FOCUSRITE_CMD_ID_PHANTOM14          98
#define FOCUSRITE_CMD_ID_PHANTOM58          99
#define FOCUSRITE_CMD_ID_INSERT1            100
#define FOCUSRITE_CMD_ID_INSERT2            101
#define FOCUSRITE_CMD_ID_AC3_PASSTHROUGH    103
#define FOCUSRITE_CMD_ID_MIDI_TRU           104

#define FOCUSRITE_CMD_SAMPLERATE_44K1   1
#define FOCUSRITE_CMD_SAMPLERATE_48K    2
#define FOCUSRITE_CMD_SAMPLERATE_88K2   3
#define FOCUSRITE_CMD_SAMPLERATE_96K    4
#define FOCUSRITE_CMD_SAMPLERATE_176K4  5
#define FOCUSRITE_CMD_SAMPLERATE_192K   6

#define FOCUSRITE_CMD_ID_PC1_TO_OUT1    52
#define FOCUSRITE_CMD_ID_PC2_TO_OUT2    54
#define FOCUSRITE_CMD_ID_MIX1_TO_OUT1   53
#define FOCUSRITE_CMD_ID_MIX2_TO_OUT2   55

#define FOCUSRITE_CMD_ID_PC1_TO_OUT3    56
#define FOCUSRITE_CMD_ID_PC2_TO_OUT4    59
#define FOCUSRITE_CMD_ID_PC3_TO_OUT3    57
#define FOCUSRITE_CMD_ID_PC4_TO_OUT4    60
#define FOCUSRITE_CMD_ID_MIX1_TO_OUT3   58
#define FOCUSRITE_CMD_ID_MIX2_TO_OUT4   61

#define FOCUSRITE_CMD_ID_PC1_TO_OUT5    62
#define FOCUSRITE_CMD_ID_PC2_TO_OUT6    65
#define FOCUSRITE_CMD_ID_PC5_TO_OUT5    63
#define FOCUSRITE_CMD_ID_PC6_TO_OUT6    66
#define FOCUSRITE_CMD_ID_MIX1_TO_OUT5   64
#define FOCUSRITE_CMD_ID_MIX2_TO_OUT6   67

#define FOCUSRITE_CMD_ID_PC1_TO_OUT7    68
#define FOCUSRITE_CMD_ID_PC2_TO_OUT8    71
#define FOCUSRITE_CMD_ID_PC7_TO_OUT7    69
#define FOCUSRITE_CMD_ID_PC8_TO_OUT8    72
#define FOCUSRITE_CMD_ID_MIX1_TO_OUT7   70
#define FOCUSRITE_CMD_ID_MIX2_TO_OUT8   73

#define FOCUSRITE_CMD_ID_PC1_TO_OUT9    74
#define FOCUSRITE_CMD_ID_PC2_TO_OUT10   77
#define FOCUSRITE_CMD_ID_PC9_TO_OUT9    75
#define FOCUSRITE_CMD_ID_PC10_TO_OUT10  78
#define FOCUSRITE_CMD_ID_MIX1_TO_OUT9   76
#define FOCUSRITE_CMD_ID_MIX2_TO_OUT10  79

namespace BeBoB {
namespace Focusrite {

class FocusriteVendorDependentCmd: public AVC::VendorDependentCmd
{
public:
    FocusriteVendorDependentCmd(Ieee1394Service& ieee1394service);
    virtual ~FocusriteVendorDependentCmd();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "FocusriteVendorDependentCmd"; }
    
    byte_t m_arg1;
    byte_t m_arg2;
    
    quadlet_t m_id;
    quadlet_t m_value;
    
};

}
}

#endif // FOCUSRITEVENDORDEPENDENT_H
