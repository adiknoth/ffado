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

#define RME_CTRL_PHANTOM_SW            0x00
#define RME_CTRL_SPDIF_INPUT_MODE      0x01
#define RME_CTRL_SPDIF_OUTPUT_OPTIONS  0x02
#define RME_CTRL_CLOCK_MODE            0x03
#define RME_CTRL_SYNC_REF              0x04
#define RME_CTRL_DEV_OPTIONS           0x05
#define RME_CTRL_LIMIT_BANDWIDTH       0x06
#define RME_CTRL_INPUT_LEVEL           0x07
#define RME_CTRL_OUTPUT_LEVEL          0x08
#define RME_CTRL_INSTRUMENT_OPTIONS    0x09
#define RME_CTRL_WCLK_SINGLE_SPEED     0x0a
#define RME_CTRL_PHONES_LEVEL          0x0b
#define RME_CTRL_INPUT0_OPTIONS        0x0c
#define RME_CTRL_INPUT1_OPTIONS        0x0d
#define RME_CTRL_INPUT2_OPTIONS        0x0e

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
                    if (!err && on) {
                        m_value |= (0x01 << i);
                    } else {
                        m_value &= ~(0x01 << i);
                    }
                }
            }
            break;
    }

    return err==0?true:false;
}

int
RmeSettingsCtrl::getValue() {

    switch (m_type) {
        case RME_CTRL_PHANTOM_SW:
            return m_value;
            break;
    }
    return 0;
}

}
