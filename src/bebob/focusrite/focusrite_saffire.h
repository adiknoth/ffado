/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef BEBOB_FOCUSRITE_SAFFIRE_DEVICE_H
#define BEBOB_FOCUSRITE_SAFFIRE_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "focusrite_generic.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

// The ID's for the hardware input matrix mixer
#define FR_SAFFIRE_CMD_ID_IN1_TO_OUT1       1
#define FR_SAFFIRE_CMD_ID_IN1_TO_OUT3       2
#define FR_SAFFIRE_CMD_ID_IN1_TO_OUT5       3
#define FR_SAFFIRE_CMD_ID_IN1_TO_OUT7       4
#define FR_SAFFIRE_CMD_ID_IN1_TO_OUT9       0
#define FR_SAFFIRE_CMD_ID_IN2_TO_OUT2       16
#define FR_SAFFIRE_CMD_ID_IN2_TO_OUT4       17
#define FR_SAFFIRE_CMD_ID_IN2_TO_OUT6       18
#define FR_SAFFIRE_CMD_ID_IN2_TO_OUT8       19
#define FR_SAFFIRE_CMD_ID_IN2_TO_OUT10      15
#define FR_SAFFIRE_CMD_ID_IN3_TO_OUT1       6
#define FR_SAFFIRE_CMD_ID_IN3_TO_OUT3       7
#define FR_SAFFIRE_CMD_ID_IN3_TO_OUT5       8
#define FR_SAFFIRE_CMD_ID_IN3_TO_OUT7       9
#define FR_SAFFIRE_CMD_ID_IN3_TO_OUT9       5
#define FR_SAFFIRE_CMD_ID_IN4_TO_OUT2       21
#define FR_SAFFIRE_CMD_ID_IN4_TO_OUT4       22
#define FR_SAFFIRE_CMD_ID_IN4_TO_OUT6       23
#define FR_SAFFIRE_CMD_ID_IN4_TO_OUT8       24
#define FR_SAFFIRE_CMD_ID_IN4_TO_OUT10      20

// reverb return id's (part of hardware input mixer)
#define FR_SAFFIRE_CMD_ID_REV1_TO_OUT1       11
#define FR_SAFFIRE_CMD_ID_REV2_TO_OUT2       26
#define FR_SAFFIRE_CMD_ID_REV1_TO_OUT3       12
#define FR_SAFFIRE_CMD_ID_REV2_TO_OUT4       27
#define FR_SAFFIRE_CMD_ID_REV1_TO_OUT5       13
#define FR_SAFFIRE_CMD_ID_REV2_TO_OUT6       28
#define FR_SAFFIRE_CMD_ID_REV1_TO_OUT7       14
#define FR_SAFFIRE_CMD_ID_REV2_TO_OUT8       29
#define FR_SAFFIRE_CMD_ID_REV1_TO_OUT9       10
#define FR_SAFFIRE_CMD_ID_REV2_TO_OUT10      25

// The ID's for the playback matrix mixer
#define FR_SAFFIRE_CMD_ID_PC12_TO_OUT12     36
#define FR_SAFFIRE_CMD_ID_PC12_TO_OUT34     37
#define FR_SAFFIRE_CMD_ID_PC12_TO_OUT56     38
#define FR_SAFFIRE_CMD_ID_PC12_TO_OUT79     39
#define FR_SAFFIRE_CMD_ID_PC12_TO_OUT910    35
#define FR_SAFFIRE_CMD_ID_PC34_TO_OUT12     41
#define FR_SAFFIRE_CMD_ID_PC34_TO_OUT34     42
#define FR_SAFFIRE_CMD_ID_PC34_TO_OUT56     43
#define FR_SAFFIRE_CMD_ID_PC34_TO_OUT79     44
#define FR_SAFFIRE_CMD_ID_PC34_TO_OUT910    40
#define FR_SAFFIRE_CMD_ID_PC56_TO_OUT12     46
#define FR_SAFFIRE_CMD_ID_PC56_TO_OUT34     47
#define FR_SAFFIRE_CMD_ID_PC56_TO_OUT56     48
#define FR_SAFFIRE_CMD_ID_PC56_TO_OUT79     49
#define FR_SAFFIRE_CMD_ID_PC56_TO_OUT910    45
#define FR_SAFFIRE_CMD_ID_PC78_TO_OUT12     51
#define FR_SAFFIRE_CMD_ID_PC78_TO_OUT34     52
#define FR_SAFFIRE_CMD_ID_PC78_TO_OUT56     53
#define FR_SAFFIRE_CMD_ID_PC78_TO_OUT79     54
#define FR_SAFFIRE_CMD_ID_PC78_TO_OUT910    50
#define FR_SAFFIRE_CMD_ID_PC910_TO_OUT12    31
#define FR_SAFFIRE_CMD_ID_PC910_TO_OUT34    32
#define FR_SAFFIRE_CMD_ID_PC910_TO_OUT56    33
#define FR_SAFFIRE_CMD_ID_PC910_TO_OUT79    34
#define FR_SAFFIRE_CMD_ID_PC910_TO_OUT910   30

// the control ID's
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT12      55
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT34      56
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT56      57
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT78      58
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT910     59

#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DIM       24
#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE      25
#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_HWCTRL    26

// other stuff
#define FR_SAFFIRE_CMD_ID_MONITOR_DIAL    61
#define FR_SAFFIRE_CMD_ID_SPDIF_SWITCH    62

#define FR_SAFFIRE_CMD_ID_METERING_IN1      64
#define FR_SAFFIRE_CMD_ID_METERING_IN2      65
#define FR_SAFFIRE_CMD_ID_METERING_IN3      66
#define FR_SAFFIRE_CMD_ID_METERING_IN4      67

#define FR_SAFFIRE_CMD_ID_SPDIF_DETECT    79

namespace BeBoB {
namespace Focusrite {

class SaffireDevice;

class SaffireMatrixMixer : public FocusriteMatrixMixer
{
public:
    enum eMatrixMixerType {
        eMMT_InputMix,
        eMMT_PCMix
    };
    
public:
    SaffireMatrixMixer(SaffireDevice& parent, enum eMatrixMixerType type);
    SaffireMatrixMixer(SaffireDevice& parent, enum eMatrixMixerType type, std::string n);
    virtual ~SaffireMatrixMixer() {};

    virtual void show();

protected:
    void init();
    enum eMatrixMixerType m_type;
};

class SaffireDevice : public FocusriteDevice {
public:
    SaffireDevice( Ieee1394Service& ieee1394Service,
              std::auto_ptr<ConfigRom>( configRom ));
    virtual ~SaffireDevice();

    virtual void showDevice();
    virtual void setVerboseLevel(int l);

    virtual bool buildMixer();
    virtual bool destroyMixer();

private:
    Control::Container *m_MixerContainer;
};

} // namespace Focusrite
} // namespace BeBoB

#endif
