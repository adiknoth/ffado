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

FocusriteEAP::FocusriteEAP(Dice::Device& dev) : Dice::EAP(dev) {
}

/**
 * Application space register read/write
 */
bool FocusriteEAP::readApplicationReg(unsigned offset, quadlet_t* quadlet) {
    bool ret = readReg(Dice::EAP::eRT_Application, offset, quadlet);
    return ret;
}

bool FocusriteEAP::writeApplicationReg(unsigned offset, quadlet_t quadlet) {
    // Do not write beyond the limit
    if (offset > FOCUSRITE_EAP_REGISTER_APP_MONITORING_LIMIT)
    {
      debugWarning(" Writing beyond address 0x%02x prohibited\n",
                   FOCUSRITE_EAP_REGISTER_APP_MONITORING_LIMIT);
      return false;
    }
    
    bool ret = writeReg(Dice::EAP::eRT_Application, offset, quadlet);
    if (!ret) {
        debugWarning("Couldn't write %i to register %x!\n", quadlet, offset);
        return false;
    }
    return ret;
}

// Message set specific register
bool FocusriteEAP::messageSet(unsigned offset, quadlet_t quadlet) {
    // Do not write beyond the limit
    if (offset > FOCUSRITE_EAP_REGISTER_APP_MONITORING_LIMIT)
    {
      debugWarning(" Message set register can not be beyond address 0x%02x\n",
                   FOCUSRITE_EAP_REGISTER_APP_MONITORING_LIMIT);
      return false;
    }

    bool ret = writeApplicationReg(offset, quadlet);
    // Send NO_MESSAGE after any non-zero messages (Focusrite recommandation)
    writeApplicationReg(offset, (quadlet_t) FOCUSRITE_EAP_MESSAGE_SET_NO_MESSAGE);
    return ret;
}

/**
 *  Potentiometer Class
 */
FocusriteEAP::Poti::Poti(Dice::Focusrite::FocusriteEAP* eap, std::string name,
    size_t offset, size_t msgset_offset, int msgset_value) : Control::Discrete(eap, name)
    , m_eap(eap)
    , m_name(name)
    , m_offset(offset)
    , m_msgset_offset(msgset_offset)
    , m_msgset_value(msgset_value)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create Poti %s)\n", m_name.c_str());    
}

int FocusriteEAP::Poti::getValue() {
    int m_value;
    quadlet_t tmp;

    m_eap->readApplicationReg(m_offset, &tmp);
    m_value = -tmp;
    return m_value;
}

bool FocusriteEAP::Poti::setValue(int n) {
    // Might be the value has been modified via hardware; better to read the current value
    int m_value = getValue();

    if (n == m_value)
        return true;
    m_value = -n;
    m_eap->writeApplicationReg(m_offset, (quadlet_t) m_value);
    m_eap->messageSet(m_msgset_offset, (quadlet_t) m_msgset_value);
    return true;
}

/**
 * Switch class
 */
FocusriteEAP::Switch::Switch(Dice::Focusrite::FocusriteEAP* eap, std::string name,
    size_t offset, int activevalue, size_t msgset_offset, int msgset_value )
    : Control::Boolean(eap, name)
    , m_eap(eap)
    , m_name(name)
    , m_offset(offset)
    , m_activevalue(activevalue)
    , m_msgset_offset(msgset_offset)
    , m_msgset_value(msgset_value)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create Switch %s)\n", m_name.c_str());    
}

bool FocusriteEAP::Switch::selected() {
    quadlet_t state_tmp;

    m_eap->readApplicationReg(m_offset, &state_tmp);
    bool is_selected = (state_tmp&m_activevalue)?true:false;
    return is_selected;
}

bool FocusriteEAP::Switch::select(bool n) {
    quadlet_t state_tmp;
    m_eap->readApplicationReg(m_offset, &state_tmp);
    bool is_selected = (state_tmp&m_activevalue)?true:false;

    // Switch the corresponding bit(s)
    if ( n != is_selected ) {
        m_eap->writeApplicationReg(m_offset, state_tmp^m_activevalue);
        m_eap->messageSet(m_msgset_offset, (quadlet_t) m_msgset_value);
        is_selected = n;
    }
    return is_selected;
}

/**
 * Volume Control Class
 */
FocusriteEAP::VolumeControl::VolumeControl(Dice::Focusrite::FocusriteEAP* eap,
    std::string name, size_t offset, int bitshift, size_t msgset_offset, int msgset_value)
    : Control::Discrete(eap, name)
    , m_eap(eap)
    , m_name(name)
    , m_offset(offset)
    , m_bitshift(bitshift)
    , m_msgset_offset(msgset_offset)
    , m_msgset_value(msgset_value)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create Volume Control %s)\n", m_name.c_str());    
}

int FocusriteEAP::VolumeControl::getValue() {
    int m_value;
    quadlet_t tmp;
    m_eap->readApplicationReg(m_offset, &tmp);
    m_value = - ((tmp>>m_bitshift)&0xff);
    return m_value;
}

bool FocusriteEAP::VolumeControl::setValue(int n) {
    int m_value;
    quadlet_t tmp;
    m_eap->readApplicationReg(m_offset, &tmp);
    m_value = - ((tmp>>m_bitshift)&0xff);
    if (n == m_value)
        return true;

    m_value = n;
    tmp &= ~(0xff<<m_bitshift);
    bool ret = m_eap->writeApplicationReg(m_offset, ((-n)<<m_bitshift)|tmp);
    m_eap->messageSet(m_msgset_offset, (quadlet_t) m_msgset_value);
    return ret;
}

}
}

// vim: et
