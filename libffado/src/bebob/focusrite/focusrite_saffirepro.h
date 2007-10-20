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

#ifndef BEBOB_FOCUSRITE_SAFFIRE_PRO_DEVICE_H
#define BEBOB_FOCUSRITE_SAFFIRE_PRO_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "focusrite_generic.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

#include <vector>
#include <string>

#define FR_SAFFIREPRO_CMD_ID_SAMPLERATE         84
#define FR_SAFFIREPRO_CMD_ID_SAMPLERATE_MIRROR  115

#define FOCUSRITE_CMD_SAMPLERATE_44K1   1
#define FOCUSRITE_CMD_SAMPLERATE_48K    2
#define FOCUSRITE_CMD_SAMPLERATE_88K2   3
#define FOCUSRITE_CMD_SAMPLERATE_96K    4
#define FOCUSRITE_CMD_SAMPLERATE_176K4  5
#define FOCUSRITE_CMD_SAMPLERATE_192K   6

#define FR_SAFFIREPRO_CMD_ID_STREAMING          90

#define FR_SAFFIREPRO_CMD_ID_PHANTOM14          98
#define FR_SAFFIREPRO_CMD_ID_PHANTOM58          99
#define FR_SAFFIREPRO_CMD_ID_INSERT1            100
#define FR_SAFFIREPRO_CMD_ID_INSERT2            101
#define FR_SAFFIREPRO_CMD_ID_AC3_PASSTHROUGH    103
#define FR_SAFFIREPRO_CMD_ID_MIDI_TRU           104

#define FR_SAFFIREPRO_CMD_ID_DIM_INDICATOR      88
#define FR_SAFFIREPRO_CMD_ID_MUTE_INDICATOR      88

#define FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT12      80
#define FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT34      81
#define FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT56      82
#define FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT78      83

#define FR_SAFFIREPRO_CMD_ID_BITFIELD_BIT_MUTE      24
#define FR_SAFFIREPRO_CMD_ID_BITFIELD_BIT_HWCTRL    26
#define FR_SAFFIREPRO_CMD_ID_BITFIELD_BIT_PAD       27
#define FR_SAFFIREPRO_CMD_ID_BITFIELD_BIT_DIM       28


#define FR_SAFFIREPRO_CMD_ID_AN1_TO_IMIXL        0
#define FR_SAFFIREPRO_CMD_ID_AN1_TO_IMIXR        1
#define FR_SAFFIREPRO_CMD_ID_AN2_TO_IMIXL        2
#define FR_SAFFIREPRO_CMD_ID_AN2_TO_IMIXR        3
#define FR_SAFFIREPRO_CMD_ID_AN3_TO_IMIXL        4
#define FR_SAFFIREPRO_CMD_ID_AN3_TO_IMIXR        5
#define FR_SAFFIREPRO_CMD_ID_AN4_TO_IMIXL        6
#define FR_SAFFIREPRO_CMD_ID_AN4_TO_IMIXR        7
#define FR_SAFFIREPRO_CMD_ID_AN5_TO_IMIXL        8
#define FR_SAFFIREPRO_CMD_ID_AN5_TO_IMIXR        9
#define FR_SAFFIREPRO_CMD_ID_AN6_TO_IMIXL       10
#define FR_SAFFIREPRO_CMD_ID_AN6_TO_IMIXR       11
#define FR_SAFFIREPRO_CMD_ID_AN7_TO_IMIXL       12
#define FR_SAFFIREPRO_CMD_ID_AN7_TO_IMIXR       13
#define FR_SAFFIREPRO_CMD_ID_AN8_TO_IMIXL       14
#define FR_SAFFIREPRO_CMD_ID_AN8_TO_IMIXR       15
#define FR_SAFFIREPRO_CMD_ID_SPDIFL_TO_IMIXL    16
#define FR_SAFFIREPRO_CMD_ID_SPDIFL_TO_IMIXR    17
#define FR_SAFFIREPRO_CMD_ID_SPDIFR_TO_IMIXL    18
#define FR_SAFFIREPRO_CMD_ID_SPDIFR_TO_IMIXR    19

#define FR_SAFFIREPRO_CMD_ID_ADAT11_TO_IMIXL    20
#define FR_SAFFIREPRO_CMD_ID_ADAT11_TO_IMIXR    21
#define FR_SAFFIREPRO_CMD_ID_ADAT12_TO_IMIXL    22
#define FR_SAFFIREPRO_CMD_ID_ADAT12_TO_IMIXR    23
#define FR_SAFFIREPRO_CMD_ID_ADAT13_TO_IMIXL    24
#define FR_SAFFIREPRO_CMD_ID_ADAT13_TO_IMIXR    25
#define FR_SAFFIREPRO_CMD_ID_ADAT14_TO_IMIXL    26
#define FR_SAFFIREPRO_CMD_ID_ADAT14_TO_IMIXR    27
#define FR_SAFFIREPRO_CMD_ID_ADAT15_TO_IMIXL    28
#define FR_SAFFIREPRO_CMD_ID_ADAT15_TO_IMIXR    29
#define FR_SAFFIREPRO_CMD_ID_ADAT16_TO_IMIXL    30
#define FR_SAFFIREPRO_CMD_ID_ADAT16_TO_IMIXR    31
#define FR_SAFFIREPRO_CMD_ID_ADAT17_TO_IMIXL    32
#define FR_SAFFIREPRO_CMD_ID_ADAT17_TO_IMIXR    33
#define FR_SAFFIREPRO_CMD_ID_ADAT18_TO_IMIXL    34
#define FR_SAFFIREPRO_CMD_ID_ADAT18_TO_IMIXR    35

#define FR_SAFFIREPRO_CMD_ID_ADAT21_TO_IMIXL    36
#define FR_SAFFIREPRO_CMD_ID_ADAT21_TO_IMIXR    37
#define FR_SAFFIREPRO_CMD_ID_ADAT22_TO_IMIXL    38
#define FR_SAFFIREPRO_CMD_ID_ADAT22_TO_IMIXR    39
#define FR_SAFFIREPRO_CMD_ID_ADAT23_TO_IMIXL    40
#define FR_SAFFIREPRO_CMD_ID_ADAT23_TO_IMIXR    41
#define FR_SAFFIREPRO_CMD_ID_ADAT24_TO_IMIXL    42
#define FR_SAFFIREPRO_CMD_ID_ADAT24_TO_IMIXR    43
#define FR_SAFFIREPRO_CMD_ID_ADAT25_TO_IMIXL    44
#define FR_SAFFIREPRO_CMD_ID_ADAT25_TO_IMIXR    45
#define FR_SAFFIREPRO_CMD_ID_ADAT26_TO_IMIXL    46
#define FR_SAFFIREPRO_CMD_ID_ADAT26_TO_IMIXR    47
#define FR_SAFFIREPRO_CMD_ID_ADAT27_TO_IMIXL    48
#define FR_SAFFIREPRO_CMD_ID_ADAT27_TO_IMIXR    49
#define FR_SAFFIREPRO_CMD_ID_ADAT28_TO_IMIXL    50
#define FR_SAFFIREPRO_CMD_ID_ADAT28_TO_IMIXR    51

#define FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT1    52
#define FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT2    54
#define FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT1   53
#define FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT2   55

#define FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT3    56
#define FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT4    59
#define FR_SAFFIREPRO_CMD_ID_PC3_TO_OUT3    57
#define FR_SAFFIREPRO_CMD_ID_PC4_TO_OUT4    60
#define FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT3   58
#define FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT4   61

#define FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT5    62
#define FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT6    65
#define FR_SAFFIREPRO_CMD_ID_PC5_TO_OUT5    63
#define FR_SAFFIREPRO_CMD_ID_PC6_TO_OUT6    66
#define FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT5   64
#define FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT6   67

#define FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT7    68
#define FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT8    71
#define FR_SAFFIREPRO_CMD_ID_PC7_TO_OUT7    69
#define FR_SAFFIREPRO_CMD_ID_PC8_TO_OUT8    72
#define FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT7   70
#define FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT8   73

#define FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT9    74
#define FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT10   77
#define FR_SAFFIREPRO_CMD_ID_PC9_TO_OUT9    75
#define FR_SAFFIREPRO_CMD_ID_PC10_TO_OUT10  78
#define FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT9   76
#define FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT10  79

namespace BeBoB {
namespace Focusrite {

class SaffireProDevice;

class SaffireProMatrixMixer : public Control::MatrixMixer
{
public:
    enum eMatrixMixerType {
        eMMT_InputMix,
        eMMT_OutputMix
    };
    
public:
    SaffireProMatrixMixer(SaffireProDevice& parent, enum eMatrixMixerType type);
    SaffireProMatrixMixer(SaffireProDevice& parent, enum eMatrixMixerType type, std::string n);
    virtual ~SaffireProMatrixMixer() {};

    virtual void show();

    virtual std::string getRowName( const int );
    virtual std::string getColName( const int );
    virtual int canWrite( const int, const int );
    virtual double setValue( const int, const int, const double );
    virtual double getValue( const int, const int );
    virtual int getRowCount( );
    virtual int getColCount( );

private:
    struct sSignalInfo {
        std::string name;
        std::string label;
        std::string description;
    };
    struct sCellInfo {
        int row;
        int col;
        // indicates whether a cell can be valid, this
        // doesn't mean that it is writable. Just that it can be.
        bool valid;
        // the address to use when manipulating this cell
        int address;
    };
    
    void init(enum eMatrixMixerType type);
    void addSignalInfo(std::vector<struct sSignalInfo> &target,
                       std::string name, std::string label, std::string descr);
    void setCellInfo(int row, int col, int addr, bool valid);

    std::vector<struct sSignalInfo> m_RowInfo;
    std::vector<struct sSignalInfo> m_ColInfo;
    std::vector< std::vector<struct sCellInfo> > m_CellInfo;
    
    SaffireProDevice&       m_Parent;
    enum eMatrixMixerType   m_type;
};


class SaffireProDevice : public FocusriteDevice
{
public:
    SaffireProDevice( Ieee1394Service& ieee1394Service,
              std::auto_ptr<ConfigRom>( configRom ));
    virtual ~SaffireProDevice();

    virtual void showDevice();
    virtual void setVerboseLevel(int l);

    virtual bool setSamplingFrequency( int );
    virtual int getSamplingFrequency( );

    virtual bool buildMixer();
    virtual bool destroyMixer();

private:
    virtual bool setSamplingFrequencyDo( int );
    virtual int getSamplingFrequencyMirror( );

    bool isStreaming();

    Control::Container *m_MixerContainer;
};

} // namespace Focusrite
} // namespace BeBoB

#endif
