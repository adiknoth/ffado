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

#ifndef BEBOB_FOCUSRITE_DEVICE_H
#define BEBOB_FOCUSRITE_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "bebob/bebob_avdevice.h"

#include "libcontrol/BasicElements.h"

namespace BeBoB {
namespace Focusrite {

class SaffireProDevice;

class BinaryControl
    : public Control::Discrete
{
public:
    BinaryControl(SaffireProDevice& parent, int id);
    BinaryControl(SaffireProDevice& parent, int id,
                  std::string name, std::string label, std::string descr);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    SaffireProDevice&       m_Parent;
    unsigned int            m_cmd_id;
};

class VolumeControl
    : public Control::Discrete
{
public:
    VolumeControl(SaffireProDevice& parent, int id);
    VolumeControl(SaffireProDevice& parent, int id,
                  std::string name, std::string label, std::string descr);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    SaffireProDevice&       m_Parent;
    unsigned int            m_cmd_id;
};


class SaffireProDevice : public BeBoB::AvDevice {
public:
    SaffireProDevice( Ieee1394Service& ieee1394Service,
              std::auto_ptr<ConfigRom>( configRom ));
    virtual ~SaffireProDevice();

    virtual void showDevice();
    virtual void setVerboseLevel(int l);

    virtual bool setSamplingFrequency( int );
    virtual int getSamplingFrequency( );

public:
    bool setSpecificValue(uint32_t id, uint32_t v);
    bool getSpecificValue(uint32_t id, uint32_t *v);

private:
    virtual bool setSamplingFrequencyDo( int );
    virtual int getSamplingFrequencyMirror( );
    int convertDefToSr( uint32_t def );
    uint32_t convertSrToDef( int sr );

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
    
};

} // namespace Focusrite
} // namespace BeBoB

#endif
