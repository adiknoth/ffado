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

#include "saffire_pro24.h"

namespace Dice {
namespace Focusrite {

const int msgSet = 0x68;

SaffirePro24::SaffirePro24( DeviceManager& d,
                            std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device(d , configRom)
    , m_ch1(NULL)
    , m_ch2(NULL)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro24 (NodeID %d)\n",
                getConfigRom().getNodeId());
}

SaffirePro24::~SaffirePro24()
{
    getEAP()->deleteElement(m_ch1);
    getEAP()->deleteElement(m_ch2);
    if (m_ch1) delete m_ch1;
    if (m_ch2) delete m_ch2;
}

bool SaffirePro24::discover() {
    if (Dice::Device::discover()) {
        fb_quadlet_t* tmp = (fb_quadlet_t *)calloc(2, sizeof(fb_quadlet_t));
        getEAP()->readRegBlock(Dice::Device::EAP::eRT_Application, 0x58, tmp, 2*sizeof(fb_quadlet_t));
        hexDumpQuadlets(tmp, 2);

        m_ch1 = new LineInstSwitch(getEAP(), "LineInstCh1", 0x58, 2);
        getEAP()->addElement(m_ch1);
        m_ch2 = new LineInstSwitch(getEAP(), "LineInstCh2", 0x58, 2<<16);
        getEAP()->addElement(m_ch2);
        m_ch3 = new LevelSwitch(getEAP(), "LevelCh3", 0x5C, 1);
        getEAP()->addElement(m_ch3);
        m_ch4 = new LevelSwitch(getEAP(), "LevelCh4", 0x5C, 1<<16);
        getEAP()->addElement(m_ch4);
        return true;
    }
    return false;
}

void SaffirePro24::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Focusrite::SaffirePro24\n");
    Dice::Device::showDevice();
}

bool SaffirePro24::setNickName( std::string name ) {
    return getEAP()->writeRegBlock( Dice::Device::EAP::eRT_Application, 0x40, (fb_quadlet_t*)name.c_str(), name.size() );
}

std::string SaffirePro24::getNickName() {
    char name[16];
    getEAP()->readRegBlock( Dice::Device::EAP::eRT_Application, 0x40, (fb_quadlet_t*)name, 16 );
    return std::string( name );
}

SaffirePro24::LineInstSwitch::LineInstSwitch(Dice::Device::EAP* eap, std::string name, size_t offset, int activevalue )
    : Control::Enum(eap, name)
    , m_eap(eap)
    , m_selected(0)
    , m_offset(offset)
    , m_activevalue(activevalue)
{
    m_eap->readReg(Dice::Device::EAP::eRT_Application, m_offset, &m_state_tmp);
    printf("%s: Active?%i\n", name.c_str(), m_state_tmp&m_activevalue);
    m_selected = (m_state_tmp&m_activevalue)?1:0;
}

int SaffirePro24::LineInstSwitch::selected() {
    return m_selected;
}

bool SaffirePro24::LineInstSwitch::select(int n) {
    if ( n != m_selected ) {
        m_selected = n;
        m_eap->readReg(Dice::Device::EAP::eRT_Application, m_offset, &m_state_tmp);
        m_eap->writeReg(Dice::Device::EAP::eRT_Application, m_offset, m_state_tmp^m_activevalue);
        m_eap->writeReg(Dice::Device::EAP::eRT_Application, msgSet, 4);
    }
    return true;
}
std::string SaffirePro24::LineInstSwitch::getEnumLabel(int n) {
    if ( n == 1 ) {
        return "Instrument";
    }
    return "Line";
}

SaffirePro24::LevelSwitch::LevelSwitch(Dice::Device::EAP* eap, std::string name, size_t offset, int activevalue)
    : LineInstSwitch(eap, name, offset, activevalue)
{
}
std::string SaffirePro24::LevelSwitch::getEnumLabel(int n) {
    switch (n) {
        case 0:
            return "Low";
        case 1:
            return "High";
        default:
            return "";
    }
}


}
}

// vim: et
