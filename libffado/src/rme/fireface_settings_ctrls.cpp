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
            // High 16 bits are "write enable" bits for the correspoding
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

}
