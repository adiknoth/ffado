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

SaffirePro24::Switch::Switch(Dice::Device::EAP* eap, std::string name, size_t offset, int activevalue )
    : Control::Boolean(eap, name)
    , m_eap(eap)
    , m_selected(0)
    , m_offset(offset)
    , m_activevalue(activevalue)
{
    m_eap->readReg(Dice::Device::EAP::eRT_Application, m_offset, &m_state_tmp);
    printf("%s: Active?%i\n", name.c_str(), m_state_tmp&m_activevalue);
    debugOutput(DEBUG_LEVEL_VERBOSE, "Probably the initialization is the other way round.\n");
    m_selected = (m_state_tmp&m_activevalue)?true:false;
}

bool SaffirePro24::Switch::selected() {
    return m_selected;
}

bool SaffirePro24::Switch::select(bool n) {
    if ( n != m_selected ) {
        m_selected = n;
        m_eap->readReg(Dice::Device::EAP::eRT_Application, m_offset, &m_state_tmp);
        m_eap->writeReg(Dice::Device::EAP::eRT_Application, m_offset, m_state_tmp^m_activevalue);
        int msg = 0;
        if (m_offset<0x14) msg = 2;
        if (m_offset<0x3C && m_offset>=0x14) msg = 1;
        if (m_offset<0x40 && m_offset>=0x3C) msg = 3;
        if (m_offset<0x60 && m_offset>=0x58) msg = 4;
        debugOutput(DEBUG_LEVEL_VERBOSE, "Switch will write the msg %i.\n", msg);
        m_eap->writeReg(Dice::Device::EAP::eRT_Application, msgSet, msg);
    }
    return true;
}

class SaffirePro24::LineInstSwitch : public Switch
{
public:
    LineInstSwitch(Dice::Device::EAP* eap, std::string name, size_t offset, int activevalue)
        : Switch(eap, name, offset, activevalue) {}
    std::string getBooleanLabel(bool n) {
        if ( n ) return "Instrument";
        return "Line";
    }
};
class SaffirePro24::LevelSwitch : public Switch
{
public:
    LevelSwitch(Dice::Device::EAP* eap, std::string name, size_t offset, int activevalue)
        : Switch(eap, name, offset, activevalue) {}
    std::string getBooleanLabel(bool n) {
        if ( n ) return "High";
        return "Low";
    }
};

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
    //debugOutput(DEBUG_LEVEL_VERBOSE, "Deleting the saffirePro24\n");
    /// I wonder whether we should really save only on clean exits or also each time a setting is
    //  changed. Or should we provide a function (and thus gui-button) to save the state of the
    //  device?
    getEAP()->storeFlashConfig();
    getEAP()->deleteElement(m_ch1);
    getEAP()->deleteElement(m_ch2);
    if (m_ch1) delete m_ch1;
    if (m_ch2) delete m_ch2;
}

bool SaffirePro24::discover() {
    if (Dice::Device::discover()) {
        fb_quadlet_t* tmp = (fb_quadlet_t *)calloc(2, sizeof(fb_quadlet_t));
        getEAP()->readRegBlock(Dice::Device::EAP::eRT_Application, 0x00, tmp, 1*sizeof(fb_quadlet_t));
        //hexDumpQuadlets(tmp, 2); // DEBUG
        if (tmp[0] != 0x00010004 ) {
            debugError("This is a Focusrite Saffire Pro24 but not the right firmware. Better stop here before something goes wrong.\n");
            debugError("This device has firmware 0x%x while we only know about version 0x%x.\n", tmp[0], 0x10004);
            return false;
        }
        //getEAP()->readRegBlock(Dice::Device::EAP::eRT_Command, 0x00, tmp, 2*sizeof(fb_quadlet_t)); // DEBUG
        //hexDumpQuadlets(tmp, 2); // DEBUG

        m_ch1 = new LineInstSwitch(getEAP(), "Ch1LineInst", 0x58, 2);
        getEAP()->addElement(m_ch1);
        m_ch2 = new LineInstSwitch(getEAP(), "Ch2LineInst", 0x58, 2<<16);
        getEAP()->addElement(m_ch2);
        m_ch3 = new LevelSwitch(getEAP(), "Ch3Level", 0x5C, 1);
        getEAP()->addElement(m_ch3);
        m_ch4 = new LevelSwitch(getEAP(), "Ch4Level", 0x5C, 1<<16);
        getEAP()->addElement(m_ch4);

        m_monitor = new MonitorSection(getEAP(), "Monitoring");
        getEAP()->addElement(m_monitor);
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


SaffirePro24::MonitorSection::MonitorSection(Dice::Device::EAP* eap, std::string name)
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
