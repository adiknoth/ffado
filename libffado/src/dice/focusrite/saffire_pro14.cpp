/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2012 by Philippe Carriere
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

#include "saffire_pro14.h"

#include "focusrite_eap.h"

namespace Dice {
namespace Focusrite {

int SaffirePro14::SaffirePro14EAP::commandToFix(unsigned offset) {
    if (offset<0x14) return 2;
    if (offset<0x3C && offset>=0x14) return 1;
    if (offset<0x5C && offset>=0x54) return 1;
    if (offset<0x44 && offset>=0x3C) return 3;
    if (offset == 0x5C) return 4;
    return 0;
}
FocusriteEAP::Poti* SaffirePro14::SaffirePro14EAP::getMonitorPoti(std::string name) {
    return new FocusriteEAP::Poti(this, name, 0x54);
}
FocusriteEAP::Poti* SaffirePro14::SaffirePro14EAP::getDimPoti(std::string name) {
    return new FocusriteEAP::Poti(this, name, 0x58);
}

//
// From Focusrite doc, whatever is the samplerate, Pro 14 has
//  - 4 analogic inputs (mic/line)
//  - 2 SPDIF inputs
//  - 8 ieee1394 inputs
//  - 18 mixer inputs (this is the standart for all Dice Jr)
//
//  - 4 analogic outputs
//  - 2 SPDIF outputs
//  - 12 ieee1394 outputs
//  - 16 mixer outputs
//
// *** This has mainly to be checked ***
//
void SaffirePro14::SaffirePro14EAP::setupSources_low() {
    addSource("SPDIF/In",  6,  2, eRS_AES, 1);
    addSource("Mic/Lin", 0,  4, eRS_InS0, 1);
    addSource("Mixer/Out",  0, 16, eRS_Mixer, 1);
    addSource("1394",   0, 12, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}

void SaffirePro14::SaffirePro14EAP::setupDestinations_low() {
    addDestination("SPDIF/Out",  6,  2, eRD_AES, 1);
    addDestination("Line", 0,  4, eRD_InS0, 1);
    addDestination("Mixer/In",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In",  0,  2, eRD_Mixer1, 17);
    addDestination("1394",   0, 6, eRD_ATX0, 1);
    addDestination("Loop",   6,  2, eRD_ATX0, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

// 88.2/96 kHz
//
void SaffirePro14::SaffirePro14EAP::setupSources_mid() {
    setupSources_low();
}

void SaffirePro14::SaffirePro14EAP::setupDestinations_mid() {
    setupDestinations_low();
}

//
// 192 kHz is not supported
//
void SaffirePro14::SaffirePro14EAP::setupSources_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 14\n");
}

void SaffirePro14::SaffirePro14EAP::setupDestinations_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 14\n");
}

SaffirePro14::SaffirePro14( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro14 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

/**
 * The default configurations for the Saffire Pro 14 router.
 *  For coherence with hardware, destinations must follow a specific ordering
 *** Encode by analogy with Pro40/Pro24: Testing required ***
 */
void
SaffirePro14::SaffirePro14EAP::setupDefaultRouterConfig_low() {
    unsigned int i;
    // the 1394 stream receivers except the two "loops" one
    for (i=0; i<4; i++) {
        addRoute(eRS_InS1, i, eRD_ATX0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX0, i+4);
    }
    // The audio ports
    // Ensure that audio port are not muted
    for (i=0; i<4; i++) {
        addRoute(eRS_ARX0, i, eRD_InS0, i);
    }
    // the SPDIF receiver
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_AES, i);
    }
    // the "loops" 1394 stream receivers
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_ATX0, i+6);
    }
    // the Mixer inputs
    for (i=0; i<4; i++) {
        addRoute(eRS_InS1, i, eRD_Mixer0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_Mixer0, i+4);
    }
    for (i=0; i<10; i++) {
        addRoute(eRS_Muted, 0, eRD_Mixer0, i+6);
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
 *  Assume to be identical to low rate
 */
void
SaffirePro14::SaffirePro14EAP::setupDefaultRouterConfig_mid() {
    setupDefaultRouterConfig_low();
}

/**
 *  High rate not supported
 */
void
SaffirePro14::SaffirePro14EAP::setupDefaultRouterConfig_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 14\n");
}

SaffirePro14::~SaffirePro14()
{
    getEAP()->storeFlashConfig();
}

bool SaffirePro14::discover() {
    if (Dice::Device::discover()) {
        m_monitor = new FocusriteEAP::MonitorSection(dynamic_cast<FocusriteEAP*>(getEAP()), "Monitoring");
        getEAP()->addElement(m_monitor);
        return true;
    }
    return false;
}

void
SaffirePro14::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Focusrite::SaffirePro14\n");
    Dice::Device::showDevice();
}

Dice::EAP* SaffirePro14::createEAP() {
    return new SaffirePro14EAP(*this);
}

bool SaffirePro14::setNickname(std::string name) {
    return getEAP()->writeRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name.c_str(), name.size());
}
std::string SaffirePro14::getNickname() {
    char name[16];
    getEAP()->readRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name, 16);
    return std::string(name);
}

}
}
// vim: et
