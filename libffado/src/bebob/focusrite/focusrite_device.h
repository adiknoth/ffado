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

class PhantomPowerControl
    : public Control::Discrete
{
public:
    PhantomPowerControl(SaffireProDevice& parent, int channel);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    SaffireProDevice&       m_Parent;
    unsigned int            m_channel;
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

private:
    PhantomPowerControl * m_Phantom1;
    PhantomPowerControl * m_Phantom2;
};

} // namespace Focusrite
} // namespace BeBoB

#endif
