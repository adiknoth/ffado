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
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for channel fader 0x%04x to %d\n", m_register, v);

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
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for channel fader 0x%04x\n", m_register);

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
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for channel pan 0x%04x to %d\n", m_register, v);

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
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for channel pan 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return ((val >> 8) & 0xff) - 0x40;
}

ChannelMute::ChannelMute(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

ChannelMute::ChannelMute(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
ChannelMute::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for channel mute 0x%04x to %d\n", m_register, v);

    // Mute status is bit 16
    val = (v==0)?0:0x00010000;
    // Bit 24 indicates that mute is being set
    val |= 0x01000000;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
ChannelMute::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for channel mute 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return (val & 0x00010000) != 0;
}

ChannelSolo::ChannelSolo(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

ChannelSolo::ChannelSolo(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
ChannelSolo::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for channel solo 0x%04x to %d\n", m_register, v);

    // Solo status is bit 17
    val = (v==0)?0:0x00020000;
    // Bit 25 indicates that solo is being set
    val |= 0x02000000;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
ChannelSolo::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for channel solo 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return (val & 0x00020000) != 0;
}

MixFader::MixFader(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

MixFader::MixFader(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
MixFader::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for mix fader 0x%04x to %d\n", m_register, v);

    val = v<0?0:v;
    if (val > 0x80)
      val = 0x80;
    // Bit 24 indicates that the mix fader is being set
    val |= 0x01000000;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
MixFader::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for mix fader 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return val & 0xff;
}

MixMute::MixMute(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

MixMute::MixMute(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
MixMute::setValue(int v)
{
    unsigned int val, dest;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for mix mute 0x%04x to %d\n", m_register, v);

    // Need to read current destination so we can preserve that when setting
    // mute status (mute and destination are always set together).
    dest = m_parent.ReadRegister(m_register) & 0x00000f00;
    // Mute status is bit 12
    val = (v==0)?0:0x00001000;
    // Bit 25 indicates that mute and destination are being set.  Also
    // preserve the current destination.
    val |= 0x02000000 | dest;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
MixMute::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for mix mute 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return (val & 0x00001000) != 0;
}

MixDest::MixDest(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

MixDest::MixDest(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
MixDest::setValue(int v)
{
    unsigned int val, mute;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for mix destination 0x%04x to %d\n", m_register, v);

    // Need to get current mute status so we can preserve it
    mute = m_parent.ReadRegister(m_register) & 0x00001000;
    val = v;
    /* Currently destination values between 0 and 0x0b are accepted. 
     * Ultimately this will be device (and device configuration) dependent.
     */
    if (val<0 || val>0x0b)
      val = 0;
    /* Destination is given by bits 11-8.  Add in the current mute status so
     * it can be preserved (it's set concurrently with the destination).
     */
    val = (val << 8) | mute;
    // Bit 25 indicates that mute and destination are being set
    val |= 0x02000000;
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
MixDest::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for mix destination 0x%04x\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return (val >> 8) & 0x0f;
}

}
