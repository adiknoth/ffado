/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2009 by Arnold Krille
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

#include "focusrite_eap.h"

namespace Dice {
namespace Focusrite {

const int msgSet = 0x68;

FocusriteEAP::FocusriteEAP(Dice::Device& dev) : Dice::Device::EAP(dev) {
}

bool FocusriteEAP::readApplicationReg(unsigned offset, quadlet_t* quadlet) {
    return readReg(eRT_Application, offset, quadlet);
}
bool FocusriteEAP::writeApplicationReg(unsigned offset, quadlet_t quadlet) {
    bool ret = writeReg(eRT_Application, offset, quadlet);
    if (!ret) return false;
    return writeReg(eRT_Application, msgSet, commandToFix(offset));
}


FocusriteEAP::Switch::Switch(Dice::Focusrite::FocusriteEAP* eap, std::string name, size_t offset, int activevalue )
    : Control::Boolean(eap, name)
    , m_eap(eap)
    , m_selected(0)
    , m_offset(offset)
    , m_activevalue(activevalue)
{
    m_eap->readApplicationReg(m_offset, &m_state_tmp);
    printf("%s: Active?%i\n", name.c_str(), m_state_tmp&m_activevalue);
    debugOutput(DEBUG_LEVEL_VERBOSE, "Probably the initialization is the other way round.\n");
    m_selected = (m_state_tmp&m_activevalue)?true:false;
}

bool FocusriteEAP::Switch::selected() {
    return m_selected;
}

bool FocusriteEAP::Switch::select(bool n) {
    if ( n != m_selected ) {
        m_selected = n;
        m_eap->readApplicationReg(m_offset, &m_state_tmp);
        m_eap->writeApplicationReg(m_offset, m_state_tmp^m_activevalue);
    }
    return true;
}



FocusriteEAP::MonitorSection::MonitorSection(Dice::Focusrite::FocusriteEAP* eap, std::string name)
    : Control::Container(eap, name)
    , m_eap(eap)
{
    m_mute = new Switch(m_eap, "Mute", 0x0C, 1);
    addElement(m_mute);
    m_dim = new Switch(m_eap, "Dim", 0x10, 1);
    addElement(m_dim);
    for (int i=0; i<10; ++i) {
        std::stringstream stream;
        stream << "MuteAffectsCh" << i;
        Switch* s = new Switch(m_eap, stream.str(), 0x3C, 1<<i);
        addElement(s);
        m_mute_affected.push_back(s);
        stream.str(std::string());
        stream << "DimAffectsCh" << i;
        s = new Switch(m_eap, stream.str(), 0x3C, 1<<(10+i));
        addElement(s);
        m_dim_affected.push_back(s);
    }
    for (int i=0; i<5; ++i) {
        std::stringstream stream;
        stream << "Mono_" << i*2 << "_" << i*2+1;
        Switch* s = new Switch(m_eap, stream.str(), 0x3C, 1<<(20+i));
        addElement(s);
        m_mono.push_back(s);
    }
}

}
}

// vim: et
