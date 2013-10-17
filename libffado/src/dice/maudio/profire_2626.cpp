/*
 * Copyright (C) 2009 by Pieter Palmers
 * Copyright (C) 2012 by Philippe Carriere
 * Copyright (C) 2012 by Jano Svitok
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

#include "profire_2626.h"

namespace Dice {
namespace Maudio {

//
// Profire 2626 has
//  - 8 mic/line inputs
//  - 2x8 ADAT inputs
//  - 2 coaxial SPDIF inputs
//  - 32 ieee1394 inputs
//  - 18 mixer inputs
//  - 1 MIDI input
//
//  - 8 line outputs (first two pairs are routed to 2 headphone jacks
//  - 2x8 ADAT outputs
//  - 2 coaxial SPDIF outputs
//  - 32 ieee1394 outputs
//  - 16 mixer outputs
//  - 1 MIDI output
//
void Profire2626::Profire2626EAP::setupSources_low()
{
    addSource("Mic/Line/In", 0, 8, eRS_InS1, 1);
    addSource("ADAT A/In", 0, 8, eRS_ADAT, 1);
    addSource("ADAT B/In", 9, 8, eRS_ADAT, 1);
    addSource("SPDIF/In", 14, 2, eRS_AES, 1);
    addSource("Mixer/Out", 0, 16, eRS_Mixer, 1);
    addSource("1394/In", 0, 16, eRS_ARX0, 1);
    addSource("1394/In", 0, 10, eRS_ARX1, 17);
    addSource("Mute", 0, 1, eRS_Muted);
}

void Profire2626::Profire2626EAP::setupDestinations_low()
{
    addDestination("Line/Out", 0, 8, eRD_InS1, 1);
    addDestination("ADAT A/Out", 0, 8, eRD_ADAT, 1);
    addDestination("ADAT B/Out", 8, 8, eRD_ADAT, 1);
    addDestination("SPDIF/Out", 0, 2, eRD_AES, 1);
    addDestination("Mixer/In", 0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In", 0, 2, eRD_Mixer1, 17);
    addDestination("1394/Out", 0, 16, eRD_ATX0, 1);
    addDestination("1394/Out", 0, 10, eRD_ATX1, 1);
    addDestination("Mute", 0, 1, eRD_Muted);
}

void Profire2626::Profire2626EAP::setupSources_mid()
{
    addSource("Mic/Line/In", 0, 8, eRS_InS1, 1);
    addSource("ADAT A/In", 0, 4, eRS_ADAT, 1);
    addSource("ADAT B/In", 4, 4, eRS_ADAT, 1);
    addSource("SPDIF/In", 14, 2, eRS_AES, 1);
    addSource("Mixer/Out", 0, 16, eRS_Mixer, 1);
    addSource("1394/In", 0, 16, eRS_ARX0, 1);
    addSource("1394/In", 0, 10, eRS_ARX1, 17);
    addSource("Mute", 0, 1, eRS_Muted);
}

void Profire2626::Profire2626EAP::setupDestinations_mid()
{
    addDestination("Line/Out", 0, 8, eRD_InS1, 1);
    addDestination("ADAT A/Out", 0, 4, eRD_ADAT, 1);
    addDestination("ADAT B/Out", 4, 4, eRD_ADAT, 1);
    addDestination("SPDIF/Out", 0, 2, eRD_AES, 1);
    addDestination("Mixer/In", 0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In", 0, 2, eRD_Mixer1, 17);
    addDestination("1394/Out", 0, 16, eRD_ATX0, 1);
    addDestination("1394/Out", 0, 10, eRD_ATX1, 1);
    addDestination("Mute", 0, 1, eRD_Muted);
}

void Profire2626::Profire2626EAP::setupSources_high()
{
    addSource("Mic/Line/In", 0, 8, eRS_InS1, 1);
    addSource("ADAT A/In", 0, 2, eRS_ADAT, 1);
    addSource("ADAT B/In", 2, 2, eRS_ADAT, 1);
    addSource("SPDIF/In", 14, 2, eRS_AES, 1);
    addSource("Mixer/Out", 0, 16, eRS_Mixer, 1);
    addSource("1394/In", 0, 16, eRS_ARX0, 1);
    addSource("1394/In", 0, 10, eRS_ARX1, 17);
    addSource("Mute", 0, 1, eRS_Muted);
}

void Profire2626::Profire2626EAP::setupDestinations_high()
{
    addDestination("Line/Out", 0, 8, eRD_InS1, 1);
    addDestination("ADAT A/Out", 0, 2, eRD_ADAT, 1);
    addDestination("ADAT B/Out", 2, 2, eRD_ADAT, 1);
    addDestination("SPDIF/Out", 0, 2, eRD_AES, 1);
    addDestination("Mixer/In", 0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In", 0, 2, eRD_Mixer1, 17);
    addDestination("1394/Out", 0, 16, eRD_ATX0, 1);
    addDestination("1394/Out", 0, 10, eRD_ATX1, 1);
    addDestination("Mute", 0, 1, eRD_Muted);
}

/**
 * The default configurations for the Profire 2626.
 * 91 destinations
 * This tries to mimic default config from Windows control panel
 */
void Profire2626::Profire2626EAP::setupDefaultRouterConfig_low()
{
    unsigned int i;

    // ======== the 1394 stream receivers

    // LINE IN
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_ATX0, i);
    }

    // ADAT A
    for (i=0; i<8; i++) {
        addRoute(eRS_ADAT, i, eRD_ATX0, i+8);
    }

    // ADAT B
    for (i=0; i<8; i++) {
        addRoute(eRS_ADAT, i+8, eRD_ATX1, i);
    }

    // SPDIF
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX1, i+8);
    }


    // ======== Mixer inputs 

    // LINE IN
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_Mixer0, i);
    }

    // ADAT A
    for (i=0; i<8; i++) {
        addRoute(eRS_ADAT, i, eRD_Mixer0, i+8);
    }

    // SPDIF
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_Mixer1, i);
    }

    // ======== Outputs

    // LINE OUT
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX0, i, eRD_InS1, i+2);
    }

    // ADAT A
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX0, i+8, eRD_ADAT, i);
    }

    // ADAT B
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX1, i, eRD_ADAT, i+8);
    }

    // SPDIF
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX1, i+8, eRD_AES, i);
    }

    // Mixer is muted
    for (i=0; i<16; i++) {
        addRoute(eRS_Mixer, i, eRD_Muted, 0);
    }
}

void Profire2626::Profire2626EAP::setupDefaultRouterConfig_mid()
{
    unsigned int i;

    // ======== the 1394 stream receivers

    // LINE IN
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_ATX0, i);
    }

    // ADAT A
    for (i=0; i<4; i++) {
        addRoute(eRS_ADAT, i, eRD_ATX0, i+8);
    }

    // ADAT B
    for (i=0; i<4; i++) {
        addRoute(eRS_ADAT, i+4, eRD_ATX1, i);
    }

    // SPDIF
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX1, i+8);
    }


    // ======== Mixer inputs 

    // LINE IN
    for (i=0; i<8; i++) {
        addRoute(eRS_InS1, i, eRD_Mixer0, i);
    }

    // ADAT
    for (i=0; i<8; i++) {
        addRoute(eRS_ADAT, i, eRD_Mixer0, i+8);
    }

    // SPDIF
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_Mixer1, i);
    }

    // ======== Outputs

    // LINE OUT
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX0, i, eRD_InS1, i+2);
    }

    // ADAT A
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX0, i+8, eRD_ADAT, i);
    }

    // ADAT B
    for (i=0; i<8; i++) {
        addRoute(eRS_ARX1, i, eRD_ADAT, i+8);
    }

    // SPDIF
    for (i=0; i<2; i++) {
        addRoute(eRS_ARX1, i+8, eRD_AES, i);
    }

    // Mixer is muted
    for (i=0; i<16; i++) {
        addRoute(eRS_Mixer, i, eRD_Muted, 0);
    }
}

void Profire2626::Profire2626EAP::setupDefaultRouterConfig_high() 
{
    printMessage("Don't know how to handle High (192 kHz) sample rate for Profire2626\n");
}

/**
  Device
*/
Profire2626::Profire2626( DeviceManager& d, std::auto_ptr<ConfigRom>(configRom))
    : Dice::Device( d, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Maudio::Profire2626 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Profire2626::~Profire2626()
{
    getEAP()->storeFlashConfig();
}

bool Profire2626::discover() 
{
    if (Dice::Device::discover()) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Discovering Dice::Maudio::Profire2626\n");
        return true;
    }
    return false;
}

void Profire2626::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Maudio::Profire2626\n");
    Dice::Device::showDevice();
}

Dice::EAP* Profire2626::createEAP()
{
    return new Profire2626EAP(*this);
}

}
}
// vim: et
