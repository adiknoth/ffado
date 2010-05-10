/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2008-2009 by Jonathan Woithe
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

    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }

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

    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return 0;
    }

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

    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }

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

    // Silently swallow attempts to read non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return 0;
    }

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

    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }

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

    // Silently swallow attempts to read non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return 0;
    }

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(m_register);
    return ((val >> 8) & 0xff) - 0x40;
}


MotuMatrixMixer::MotuMatrixMixer(MotuDevice &parent)
: Control::MatrixMixer(&parent, "MatrixMixer")
, m_parent(parent)
{
}

MotuMatrixMixer::MotuMatrixMixer(MotuDevice &parent, std::string name)
: Control::MatrixMixer(&parent, name)
, m_parent(parent)
{
}

void MotuMatrixMixer::addRowInfo(std::string name, unsigned int flags, 
  unsigned int address)
{
    struct sSignalInfo s;
    s.name = name;
    s.flags = flags;
    s.address = address;
    m_RowInfo.push_back(s);
}

void MotuMatrixMixer::addColInfo(std::string name, unsigned int flags, 
  unsigned int address)
{
    struct sSignalInfo s;
    s.name = name;
    s.flags = flags;
    s.address = address;
    m_ColInfo.push_back(s);
}

uint32_t MotuMatrixMixer::getCellRegister(const unsigned int row, const unsigned int col)
{
    if (m_RowInfo.at(row).address==MOTU_CTRL_NONE ||
        m_ColInfo.at(col).address==MOTU_CTRL_NONE)
        return MOTU_CTRL_NONE;
    return m_RowInfo.at(row).address + m_ColInfo.at(col).address;
}

void MotuMatrixMixer::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "MOTU matrix mixer\n");
}

std::string MotuMatrixMixer::getRowName(const int row)
{
    return m_RowInfo.at(row).name;
}

std::string MotuMatrixMixer::getColName(const int col)
{
    return m_ColInfo.at(col).name;
}

int MotuMatrixMixer::getRowCount()
{
    return m_RowInfo.size();
}

int MotuMatrixMixer::getColCount()
{
    return m_ColInfo.size();
}

ChannelFaderMatrixMixer::ChannelFaderMatrixMixer(MotuDevice &parent)
: MotuMatrixMixer(parent, "ChannelFaderMatrixMixer")
{
}

ChannelFaderMatrixMixer::ChannelFaderMatrixMixer(MotuDevice &parent, std::string name)
: MotuMatrixMixer(parent, name)
{
}

double ChannelFaderMatrixMixer::setValue(const int row, const int col, const double val)
{
    uint32_t v, reg;
    v = val<0?0:(uint32_t)val;
    if (v > 0x80)
      v = 0x80;
    debugOutput(DEBUG_LEVEL_VERBOSE, "ChannelFader setValue for row %d col %d to %lf (%d)\n",
      row, col, val, v);
    reg = getCellRegister(row,col);

    // Silently swallow attempts to set non-existent controls for now
    if (reg == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ignoring control marked as non-existent\n");
        return true;
    }
    // Bit 30 indicates that the channel fader is being set
    v |= 0x40000000;
    m_parent.WriteRegister(reg, v);

    return true;
}

double ChannelFaderMatrixMixer::getValue(const int row, const int col)
{
    uint32_t val, reg;
    reg = getCellRegister(row,col);

    // Silently swallow attempts to read non-existent controls for now
    if (reg == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ignoring control marked as non-existent\n");
        return 0;
    }
    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(reg) & 0xff;

    debugOutput(DEBUG_LEVEL_VERBOSE, "ChannelFader getValue for row %d col %d = %u\n",
      row, col, val);
    return val;
}

ChannelPanMatrixMixer::ChannelPanMatrixMixer(MotuDevice &parent)
: MotuMatrixMixer(parent, "ChannelPanMatrixMixer")
{
}

ChannelPanMatrixMixer::ChannelPanMatrixMixer(MotuDevice &parent, std::string name)
: MotuMatrixMixer(parent, name)
{
}

double ChannelPanMatrixMixer::setValue(const int row, const int col, const double val)
{
    uint32_t v, reg;
    v = ((val<-64?-64:(int32_t)val)+64) & 0xff;
    if (v > 0x80)
      v = 0x80;

    debugOutput(DEBUG_LEVEL_VERBOSE, "ChannelPan setValue for row %d col %d to %lf (%d)\n",
      row, col, val, v);
    reg = getCellRegister(row,col);

    // Silently swallow attempts to set non-existent controls for now
    if (reg == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ignoring control marked as non-existent\n");
        return true;
    }

    // Bit 31 indicates that pan is being set
    v = (v << 8) | 0x80000000;
    m_parent.WriteRegister(reg, v);

    return true;
}

double ChannelPanMatrixMixer::getValue(const int row, const int col)
{
    int32_t val;
    uint32_t reg;
    reg = getCellRegister(row,col);

    // Silently swallow attempts to read non-existent controls for now
    if (reg == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ignoring control marked as non-existent\n");
        return 0;
    }

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(reg);
    val = ((val >> 8) & 0xff) - 0x40;

    debugOutput(DEBUG_LEVEL_VERBOSE, "ChannelPan getValue for row %d col %d = %u\n",
      row, col, val);
    return val;
}

ChannelBinSwMatrixMixer::ChannelBinSwMatrixMixer(MotuDevice &parent)
: MotuMatrixMixer(parent, "ChannelPanMatrixMixer")
, m_value_mask(0)
, m_setenable_mask(0)
{
}

/* If no "write enable" is implemented for a given switch it's safe to 
 * pass zero in to setenable_mask.
 */
ChannelBinSwMatrixMixer::ChannelBinSwMatrixMixer(MotuDevice &parent, std::string name,
  unsigned int val_mask, unsigned int setenable_mask)
: MotuMatrixMixer(parent, name)
, m_value_mask(val_mask)
, m_setenable_mask(setenable_mask)
{
}

double ChannelBinSwMatrixMixer::setValue(const int row, const int col, const double val)
{
    uint32_t v, reg;

    debugOutput(DEBUG_LEVEL_VERBOSE, "BinSw setValue for row %d col %d to %lf (%d)\n",
      row, col, val, val==0?0:1);
    reg = getCellRegister(row,col);

    // Silently swallow attempts to set non-existent controls for now
    if (reg == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ignoring control marked as non-existent\n");
        return true;
    }

    // Set the value
    if (m_setenable_mask) {
      v = (val==0)?0:m_value_mask;
      // Set the "write enable" bit for the value being set
      v |= m_setenable_mask;
    } else {
      // It would be good to utilise the cached value from the receive
      // processor (if running) later on.  For now we'll just fetch the
      // current register value directly when needed.
      v = m_parent.ReadRegister(reg);
      if (v==0)
        v &= ~m_value_mask;
      else
        v |= m_value_mask;
    }
    m_parent.WriteRegister(reg, v);

    return true;
}

double ChannelBinSwMatrixMixer::getValue(const int row, const int col)
{
    uint32_t val, reg;
    reg = getCellRegister(row,col);

    // Silently swallow attempts to read non-existent controls for now
    if (reg == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ignoring control marked as non-existent\n");
        return 0;
    }

    // FIXME: we could just read the appropriate mixer status field from the 
    // receive stream processor once we work out an efficient way to do this.
    val = m_parent.ReadRegister(reg);
    val = (val & m_value_mask) != 0;

    debugOutput(DEBUG_LEVEL_VERBOSE, "BinSw getValue for row %d col %d = %u\n",
      row, col, val);
    return val;
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

    // Silently swallow attempts to set non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }
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

    // Silently swallow attempts to read non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return 0;
    }

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

    // Silently swallow attempts to set non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }

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

    // Silently swallow attempts to read non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return 0;
    }

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

    // Silently swallow attempts to set non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }
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

    // Silently swallow attempts to read non-existent controls for now
    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }
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
    unsigned int val, dir;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for optical mode %d to %d\n", m_register, v);

    /* Assume v is 0 for "off", 1 for "ADAT" and 2 for "Toslink" */
    switch (v) {
        case 0: val = MOTU_OPTICAL_MODE_OFF; break;
        case 1: val = MOTU_OPTICAL_MODE_ADAT; break;
        case 2: val = MOTU_OPTICAL_MODE_TOSLINK; break;
        default: return true;
    }
    dir = (m_register==MOTU_CTRL_DIR_IN)?MOTU_DIR_IN:MOTU_DIR_OUT;
    m_parent.setOpticalMode(0, dir, val);
    return true;
}

int
OpticalMode::getValue()
{
    unsigned int dir;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for optical mode %d\n", m_register);

    // FIXME: we could just read the appropriate mixer status field from the
    // receive stream processor once we work out an efficient way to do this.
    dir = (m_register==MOTU_CTRL_DIR_IN)?MOTU_DIR_IN:MOTU_DIR_OUT;
    switch (m_parent.getOpticalMode(0, dir)) {
        case MOTU_OPTICAL_MODE_OFF: return 0;
        case MOTU_OPTICAL_MODE_ADAT: return 1;
        case MOTU_OPTICAL_MODE_TOSLINK: return 2;
        default: return 0;
    }
    return 0;
}

InputGainPadInv::InputGainPadInv(MotuDevice &parent, unsigned int channel, unsigned int mode)
: MotuDiscreteCtrl(parent, channel)
{
    m_mode = mode;
    validate();
}

InputGainPadInv::InputGainPadInv(MotuDevice &parent, unsigned int channel, unsigned int mode,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, channel, name, label, descr)
{
    m_mode = mode;
    validate();
}

void InputGainPadInv::validate(void) {
    if ((m_mode==MOTU_CTRL_MODE_PAD || m_mode==MOTU_CTRL_MODE_TRIMGAIN) &&
        m_register>MOTU_CTRL_TRIMGAINPAD_MAX_CHANNEL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Invalid channel %d: max supported is %d, assuming 0\n", 
            m_register, MOTU_CTRL_TRIMGAINPAD_MAX_CHANNEL);
        m_register = 0;
    }
    if ((m_mode==MOTU_CTRL_MODE_UL_GAIN || m_mode==MOTU_CTRL_MODE_PHASE_INV) &&
        m_register>MOTU_CTRL_GAINPHASEINV_MAX_CHANNEL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Invalid ultralite channel %d: max supported is %d, assuming 0\n", 
            m_register, MOTU_CTRL_GAINPHASEINV_MAX_CHANNEL);
        m_register = 0;
    }
    if (m_mode!=MOTU_CTRL_MODE_PAD && m_mode!=MOTU_CTRL_MODE_TRIMGAIN &&
        m_mode!=MOTU_CTRL_MODE_UL_GAIN && m_mode!=MOTU_CTRL_MODE_PHASE_INV) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Invalid mode %d, assuming %d\n", m_mode, MOTU_CTRL_MODE_PAD);
        m_mode = MOTU_CTRL_MODE_PAD;
    }
}

unsigned int InputGainPadInv::dev_register(void) {
    /* Work out the device register to use for the associated channel */
    /* Registers for gain/phase inversion controls on the Ultralite differ from those
     * of other devices.
     */
    if (m_mode==MOTU_CTRL_MODE_PAD || m_mode==MOTU_CTRL_MODE_TRIMGAIN) {
       if (m_register>=0 && m_register<=3) {
          return MOTU_REG_INPUT_GAIN_PAD_0;      
       } else {
          debugOutput(DEBUG_LEVEL_VERBOSE, "unsupported channel %d\n", m_register);
       }
    } else {
       if (m_register>=0 && m_register<=3)
          return MOTU_REG_INPUT_GAIN_PHINV0;
       else if (m_register>=4 && m_register<=7)
          return MOTU_REG_INPUT_GAIN_PHINV1;
       else if (m_register>=8 && m_register<=11)
          return MOTU_REG_INPUT_GAIN_PHINV2;
       else {
          debugOutput(DEBUG_LEVEL_VERBOSE, "unsupported ultralite channel %d\n", m_register);
       }
    }
    return 0;
}
             
bool
InputGainPadInv::setValue(int v)
{
    unsigned int val;
    unsigned int reg, reg_shift;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for mode %d input pad/trim %d to %d\n", m_mode, m_register, v);

    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return true;
    }

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
        case MOTU_CTRL_MODE_PHASE_INV:
            // Set pad/phase inversion bit (bit 6 of relevant channel's byte)
            if (v == 0) {
                val &= ~(0x40 << reg_shift);
            } else {
                val |= (0x40 << reg_shift);
            }
            break;
      case MOTU_CTRL_MODE_TRIMGAIN:
      case MOTU_CTRL_MODE_UL_GAIN:
            // Set the gain trim (bits 0-5 of the channel's byte).  Maximum
            // gain is 53 dB for trimgain on non-ultralite devices.  For
            // ultralites, mic inputs max out at 0x18, line inputs at 0x12
            // and spdif inputs at 0x0c.  We just clip at 0x18 for now.
            if (m_mode==MOTU_CTRL_MODE_TRIMGAIN) {
               if (v > 0x35)
                  v = 0x35;
            } else {
               if (v > 0x18)
                  v = 0x18;
            }
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
InputGainPadInv::getValue()
{
    unsigned int val;
    unsigned int reg, reg_shift;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for mode %d input pad/trim %d\n", m_mode, m_register);

    if (m_register == MOTU_CTRL_NONE) {
        debugOutput(DEBUG_LEVEL_WARNING, "use of MOTU_CTRL_NONE in non-matrix control\n");
        return 0;
    }

    reg = dev_register();
    if (reg == 0)
        return false;
    reg_shift = (m_register & 0x03) * 8;

    // The pad/phase inversion status is in bit 6 of the channel's
    // respective byte with the trim in bits 0-5.  Bit 7 is the write enable
    // bit for the channel.
    val = m_parent.ReadRegister(reg);

    switch (m_mode) {
       case MOTU_CTRL_MODE_PAD:
       case MOTU_CTRL_MODE_PHASE_INV:
          val = ((val >> reg_shift) & 0x40) != 0;
          break;
       case MOTU_CTRL_MODE_TRIMGAIN:
       case MOTU_CTRL_MODE_UL_GAIN:
          val = ((val >> reg_shift) & 0x3f);
          break;
       default:
          debugOutput(DEBUG_LEVEL_VERBOSE, "unsupported mode %d\n", m_mode);
          return 0;
    }

    return val;
}

MeterControl::MeterControl(MotuDevice &parent, unsigned int ctrl_mask, unsigned int ctrl_shift)
: MotuDiscreteCtrl(parent, ctrl_mask)
{
    m_shift = ctrl_shift;
    validate();
}

MeterControl::MeterControl(MotuDevice &parent, unsigned int ctrl_mask, unsigned int ctrl_shift,
             std::string name, std::string label, std::string descr)
: MotuDiscreteCtrl(parent, ctrl_mask, name, label, descr)
{
    m_shift = ctrl_shift;
    validate();
}

void MeterControl::validate(void) {
    if ((m_register & (1<< m_shift)) == 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Inconsistent mask/shift: 0x%08x/%d\n", m_register, m_shift);
    }
}

bool
MeterControl::setValue(int v)
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "setValue for meter control 0x%08x/%d: %d\n", 
        m_register, m_shift, v);

    // Need to get current register setting so we can preserve the parts not
    // being controlled by this object.  m_register holds the mask for the 
    // parts we're changing.
    val = m_parent.ReadRegister(MOTU_REG_896HD_METER_CONF) & ~m_register;
    val |= (v << m_shift) & m_register;

    m_parent.WriteRegister(MOTU_REG_896HD_METER_CONF, val);

    // Drivers under other OSes set MOTU_REG_896HD_METER_REG (0x0b1c) to
    // 0x0400 whenever MOTU_REG_896HD_METER_CONF (0x0b24) is changed. 
    // There's no obvious reason why they do this, but since it's no hassle
    // we might as well do the same.
    m_parent.WriteRegister(MOTU_REG_896HD_METER_REG, 0x0400);

    return true;
}

int
MeterControl::getValue()
{
    unsigned int val;
    debugOutput(DEBUG_LEVEL_VERBOSE, "getValue for meter control 0x%08x/%d\n", 
        m_register, m_shift);

    // m_register holds the mask of the part of interest
    val = (m_parent.ReadRegister(MOTU_REG_896HD_METER_CONF) & m_register) >> m_shift;

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
        case MOTU_INFO_MODEL:
            res = m_parent.m_motu_model;
            debugOutput(DEBUG_LEVEL_VERBOSE, "Model: %d\n", res);
            break;
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
    }
    return res;
}

}
