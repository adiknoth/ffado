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

    switch (m_type) {
        case RME_CTRL_PHANTOM_SW:
            break;
    }
    return true;
}

int
RmeSettingsCtrl::getValue() {

    switch (m_type) {
        case RME_CTRL_PHANTOM_SW:
            break;
    }
    return 0;
}

}
