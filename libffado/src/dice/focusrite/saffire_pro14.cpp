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

#include "libutil/ByteSwap.h"

namespace Dice {
namespace Focusrite {
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
void SaffirePro14::SaffirePro14EAP::setupSources_low() {
    addSource("SPDIF/In",  6,  2, eRS_AES, 1);
    addSource("Mic/Lin/Inst", 0,  2, eRS_InS0, 1);
    addSource("Mic/Lin/In", 2,  2, eRS_InS0, 3);
    addSource("Mixer/Out",  0, 16, eRS_Mixer, 1);
    addSource("1394/In",   0, 12, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}

void SaffirePro14::SaffirePro14EAP::setupDestinations_low() {
    addDestination("SPDIF/Out",  6,  2, eRD_AES, 1);
    addDestination("Line/Out", 0,  4, eRD_InS0, 1);
    addDestination("Mixer/In",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer/In",  0,  2, eRD_Mixer1, 17);
    addDestination("1394/Out",   0, 6, eRD_ATX0, 1);
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

/**
 * The default configurations for the Saffire Pro 14 router.
 *  For coherence with hardware, destinations must follow a specific ordering
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

/**
 *  Pro 14 Monitor section
 */ 
SaffirePro14::SaffirePro14EAP::MonitorSection::MonitorSection(Dice::Focusrite::FocusriteEAP* eap, 
    std::string name) : Control::Container(eap, name)
    , m_eap(eap)
{
    // Global Mute control
    Control::Container* grp_globalmute = new Control::Container(m_eap, "GlobalMute");
    addElement(grp_globalmute);
    FocusriteEAP::Switch* m_mute =
        new FocusriteEAP::Switch(m_eap, "State",
                                 SAFFIRE_PRO14_REGISTER_APP_GLOBAL_MUTE_SWITCH,
                                 FOCUSRITE_EAP_GLOBAL_MUTE_SWITCH_VALUE,
                                 SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_PRO14_MESSAGE_SET_GLOBAL_DIM_MUTE_SWITCH);
    grp_globalmute->addElement(m_mute);


    // Global Dim control
    Control::Container* grp_globaldim = new Control::Container(m_eap, "GlobalDim");
    addElement(grp_globaldim);
    FocusriteEAP::Switch* m_dim =
        new FocusriteEAP::Switch(m_eap, "State",
                                 SAFFIRE_PRO14_REGISTER_APP_GLOBAL_DIM_SWITCH,
                                 FOCUSRITE_EAP_GLOBAL_DIM_SWITCH_VALUE, 
                                 SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_PRO14_MESSAGE_SET_GLOBAL_DIM_MUTE_SWITCH);
    grp_globaldim->addElement(m_dim);
    FocusriteEAP::Poti* m_dimlevel =
        new FocusriteEAP::Poti(m_eap, "Level",
                               SAFFIRE_PRO14_REGISTER_APP_GLOBAL_DIM_VOLUME,
                               SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                               SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
    grp_globaldim->addElement(m_dimlevel);

    FocusriteEAP::Switch* s;
    // Mono/stereo switch
    Control::Container* grp_mono = new Control::Container(m_eap, "Mono");
    addElement(grp_mono);
    for (unsigned int i=0; i<SAFFIRE_PRO14_APP_STEREO_LINEOUT_SIZE; ++i) {
        std::stringstream stream;
        stream << "Line" << i*2+1 << "Line" << i*2+2;
        s = 
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                      <<(FOCUSRITE_EAP_SWITCH_CONTROL_MONO_SHIFT+i),
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_mono->addElement(s);
    }

    // Independent control of each line/out
    Control::Container* grp_perchannel = new Control::Container(m_eap, "LineOut");
    addElement(grp_perchannel);
    FocusriteEAP::VolumeControl* vol;

    // per Line/Out monitoring
    for (unsigned int i=0; i<SAFFIRE_PRO14_APP_STEREO_LINEOUT_SIZE; ++i) {
        std::stringstream stream;

        // Activate/Unactivate per Line/Out volume monitoring
        stream.str(std::string());
        stream << "UnActivate" << i*2+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_1, 
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);
        stream.str(std::string());
        stream << "UnActivate" << i*2+2;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_2,
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);

        // per Line/Out mute/unmute
        stream.str(std::string());
        stream << "Mute" << i*2+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_3, 
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);
        stream.str(std::string());
        stream << "Mute" << i*2+2;
        s = 
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_MONITOR_SWITCH+i*sizeof(quadlet_t),
                                   FOCUSRITE_EAP_SWITCH_BIT_4, 
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(s);

        // per Line/Out global mute activation/unactivation
        stream.str(std::string());
        stream << "GMute" << 2*i+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_MUTE_SHIFT+2*i),
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        stream.str(std::string());
        stream << "GMute" << 2*i+2;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(),
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_MUTE_SHIFT+2*i+1),
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        // per Line/Out global dim activation/unactivation
        stream.str(std::string());
        stream << "GDim" << 2*i+1;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_DIM_SHIFT+2*i),
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        stream.str(std::string());
        stream << "GDim" << 2*i+2;
        s =
          new FocusriteEAP::Switch(m_eap, stream.str(), 
                                   SAFFIRE_PRO14_REGISTER_APP_LINEOUT_SWITCH_CONTROL,
                                   FOCUSRITE_EAP_SWITCH_CONTROL_VALUE
                                        <<(FOCUSRITE_EAP_SWITCH_CONTROL_DIM_SHIFT+2*i+1),
                                   SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                   SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_SWITCH_CONTROL);
        grp_perchannel->addElement(s);

        // per Line/Out volume control
        stream.str(std::string());
        stream << "Volume" << i*2+1;
        vol =
          new FocusriteEAP::VolumeControl(m_eap, stream.str(),
                                          SAFFIRE_PRO14_REGISTER_APP_LINEOUT_MONITOR_VOLUME
                                              +i*sizeof(quadlet_t),
                                          FOCUSRITE_EAP_LINEOUT_VOLUME_SET_1,
                                          SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                          SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(vol);
        stream.str(std::string());
        stream << "Volume" << i*2+2;
        vol =
          new FocusriteEAP::VolumeControl(m_eap, stream.str(), 
                                          SAFFIRE_PRO14_REGISTER_APP_LINEOUT_MONITOR_VOLUME
                                              +i*sizeof(quadlet_t),
                                          FOCUSRITE_EAP_LINEOUT_VOLUME_SET_2,
                                          SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                          SAFFIRE_PRO14_MESSAGE_SET_LINEOUT_MONITOR_VOLUME);
        grp_perchannel->addElement(vol);
    }

    // Line/Inst and Hi/Lo Gain switches
    Control::Container* grp_LineInstGain = new Control::Container(m_eap, "LineInstGain");
    addElement(grp_LineInstGain);
    FocusriteEAP::Switch* m_lineinst =
        new FocusriteEAP::Switch(m_eap, "LineInst1",
                                 SAFFIRE_PRO14_REGISTER_APP_LINEOUT_INST_SWITCH,
                                 SAFFIRE_PRO14_LINEOUT_SWITCH_INST_VALUE,
                                 SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_PRO14_MESSAGE_SET_INSTLINE);
    grp_LineInstGain->addElement(m_lineinst);
    m_lineinst =
        new FocusriteEAP::Switch(m_eap, "LineInst2",
                                 SAFFIRE_PRO14_REGISTER_APP_LINEOUT_INST_SWITCH,
                                 SAFFIRE_PRO14_LINEOUT_SWITCH_INST_VALUE
                                    <<FOCUSRITE_EAP_LINEOUT_SWITCH_INST_SHIFT,
                                 SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_PRO14_MESSAGE_SET_INSTLINE);
    grp_LineInstGain->addElement(m_lineinst);
    m_lineinst =
        new FocusriteEAP::Switch(m_eap, "LineGain3",
                                 SAFFIRE_PRO14_REGISTER_APP_LINEOUT_GAIN_SWITCH,
                                 SAFFIRE_PRO14_LINEOUT_SWITCH_GAIN_VALUE,
                                 SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_PRO14_MESSAGE_SET_INSTLINE);
    grp_LineInstGain->addElement(m_lineinst);
    m_lineinst =
        new FocusriteEAP::Switch(m_eap, "LineGain4",
                                 SAFFIRE_PRO14_REGISTER_APP_LINEOUT_GAIN_SWITCH,
                                 SAFFIRE_PRO14_LINEOUT_SWITCH_GAIN_VALUE
                                    <<FOCUSRITE_EAP_LINEOUT_SWITCH_GAIN_SHIFT,
                                 SAFFIRE_PRO14_REGISTER_APP_MESSAGE_SET,
                                 SAFFIRE_PRO14_MESSAGE_SET_INSTLINE);
    grp_LineInstGain->addElement(m_lineinst);
}

/**
  Device
*/
SaffirePro14::SaffirePro14( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro14 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

SaffirePro14::~SaffirePro14()
{
    getEAP()->storeFlashConfig();
}

bool SaffirePro14::discover() {
    if (Dice::Device::discover()) {
        FocusriteEAP* eap = dynamic_cast<FocusriteEAP*>(getEAP());
        m_monitor = new SaffirePro14EAP::MonitorSection(eap, "Monitoring");
        eap->addElement(m_monitor);
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
    char nickname[SAFFIRE_PRO14_APP_NICK_NAME_SIZE+1];

    // The device has room for SAFFIRE_PRO14_APP_NICK_NAME_SIZE characters.
    // Erase supplementary characters or fill-in with NULL character if necessary
    strncpy(nickname, name.c_str(), SAFFIRE_PRO14_APP_NICK_NAME_SIZE);

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)nickname, SAFFIRE_PRO14_APP_NICK_NAME_SIZE/4);
    #endif

    if (!getEAP()->writeRegBlock(Dice::EAP::eRT_Application, SAFFIRE_PRO14_REGISTER_APP_NICK_NAME, 
                                 (quadlet_t*)nickname, SAFFIRE_PRO14_APP_NICK_NAME_SIZE)) {
        debugError("Could not write nickname string \n");
        return false;
    }
    return true;
}
std::string SaffirePro14::getNickname() {
    char nickname[SAFFIRE_PRO14_APP_NICK_NAME_SIZE+1];
    if (!getEAP()->readRegBlock(Dice::EAP::eRT_Application, SAFFIRE_PRO14_REGISTER_APP_NICK_NAME, 
                                (quadlet_t*)nickname, SAFFIRE_PRO14_APP_NICK_NAME_SIZE)){
        debugError("Could not read nickname string \n");
        return std::string("(unknown)");
    }

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)nickname, SAFFIRE_PRO14_APP_NICK_NAME_SIZE/4);
    #endif

    // The device supplies at most SAFFIRE_PRO14_APP_NICK_NAME_SIZE characters.  Ensure the string is
    // NULL terminated.
    nickname[SAFFIRE_PRO14_APP_NICK_NAME_SIZE] = 0;
    return std::string(nickname);
}

}
}
// vim: et
