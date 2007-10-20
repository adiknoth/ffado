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

#ifndef BEBOB_FOCUSRITE_GENERIC_DEVICE_H
#define BEBOB_FOCUSRITE_GENERIC_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "bebob/bebob_avdevice.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

namespace BeBoB {
namespace Focusrite {

class FocusriteDevice;

class BinaryControl
    : public Control::Discrete
{
public:
    BinaryControl(FocusriteDevice& parent, int id, int bit);
    BinaryControl(FocusriteDevice& parent, int id, int bit,
                  std::string name, std::string label, std::string descr);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    FocusriteDevice&        m_Parent;
    unsigned int            m_cmd_id;
    unsigned int            m_cmd_bit;
};

class VolumeControl
    : public Control::Discrete
{
public:
    VolumeControl(FocusriteDevice& parent, int id);
    VolumeControl(FocusriteDevice& parent, int id,
                  std::string name, std::string label, std::string descr);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    FocusriteDevice&        m_Parent;
    unsigned int            m_cmd_id;
};

class VolumeControlLowRes
    : public Control::Discrete
{
public:
    VolumeControlLowRes(FocusriteDevice& parent, int id, int shift);
    VolumeControlLowRes(FocusriteDevice& parent, int id, int shift,
                  std::string name, std::string label, std::string descr);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    FocusriteDevice&        m_Parent;
    unsigned int            m_cmd_id;
    unsigned int            m_bit_shift;
};

class FocusriteMatrixMixer : public Control::MatrixMixer
{
public:
    FocusriteMatrixMixer(FocusriteDevice& parent);
    FocusriteMatrixMixer(FocusriteDevice& parent, std::string n);
    virtual ~FocusriteMatrixMixer() {};

    virtual void show();

    virtual std::string getRowName( const int );
    virtual std::string getColName( const int );
    virtual int canWrite( const int, const int );
    virtual double setValue( const int, const int, const double );
    virtual double getValue( const int, const int );
    virtual int getRowCount( );
    virtual int getColCount( );

protected:
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
    
    virtual void init() = 0;
    virtual void addSignalInfo(std::vector<struct sSignalInfo> &target,
                       std::string name, std::string label, std::string descr);
    virtual void setCellInfo(int row, int col, int addr, bool valid);

    std::vector<struct sSignalInfo> m_RowInfo;
    std::vector<struct sSignalInfo> m_ColInfo;
    std::vector< std::vector<struct sCellInfo> > m_CellInfo;
    
    FocusriteDevice&        m_Parent;
};

class FocusriteDevice : public BeBoB::AvDevice {
public:
    FocusriteDevice( Ieee1394Service& ieee1394Service,
              std::auto_ptr<ConfigRom>( configRom ));
    virtual ~FocusriteDevice() {};

    virtual void showDevice();
    virtual void setVerboseLevel(int l);

public:
    bool setSpecificValue(uint32_t id, uint32_t v);
    bool getSpecificValue(uint32_t id, uint32_t *v);

protected:
    int convertDefToSr( uint32_t def );
    uint32_t convertSrToDef( int sr );
};

} // namespace Focusrite
} // namespace BeBoB

#endif
