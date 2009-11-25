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

FocusriteEAP::Poti::Poti(Dice::Focusrite::FocusriteEAP* eap, std::string name, size_t offset)
    : Control::Discrete(eap, name)
    , m_eap(eap)
    , m_offset(offset)
{
    m_eap->readApplicationReg(m_offset, &m_tmp);
    printf("%s: Value: %i\n", name.c_str(), m_tmp);
    m_value = /*127*/-m_tmp;
}

int FocusriteEAP::Poti::getValue() {
    return m_value;
}
bool FocusriteEAP::Poti::setValue(int n) {
    if (n == m_value)
        return true;
    m_value = n;
    return m_eap->writeApplicationReg(m_offset, -n);
}


FocusriteEAP::FocusriteEAP(Dice::Device& dev) : Dice::Device::EAP(dev) {
}

bool FocusriteEAP::readApplicationReg(unsigned offset, quadlet_t* quadlet) {
    return readReg(eRT_Application, offset, quadlet);
}
bool FocusriteEAP::writeApplicationReg(unsigned offset, quadlet_t quadlet) {
    bool ret = writeReg(eRT_Application, offset, quadlet);
    if (!ret) return false;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Will sent command %i.\n", commandToFix(offset));
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
    printf("%s: %s\n", name.c_str(), (m_state_tmp&m_activevalue)?"true":"false");
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


class VolumeControl : public Control::Discrete
{
public:
    VolumeControl(FocusriteEAP* eap, std::string name, size_t offset, int bitshift)
        : Control::Discrete(eap, name)
        , m_eap(eap)
        , m_offset(offset)
        , m_bitshift(bitshift)
    {
        quadlet_t tmp;
        m_eap->readApplicationReg(m_offset, &tmp);
        m_value = - ((tmp>>m_bitshift)&0xff);
        printf("%s: %i -> %i\n", name.c_str(), tmp, m_value);
    }

    int getValue(int) { return getValue(); }
    bool setValue(int, int n) { return setValue(n); }

    int getMinimum() { return -255; }
    int getMaximum() { return 0; }

    int getValue() { return m_value; }
    bool setValue(int n) {
        if (n == m_value)
            return true;
        m_value = n;
        return m_eap->writeApplicationReg(m_offset, (-n)<<m_bitshift);
    }
private:
    FocusriteEAP* m_eap;
    size_t m_offset;
    int m_bitshift;
    int m_value;
};


FocusriteEAP::MonitorSection::MonitorSection(Dice::Focusrite::FocusriteEAP* eap, std::string name)
    : Control::Container(eap, name)
    , m_eap(eap)
    , m_monitorlevel(0)
    , m_dimlevel(0)
{
    Control::Container* grp_globalmute = new Control::Container(m_eap, "GlobalMute");
    addElement(grp_globalmute);
    m_mute = new Switch(m_eap, "State", 0x0C, 1);
    grp_globalmute->addElement(m_mute);

    Control::Container* grp_globaldim = new Control::Container(m_eap, "GlobalDim");
    addElement(grp_globaldim);
    m_dim = new Switch(m_eap, "State", 0x10, 1);
    grp_globaldim->addElement(m_dim);
    m_dimlevel = m_eap->getDimPoti("Level");
    grp_globaldim->addElement(m_dimlevel);

    Control::Container* grp_mono = new Control::Container(m_eap, "Mono");
    addElement(grp_mono);

    Control::Container* grp_globalvolume = new Control::Container(m_eap, "GlobalVolume");
    addElement(grp_globalvolume);
    m_monitorlevel = m_eap->getMonitorPoti("Level");
    grp_globalvolume->addElement(m_monitorlevel);

    Control::Container* grp_perchannel = new Control::Container(m_eap, "PerChannel");
    addElement(grp_perchannel);

    for (int i=0; i<10; ++i) {
        std::stringstream stream;
        stream << "AffectsCh" << i;
        Switch* s = new Switch(m_eap, stream.str(), 0x3C, 1<<i);
        grp_globalmute->addElement(s);
        //m_mute_affected.push_back(s);
        s = new Switch(m_eap, stream.str(), 0x3C, 1<<(10+i));
        grp_globaldim->addElement(s);
        //m_dim_affected.push_back(s);
    }
    for (int i=0; i<5; ++i) {
        std::stringstream stream;
        stream << "Ch" << i*2 << "Ch" << i*2+1;
        Switch* s = new Switch(m_eap, stream.str(), 0x3C, 1<<(20+i));
        grp_mono->addElement(s);
        //m_mono.push_back(s);

        stream.str(std::string());
        stream << "AffectsCh" << i*2;
        s = new Switch(m_eap, stream.str(), 0x28+i*4, 1);
        grp_globalvolume->addElement(s);
        stream.str(std::string());
        stream << "AffectsCh" << i*2+1;
        s = new Switch(m_eap, stream.str(), 0x28+i*4, 2);
        grp_globalvolume->addElement(s);

        stream.str(std::string());
        stream << "Mute" << i*2;
        s = new Switch(m_eap, stream.str(), 0x28+i*4, 4);
        grp_perchannel->addElement(s);
        stream.str(std::string());
        stream << "Mute" << i*2+1;
        s = new Switch(m_eap, stream.str(), 0x28+i*4, 8);
        grp_perchannel->addElement(s);

        stream.str(std::string());
        stream << "Volume" << i*2;
        VolumeControl* vol = new VolumeControl(m_eap, stream.str(), 0x14+i*4, 0);
        grp_perchannel->addElement(vol);
        stream.str(std::string());
        stream << "Volume" << i*2+1;
        vol = new VolumeControl(m_eap, stream.str(), 0x14+i*4, 8);
        grp_perchannel->addElement(vol);
    }
}

}
}

// vim: et
