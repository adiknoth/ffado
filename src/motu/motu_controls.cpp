/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2008 by Jonathan Woithe
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

// This also includes motu_controls.h
#include "motu_avdevice.h"

namespace Motu {

MotuDiscreteCtrl::MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg)
: Control::Discrete()
, m_parent(parent)
, m_register(dev_reg)
{
}

MotuDiscreteCtrl::MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: Control::Discrete()
, m_parent(parent)
, m_register(dev_reg)
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

ChannelFader::ChannelFader(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

ChannelFader::ChannelFader(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
ChannelFader::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id 0x%04x to %d\n", m_register, v);

    val = v<0?0:v;
    if (val > 0x80)
      val = 0x80;
    // Bit 30 indicates that the channel fader is being set
    val |= 0x40000000;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
ChannelFader::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for id 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return val & 0xff;
}

ChannelPan::ChannelPan(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

ChannelPan::ChannelPan(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
ChannelPan::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for id 0x%04x to %d\n", m_register, v);

    val = ((v<-64?-64:v)+64) & 0xff;
    if (val > 0x80)
      val = 0x80;
    // Bit 31 indicates that pan is being set
    val = (val << 8) | 0x80000000;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
ChannelPan::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for id 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return ((val >> 8) & 0xff) - 0x40;
}

}
