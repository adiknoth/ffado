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
#include <math.h>

#include "rme/rme_avdevice.h"
#include "rme/fireface_settings_ctrls.h"

namespace Rme {

RmeSettingsCtrl::RmeSettingsCtrl(Device &parent, unsigned int type, 
    unsigned int info)
: Control::Discrete(&parent)
, m_parent(parent)
, m_type(type)
, m_value(0)
, m_info(info)
{
}

RmeSettingsCtrl::RmeSettingsCtrl(Device &parent, unsigned int type,
    unsigned int info,
    std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_parent(parent)
, m_type(type)
, m_value(0)
, m_info(info)
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
RmeSettingsCtrl::setValue(int v) {

signed int i;
signed int err = 0;

    switch (m_type) {
        case RME_CTRL_NONE:
            debugOutput(DEBUG_LEVEL_ERROR, "control has no type set\n");
            err = 1;
            break;
        case RME_CTRL_PHANTOM_SW:
            // Lowest 16 bits are phantom status bits (max 16 channels). 
            // High 16 bits are "write enable" bits for the corresponding
            // channel represented in the low 16 bits.  This way changes can
            // be made to one channel without needing to first read the
            // existing value.
            //
            // At present there are at most 4 phantom-capable channels.
            // Flag attempts to write to the bits corresponding to channels
            // beyond this.
            if (v & 0xfff00000) {
                debugOutput(DEBUG_LEVEL_WARNING, "Ignored out-of-range phantom set request: mask=0x%04x, value=0x%04x\n",
                  (v >> 16) & 0xfff0, v && 0xfff0);
            }

            for (i=0; i<4; i++) {
                if (v & (0x00010000 << i)) {
                    unsigned int on = (v & (0x00000001 << i)) != 0;
                    err = m_parent.setPhantom(i, on);
                    if (!err) {
                        if (on) {
                            m_value |= (0x01 << i);
                        } else {
                            m_value &= ~(0x01 << i);
                        }
                    }
                }
            }
            break;
        case RME_CTRL_INPUT_LEVEL:
            if (m_parent.setInputLevel(v)) {
                m_value = v;
            }
            break;
        case RME_CTRL_OUTPUT_LEVEL:
            if (m_parent.setOutputLevel(v)) {
                m_value = v;
            }
            break;
        case RME_CTRL_FF400_PAD_SW:
            // Object's "m_info" field is the channel
            if (m_parent.setInputPadOpt(m_info, v)) {
                m_value = (v != 0);
            }
            break;
        case RME_CTRL_FF400_INSTR_SW:
            // Object's "m_info" field is the channel
            if (m_parent.setInputInstrOpt(m_info, v)) {
                m_value = (v != 0);
            }
            break;
        case RME_CTRL_PHONES_LEVEL:
            if (m_parent.setPhonesLevel(v)) {
                m_value = v;
            }
            break;

        // All RME_CTRL_INFO_* controls are read-only.  Warn on attempts to
        // set these.
        case RME_CTRL_INFO_MODEL:
        case RME_CTRL_INFO_TCO_PRESENT:
            debugOutput(DEBUG_LEVEL_ERROR, "Attempt to set readonly info control 0x%08x\n", m_type);
            err = 1;
            break;

        default:
            debugOutput(DEBUG_LEVEL_ERROR, "Unknown control type 0x%08x\n", m_type);
            err = 1;
    }

    return err==0?true:false;
}

int
RmeSettingsCtrl::getValue() {

signed int i;
signed int val = 0;

    switch (m_type) {
        case RME_CTRL_NONE:
            debugOutput(DEBUG_LEVEL_ERROR, "control has no type set\n");
            break;

        case RME_CTRL_PHANTOM_SW:
            for (i=0; i<3; i++)
                val |= (m_parent.getPhantom(i) << i);
            return val;
            break;
        case RME_CTRL_INPUT_LEVEL:
            return m_parent.getInputLevel();
            break;
        case RME_CTRL_OUTPUT_LEVEL:
            return m_parent.getOutputLevel();
            break;
        case RME_CTRL_FF400_PAD_SW:
            return m_parent.getInputPadOpt(m_info);
            break;
        case RME_CTRL_FF400_INSTR_SW:
            return m_parent.getInputInstrOpt(m_info);
            break;
        case RME_CTRL_PHONES_LEVEL:
            return m_parent.getPhonesLevel();
            break;

        case RME_CTRL_INFO_MODEL:
            return m_parent.getRmeModel();
            break;

        case RME_CTRL_INFO_TCO_PRESENT:
            return m_parent.getTcoPresent();

        default:
            debugOutput(DEBUG_LEVEL_ERROR, "Unknown control type 0x%08x\n", m_type);
    }

    return 0;
}


RmeSettingsMatrixCtrl::RmeSettingsMatrixCtrl(Device &parent, unsigned int type)
: Control::MatrixMixer(&parent)
, m_parent(parent)
, m_type(type)
{
}

RmeSettingsMatrixCtrl::RmeSettingsMatrixCtrl(Device &parent, unsigned int type,
    std::string name)
: Control::MatrixMixer(&parent)
, m_parent(parent)
, m_type(type)
{
    setName(name);
}

void RmeSettingsMatrixCtrl::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "RME matrix mixer type %d\n", m_type);
}

std::string RmeSettingsMatrixCtrl::getRowName(const int row)
{
    char buf[64];
    if (m_parent.getRmeModel() == RME_MODEL_FIREFACE400) {
        if (row >= 10)
            snprintf(buf, sizeof(buf), "ADAT out %d", row-9);
        else
        if (row >= 8)
            snprintf(buf, sizeof(buf), "SPDIF out %d", row-7);
        else
        if (row >= 6)
            snprintf(buf, sizeof(buf), "Mon out %d", row+1);
        else
            snprintf(buf, sizeof(buf), "Line out %d", row+1);
    } else {
        snprintf(buf, sizeof(buf), "row %d", row);
    }
    return buf;
}

std::string RmeSettingsMatrixCtrl::getColName(const int col)
{
    char buf[64];
    if (m_type == RME_MATRIXCTRL_PLAYBACK_FADER)
        return "";
    if (m_parent.getRmeModel() == RME_MODEL_FIREFACE400) {
        if (col >= 10)
            snprintf(buf, sizeof(buf), "ADAT in %d", col-9);
        else
        if (col >= 8)
            snprintf(buf, sizeof(buf), "SPDIF in %d", col-7);
        else
        if (col >= 4)
            snprintf(buf, sizeof(buf), "Line in %d", col+1);
        else
        if (col >= 2)
            snprintf(buf, sizeof(buf), "Inst/line %d", col+1);
        else
            snprintf(buf, sizeof(buf), "Mic/line %d", col+1);
    } else {
        snprintf(buf, sizeof(buf), "col %d", col);
    }
    return buf;
}

int RmeSettingsMatrixCtrl::getRowCount() 
{
    switch (m_type) {
        case RME_MATRIXCTRL_GAINS:
            if (m_parent.getRmeModel() == RME_MODEL_FIREFACE400)
                return 1;
            break;
        case RME_MATRIXCTRL_INPUT_FADER:
        case RME_MATRIXCTRL_PLAYBACK_FADER:
            if (m_parent.getRmeModel() == RME_MODEL_FIREFACE400)
                return RME_FF400_MAX_CHANNELS;
            else
                return RME_FF800_MAX_CHANNELS;
            break;
        case RME_MATRIXCTRL_OUTPUT_FADER:
            return 1;
            break;
    }

    return 0;
}

int RmeSettingsMatrixCtrl::getColCount() 
{
    switch (m_type) {
        case RME_MATRIXCTRL_GAINS:
            if (m_parent.getRmeModel() == RME_MODEL_FIREFACE400)
                return 22;
            break;
        case RME_MATRIXCTRL_INPUT_FADER:
        case RME_MATRIXCTRL_PLAYBACK_FADER:
        case RME_MATRIXCTRL_OUTPUT_FADER:
            if (m_parent.getRmeModel() == RME_MODEL_FIREFACE400)
                return RME_FF400_MAX_CHANNELS;
            else
                return RME_FF800_MAX_CHANNELS;
            break;
    }

    return 0;
}

double RmeSettingsMatrixCtrl::setValue(const int row, const int col, const double val) 
{
    signed int ret = true;
    signed int i;

    switch (m_type) {
        case RME_MATRIXCTRL_GAINS:
            i = val;
            if (i >= 0)
                ret = m_parent.setAmpGain(col, val);
            else
                ret = -1;
            break;
        case RME_MATRIXCTRL_INPUT_FADER:
          return m_parent.setMixerGain(RME_FF_MM_INPUT, col, row, val);
          break;
        case RME_MATRIXCTRL_PLAYBACK_FADER:
          return m_parent.setMixerGain(RME_FF_MM_PLAYBACK, col, row, val);
          break;
        case RME_MATRIXCTRL_OUTPUT_FADER:
          return m_parent.setMixerGain(RME_FF_MM_OUTPUT, col, row, val);
          break;
    }

    return ret;
}

double RmeSettingsMatrixCtrl::getValue(const int row, const int col) 
{
    double val = 0.0;
    switch (m_type) {
        case RME_MATRIXCTRL_GAINS:
            val = m_parent.getAmpGain(col);
            break;
        case RME_MATRIXCTRL_INPUT_FADER:
            val = m_parent.getMixerGain(RME_FF_MM_INPUT, col, row);
            break;
        case RME_MATRIXCTRL_PLAYBACK_FADER:
            val = m_parent.getMixerGain(RME_FF_MM_PLAYBACK, col, row);
            break;
        case RME_MATRIXCTRL_OUTPUT_FADER:
            val = m_parent.getMixerGain(RME_FF_MM_OUTPUT, col, row);
            break;
    }

    return val;
}
      

}
