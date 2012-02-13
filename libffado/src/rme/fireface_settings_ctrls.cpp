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
            if (m_parent.setInputLevel(v) == 0) {
                m_value = v;
            }
            break;
        case RME_CTRL_OUTPUT_LEVEL:
            if (m_parent.setOutputLevel(v) == 0) {
                m_value = v;
            }
            break;
        case RME_CTRL_FF400_PAD_SW:
            // Object's "m_info" field is the channel
            if (m_parent.setInputPadOpt(m_info, v) == 0) {
                m_value = (v != 0);
            }
            break;
        case RME_CTRL_FF400_INSTR_SW:
            // Object's "m_info" field is the channel
            if (m_parent.setInputInstrOpt(m_info, v) == 0) {
                m_value = (v != 0);
            }
            break;
        case RME_CTRL_SPDIF_INPUT_MODE:
            if (m_parent.setSpdifInputMode(v==0?FF_SWPARAM_SPDIF_INPUT_COAX:FF_SWPARAM_SPDIF_INPUT_OPTICAL)) {
                m_value = v;
            }
            break;
        case RME_CTRL_SPDIF_OUTPUT_OPTICAL:
            if (m_parent.setSpdifOutputIsOptical(v!=0) == 0) {
               m_value = (v != 0);
            }
            break;
        case RME_CTRL_SPDIF_OUTPUT_PRO:
            if (m_parent.setSpdifOutputProOn(v!=0) == 0) {
               m_value = (v != 0);
            }
            break;
        case RME_CTRL_SPDIF_OUTPUT_EMPHASIS:
            if (m_parent.setSpdifOutputEmphasisOn(v!=0) == 0) {
               m_value = (v != 0);
            }
            break;
        case RME_CTRL_SPDIF_OUTPUT_NONAUDIO:
            if (m_parent.setSpdifOutputNonAudioOn(v!=0) == 0) {
               m_value = (v != 0);
            }
            break;
        case RME_CTRL_PHONES_LEVEL:
            if (m_parent.setPhonesLevel(v) == 0) {
                m_value = v;
            }
            break;

        case RME_CTRL_CLOCK_MODE:
            if (m_parent.setClockMode(v==1?FF_SWPARAM_CLOCK_MODE_AUTOSYNC:FF_SWPARAM_CLOCK_MODE_MASTER) == 0) {
                m_value = v;
            }
            break;
        case RME_CTRL_SYNC_REF: {
            signed int val;
            switch (v) {
              case 0: val = FF_SWPARAM_SYNCREF_WORDCLOCK; break;
              case 1: val = FF_SWPARAM_SYNCREF_ADAT1; break;
              case 2: val = FF_SWPARAM_SYNCREF_ADAT2; break;
              case 3: val = FF_SWPARAM_SYNCREF_SPDIF; break;
              case 4: val = FF_SWPARAM_SYNCREC_TCO; break;
              default:
                val = FF_SWPARAM_SYNCREF_WORDCLOCK;
            }
            if (m_parent.setSyncRef(val) == 0) {
              m_value = v;
            }
            break;
        }

        // All RME_CTRL_INFO_* controls are read-only.  Warn on attempts to
        // set these.
        case RME_CTRL_INFO_MODEL:
        case RME_CTRL_INFO_TCO_PRESENT:
        case RME_CTRL_INFO_SYSCLOCK_MODE:
        case RME_CTRL_INFO_SYSCLOCK_FREQ:
        case RME_CTRL_INFO_AUTOSYNC_FREQ:
        case RME_CTRL_INFO_AUTOSYNC_SRC:
        case RME_CTRL_INFO_SYNC_STATUS:
        case RME_CTRL_INFO_SPDIF_FREQ:
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
FF_state_t ff_state;

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
        case RME_CTRL_SPDIF_INPUT_MODE:
            i = m_parent.getSpdifInputMode();
            return i==FF_SWPARAM_SPDIF_INPUT_COAX?0:1;
            break;
        case RME_CTRL_SPDIF_OUTPUT_OPTICAL:
            return m_parent.getSpdifOutputIsOptical();
            break;
        case RME_CTRL_SPDIF_OUTPUT_PRO:
            return m_parent.getSpdifOutputProOn();
            break;
        case RME_CTRL_SPDIF_OUTPUT_EMPHASIS:
            return m_parent.getSpdifOutputEmphasisOn();
            break;
        case RME_CTRL_SPDIF_OUTPUT_NONAUDIO:
            return m_parent.getSpdifOutputNonAudioOn();
            break;
        case RME_CTRL_PHONES_LEVEL:
            return m_parent.getPhonesLevel();
            break;
        case RME_CTRL_CLOCK_MODE:
            return m_parent.getClockMode()==FF_SWPARAM_CLOCK_MODE_AUTOSYNC?1:0;
            break;
        case RME_CTRL_SYNC_REF: {
            signed int val = m_parent.getSyncRef();
            switch (val) {
              case FF_SWPARAM_SYNCREF_WORDCLOCK: return 0;
              case FF_SWPARAM_SYNCREF_ADAT1: return 1;
              case FF_SWPARAM_SYNCREF_ADAT2: return 2;
              case FF_SWPARAM_SYNCREF_SPDIF: return 3;
              case FF_SWPARAM_SYNCREC_TCO: return 4;
              default:
                return 0;
            }
            break;
        }
        case RME_CTRL_INFO_MODEL:
            return m_parent.getRmeModel();
            break;

        case RME_CTRL_INFO_TCO_PRESENT:
            return m_parent.getTcoPresent();

        case RME_CTRL_INFO_SYSCLOCK_MODE:
            if (m_parent.get_hardware_state(&ff_state) == 0)
                return ff_state.clock_mode;
            break;
        case RME_CTRL_INFO_SYSCLOCK_FREQ:
            return m_parent.getSamplingFrequency();
        case RME_CTRL_INFO_AUTOSYNC_FREQ:
            if (m_parent.get_hardware_state(&ff_state) == 0)
                return ff_state.autosync_freq;
            break;
        case RME_CTRL_INFO_AUTOSYNC_SRC:
            if (m_parent.get_hardware_state(&ff_state) == 0)
                return ff_state.autosync_source;
            break;
        case RME_CTRL_INFO_SYNC_STATUS:
            if (m_parent.get_hardware_state(&ff_state) == 0)
                return (ff_state.adat1_sync_status) |
                       (ff_state.adat2_sync_status << 2) |
                       (ff_state.spdif_sync_status << 4) |
                       (ff_state.wclk_sync_status << 6) |
                       (ff_state.tco_sync_status << 8);
            break;
        case RME_CTRL_INFO_SPDIF_FREQ:
            if (m_parent.get_hardware_state(&ff_state) == 0)
                return ff_state.spdif_freq;
            break;

        default:
            debugOutput(DEBUG_LEVEL_ERROR, "Unknown control type 0x%08x\n", m_type);
    }

    return 0;
}


static std::string getOutputName(const signed int model, const int idx)
{
    char buf[64];
    if (model == RME_MODEL_FIREFACE400) {
        if (idx >= 10)
            snprintf(buf, sizeof(buf), "ADAT out %d", idx-9);
        else
        if (idx >= 8)
            snprintf(buf, sizeof(buf), "SPDIF out %d", idx-7);
        else
        if (idx >= 6)
            snprintf(buf, sizeof(buf), "Mon out %d", idx+1);
        else
            snprintf(buf, sizeof(buf), "Line out %d", idx+1);
    } else {
        snprintf(buf, sizeof(buf), "out %d", idx);
    }
    return buf;
}

static std::string getInputName(const signed int model, const int idx)
{
    char buf[64];
    if (model == RME_MODEL_FIREFACE400) {
        if (idx >= 10)
            snprintf(buf, sizeof(buf), "ADAT in %d", idx-9);
        else
        if (idx >= 8)
            snprintf(buf, sizeof(buf), "SPDIF in %d", idx-7);
        else
        if (idx >= 4)
            snprintf(buf, sizeof(buf), "Line in %d", idx+1);
        else
        if (idx >= 2)
            snprintf(buf, sizeof(buf), "Inst/line %d", idx+1);
        else
            snprintf(buf, sizeof(buf), "Mic/line %d", idx+1);
    } else {
        snprintf(buf, sizeof(buf), "in %d", idx);
    }
    return buf;
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
    if (m_type == RME_MATRIXCTRL_OUTPUT_FADER)
        return "";
    return getOutputName(m_parent.getRmeModel(), row);
}

std::string RmeSettingsMatrixCtrl::getColName(const int col)
{
    if (m_type == RME_MATRIXCTRL_PLAYBACK_FADER)
        return "";
    if (m_type == RME_MATRIXCTRL_OUTPUT_FADER)
        return getOutputName(m_parent.getRmeModel(), col);

    return getInputName(m_parent.getRmeModel(), col);
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

        case RME_MATRIXCTRL_INPUT_MUTE:
          return m_parent.setMixerFlags(RME_FF_MM_INPUT, col, row, FF_SWPARAM_MF_MUTED, val!=0);
          break;
        case RME_MATRIXCTRL_PLAYBACK_MUTE:
          return m_parent.setMixerFlags(RME_FF_MM_PLAYBACK, col, row, FF_SWPARAM_MF_MUTED, val!=0);
          break;
        case RME_MATRIXCTRL_OUTPUT_MUTE:
          return m_parent.setMixerFlags(RME_FF_MM_OUTPUT, col, row, FF_SWPARAM_MF_MUTED, val!=0);
          break;
        case RME_MATRIXCTRL_INPUT_INVERT:
          return m_parent.setMixerFlags(RME_FF_MM_INPUT, col, row, FF_SWPARAM_MF_INVERTED, val!=0);
          break;
        case RME_MATRIXCTRL_PLAYBACK_INVERT:
          return m_parent.setMixerFlags(RME_FF_MM_PLAYBACK, col, row, FF_SWPARAM_MF_INVERTED, val!=0);
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

        case RME_MATRIXCTRL_INPUT_MUTE:
          return m_parent.getMixerFlags(RME_FF_MM_INPUT, col, row, FF_SWPARAM_MF_MUTED) != 0;
          break;
        case RME_MATRIXCTRL_PLAYBACK_MUTE:
          return m_parent.getMixerFlags(RME_FF_MM_PLAYBACK, col, row, FF_SWPARAM_MF_MUTED) != 0;
          break;
        case RME_MATRIXCTRL_OUTPUT_MUTE:
          return m_parent.getMixerFlags(RME_FF_MM_OUTPUT, col, row, FF_SWPARAM_MF_MUTED) != 0;
          break;
        case RME_MATRIXCTRL_INPUT_INVERT:
          return m_parent.getMixerFlags(RME_FF_MM_INPUT, col, row, FF_SWPARAM_MF_INVERTED) != 0;
          break;
        case RME_MATRIXCTRL_PLAYBACK_INVERT:
          return m_parent.getMixerFlags(RME_FF_MM_PLAYBACK, col, row, FF_SWPARAM_MF_INVERTED) != 0;
          break;
    }

    return val;
}
      

}
