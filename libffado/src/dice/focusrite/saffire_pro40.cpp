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

//
// Under 48kHz Saffire pro 40 has
//  - 8 analogic inputs (mic/line)
//  - 8 ADAT inputs
//  - 2 SPDIF inputs
//  - 20 ieee1394 inputs
//  - 18 mixer inputs
//
//  - 10 analogic outputs
//  - 8 ADAT outputs
//  - 2 SPDIF outputs
//  - 20 ieee1394 outputs
//  - 16 mixer outputs
//
void SaffirePro40::SaffirePro40EAP::setupSources_low() {
    addSource("SPDIF",  0,  2, eRS_AES, 1);
    addSource("ADAT",   0,  8, eRS_ADAT, 1);
    addSource("Mic/Lin", 0,  8, eRS_InS1, 1);
    addSource("Mixer",  0, 16, eRS_Mixer, 1);
    addSource("1394",   0, 12, eRS_ARX0, 1);
    addSource("1394",   0,  8, eRS_ARX1, 13);
    addSource("Mute",   0,  1, eRS_Muted);
}

void SaffirePro40::SaffirePro40EAP::setupDestinations_low() {
    addDestination("SPDIF",  0,  2, eRD_AES, 1);
    addDestination("ADAT",   0,  8, eRD_ADAT, 1);
    addDestination("Line", 0,  2, eRD_InS0, 1);
    addDestination("Line", 0,  8, eRD_InS1, 3);
    addDestination("Mixer",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer",  0,  2, eRD_Mixer1, 17);
    addDestination("1394",   0, 10, eRD_ATX0, 1);
    addDestination("1394",   0,  8, eRD_ATX1, 11);
    addDestination("Loop",   8,  2, eRD_ATX1, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

//
// Under 96kHz Saffire pro 40 has
//  - 8 analogic inputs (mic/line)
//  - 4 ADAT inputs
//  - 2 SPDIF inputs
//  - 16 ieee1394 inputs
//  - 18 mixer inputs
//
//  - 10 analogic outputs
//  - 4 ADAT outputs
//  - 2 SPDIF outputs
//  - 16 ieee1394 outputs
//  - 16 mixer outputs
//
void SaffirePro40::SaffirePro40EAP::setupSources_mid() {
    addSource("SPDIF",  0,  2, eRS_AES, 1);
    addSource("ADAT",   0,  4, eRS_ADAT, 1);
    addSource("Mic/Lin", 0,  8, eRS_InS1, 1);
    addSource("Mixer",  0, 16, eRS_Mixer, 1);
    addSource("1394",   0, 16, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}

void SaffirePro40::SaffirePro40EAP::setupDestinations_mid() {
    addDestination("SPDIF",  0,  2, eRD_AES, 1);
    addDestination("ADAT",   0,  4, eRD_ADAT, 1);
    addDestination("Line", 0,  2, eRD_InS0, 1);
    addDestination("Line", 0,  8, eRD_InS1, 3);
    addDestination("Mixer",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer",  0,  2, eRD_Mixer1, 17);
    addDestination("1394",   0, 14, eRD_ATX0, 1);
    addDestination("Loop",   14, 2, eRD_ATX0, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

//
// 192 kHz is not supported
//
void SaffirePro40::SaffirePro40EAP::setupSources_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 40\n");
}

void SaffirePro40::SaffirePro40EAP::setupDestinations_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 40\n");
}

SaffirePro40::SaffirePro40( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro40 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

/**
 * The default configurations for the Saffire Pro 40 router.
 *  For coherence with hardware, destinations must follow a specific ordering
 *  There must be 60 destinations at low samplerate
 *  Front LEDs are connected to the first eight router entries
 */
void
SaffirePro40::SaffirePro40EAP::setupDefaultRouterConfig_low() {
    unsigned int i;
    // the 1394 stream receivers except the two "loops" one
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_ATX0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX0, i+8);
    }
    for (i=0; i<8; i++) {
        addRoute(eRS_ADAT, i, eRD_ATX1, i);
    }
    // The audio ports
    // Ensure that audio port are not muted
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX0, i, eRD_InS0, i);
    }
    for (i=0; i<8; i++) {
      addRoute(eRS_ARX0, i%2, eRD_InS1, i);
    }
    // the SPDIF receiver
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_AES, i);
    }
    // the ADAT receiver
    for (i=0; i<8; i++) {
        addRoute(eRS_Muted, 0, eRD_ADAT, i);
    }
    // the "loops" 1394 stream receivers
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_ATX1, i+8);
    }
    // the Mixer inputs
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_Mixer0, i);
    }
    for (i=0; i<8; i++) {
        addRoute(eRS_ADAT, i, eRD_Mixer0, i+8);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX0, i, eRD_Mixer1, i);
    }
    // The two mute destinations
    for (i=0; i<2; i++) {
        addRoute(eRS_Mixer, i, eRD_Muted, 0);
    }
}

/**
 *  There must be 52 destinations at mid samplerate
 *  Front LEDs are connected to the first eight router entries
 */
void
SaffirePro40::SaffirePro40EAP::setupDefaultRouterConfig_mid() {
    unsigned int i;
    // the 1394 stream receivers except the two "loops" one
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_ATX0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX0, i+8);
    }
    for (i=0; i<4; i++) {
        addRoute(eRS_ADAT, i, eRD_ATX0, i+10);
    }
    // The audio ports
    // Ensure that audio port are not muted
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX0, i, eRD_InS0, i);
    }
    for (i=0; i<8; i++) {
      addRoute(eRS_ARX0, i%2, eRD_InS1, i);
    }
    // the SPDIF receiver
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_AES, i);
    }
    // the ADAT receiver
    for (i=0; i<4; i++) {
        addRoute(eRS_Muted, 0, eRD_ADAT, i);
    }
    // the "loops" 1394 stream receivers
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_ATX0, i+14);
    }
    // the Mixer inputs
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_Mixer0, i);
    }
    for (i=0; i<4; i++) {
        addRoute(eRS_ADAT, i, eRD_Mixer0, i+8);
    }
    for (i=0; i<4; i++) {
        addRoute(eRS_Muted, 0, eRD_Mixer0, i+12);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX0, i, eRD_Mixer1, i);
    }
    // The two mute destinations
    for (i=0; i<2; i++) {
        addRoute(eRS_Mixer, i, eRD_Muted, 0);
    }
}

/**
 *  High rate not supported
 */
void
SaffirePro40::SaffirePro40EAP::setupDefaultRouterConfig_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 40\n");
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

bool SaffirePro40::setNickname(std::string name) {
    return getEAP()->writeRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name.c_str(), name.size());
}
std::string SaffirePro40::getNickname() {
    char name[16];
    getEAP()->readRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name, 16);
    return std::string(name);
}

}
}
// vim: et
