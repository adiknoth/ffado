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
: Control::Discrete(&parent)
, m_parent(parent)
, m_register(dev_reg)
{
}

MotuDiscreteCtrl::MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_parent(parent)
, m_register(dev_reg)
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

MotuBinarySwitch::MotuBinarySwitch(MotuDevice &parent, unsigned int dev_reg, 
  unsigned int val_mask, unsigned int setenable_mask)
: MotuDiscreteCtrl(parent, dev_reg)
{
    m_value_mask = val_mask;
    /* If no "write enable" is implemented for a given switch it's safe to 
     * pass zero in to setenable_mask.
     */
    m_setenable_mask = setenable_mask;
}

MotuBinarySwitch::MotuBinarySwitch(MotuDevice &parent, unsigned int dev_reg,
    unsigned int val_mask, unsigned int setenable_mask,
    std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
    m_value_mask = val_mask;
    /* If no "write enable" is implemented for a given switch it's safe to 
     * pass zero in to setenable_mask.
     */
    m_setenable_mask = setenable_mask;
}
             
bool
MotuBinarySwitch::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for switch %s (0x%04x) to %d\n", 
      getName().c_str(), m_register, v);

    // Set the value
    if (m_setenable_mask) {
      val = (v==0)?0:m_value_mask;
      // Set the "write enable" bit for the value being set
      val |= m_setenable_mask;
    } else {
      // It would be good to utilise the cached value from the receive
      // processor (if running) later on.  For now we'll just fetch the
      // current register value directly when needed.
      val = m_parent.ReadRegister(m_register);
      if (v==0)
        val &= ~m_value_mask;
      else
        val |= m_value_mask;
    }
    m_parent.WriteRegister(m_register, val);

    return true;
}

int
MotuBinarySwitch::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for switch %s (0x%04x)\n", 
      getName().c_str(), m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return (val & m_value_mask) != 0;
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

PhonesSrc::PhonesSrc(MotuDevice &parent)
: MotuDiscreteCtrl(parent, 0)
{
}

PhonesSrc::PhonesSrc(MotuDevice &parent,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, 0, name, label, descr)
{
}
             
bool
PhonesSrc::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for phones destination to %d\n", v);

    /* Currently destination values between 0 and 0x0b are accepted. 
     * Ultimately this will be device (and device configuration) dependent.
     */
    val = v;
    if (val<0 || val>0x0b)
      val = 0;
    // Destination is given by bits 3-0.
    // Bit 24 indicates that the phones source is being set.
    val |= 0x01000000;
    m_parent.WriteRegister(MOTU_REG_ROUTE_PORT_CONF, val);

    return true;
}

int
PhonesSrc::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for phones destination\n");

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(MOTU_REG_ROUTE_PORT_CONF);
    return val & 0x0f;
}

OpticalMode::OpticalMode(MotuDevice &parent, unsigned int dev_reg)
: MotuDiscreteCtrl(parent, dev_reg)
{
}

OpticalMode::OpticalMode(MotuDevice &parent, unsigned int dev_reg,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, dev_reg, name, label, descr)
{
}
             
bool
OpticalMode::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for optical mode %d to %d\n", m_register, v);

    // Need to get current optical modes so we can preserve the one we're
    // not setting.  Input mode is in bits 9-8, output is in bits 11-10.
    val = m_parent.ReadRegister(MOTU_REG_ROUTE_PORT_CONF) & 0x00000f00;

    // Set mode as requested.  An invalid setting is effectively ignored.
    if (v>=0 && v<=3) {
      if (m_register == MOTU_DIR_IN) {
        val = (val & ~0x0300) | ((v & 0x03) << 8);
      } else {
        val = (val & ~0x0c00) | ((v & 0x03) << 10);
      }
    }
    // Bit 25 indicates that optical modes are being set
    val |= 0x02000000;
    m_parent.WriteRegister(MOTU_REG_ROUTE_PORT_CONF, val);

    return true;
}

int
OpticalMode::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for optical mode %d\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(MOTU_REG_ROUTE_PORT_CONF);
    if (m_register == MOTU_DIR_IN)
      val = (val >> 8) & 0x03;
    else
      val = (val >> 10) & 0x03;
    return val;
}

InputGainPad::InputGainPad(MotuDevice &parent, unsigned int channel, unsigned int mode)
: MotuDiscreteCtrl(parent, channel)
{
    m_mode = mode;
    validate();
}

InputGainPad::InputGainPad(MotuDevice &parent, unsigned int channel, unsigned int mode,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, channel, name, label, descr)
{
    m_mode = mode;
    validate();
}

void InputGainPad::validate(void) {
    if (m_register > MOTU_CTRL_TRIMGAINPAD_MAX_CHANNEL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Invalid channel %d: max supported is %d, assuming 0\n", 
            m_register, MOTU_CTRL_TRIMGAINPAD_MAX_CHANNEL);
        m_register = 0;
    }
    if (m_mode!=MOTU_CTRL_MODE_PAD && m_mode!=MOTU_CTRL_MODE_TRIMGAIN) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Invalid mode %d, assuming %d\n", m_mode, MOTU_CTRL_MODE_PAD);
        m_mode = MOTU_CTRL_MODE_PAD;
    }
}

unsigned int InputGainPad::dev_register(void) {
    /* Work out the device register to use for the associated channel */
    if (m_register>=0 && m_register<=3) {
      return MOTU_REG_INPUT_GAIN_PAD_0;      
    } else {
      debugOutput(DEBUG_LEVEL_VERBOSE, "unsupported channel %d\n", m_register);
    }
    return 0;
}
             
bool
InputGainPad::setValue(int v)
{
    unsigned int val;
    unsigned int reg, reg_shift;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for mode %d input pad/trim %d to %d\n", m_mode, m_register, v);

    reg = dev_register();
    if (reg == 0)
        return false;
    reg_shift = (m_register & 0x03) * 8;

    // Need to get current gain trim / pad value so we can preserve one
    // while setting the other.  The pad status is in bit 6 of the channel's
    // respective byte with the trim in bits 0-5.  Bit 7 is the write enable
    // bit for the channel.
    val = m_parent.ReadRegister(reg) & (0xff << reg_shift);

    switch (m_mode) {
        case MOTU_CTRL_MODE_PAD:
            // Set pad bit (bit 6 of relevant channel's byte)
            if (v == 0) {
                val &= ~(0x40 << reg_shift);
            } else {
                val |= (0x40 << reg_shift);
            }
            break;
      case MOTU_CTRL_MODE_TRIMGAIN:
            // Set the gain trim (bits 0-5 of the channel's byte).  Maximum
            // gain is 53 dB.
            if (v > 0x35)
                v = 0x35;
            val = (val & ~(0x3f << reg_shift)) | (v << reg_shift);
            break;
      default:
        debugOutput(DEBUG_LEVEL_VERBOSE, "unsupported mode %d\n", m_mode);
        return false;
    }

    // Set the channel's write enable bit
    val |= (0x80 << reg_shift);

    m_parent.WriteRegister(reg, val);

    return true;
}

int
InputGainPad::getValue()
{
    unsigned int val;
    unsigned int reg, reg_shift;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for mode %d input pad/trim %d\n", m_mode, m_register);

    reg = dev_register();
    if (reg == 0)
        return false;
    reg_shift = (m_register & 0x03) * 8;

    // The pad status is in bit 6 of the channel's respective byte with the
    // trim in bits 0-5.  Bit 7 is the write enable bit for the channel.
    val = m_parent.ReadRegister(reg);

    switch (m_mode) {
        case MOTU_CTRL_MODE_PAD:
            val = ((val >> reg_shift) & 0x40) != 0;
            break;
      case MOTU_CTRL_MODE_TRIMGAIN:
            val = ((val >> reg_shift) & 0x3f);
            break;
      default:
        debugOutput(DEBUG_LEVEL_VERBOSE, "unsupported mode %d\n", m_mode);
        return 0;
    }

    return val;
}

InfoElement::InfoElement(MotuDevice &parent, unsigned infotype)
: MotuDiscreteCtrl(parent, infotype)
{
}

InfoElement::InfoElement(MotuDevice &parent, unsigned infotype,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, infotype, name, label, descr)
{
}
             
bool
InfoElement::setValue(int v)
{
    /* This is a read-only field, so any call to setValue() is technically
     * an error.
     */
    debugOutput(DEBUG_LEVEL_VERBOSE, "InfoElement (%d) is read-only\n", m_register);
    return false;
}

int
InfoElement::getValue()
{
    unsigned int val;
    signed int res = 0;

    switch (m_register) {
        case MOTU_INFO_IS_STREAMING:
            val = m_parent.ReadRegister(MOTU_REG_ISOCTRL);
            /* Streaming is active if either bit 22 (Motu->PC streaming
             * enable) or bit 30 (PC->Motu streaming enable) is set.
             */
            res = (val & 0x40400000) != 0;
            debugOutput(DEBUG_LEVEL_VERBOSE, "IsStreaming: %d (reg=%08x)\n", res, val);
            break;
        case MOTU_INFO_SAMPLE_RATE:
            res = m_parent.getSamplingFrequency();
            debugOutput(DEBUG_LEVEL_VERBOSE, "SampleRate: %d\n", res);
            break;
        case MOTU_INFO_HAS_MIC_INPUTS:
            /* Only the 828Mk2 has separate mic inputs.  In time this may be
             * deduced by walking the port info array within the parent.
             */
            res = m_parent.m_motu_model == MOTU_MODEL_828mkII ? 1:0;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Has mic inputs: %d\n", res);
            break;
        case MOTU_INFO_HAS_AESEBU_INPUTS:
            /* AES/EBU inputs are currently present on the Traveler and
             * 896HD.  In time this may be deduced by walking the port info
             * array within the parent.
             */
            val = m_parent.m_motu_model;
            res = (val==MOTU_MODEL_TRAVELER || val==MOTU_MODEL_896HD);
            debugOutput(DEBUG_LEVEL_VERBOSE, "HasAESEBUInputs: %d\n", res);
            break;
        case MOTU_INFO_HAS_SPDIF_INPUTS:
            /* SPDIF inputs are present on all supported models except the
             * 896HD and the 8pre.  In time this may be deduced by walking
             * the port info array within the parent.
             */
            val = m_parent.m_motu_model;
            res = (val!=MOTU_MODEL_8PRE && val!=MOTU_MODEL_896HD);
            debugOutput(DEBUG_LEVEL_VERBOSE, "HasSPDIFInputs: %d\n", res);
            break;
        case MOTU_INFO_HAS_OPTICAL_SPDIF:
            /* THe 896HD doesn't have optical SPDIF capability */
            val = m_parent.m_motu_model;
            res = (val != MOTU_MODEL_896HD);
            debugOutput(DEBUG_LEVEL_VERBOSE, "HasOpticalSPDIF: %d\n", res);
            break;
    }
    return res;
}

}
