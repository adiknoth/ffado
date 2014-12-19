/*
 * Copyright (C) 2009 by Pieter Palmers
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

#include "saffire_56.h"

#include "focusrite_eap.h"

#include "libutil/ByteSwap.h"

namespace Dice {
namespace Focusrite {

// ADAT as SPDIF state
bool Saffire56::Saffire56EAP::getADATSPDIF_state() {
    quadlet_t state_tmp;
    bool adatspdif = false;

    if (!readReg(Dice::EAP::eRT_Application, SAFFIRE_56_REGISTER_APP_ADATSPDIF_SWITCH_CONTROL,
                 &state_tmp)) {
        debugWarning("Could not read ADAT/SPDIF switch register: assume ADAT \n");
    }
    else {
        adatspdif = (state_tmp&FOCUSRITE_EAP_ADATSPDIF_SWITCH_VALUE)?true:false;
    }
    return adatspdif;
}

//
// Under 48kHz Saffire 56 has
//  - 8 analogic inputs (mic/line)
//  - 2x8 ADAT inputs or 8 ADAT and 2 additional optical SPDIF inputs
//  - 2 SPDIF inputs
//  - 28 ieee1394 inputs
//  - 18 mixer inputs
//
//  - 10 analogic outputs
//  - 2x8 ADAT outputs or 4 ADAT and ? SPDIF outputs
//  - 2 SPDIF outputs
//  - 28 ieee1394 outputs
//  - 16 mixer outputs
//
void Saffire56::Saffire56EAP::setupSources_low() {
    bool adatspdif = getADATSPDIF_state();

    addSource("SPDIF/In",  0,  2, eRS_AES, 1);
    if (!adatspdif) {
      addSource("ADAT/In",   0, 16, eRS_ADAT, 1);
    } else {
      addSource("ADAT/In",   0, 8, eRS_ADAT, 1);
      // FIXME: there is two additional SPDIF, location unknown
      addSource("SPDIF/In",  4,  2, eRS_AES, 3);
    }
    addSource("Mic/Lin/Inst", 0,  2, eRS_InS0, 1);
    addSource("Mic/Lin/In", 2,  6, eRS_InS1, 3);
    addSource("Mixer/Out",  0, 16, eRS_Mixer, 1);
    addSource("1394/In",   0, 16, eRS_ARX0, 1);
    addSource("1394/In",   0, 12, eRS_ARX1, 17);
    addSource("Mute",   0,  1, eRS_Muted);
}

void Saffire56::Saffire56EAP::setupDestinations_low() {
    bool adatspdif = getADATSPDIF_state();

    addDestination("SPDIF/Out",  0,  2, eRD_AES, 1);
    if (!adatspdif) {
      addDestination("ADAT/Out",   0,  16, eRD_ADAT, 1);
    }
    addDestination("Line/Out", 0,  2, eRD_InS0, 1);
    addDestination("Line/Out", 0,  8, eRD_InS1, 3);
    addDestination("Mixer/In",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In",  0,  2, eRD_Mixer1, 17);
    addDestination("1394/Out",   0, 16, eRD_ATX0, 1);
    addDestination("1394/Out",   0,  10, eRD_ATX1, 17);
    addDestination("Loop",   10,  2, eRD_ATX1, 27);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

//
// Under 96kHz Saffire 56 has
//  - 8 analogic inputs (mic/line)
//  - 2x4 ADAT inputs or 4 ADAT and 2 additional SPDIF inputs
//  - 2 SPDIF inputs
//  - 20 ieee1394 inputs
//  - 18 mixer inputs
//
//  - 10 analogic outputs
//  - 2x4 ADAT outputs
//  - 2 SPDIF outputs
//  - 20 ieee1394 outputs
//  - 16 mixer outputs
//
void Saffire56::Saffire56EAP::setupSources_mid() {
    bool adatspdif = getADATSPDIF_state();

    addSource("SPDIF/In",  0,  2, eRS_AES, 1);
    if (!adatspdif) {
      addSource("ADAT/In",   0,  8, eRS_ADAT, 1);
    } else {
      addSource("ADAT/In",   0,  4, eRS_ADAT, 1);
      // FIXME: there is two additional SPDIF, location unknown
      addSource("SPDIF/In",  4,  2, eRS_AES, 3);
    }
    addSource("Mic/Lin/Inst", 0,  2, eRS_InS0, 1);
    addSource("Mic/Lin/In", 2,  6, eRS_InS1, 3);
    addSource("Mixer/Out",  0, 16, eRS_Mixer, 1);
    addSource("1394/In",   0, 16, eRS_ARX0, 1);
    addSource("1394/In",   0, 4, eRS_ARX1, 17);
    addSource("Mute",   0,  1, eRS_Muted);
}

void Saffire56::Saffire56EAP::setupDestinations_mid() {
    bool adatspdif = getADATSPDIF_state();

    addDestination("SPDIF/Out",  0,  2, eRD_AES, 1);
    if (!adatspdif) {
      addDestination("ADAT/Out",   0,  4, eRD_ADAT, 1);
    }
    addDestination("Line/Out", 0,  2, eRD_InS0, 1);
    addDestination("Line/Out", 0,  8, eRD_InS1, 3);
    addDestination("Mixer/In",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In",  0,  2, eRD_Mixer1, 17);
    addDestination("1394/Out",   0, 16, eRD_ATX0, 1);
    addDestination("1394/Out",   0, 2, eRD_ATX1, 17);
    addDestination("Loop",   2, 2, eRD_ATX1, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

//
// 192 kHz is not supported
//
void Saffire56::Saffire56EAP::setupSources_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 40\n");
}

void Saffire56::Saffire56EAP::setupDestinations_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 40\n");
}

/**
 * The default configurations for the Saffire 56 router.
 *  For coherence with hardware, destinations must follow a specific ordering
 *  There must be 76 destinations at low samplerate
 *  Front LEDs are connected to the first eight router entries
 */
void
Saffire56::Saffire56EAP::setupDefaultRouterConfig_low() {
    unsigned int i;
    // the 1394 stream receivers except the two "loops" one
    for (i=0; i<2; i++) {
        addRoute(eRS_InS0, i, eRD_ATX0, i);
    }
    for (i=0; i<6; i++) {
        addRoute(eRS_InS1, i+2, eRD_ATX0, i+2);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX0, i+8);
    }
    for (i=0; i<6; i++) {
        addRoute(eRS_ADAT, i, eRD_ATX0, i+10);
    }
    for (i=0; i<10; i++) {
        addRoute(eRS_ADAT, i+6, eRD_ATX1, i);
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
    for (i=0; i<16; i++) {
        addRoute(eRS_Muted, 0, eRD_ADAT, i);
    }
    // the "loop" 1394 stream receivers
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_ATX1, i+10);
    }
    // the Mixer inputs
    for (i=0; i<2; i++) {
        addRoute(eRS_InS0, i, eRD_Mixer0, i);
    }
    for (i=0; i<6; i++) {
        addRoute(eRS_InS1, i+2, eRD_Mixer0, i+2);
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
 *  There must be 60 destinations at mid samplerate
 *  Front LEDs are connected to the first eight router entries
 */
void
Saffire56::Saffire56EAP::setupDefaultRouterConfig_mid() {
    unsigned int i;
    // the 1394 stream receivers except the two "loop" ones
    for (i=0; i<2; i++) {
        addRoute(eRS_InS0, i, eRD_ATX0, i);
    }
    for (i=0; i<6; i++) {
        addRoute(eRS_InS1, i+2, eRD_ATX0, i+2);
    }
    for (i=0; i<2; i++) {
        addRoute(eRS_AES, i, eRD_ATX0, i+8);
    }
    for (i=0; i<4; i++) {
        addRoute(eRS_ADAT, i, eRD_ATX0, i+10);
    }
    for (i=0; i<4; i++) {
        addRoute(eRS_ADAT, i+4, eRD_ATX1, i);
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
    // the "loop" 1394 stream receivers
    for (i=0; i<2; i++) {
        addRoute(eRS_Muted, 0, eRD_ATX1, i+4);
    }
    // the Mixer inputs
    for (i=0; i<2; i++) {
        addRoute(eRS_InS0, i, eRD_Mixer0, i);
    }
    for (i=0; i<6; i++) {
        addRoute(eRS_InS1, i+2, eRD_Mixer0, i+2);
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
 *  High rate not supported
 */
void
Saffire56::Saffire56EAP::setupDefaultRouterConfig_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 40\n");
}

/**
 *  Pro 56 Monitor section
 *  FIXME Must be thoroughly tested and registers location checked before enabling
 */ 
// Subclassed switch
Saffire56::Saffire56EAP::Switch::Switch(Dice::Focusrite::FocusriteEAP* eap, std::string name,
    size_t offset, int activevalue, size_t msgset_offset, int msgset_value)
    : FocusriteEAP::Switch(eap, name, offset, activevalue, msgset_offset, msgset_value)
    , m_eap(eap)
    , m_name(name)
    , m_offset(offset)
    , m_activevalue(activevalue)
    , m_msgset_offset(msgset_offset)
    , m_msgset_value(msgset_value)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Create Saffire 56 Switch %s)\n", m_name.c_str());    
}

bool Saffire56::Saffire56EAP::Switch::select(bool n) {
    bool is_selected = FocusriteEAP::Switch::select(n);
    m_eap->update();
    return is_selected;
}

Saffire56::Saffire56EAP::MonitorSection::MonitorSection(Dice::Focusrite::FocusriteEAP* eap, 
    std::string name) : Control::Container(eap, name)
    , m_eap(eap)
{
    // Global Mute control
    Control::Container* grp_globalmute = new Control::Container(m_eap, "GlobalMute");
    addElement(grp_globalmute);
    FocusriteEAP::Switch* mute =
        new FocusriteEAP::Switch(m_eap, "State",
                                 SAFFIRE_56_REGISTER_APP_GLOBAL_MUTE_SWITCH,
                                 FOCUSRITE_EAP_GLOBAL_MUTE_SWITCH_VALUE,
                                 SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_56_MESSAGE_SET_GLOBAL_DIM_MUTE_SWITCH);
    grp_globalmute->addElement(mute);

    // ADAT as optical SPDIF switch control
    Control::Container* grp_adatspdif = new Control::Container(m_eap, "AdatSpdif");
    addElement(grp_adatspdif);
    Saffire56::Saffire56EAP::Switch* adatspdif =
        new Saffire56::Saffire56EAP::Switch(
                                 m_eap, "State",
                                 SAFFIRE_56_REGISTER_APP_ADATSPDIF_SWITCH_CONTROL,
                                 FOCUSRITE_EAP_ADATSPDIF_SWITCH_VALUE,
                                 SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_56_MESSAGE_SET_INSTLINE);
    grp_adatspdif->addElement(adatspdif);

    // Global Dim control
    Control::Container* grp_globaldim = new Control::Container(m_eap, "GlobalDim");
    addElement(grp_globaldim);
    FocusriteEAP::Switch* dim =
        new FocusriteEAP::Switch(m_eap, "State",
                                 SAFFIRE_56_REGISTER_APP_GLOBAL_DIM_SWITCH,
                                 FOCUSRITE_EAP_GLOBAL_DIM_SWITCH_VALUE, 
                                 SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_56_MESSAGE_SET_GLOBAL_DIM_MUTE_SWITCH);
    grp_globaldim->addElement(dim);
    FocusriteEAP::Poti* dimlevel =
        new FocusriteEAP::Poti(m_eap, "Level",
                               SAFFIRE_56_REGISTER_APP_GLOBAL_DIM_VOLUME,
                               SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                               SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
    grp_globaldim->addElement(dimlevel);

    FocusriteEAP::Switch* s;
    // Mono/stereo switch
    Control::Container* grp_mono = new Control::Container(m_eap, "Mono");
    addElement(grp_mono);
    for (unsigned int i=0; i<SAFFIRE_56_APP_STEREO_LINEOUT_SIZE; ++i) {
        std::stringstream stream;
        stream << "Line" << i*2+1 << "Line" << i*2+2;
        s = 
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                      <<(FOCUSRITE_EAP_SWITCH_CONTROL_MONO_SHIFT+i),
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_mono->addElement(s);
    }

    // Independent control of each line/out
    Control::Container* grp_perchannel = new Control::Container(m_eap, "LineOut");
    addElement(grp_perchannel);
    FocusriteEAP::VolumeControl* vol;

    // per Line/Out monitoring
    for (unsigned int i=0; i<SAFFIRE_56_APP_STEREO_LINEOUT_SIZE; ++i) {
        std::stringstream stream;

        // Activate/Unactivate per Line/Out volume monitoring
        stream.str(std::string());
        stream << "UnActivate" << i*2+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_1, 
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);
        stream.str(std::string());
        stream << "UnActivate" << i*2+2;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_2,
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);

        // per Line/Out mute/unmute
        stream.str(std::string());
        stream << "Mute" << i*2+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_3, 
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);
        stream.str(std::string());
        stream << "Mute" << i*2+2;
        s = 
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_4, 
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);

        // per Line/Out global mute activation/unactivation
        stream.str(std::string());
        stream << "GMute" << 2*i+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_MUTE_SHIFT+2*i),
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        stream.str(std::string());
        stream << "GMute" << 2*i+2;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_MUTE_SHIFT+2*i+1),
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        // per Line/Out global dim activation/unactivation
        stream.str(std::string());
        stream << "GDim" << 2*i+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_DIM_SHIFT+2*i),
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        stream.str(std::string());
        stream << "GDim" << 2*i+2;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_56_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_DIM_SHIFT+2*i+1),
                                   SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_56_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        // per Line/Out volume control
        stream.str(std::string());
        stream << "Volume" << i*2+1;
        vol =
          new FocusriteEAP::VolumeControl(m_eap, stream.str(),
                                          SAFFIRE_56_REGISTER_APP_LINEOUT_MONITOR_VOLUME
                                              +i*sizeof(quadlet_t),
                                          FOCUSRITE_EAP_LINEOUT_VOLUME_SET_1,
                                          SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                          SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(vol);
        stream.str(std::string());
        stream << "Volume" << i*2+2;
        vol =
          new FocusriteEAP::VolumeControl(m_eap, stream.str(), 
                                          SAFFIRE_56_REGISTER_APP_LINEOUT_MONITOR_VOLUME
                                              +i*sizeof(quadlet_t),
                                          FOCUSRITE_EAP_LINEOUT_VOLUME_SET_2,
                                          SAFFIRE_56_REGISTER_APP_MESSAGE_SET,
                                          SAFFIRE_56_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(vol);
    }
}

/**
  Device
*/
Saffire56::Saffire56( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::Saffire56 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Saffire56::~Saffire56()
{
    getEAP()->storeFlashConfig();
}

bool Saffire56::discover() {
    if (Dice::Device::discover()) {
        FocusriteEAP* eap = dynamic_cast<FocusriteEAP*>(getEAP());
        Saffire56EAP::MonitorSection* monitor = new Saffire56EAP::MonitorSection(eap, "Monitoring");
        eap->addElement(monitor);
        return true;
    }
    return false;
}

void
Saffire56::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Focusrite::Saffire56\n");
    Dice::Device::showDevice();
}

Dice::EAP* Saffire56::createEAP() {
    return new Saffire56EAP(*this);
}

/**
 *  Nickname
 */
bool Saffire56::setNickname(std::string name) {
    char nickname[SAFFIRE_56_APP_NICK_NAME_SIZE+1];

    // The device has room for SAFFIRE_56_APP_NICK_NAME_SIZE characters.
    // Erase supplementary characters or fill-in with NULL character if necessary
    strncpy(nickname, name.c_str(), SAFFIRE_56_APP_NICK_NAME_SIZE);

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)nickname, SAFFIRE_56_APP_NICK_NAME_SIZE/4);
    #endif

    if (!getEAP()->writeRegBlock(Dice::EAP::eRT_Application, SAFFIRE_56_REGISTER_APP_NICK_NAME, 
                                 (quadlet_t*)nickname, SAFFIRE_56_APP_NICK_NAME_SIZE)) {
        debugError("Could not write nickname string \n");
        return false;
    }
    return true;
}
std::string Saffire56::getNickname() {
    char nickname[SAFFIRE_56_APP_NICK_NAME_SIZE+1];
    if (!getEAP()->readRegBlock(Dice::EAP::eRT_Application, SAFFIRE_56_REGISTER_APP_NICK_NAME, 
                                (quadlet_t*)nickname, SAFFIRE_56_APP_NICK_NAME_SIZE)){
        debugError("Could not read nickname string \n");
        return std::string("(unknown)");
    }

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)nickname, SAFFIRE_56_APP_NICK_NAME_SIZE/4);
    #endif

    // The device supplies at most SAFFIRE_56_APP_NICK_NAME_SIZE characters.  Ensure the string is
    // NULL terminated.
    nickname[SAFFIRE_56_APP_NICK_NAME_SIZE] = 0;
    return std::string(nickname);
}

}
}
// vim: et
