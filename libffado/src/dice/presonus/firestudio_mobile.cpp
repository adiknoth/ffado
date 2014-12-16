/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2012 by Philippe Carriere
 * Copyright (C) 2014 by Philippe Carriere
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

#include "firestudio_mobile.h"

namespace Dice {
namespace Presonus {

//
// Firestudio Mobile has
//  - 2 mic/inst inputs
//  - 6 line inputs
//  - 2 SPDIF inputs
//  - 6 ieee1394 inputs
//  - 18 mixer inputs
//
//  - 4 analogic line outputs (stereo main, stereo phone)
//  - 2 SPDIF outputs
//  - 10 ieee1394 outputs
//  - 16 mixer outputs
//
void FirestudioMobile::FirestudioMobileEAP::setupSources_low() {
    addSource("SPDIF/In",  2,  2, eRS_AES, 1);
    addSource("Mic/Inst/In", 0,  2, eRS_InS0, 1);
    addSource("Lin/In", 2,  6, eRS_InS0, 3);
    addSource("Mixer/Out",  0, 16, eRS_Mixer, 1);
    addSource("1394/In",   0, 6, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}

void FirestudioMobile::FirestudioMobileEAP::setupDestinations_low() {
    addDestination("SPDIF/Out",  2,  2, eRD_AES, 1);
    addDestination("Line/Out", 0,  4, eRD_InS0, 1);
    addDestination("Mixer/In",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In",  0,  2, eRD_Mixer1, 17);
    addDestination("1394/Out",   0, 10, eRD_ATX0, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

//
// Independent of samplerate
void FirestudioMobile::FirestudioMobileEAP::setupSources_mid() {
    setupSources_low();
}

void FirestudioMobile::FirestudioMobileEAP::setupDestinations_mid() {
    setupDestinations_low();
}

//
// 192 kHz is not supported
//
void FirestudioMobile::FirestudioMobileEAP::setupSources_high() {
    printMessage("High (192 kHz) sample rate not supported by Firestudio Tube\n");
}

void FirestudioMobile::FirestudioMobileEAP::setupDestinations_high() {
    printMessage("High (192 kHz) sample rate not supported by Firestudio Tube\n");
}

/**
 * The default configuration for the Firestudio Mobile.
 * 82 destinations; each "group" every 32 registers
 **FIXME What follows is extracted from a listing of an existing router configuration.
 *       However, the origin of such a router configuration was unknown.
 */
void
FirestudioMobile::FirestudioMobileEAP::setupDefaultRouterConfig_low() {
    unsigned int i;
    // the 1394 stream receivers
    for (i=0; i<8; i++) {
        addRoute(eRS_InS0, i, eRD_ATX0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i+2, eRD_ATX0, i+8);
    }
    // Then 22 muted destinations
    for (i=0; i<22; i++) {
        addRoute(eRS_Muted, 0, eRD_Muted, 0);
    }
    
    // the Mixer inputs
    for (i=0; i<8; i++) {
        addRoute(eRS_InS0, i, eRD_Mixer0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i+2, eRD_Mixer0, i+8);
    }
    for (i=0; i<6; i++) {
        addRoute(eRS_ARX0, i, eRD_Mixer0, i+10);
    }
    // Then 16 muted destinations
    for (i=0; i<16; i++) {
        addRoute(eRS_Muted, 0, eRD_Muted, 0);
    }

    // The audio ports
    // Ensure that audio port are not muted
    for (i=0; i<4; i++) {
        addRoute(eRS_Mixer, i, eRD_InS0, i);
    }
    // The SPDIF ports
    for (i=0; i<2; i++) {
        addRoute(eRS_Mixer, i+4, eRD_AES, i+2);
    }
    // Then 12 muted destinations
    for (i=0; i<12; i++) {
        addRoute(eRS_Muted, 0, eRD_Muted, 0);
    }  
}

/**
 *  Identical to mid-rate
 */
void
FirestudioMobile::FirestudioMobileEAP::setupDefaultRouterConfig_mid() {
    setupDefaultRouterConfig_low();
}

/**
 *  High rate not supported
 */
void
FirestudioMobile::FirestudioMobileEAP::setupDefaultRouterConfig_high() {
    printMessage("High (192 kHz) sample rate not supported by Firestudio Mobile\n");
}


/**
  Device
*/
FirestudioMobile::FirestudioMobile( DeviceManager& d,
                                    std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Presonus::FirestudioMobile (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

FirestudioMobile::~FirestudioMobile()
{
    getEAP()->storeFlashConfig();
}

bool FirestudioMobile::discover() {
    if (Dice::Device::discover()) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Discovering Dice::Presonus::FirestudioMobile\n");
        return true;
    }
    return false;
}

void
FirestudioMobile::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Presonus::FirestudioMobile\n");
    Dice::Device::showDevice();
}

Dice::EAP* FirestudioMobile::createEAP() {
    return new FirestudioMobileEAP(*this);
}

}
}
// vim: et
