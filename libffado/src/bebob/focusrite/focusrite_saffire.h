/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef BEBOB_FOCUSRITE_SAFFIRE_DEVICE_H
#define BEBOB_FOCUSRITE_SAFFIRE_DEVICE_H

#include "focusrite_generic.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

// -- Original Saffire --

// no need for control id's, we can directly compute the addresses of the matrix mixer
/*

MIXER LAYOUT ():

OFFSET: 30

   |-- Out9/10--| |-- Out1/2 --| |-- Out3/4 --| |-- Out5/6 --| |-- Out7/8 --|
P5  0:    0/    0  1:  110/  110  2:    0/    0  3:    0/    0  4:    0/    0
P1  5:    0/    0  6:32767/32767  7:    0/    0  8:    0/    0  9:    0/    0
P2 10:    0/    0 11:    0/    0 12:32767/32767 13:    0/    0 14:    0/    0
P3 15:    0/    0 16:    0/    0 17:    0/    0 18:32767/32767 19:    0/    0
P4 20:    0/    0 21:    0/    0 22:    0/    0 23:    0/    0 24:32767/32767
R1 25:    0/    0 26:    0/    0 27:    0/    0 28:    0/    0 29:    0/    0
R2 30:    0/    0 31:    0/    0 32:    0/    0 33:    0/    0 34:    0/    0
Fx 35:    0/    0 36:    0/    0 37:    0/    0 38:    0/    0 39:    0/    0

P5: DAW ch 9/10
P1: DAW ch 1/2
P2: DAW ch 3/4
P3: DAW ch 5/6
P4: DAW ch 7/8
R1: HW INPUT ch 1/2 / Reverb ch 1
R2: HW INPUT ch 3/4 / Reverb ch 2
Fx: reverb/fx return? / input mix

*/

// the control ID's
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT12      55
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT34      56
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT56      57
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT78      58
#define FR_SAFFIRE_CMD_ID_BITFIELD_OUT910     59

#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DIM       24
#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_MUTE      25
#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DACIGNORE 26
#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_HWCTRL    27
#define FR_SAFFIRE_CMD_ID_BITFIELD_BIT_DAC        0

// other stuff
#define FR_SAFFIRE_CMD_ID_MONITOR_DIAL      61

#define FR_SAFFIRE_CMD_ID_INPUT_SOURCE      62
#define FR_SAFFIRE_CMD_ID_INPUT_SOURCE_SPDIF  1
#define FR_SAFFIRE_CMD_ID_INPUT_SOURCE_ANALOG 0

#define FR_SAFFIRE_CMD_ID_MONO_MODE         63
#define FR_SAFFIRE_CMD_ID_MONO_MODE_STEREO   0
#define FR_SAFFIRE_CMD_ID_MONO_MODE_MONO     1

#define FR_SAFFIRE_CMD_ID_METERING_IN1      64
#define FR_SAFFIRE_CMD_ID_METERING_IN2      65
#define FR_SAFFIRE_CMD_ID_METERING_IN3      66
#define FR_SAFFIRE_CMD_ID_METERING_IN4      67

#define FR_SAFFIRE_CMD_ID_METERING_PC1      68
#define FR_SAFFIRE_CMD_ID_METERING_PC2      69
#define FR_SAFFIRE_CMD_ID_METERING_PC3      70
#define FR_SAFFIRE_CMD_ID_METERING_PC4      71
#define FR_SAFFIRE_CMD_ID_METERING_PC5      72
#define FR_SAFFIRE_CMD_ID_METERING_PC6      73
#define FR_SAFFIRE_CMD_ID_METERING_PC7      74
#define FR_SAFFIRE_CMD_ID_METERING_PC8      75
#define FR_SAFFIRE_CMD_ID_METERING_PC9      76
#define FR_SAFFIRE_CMD_ID_METERING_PC10     77

#define FR_SAFFIRE_CMD_ID_DEVICE_MODE       78
#define FR_SAFFIRE_CMD_ID_DEVICE_MODE_NORMAL 0
#define FR_SAFFIRE_CMD_ID_DEVICE_MODE_SCARD  1

#define FR_SAFFIRE_CMD_ID_EXTERNAL_LOCK     79
#define FR_SAFFIRE_CMD_ID_AUDIO_ON_STATUS   80
#define FR_SAFFIRE_CMD_ID_SAVE_SETTINGS     82

#define FR_SAFFIRELE_CMD_ID_DSP_REVISION        1022
#define FR_SAFFIRELE_CMD_ID_DSP_VERSION         1023

// -- Saffire LE --

// The ID's for the hardware input matrix mixer
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT1       0
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT3       1
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT2       2
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT4       3
#define FR_SAFFIRELE_CMD_ID_PC3_TO_OUT1       4
#define FR_SAFFIRELE_CMD_ID_PC3_TO_OUT3       5
#define FR_SAFFIRELE_CMD_ID_PC3_TO_OUT2       6
#define FR_SAFFIRELE_CMD_ID_PC3_TO_OUT4       7
#define FR_SAFFIRELE_CMD_ID_PC5_TO_OUT1       8
#define FR_SAFFIRELE_CMD_ID_PC5_TO_OUT3       9
#define FR_SAFFIRELE_CMD_ID_PC5_TO_OUT2       10
#define FR_SAFFIRELE_CMD_ID_PC5_TO_OUT4       11
#define FR_SAFFIRELE_CMD_ID_PC7_TO_OUT1       12
#define FR_SAFFIRELE_CMD_ID_PC7_TO_OUT3       13
#define FR_SAFFIRELE_CMD_ID_PC7_TO_OUT2       14
#define FR_SAFFIRELE_CMD_ID_PC7_TO_OUT4       15
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT1       16
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT3       17
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT2       18
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT4       19
#define FR_SAFFIRELE_CMD_ID_PC4_TO_OUT1       20
#define FR_SAFFIRELE_CMD_ID_PC4_TO_OUT3       21
#define FR_SAFFIRELE_CMD_ID_PC4_TO_OUT2       22
#define FR_SAFFIRELE_CMD_ID_PC4_TO_OUT4       23
#define FR_SAFFIRELE_CMD_ID_PC6_TO_OUT1       24
#define FR_SAFFIRELE_CMD_ID_PC6_TO_OUT3       25
#define FR_SAFFIRELE_CMD_ID_PC6_TO_OUT2       26
#define FR_SAFFIRELE_CMD_ID_PC6_TO_OUT4       27
#define FR_SAFFIRELE_CMD_ID_PC8_TO_OUT1       28
#define FR_SAFFIRELE_CMD_ID_PC8_TO_OUT3       29
#define FR_SAFFIRELE_CMD_ID_PC8_TO_OUT2       30
#define FR_SAFFIRELE_CMD_ID_PC8_TO_OUT4       31

#define FR_SAFFIRELE_CMD_ID_IN1_TO_OUT1       32
#define FR_SAFFIRELE_CMD_ID_IN1_TO_OUT3       33
#define FR_SAFFIRELE_CMD_ID_IN1_TO_OUT2       34
#define FR_SAFFIRELE_CMD_ID_IN1_TO_OUT4       35
#define FR_SAFFIRELE_CMD_ID_IN3_TO_OUT1       36
#define FR_SAFFIRELE_CMD_ID_IN3_TO_OUT3       37
#define FR_SAFFIRELE_CMD_ID_IN3_TO_OUT2       38
#define FR_SAFFIRELE_CMD_ID_IN3_TO_OUT4       39
#define FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT1    40
#define FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT3    41
#define FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT2    42
#define FR_SAFFIRELE_CMD_ID_SPDIF1_TO_OUT4    43
#define FR_SAFFIRELE_CMD_ID_IN2_TO_OUT1       44
#define FR_SAFFIRELE_CMD_ID_IN2_TO_OUT3       45
#define FR_SAFFIRELE_CMD_ID_IN2_TO_OUT2       46
#define FR_SAFFIRELE_CMD_ID_IN2_TO_OUT4       47
#define FR_SAFFIRELE_CMD_ID_IN4_TO_OUT1       48
#define FR_SAFFIRELE_CMD_ID_IN4_TO_OUT3       49
#define FR_SAFFIRELE_CMD_ID_IN4_TO_OUT2       50
#define FR_SAFFIRELE_CMD_ID_IN4_TO_OUT4       51
#define FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT1    52
#define FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT3    53
#define FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT2    54
#define FR_SAFFIRELE_CMD_ID_SPDIF2_TO_OUT4    55

#define FR_SAFFIRELE_CMD_ID_SWAP_OUT4_OUT1_48K   64

// 96kHz controls
#define FR_SAFFIRELE_CMD_ID_IN1_TO_RECMIX_96K        66
#define FR_SAFFIRELE_CMD_ID_IN3_TO_RECMIX_96K        67
#define FR_SAFFIRELE_CMD_ID_SPDIF1_TO_RECMIX_96K     68
#define FR_SAFFIRELE_CMD_ID_IN2_TO_RECMIX_96K        69
#define FR_SAFFIRELE_CMD_ID_IN4_TO_RECMIX_96K        70
#define FR_SAFFIRELE_CMD_ID_SPDIF2_TO_RECMIX_96K     71

#define FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT1_96K       72
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT1_96K          73
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT1_96K          74
#define FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT3_96K       75
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT3_96K          76
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT3_96K          77
#define FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT2_96K       78
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT2_96K          79
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT2_96K          80
#define FR_SAFFIRELE_CMD_ID_RECMIX_TO_OUT4_96K       81
#define FR_SAFFIRELE_CMD_ID_PC1_TO_OUT4_96K          82
#define FR_SAFFIRELE_CMD_ID_PC2_TO_OUT4_96K          83

#define FR_SAFFIRELE_CMD_ID_SWAP_OUT4_OUT1_96K   84

// metering
#define FR_SAFFIRELE_CMD_ID_METERING_IN1      90
#define FR_SAFFIRELE_CMD_ID_METERING_IN3      91
#define FR_SAFFIRELE_CMD_ID_METERING_SPDIF1   92
#define FR_SAFFIRELE_CMD_ID_METERING_IN2      93
#define FR_SAFFIRELE_CMD_ID_METERING_IN4      94
#define FR_SAFFIRELE_CMD_ID_METERING_SPDIF2   95

#define FR_SAFFIRELE_CMD_ID_METERING_OUT1      96
#define FR_SAFFIRELE_CMD_ID_METERING_OUT3      97
#define FR_SAFFIRELE_CMD_ID_METERING_OUT5      98
#define FR_SAFFIRELE_CMD_ID_METERING_OUT7      99
#define FR_SAFFIRELE_CMD_ID_METERING_OUT2      100
#define FR_SAFFIRELE_CMD_ID_METERING_OUT4      101
#define FR_SAFFIRELE_CMD_ID_METERING_OUT6      102
#define FR_SAFFIRELE_CMD_ID_METERING_OUT8      103

#define FR_SAFFIRELE_CMD_ID_METERING_PC1       104
#define FR_SAFFIRELE_CMD_ID_METERING_PC3       105
#define FR_SAFFIRELE_CMD_ID_METERING_PC2       106
#define FR_SAFFIRELE_CMD_ID_METERING_PC4       107

// other stuff
#define FR_SAFFIRELE_CMD_ID_HIGH_GAIN_LINE3   85
#define FR_SAFFIRELE_CMD_ID_HIGH_GAIN_LINE4   86

#define FR_SAFFIRELE_CMD_ID_BITFIELD_OUT12      87
#define FR_SAFFIRELE_CMD_ID_BITFIELD_OUT34      88
#define FR_SAFFIRELE_CMD_ID_BITFIELD_OUT56      89


#define FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_DIM       24
#define FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_MUTE      25
#define FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_DACIGNORE 26
#define FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_HWCTRL    27
#define FR_SAFFIRELE_CMD_ID_BITFIELD_BIT_DAC        0

#define FR_SAFFIRELE_CMD_ID_EXT_CLOCK_LOCK       108
#define FR_SAFFIRELE_CMD_ID_AUDIO_ON             109
#define FR_SAFFIRELE_CMD_ID_SAVE_SETTINGS        110
#define FR_SAFFIRELE_CMD_ID_MIDITHRU             111
#define FR_SAFFIRELE_CMD_ID_SPDIF_TRANSPARENT    112


namespace BeBoB {
namespace Focusrite {

class SaffireDevice;

class SaffireMatrixMixer : public FocusriteMatrixMixer
{
public:
    enum eMatrixMixerType {
        eMMT_SaffireStereoMatrixMix,
        eMMT_SaffireMonoMatrixMix,
        eMMT_LEMix48,
        eMMT_LEMix96,
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
    SaffireDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~SaffireDevice() {};

    virtual void showDevice();
    virtual void setVerboseLevel(int l);

    virtual bool buildMixer();
    virtual bool destroyMixer();

private:
    Control::Container *m_MixerContainer;
    bool m_isSaffireLE;
};

} // namespace Focusrite
} // namespace BeBoB

#endif
