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

#include "libcontrol/MatrixMixer.h"

#include <vector>
#include <string>

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

private:
    virtual bool setSamplingFrequencyDo( int );
    virtual int getSamplingFrequencyMirror( );

    BinaryControl * m_Phantom1;
    BinaryControl * m_Phantom2;
    
    BinaryControl * m_Insert1;
    BinaryControl * m_Insert2;
    BinaryControl * m_AC3pass;
    BinaryControl * m_MidiTru;
    
    VolumeControl * m_Output12[4];
    VolumeControl * m_Output34[6];
    VolumeControl * m_Output56[6];
    VolumeControl * m_Output78[6];
    VolumeControl * m_Output910[6];
    
    SaffireProMatrixMixer * m_InputMixer;
    SaffireProMatrixMixer * m_OutputMixer;
};

} // namespace Focusrite
} // namespace BeBoB

#endif
