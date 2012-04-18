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

#include "saffire_pro24.h"
#include "focusrite_eap.h"

namespace Dice {
namespace Focusrite {

int SaffirePro24::SaffirePro24EAP::commandToFix(unsigned offset) {
    if (offset<0x14) return 2;
    if (offset<0x3C && offset>=0x14) return 1;
    if (offset<0x58 && offset>=0x50) return 1;
    if (offset<0x40 && offset>=0x3C) return 3;
    if (offset<0x60 && offset>=0x58) return 4;
    return 0;
}
FocusriteEAP::Poti* SaffirePro24::SaffirePro24EAP::getMonitorPoti(std::string name) {
    return new FocusriteEAP::Poti(this, name, 0x50);
}
FocusriteEAP::Poti* SaffirePro24::SaffirePro24EAP::getDimPoti(std::string name) {
    return new FocusriteEAP::Poti(this, name, 0x54);
}

/*
void SaffirePro24::SaffirePro24EAP::setupSources() {
    addSource("SPDIF",  6,  2, eRS_AES);
    addSource("ADAT",   0,  8, eRS_ADAT);
    addSource("Analog", 0,  4, eRS_InS0);
    addSource("Mixer",  0, 16, eRS_Mixer);
    addSource("1394",   0,  8, eRS_ARX0);
    addSource("Mute",   0,  1, eRS_Muted);
}
void SaffirePro24::SaffirePro24EAP::setupDestinations() {
    addDestination("SPDIF",  6,  2, eRD_AES);
    addDestination("Analog", 0,  6, eRD_InS0);
    addDestination("Mixer",  0, 16, eRD_Mixer0);
    addDestination("Mixer",  0,  2, eRD_Mixer1, 16);
    addDestination("1394",   0, 16, eRD_ATX0);
    addDestination("Mute",   0,  1, eRD_Muted);
}
*/

void SaffirePro24::SaffirePro24EAP::setupSources_low() {
    addSource("SPDIF",  6,  2, eRS_AES, 1);
    addSource("ADAT",   0,  8, eRS_ADAT, 1);
    addSource("Analog", 0,  4, eRS_InS0, 1);
    addSource("Mixer",  0, 16, eRS_Mixer, 1);
    addSource("1394",   0,  8, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}
void SaffirePro24::SaffirePro24EAP::setupDestinations_low() {
    addDestination("SPDIF",  6,  2, eRD_AES, 1);
    addDestination("Analog", 0,  6, eRD_InS0, 1);
    addDestination("Mixer",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer",  0,  2, eRD_Mixer1, 17);
    addDestination("1394",   0, 16, eRD_ATX0, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

void SaffirePro24::SaffirePro24EAP::setupSources_mid() {
    addSource("SPDIF",  6,  2, eRS_AES, 1);
    addSource("ADAT",   0,  4, eRS_ADAT, 1);
    addSource("Analog", 0,  4, eRS_InS0, 1);
    addSource("Mixer",  0, 16, eRS_Mixer, 1);
    addSource("1394",   0,  8, eRS_ARX0, 1);
    addSource("Mute",   0,  1, eRS_Muted);
}
void SaffirePro24::SaffirePro24EAP::setupDestinations_mid() {
    addDestination("SPDIF",  6,  2, eRD_AES, 1);
    addDestination("Analog", 0,  6, eRD_InS0, 1);
    addDestination("Mixer",  0, 16, eRD_Mixer0, 1);
    addDestination("Mixer",  0,  2, eRD_Mixer1, 17);
    addDestination("1394",   0, 12, eRD_ATX0, 1);
// Is a Mute destination useful ?
//    addDestination("Mute",   0,  1, eRD_Muted);
}

void SaffirePro24::SaffirePro24EAP::setupSources_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 24\n");
}

void SaffirePro24::SaffirePro24EAP::setupDestinations_high() {
    printMessage("High (192 kHz) sample rate not supported by Saffire Pro 24\n");
}

class SaffirePro24::LineInstSwitch : public Dice::Focusrite::FocusriteEAP::Switch
{
public:
    LineInstSwitch(Dice::Focusrite::FocusriteEAP* eap, std::string name, size_t offset, int activevalue)
        : Dice::Focusrite::FocusriteEAP::Switch(eap, name, offset, activevalue) {}
    std::string getBooleanLabel(bool n) {
        if ( n ) return "Instrument";
        return "Line";
    }
};
class SaffirePro24::LevelSwitch : public Dice::Focusrite::FocusriteEAP::Switch
{
public:
    LevelSwitch(Dice::Focusrite::FocusriteEAP* eap, std::string name, size_t offset, int activevalue)
        : Dice::Focusrite::FocusriteEAP::Switch(eap, name, offset, activevalue) {}
    std::string getBooleanLabel(bool n) {
        if ( n ) return "High";
        return "Low";
    }
};

SaffirePro24::SaffirePro24( DeviceManager& d,
                            std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device(d , configRom)
    , m_ch1(NULL)
    , m_ch2(NULL)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro24 (NodeID %d)\n",
                getConfigRom().getNodeId());
}

SaffirePro24::~SaffirePro24()
{
    //debugOutput(DEBUG_LEVEL_VERBOSE, "Deleting the saffirePro24\n");
    /// I wonder whether we should really save only on clean exits or also each time a setting is
    //  changed. Or should we provide a function (and thus gui-button) to save the state of the
    //  device?
    getEAP()->storeFlashConfig();
    getEAP()->deleteElement(m_ch1);
    getEAP()->deleteElement(m_ch2);
    if (m_ch1) delete m_ch1;
    if (m_ch2) delete m_ch2;
}

bool SaffirePro24::discover() {
    if (Dice::Device::discover()) {
        fb_quadlet_t* tmp = (fb_quadlet_t *)calloc(2, sizeof(fb_quadlet_t));
        getEAP()->readRegBlock(Dice::EAP::eRT_Application, 0x00, tmp, 1*sizeof(fb_quadlet_t));
        //hexDumpQuadlets(tmp, 2); // DEBUG
        if (tmp[0] != 0x00010004 ) {
            debugError("This is a Focusrite Saffire Pro24 but not the right firmware. Better stop here before something goes wrong.\n");
            debugError("This device has firmware 0x%x while we only know about version 0x%x.\n", tmp[0], 0x10004);
            return false;
        }
        //getEAP()->readRegBlock(Dice::EAP::eRT_Command, 0x00, tmp, 2*sizeof(fb_quadlet_t)); // DEBUG
        //hexDumpQuadlets(tmp, 2); // DEBUG

        FocusriteEAP* eap = dynamic_cast<FocusriteEAP*>(getEAP());
        m_ch1 = new LineInstSwitch(eap, "Ch1LineInst", 0x58, 2);
        getEAP()->addElement(m_ch1);
        m_ch2 = new LineInstSwitch(eap, "Ch2LineInst", 0x58, 2<<16);
        getEAP()->addElement(m_ch2);
        m_ch3 = new LevelSwitch(eap, "Ch3Level", 0x5C, 1);
        getEAP()->addElement(m_ch3);
        m_ch4 = new LevelSwitch(eap, "Ch4Level", 0x5C, 1<<16);
        getEAP()->addElement(m_ch4);

        m_monitor = new FocusriteEAP::MonitorSection(eap, "Monitoring");
        getEAP()->addElement(m_monitor);
        return true;
    }
    return false;
}

void SaffirePro24::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Focusrite::SaffirePro24\n");
    Dice::Device::showDevice();
}
Dice::EAP* SaffirePro24::createEAP() {
    return new SaffirePro24EAP(*this);
}

bool SaffirePro24::setNickname( std::string name ) {
    return getEAP()->writeRegBlock( Dice::EAP::eRT_Application, 0x40, (fb_quadlet_t*)name.c_str(), name.size() );
}

std::string SaffirePro24::getNickname() {
    char name[16];
    getEAP()->readRegBlock( Dice::EAP::eRT_Application, 0x40, (fb_quadlet_t*)name, 16 );
    return std::string( name );
}

}
}

// vim: et
