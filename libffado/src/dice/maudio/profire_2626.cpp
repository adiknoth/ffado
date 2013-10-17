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

Profire2626::Profire2626EAP::Profire2626EAP(Dice::Device & dev) 
    : Dice::EAP(dev)
{
}

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
 * Application space register read/write
 */
bool Profire2626::Profire2626EAP::readApplicationReg(unsigned offset, quadlet_t* quadlet)
{
    bool ret = readReg(Dice::EAP::eRT_Application, offset, quadlet);
    return ret;
}

bool Profire2626::Profire2626EAP::writeApplicationReg(unsigned offset, quadlet_t quadlet)
{
    bool ret = writeReg(Dice::EAP::eRT_Application, offset, quadlet);
    if (!ret) {
        debugWarning("Couldn't write %i to register %x!\n", quadlet, offset);
        return false;
    }
    return ret;
}

/**
 * Switch class
 */
Profire2626::Profire2626EAP::Switch::Switch(
        Profire2626::Profire2626EAP* eap, std::string name,
        size_t offset, int activevalue)
    : Control::Boolean(eap, name)
    , m_eap(eap)
    , m_name(name)
    , m_offset(offset)
    , m_activevalue(activevalue)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create Switch %s)\n", m_name.c_str());
}

bool Profire2626::Profire2626EAP::Switch::selected()
{
    quadlet_t state_tmp;

    m_eap->readApplicationReg(m_offset, &state_tmp);
    bool is_selected = (state_tmp&m_activevalue)?true:false;
    return is_selected;
}

bool Profire2626::Profire2626EAP::Switch::select(bool n)
{
    quadlet_t state_tmp;
    m_eap->readApplicationReg(m_offset, &state_tmp);
    bool is_selected = (state_tmp&m_activevalue)?true:false;

    // Switch the corresponding bit(s)
    if ( n != is_selected ) {
        m_eap->writeApplicationReg(m_offset, state_tmp^m_activevalue);
        is_selected = n;
    }
    return is_selected;
}

/**
 *  Profire2626 Settings section
 */
Profire2626::Profire2626EAP::SettingsSection::SettingsSection(
        Profire2626::Profire2626EAP* eap, std::string name)
    : Control::Container(eap, name)
    , m_eap(eap)
{
    // Volume Knob
    Control::Container* grp_volumeknob = new Control::Container(m_eap, "VolumeKnob");
    addElement(grp_volumeknob);

    for (unsigned i=0; i<MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_SIZE; ++i) {
        std::stringstream stream;
        stream << "Line" << i*2+1 << "Line" << i*2+2;
        Profire2626EAP::Switch* outputPair =
            new Profire2626EAP::Switch(m_eap, stream.str(),
                                 MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_OFFSET,
                                 MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_VALUE
                                      <<(MAUDIO_PROFIRE2626_REGISTER_APP_VOLUME_KNOB_SHIFT+i));
        grp_volumeknob->addElement(outputPair);
    }
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

        Profire2626EAP* eap = dynamic_cast<Profire2626EAP*>(getEAP());
        Profire2626EAP::SettingsSection *settings = new Profire2626EAP::SettingsSection(eap, "Settings");
        eap->addElement(settings);

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
