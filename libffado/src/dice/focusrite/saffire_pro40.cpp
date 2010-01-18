/*
 * Copyright (C) 2009 by Pieter Palmers
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

#include "saffire_pro40.h"

#include "focusrite_eap.h"

namespace Dice {
namespace Focusrite {

int SaffirePro40::SaffirePro40EAP::commandToFix(unsigned offset) {
    if (offset<0x14) return 2;
    if (offset<0x3C && offset>=0x14) return 1;
    if (offset<0x5C && offset>=0x54) return 1;
    if (offset<0x44 && offset>=0x3C) return 3;
    if (offset == 0x5C) return 4;
    return 0;
}
FocusriteEAP::Poti* SaffirePro40::SaffirePro40EAP::getMonitorPoti(std::string name) {
    return new FocusriteEAP::Poti(this, name, 0x54);
}
FocusriteEAP::Poti* SaffirePro40::SaffirePro40EAP::getDimPoti(std::string name) {
    return new FocusriteEAP::Poti(this, name, 0x58);
}

void SaffirePro40::SaffirePro40EAP::setupSources() {
    addSource("SPDIF",  6,  2, eRS_AES);
    addSource("ADAT",   0,  8, eRS_ADAT);
    addSource("Analog", 0,  8, eRS_InS0);
    addSource("Mixer",  0, 16, eRS_Mixer);
    addSource("1394",   0, 16, eRS_ARX0);
    addSource("Mute",   0,  1, eRS_Muted);
}
void SaffirePro40::SaffirePro40EAP::setupDestinations() {
    addDestination("SPDIF",  6,  2, eRD_AES);
    addDestination("ADAT",   0,  8, eRD_ADAT);
    addDestination("Analog", 0,  8, eRD_InS0);
    addDestination("Mixer",  0, 16, eRD_Mixer0);
    addDestination("Mixer",  0,  2, eRD_Mixer1, 16);
    addDestination("1394",   0, 16, eRD_ATX0);
    addDestination("Mute",   0,  1, eRD_Muted);
}

SaffirePro40::SaffirePro40( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro40 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

SaffirePro40::~SaffirePro40()
{
    getEAP()->storeFlashConfig();
}

bool SaffirePro40::discover() {
    if (Dice::Device::discover()) {
        m_monitor = new FocusriteEAP::MonitorSection(dynamic_cast<FocusriteEAP*>(getEAP()), "Monitoring");
        getEAP()->addElement(m_monitor);
        return true;
    }
    return false;
}

void
SaffirePro40::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Focusrite::SaffirePro40\n");
    Dice::Device::showDevice();
}

Dice::EAP* SaffirePro40::createEAP() {
    return new SaffirePro40EAP(*this);
}

bool SaffirePro40::setNickName(std::string name) {
    return getEAP()->writeRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name.c_str(), name.size());
}
std::string SaffirePro40::getNickName() {
    char name[16];
    getEAP()->readRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name, 16);
    return std::string(name);
}

}
}
// vim: et
