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

#include "firestudio_tube.h"

namespace Dice {
namespace Presonus {

//
// Firestudio Tube has
//  - 8 mic/line inputs
//  - 6 line inputs
//  - 2 "Tube" analogic inputs
//  - 8 ieee1394 inputs    **FIXME
//  - 18 mixer inputs
//
//  - 6 analogic line outputs
//  - 2 analogic headphone outputs
//  - 16 ieee1394 outputs
//  - 16 mixer outputs
//
void FirestudioTube::FirestudioTubeEAP::setupSources_low() {
    addSource("Mic/Lin/In", 0,  8, eRS_InS0, 1);
    addSource("Line/In", 8,  6, eRS_InS0, 9);
    addSource("Tube/In", 14,  2, eRS_InS0, 15);
    addSource("Mixer/Out",  0, 16, eRS_Mixer, 1);
    addSource("1394/In",   0, 8, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}

void FirestudioTube::FirestudioTubeEAP::setupDestinations_low() {
    addDestination("Line/Out", 0,  6, eRD_InS0, 1);
    addDestination("Head/Out", 6,  2, eRD_InS0, 7);
    addDestination("Mixer/In",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In",  0,  2, eRD_Mixer1, 17);
    addDestination("1394/Out",   0, 16, eRD_ATX0, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

//
// Independent of samplerate
void FirestudioTube::FirestudioTubeEAP::setupSources_mid() {
    setupSources_low();
}

void FirestudioTube::FirestudioTubeEAP::setupDestinations_mid() {
    setupDestinations_low();
}

//
// 192 kHz is not supported
//
void FirestudioTube::FirestudioTubeEAP::setupSources_high() {
    printMessage("High (192 kHz) sample rate not supported by Firestudio Tube\n");
}

void FirestudioTube::FirestudioTubeEAP::setupDestinations_high() {
    printMessage("High (192 kHz) sample rate not supported by Firestudio Tube\n");
}

/**
 * The default configurations for the Firestudio Tube.
 * 82 destinations; each "group" every 32 registers
 **FIXME What follows is extracted from a listing of an existing router configuration.
 *       However, the origin of such a router configuration was unknown.
 */
void
FirestudioTube::FirestudioTubeEAP::setupDefaultRouterConfig_low() {
    unsigned int i;
    // the 1394 stream receivers
    for (i=0; i<16; i++) {
        addRoute(eRS_InS0, i, eRD_ATX0, i);
    }
    // Then 16 muted destinations
    for (i=0; i<16; i++) {
        addRoute(eRS_Muted, 0, eRD_Muted, 0);
    }
    
    // the Mixer inputs
    for (i=0; i<16; i++) {
        addRoute(eRS_InS0, i, eRD_Mixer0, i);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX0, i, eRD_Mixer1, i);
    }
    // Then 14 muted destinations
    for (i=0; i<14; i++) {
        addRoute(eRS_Muted, 0, eRD_Muted, 0);
    }

    // The audio ports
    // Ensure that audio port are not muted
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX0, i, eRD_InS0, i);
    }
    // Then 10 muted destinations
    for (i=0; i<10; i++) {
        addRoute(eRS_Muted, 0, eRD_Muted, 0);
    }  
}

/**
 *  Identical to mid-rate
 */
void
FirestudioTube::FirestudioTubeEAP::setupDefaultRouterConfig_mid() {
    setupDefaultRouterConfig_low();
}

/**
 *  High rate not supported
 */
void
FirestudioTube::FirestudioTubeEAP::setupDefaultRouterConfig_high() {
    printMessage("High (192 kHz) sample rate not supported by Firestudio Tube\n");
}


/**
  Device
*/
FirestudioTube::FirestudioTube( DeviceManager& d,
                                    std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Presonus::FirestudioTube (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

FirestudioTube::~FirestudioTube()
{
    getEAP()->storeFlashConfig();
}

bool FirestudioTube::discover() {
    if (Dice::Device::discover()) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Discovering Dice::Presonus::FirestudioTube\n");
        return true;
    }
    return false;
}

void
FirestudioTube::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Presonus::FirestudioTube\n");
    Dice::Device::showDevice();
}

Dice::EAP* FirestudioTube::createEAP() {
    return new FirestudioTubeEAP(*this);
}

}
}
// vim: et
