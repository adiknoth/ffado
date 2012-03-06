/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "config.h"

#include "dice/dice_avdevice.h"
#include "dice/dice_defines.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include "libutil/ByteSwap.h"
#include <libraw1394/csr.h>

#include <stdint.h>
#include <iostream>
#include <sstream>

#include <string>
#include <cstring>
#include <assert.h>

#include "libutil/Configuration.h"
#include "devicemanager.h"

#include "focusrite/saffire_pro40.h"
#include "focusrite/saffire_pro24.h"

using namespace std;

namespace Dice {

Device::Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_eap( NULL )
    , m_global_reg_offset (0xFFFFFFFFLU)
    , m_global_reg_size (0xFFFFFFFFLU)
    , m_tx_reg_offset (0xFFFFFFFFLU)
    , m_tx_reg_size (0xFFFFFFFFLU)
    , m_rx_reg_offset (0xFFFFFFFFLU)
    , m_rx_reg_size (0xFFFFFFFFLU)
    , m_unused1_reg_offset (0xFFFFFFFFLU)
    , m_unused1_reg_size (0xFFFFFFFFLU)
    , m_unused2_reg_offset (0xFFFFFFFFLU)
    , m_unused2_reg_size (0xFFFFFFFFLU)
    , m_nb_tx (0xFFFFFFFFLU)
    , m_tx_size (0xFFFFFFFFLU)
    , m_nb_rx (0xFFFFFFFFLU)
    , m_rx_size (0xFFFFFFFFLU)
    , m_notifier (NULL)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    addOption(Util::OptionContainer::Option("snoopMode", false));
}

Device::~Device()
{
    for ( StreamProcessorVectorIterator it = m_receiveProcessors.begin();
          it != m_receiveProcessors.end();
          ++it )
    {
        delete *it;
    }
    for ( StreamProcessorVectorIterator it = m_transmitProcessors.begin();
          it != m_transmitProcessors.end();
          ++it )
    {
        delete *it;
    }

    if (m_notifier) {
        unlock();
    }

    if(m_eap) {
        delete m_eap;
    }

}

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if (generic) {
        return false;
    } else {
        // check if device is in supported devices list
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();

        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_DICE;
    }
}

FFADODevice *
Device::createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();
    unsigned int modelId = configRom->getModelId();

    switch (vendorId) {
        case FW_VENDORID_FOCUSRITE:
            switch(modelId) {
                case 0x00000005:
                    return new Focusrite::SaffirePro40(d, configRom);
                case 0x00000007:
                case 0x00000008:
                    return new Focusrite::SaffirePro24(d, configRom);
                default: // return a plain Dice device
                    return new Device(d, configRom);
           }
        default:
            return new Device(d, configRom);
    }
    return NULL;
}

bool
Device::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_DICE) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Using generic DICE support for unsupported device '%s %s'\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }

    if ( !initIoFunctions() ) {
        debugError("Could not initialize I/O functions\n");
        return false;
    }

    m_eap = createEAP();
    if(m_eap == NULL) {
        debugError("Failed to allocate EAP.\n");
        return false;
    }
    if(!m_eap->init()) {
        debugWarning("Could not init EAP\n");
        delete m_eap;
        m_eap = NULL;
    } else {
        // register the EAP controls to the control structure
        if(!addElement(m_eap)) {
            debugError("Failed to add the EAP controls to the control tree\n");
            return false;
        }
    }
    return true;
}

EAP*
Device::createEAP() {
    return new EAP(*this);
}

enum Device::eDiceConfig
Device::getCurrentConfig()
{
    int samplerate = getSamplingFrequency();
    if(samplerate > 31999 && samplerate <= 48000) {
        return eDC_Low;
    }
    if(samplerate > 48000 && samplerate <= 96000) {
        return eDC_Mid;
    }
    if(samplerate > 96000 && samplerate <= 192000) {
        return eDC_High;
    }
    return eDC_Unknown;
}

int
Device::getSamplingFrequency() {
    int samplingFrequency;

    fb_quadlet_t clockreg;
    if (!readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clockreg)) {
        debugError("Could not read CLOCK_SELECT register\n");
        return false;
    }

    clockreg = DICE_GET_RATE(clockreg);

    switch (clockreg) {
        case DICE_RATE_32K:      samplingFrequency = 32000;  break;
        case DICE_RATE_44K1:     samplingFrequency = 44100;  break;
        case DICE_RATE_48K:      samplingFrequency = 48000;  break;
        case DICE_RATE_88K2:     samplingFrequency = 88200;  break;
        case DICE_RATE_96K:      samplingFrequency = 96000;  break;
        case DICE_RATE_176K4:    samplingFrequency = 176400; break;
        case DICE_RATE_192K:     samplingFrequency = 192000; break;
        case DICE_RATE_ANY_LOW:  samplingFrequency = 0;   break;
        case DICE_RATE_ANY_MID:  samplingFrequency = 0;   break;
        case DICE_RATE_ANY_HIGH: samplingFrequency = 0;  break;
        case DICE_RATE_NONE:     samplingFrequency = 0;     break;
        default:                 samplingFrequency = 0; break;
    }

    return samplingFrequency;
}

#define DICE_CHECK_AND_ADD_SR(v, x, reg) \
    { if(maskedCheckNotZeroGlobalReg( DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, reg)) \
       v.push_back(x); }
std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    DICE_CHECK_AND_ADD_SR(frequencies, 32000, DICE_CLOCKCAP_RATE_32K);
    DICE_CHECK_AND_ADD_SR(frequencies, 44100, DICE_CLOCKCAP_RATE_44K1);
    DICE_CHECK_AND_ADD_SR(frequencies, 48000, DICE_CLOCKCAP_RATE_48K);
    DICE_CHECK_AND_ADD_SR(frequencies, 88200, DICE_CLOCKCAP_RATE_88K2);
    DICE_CHECK_AND_ADD_SR(frequencies, 96000, DICE_CLOCKCAP_RATE_96K);
    DICE_CHECK_AND_ADD_SR(frequencies, 176400, DICE_CLOCKCAP_RATE_176K4);
    DICE_CHECK_AND_ADD_SR(frequencies, 192000, DICE_CLOCKCAP_RATE_192K);
    return frequencies;
}

int
Device::getConfigurationId()
{
    return 0;
}

bool
Device::setSamplingFrequency( int samplingFrequency )
{
    printMessage("Setting sample rate: %d\n", (samplingFrequency));

    bool supported=false;
    fb_quadlet_t select=0x0;

    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defaulting to false\n");
    }

    if(snoopMode) {
        int current_sr = getSamplingFrequency();
        if (current_sr != samplingFrequency ) {
            debugError("In snoop mode it is impossible to set the sample rate.\n");
            debugError("Please start the client with the correct setting.\n");
            return false;
        }
        return true;
    } else {
        switch ( samplingFrequency ) {
        default:
        case 22050:
        case 24000:
            supported=false;
            break;
        case 32000:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_32K);
            select=DICE_RATE_32K;
            break;
        case 44100:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_44K1);
            select=DICE_RATE_44K1;
            break;
        case 48000:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_48K);
            select=DICE_RATE_48K;
            break;
        case 88200:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_88K2);
            select=DICE_RATE_88K2;
            break;
        case 96000:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_96K);
            select=DICE_RATE_96K;
            break;
        case 176400:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_176K4);
            select=DICE_RATE_176K4;
            break;
        case 192000:
            supported=maskedCheckNotZeroGlobalReg(
                        DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES,
                        DICE_CLOCKCAP_RATE_192K);
            select=DICE_RATE_192K;
            break;
        }
    
        if (!supported) {
            debugWarning("Unsupported sample rate: %d\n", (samplingFrequency));
            return false;
        }
    
        if (isIsoStreamingEnabled()) {
            debugError("Cannot change samplerate while streaming is enabled\n");
            return false;
        }
    
        fb_quadlet_t clockreg;
        if (!readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clockreg)) {
            debugError("Could not read CLOCK_SELECT register\n");
            return false;
        }
    
        clockreg = DICE_SET_RATE(clockreg, select);
    
        if (!writeGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, clockreg)) {
            debugError("Could not write CLOCK_SELECT register\n");
            return false;
        }
    
        // check if the write succeeded
        fb_quadlet_t clockreg_verify;
        if (!readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clockreg_verify)) {
            debugError("Could not read CLOCK_SELECT register\n");
            return false;
        }
    
        if(clockreg != clockreg_verify) {
            debugError("Samplerate register write failed\n");
            return false;
        }
    }

    // Update for the new samplerate
    if ( !initIoFunctions() ) {
        debugError("Could not initialize I/O functions\n");
        return false;
    }

    if (m_eap) {
        m_eap->update();
    }
    showDevice();

    return true;
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;

    quadlet_t clock_caps;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, &clock_caps);
    uint16_t clocks_supported = (clock_caps >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock caps: 0x%08"PRIX32", supported=0x%04X\n",
                                    clock_caps, clocks_supported);

    quadlet_t clock_select;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clock_select);
    byte_t clock_selected = (clock_select) & 0xFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock select: 0x%08"PRIX32", selected=0x%04X\n",
                                    clock_select, clock_selected);
    quadlet_t extended_status;
    readGlobalReg(DICE_REGISTER_GLOBAL_EXTENDED_STATUS, &extended_status);
    #ifdef DEBUG
    uint16_t clock_status = (extended_status) & 0xFFFF;
    uint16_t clock_slipping = (extended_status >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock status: 0x%08"PRIX32", status=0x%04X, slip=0x%04X\n",
                                    extended_status, clock_status, clock_slipping);
    #endif

    stringlist names = getClockSourceNameString();
    if( names.size() < DICE_CLOCKSOURCE_COUNT) {
        debugError("Not enough clock source names on device\n");
        return r;
    }
    for (unsigned int i=0; i < DICE_CLOCKSOURCE_COUNT; i++) {
        bool supported = (((clocks_supported >> i) & 0x01) == 1);
        if (supported) {
            ClockSource s;
            s.type = clockIdToType(i);
            s.id = i;
            s.valid = true;
            s.locked = isClockSourceIdLocked(i, extended_status);
            s.slipping = isClockSourceIdSlipping(i, extended_status);
            s.active = (clock_selected == i);
            s.description = names.at(i);
            r.push_back(s);
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Clock source id %d not supported by device\n", i);
        }
    }
    return r;
}

bool
Device::isClockSourceIdLocked(unsigned int id, quadlet_t ext_status_reg) {
    switch (id) {
        default: return true;
        case  DICE_CLOCKSOURCE_AES1:
                return ext_status_reg & DICE_EXT_STATUS_AES0_LOCKED;
        case  DICE_CLOCKSOURCE_AES2:
                return ext_status_reg & DICE_EXT_STATUS_AES1_LOCKED;
        case  DICE_CLOCKSOURCE_AES3:
                return ext_status_reg & DICE_EXT_STATUS_AES2_LOCKED;
        case  DICE_CLOCKSOURCE_AES4:
                return ext_status_reg & DICE_EXT_STATUS_AES3_LOCKED;
        case  DICE_CLOCKSOURCE_AES_ANY:
                return ext_status_reg & DICE_EXT_STATUS_AES_ANY_LOCKED;
        case  DICE_CLOCKSOURCE_ADAT:
                return ext_status_reg & DICE_EXT_STATUS_ADAT_LOCKED;
        case  DICE_CLOCKSOURCE_TDIF:
                return ext_status_reg & DICE_EXT_STATUS_TDIF_LOCKED;
        case  DICE_CLOCKSOURCE_ARX1:
                return ext_status_reg & DICE_EXT_STATUS_ARX1_LOCKED;
        case  DICE_CLOCKSOURCE_ARX2:
                return ext_status_reg & DICE_EXT_STATUS_ARX2_LOCKED;
        case  DICE_CLOCKSOURCE_ARX3:
                return ext_status_reg & DICE_EXT_STATUS_ARX3_LOCKED;
        case  DICE_CLOCKSOURCE_ARX4:
                return ext_status_reg & DICE_EXT_STATUS_ARX4_LOCKED;
        case  DICE_CLOCKSOURCE_WC:
                return ext_status_reg & DICE_EXT_STATUS_WC_LOCKED;
    }
}
bool
Device::isClockSourceIdSlipping(unsigned int id, quadlet_t ext_status_reg) {
    switch (id) {
        default: return false;
        case  DICE_CLOCKSOURCE_AES1:
                return ext_status_reg & DICE_EXT_STATUS_AES0_SLIP;
        case  DICE_CLOCKSOURCE_AES2:
                return ext_status_reg & DICE_EXT_STATUS_AES1_SLIP;
        case  DICE_CLOCKSOURCE_AES3:
                return ext_status_reg & DICE_EXT_STATUS_AES2_SLIP;
        case  DICE_CLOCKSOURCE_AES4:
                return ext_status_reg & DICE_EXT_STATUS_AES3_SLIP;
        case  DICE_CLOCKSOURCE_AES_ANY:
                return false;
        case  DICE_CLOCKSOURCE_ADAT:
                return ext_status_reg & DICE_EXT_STATUS_ADAT_SLIP;
        case  DICE_CLOCKSOURCE_TDIF:
                return ext_status_reg & DICE_EXT_STATUS_TDIF_SLIP;
        case  DICE_CLOCKSOURCE_ARX1:
                return ext_status_reg & DICE_EXT_STATUS_ARX1_SLIP;
        case  DICE_CLOCKSOURCE_ARX2:
                return ext_status_reg & DICE_EXT_STATUS_ARX2_SLIP;
        case  DICE_CLOCKSOURCE_ARX3:
                return ext_status_reg & DICE_EXT_STATUS_ARX3_SLIP;
        case  DICE_CLOCKSOURCE_ARX4:
                return ext_status_reg & DICE_EXT_STATUS_ARX4_SLIP;
        case  DICE_CLOCKSOURCE_WC: // FIXME: not in driver spec, so non-existant
                return ext_status_reg & DICE_EXT_STATUS_WC_SLIP;
    }
}

enum FFADODevice::eClockSourceType
Device::clockIdToType(unsigned int id) {
    switch (id) {
        default: return eCT_Invalid;
        case  DICE_CLOCKSOURCE_AES1:
        case  DICE_CLOCKSOURCE_AES2:
        case  DICE_CLOCKSOURCE_AES3:
        case  DICE_CLOCKSOURCE_AES4:
        case  DICE_CLOCKSOURCE_AES_ANY:
                return eCT_AES;
        case  DICE_CLOCKSOURCE_ADAT:
                return eCT_ADAT;
        case  DICE_CLOCKSOURCE_TDIF:
                return eCT_TDIF;
        case  DICE_CLOCKSOURCE_ARX1:
        case  DICE_CLOCKSOURCE_ARX2:
        case  DICE_CLOCKSOURCE_ARX3:
        case  DICE_CLOCKSOURCE_ARX4:
                return eCT_SytMatch;
        case  DICE_CLOCKSOURCE_WC:
                return eCT_WordClock;
        case DICE_CLOCKSOURCE_INTERNAL:
                return eCT_Internal;
    }
}

bool
Device::setActiveClockSource(ClockSource s) {
    fb_quadlet_t clockreg;
    if (!readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clockreg)) {
        debugError("Could not read CLOCK_SELECT register\n");
        return false;
    }

    clockreg = DICE_SET_CLOCKSOURCE(clockreg, s.id);

    if (!writeGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, clockreg)) {
        debugError("Could not write CLOCK_SELECT register\n");
        return false;
    }

    // check if the write succeeded
    fb_quadlet_t clockreg_verify;
    if (!readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clockreg_verify)) {
        debugError("Could not read CLOCK_SELECT register\n");
        return false;
    }

    if(clockreg != clockreg_verify) {
        debugError("CLOCK_SELECT register write failed\n");
        return false;
    }

    return DICE_GET_CLOCKSOURCE(clockreg_verify) == s.id;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    ClockSource s;
    quadlet_t clock_caps;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, &clock_caps);
    uint16_t clocks_supported = (clock_caps >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock caps: 0x%08"PRIX32", supported=0x%04X\n",
                                    clock_caps, clocks_supported);

    quadlet_t clock_select;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clock_select);
    byte_t id = (clock_select) & 0xFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock select: 0x%08"PRIX32", selected=0x%04X\n",
                                    clock_select, id);
    quadlet_t extended_status;
    readGlobalReg(DICE_REGISTER_GLOBAL_EXTENDED_STATUS, &extended_status);
    #ifdef DEBUG
    uint16_t clock_status = (extended_status) & 0xFFFF;
    uint16_t clock_slipping = (extended_status >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock status: 0x%08"PRIX32", status=0x%04X, slip=0x%04X\n",
                                    extended_status, clock_status, clock_slipping);
    #endif

    stringlist names = getClockSourceNameString();
    if( names.size() < DICE_CLOCKSOURCE_COUNT) {
        debugError("Not enough clock source names on device\n");
        return s;
    }
    bool supported = (((clocks_supported >> id) & 0x01) == 1);
    if (supported) {
        s.type = clockIdToType(id);
        s.id = id;
        s.valid = true;
        s.locked = isClockSourceIdLocked(id, extended_status);
        s.slipping = isClockSourceIdSlipping(id, extended_status);
        s.active = true;
        s.description = names.at(id);
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Clock source id %2d not supported by device\n", id);
    }
    return s;
}

bool
Device::setNickname( std::string name)
{
    char namestring[DICE_NICK_NAME_SIZE+1];
    strncpy(namestring, name.c_str(), DICE_NICK_NAME_SIZE);

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)namestring, DICE_NICK_NAME_SIZE/4);
    #endif

    if (!writeGlobalRegBlock(DICE_REGISTER_GLOBAL_NICK_NAME,
                        (fb_quadlet_t *)namestring, DICE_NICK_NAME_SIZE)) {
        debugError("Could not write nickname string \n");
        return false;
    }
    return true;
}

std::string
Device::getNickname()
{
    char namestring[DICE_NICK_NAME_SIZE+1];

    if (!readGlobalRegBlock(DICE_REGISTER_GLOBAL_NICK_NAME,
                        (fb_quadlet_t *)namestring, DICE_NICK_NAME_SIZE)) {
        debugError("Could not read nickname string \n");
        return std::string("(unknown)");
    }

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)namestring, DICE_NICK_NAME_SIZE/4);
    #endif
    namestring[DICE_NICK_NAME_SIZE]='\0';
    return std::string(namestring);
}

void
Device::showDevice()
{
    fb_quadlet_t tmp_quadlet;
    fb_octlet_t tmp_octlet;

    debugOutput(DEBUG_LEVEL_NORMAL, "Device is a DICE device\n");
    FFADODevice::showDevice();

    printMessage(" DICE Parameter Space info:\n");
    printMessage("  Global  : offset=0x%04X size=%04d\n", m_global_reg_offset, m_global_reg_size);
    printMessage("  TX      : offset=0x%04X size=%04d\n", m_tx_reg_offset, m_tx_reg_size);
    printMessage("                nb=%4d size=%04d\n", m_nb_tx, m_tx_size);
    printMessage("  RX      : offset=0x%04X size=%04d\n", m_rx_reg_offset, m_rx_reg_size);
    printMessage("                nb=%4d size=%04d\n", m_nb_rx, m_rx_size);
    printMessage("  UNUSED1 : offset=0x%04X size=%04d\n", m_unused1_reg_offset, m_unused1_reg_size);
    printMessage("  UNUSED2 : offset=0x%04X size=%04d\n", m_unused2_reg_offset, m_unused2_reg_size);

    printMessage(" Global param space:\n");

    readGlobalRegBlock(DICE_REGISTER_GLOBAL_OWNER, reinterpret_cast<fb_quadlet_t *>(&tmp_octlet), sizeof(fb_octlet_t));
    printMessage("  Owner            : 0x%016"PRIX64"\n",tmp_octlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_NOTIFICATION, &tmp_quadlet);
    printMessage("  Notification     : 0x%08"PRIX32"\n",tmp_quadlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_NOTIFICATION, &tmp_quadlet);
    printMessage("  Nick name        : %s\n",getNickname().c_str());

    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &tmp_quadlet);
    printMessage("  Clock Select     : 0x%02X 0x%02X\n",
        (tmp_quadlet>>8) & 0xFF, tmp_quadlet & 0xFF);

    readGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, &tmp_quadlet);
    printMessage("  Enable           : %s\n",
        (tmp_quadlet&0x1?"true":"false"));

    readGlobalReg(DICE_REGISTER_GLOBAL_STATUS, &tmp_quadlet);
    printMessage("  Clock Status     : %s 0x%02X\n",
        (tmp_quadlet&0x1?"locked":"not locked"), (tmp_quadlet>>8) & 0xFF);

    readGlobalReg(DICE_REGISTER_GLOBAL_EXTENDED_STATUS, &tmp_quadlet);
    printMessage("  Extended Status  : 0x%08"PRIX32"\n", tmp_quadlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_SAMPLE_RATE, &tmp_quadlet);
    printMessage("  Samplerate       : 0x%08"PRIX32" (%"PRIu32")\n", tmp_quadlet, tmp_quadlet);

    readGlobalRegBlock(DICE_REGISTER_GLOBAL_VERSION, reinterpret_cast<fb_quadlet_t *>(&tmp_quadlet), sizeof(fb_quadlet_t));
    printMessage("  Version          : 0x%08"PRIX32"\n", tmp_quadlet);
    
    readGlobalReg(DICE_REGISTER_GLOBAL_VERSION, &tmp_quadlet);
    printMessage("  Version          : 0x%08"PRIX32" (%u.%u.%u.%u)\n",
        tmp_quadlet,
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_A(tmp_quadlet),
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_B(tmp_quadlet),
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_C(tmp_quadlet),
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_D(tmp_quadlet)
        );

    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, &tmp_quadlet);
    printMessage("  Clock caps       : 0x%08"PRIX32"\n", tmp_quadlet);

    stringlist names=getClockSourceNameString();
    printMessage("  Clock sources    :\n");

    for ( stringlist::iterator it = names.begin();
          it != names.end();
          ++it )
    {
        printMessage("    %s\n", (*it).c_str());
    }

    printMessage(" TX param space:\n");
    printMessage("  Nb of xmit        : %1d\n", m_nb_tx);
    for (unsigned int i=0;i<m_nb_tx;i++) {
        printMessage("  Transmitter %d:\n",i);

        readTxReg(i, DICE_REGISTER_TX_ISOC_BASE, &tmp_quadlet);
        printMessage("   ISO channel       : %3d\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_TX_SPEED_BASE, &tmp_quadlet);
        printMessage("   ISO speed         : %3d\n", tmp_quadlet);

        readTxReg(i, DICE_REGISTER_TX_NB_AUDIO_BASE, &tmp_quadlet);
        printMessage("   Nb audio channels : %3d\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_TX_MIDI_BASE, &tmp_quadlet);
        printMessage("   Nb midi channels  : %3d\n", tmp_quadlet);

        readTxReg(i, DICE_REGISTER_TX_AC3_CAPABILITIES_BASE, &tmp_quadlet);
        printMessage("   AC3 caps          : 0x%08"PRIX32"\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_TX_AC3_ENABLE_BASE, &tmp_quadlet);
        printMessage("   AC3 enable        : 0x%08"PRIX32"\n", tmp_quadlet);

        stringlist channel_names=getTxNameString(i);
        printMessage("   Channel names     :\n");
        for ( stringlist::iterator it = channel_names.begin();
            it != channel_names.end();
            ++it )
        {
            printMessage("     %s\n", (*it).c_str());
        }
    }

    printMessage(" RX param space:\n");
    printMessage("  Nb of recv        : %1d\n", m_nb_rx);
    for (unsigned int i=0;i<m_nb_rx;i++) {
        printMessage("  Receiver %d:\n",i);

        readRxReg(i, DICE_REGISTER_RX_ISOC_BASE, &tmp_quadlet);
        printMessage("   ISO channel       : %3d\n", tmp_quadlet);
        readRxReg(i, DICE_REGISTER_RX_SEQ_START_BASE, &tmp_quadlet);
        printMessage("   Sequence start    : %3d\n", tmp_quadlet);

        readRxReg(i, DICE_REGISTER_RX_NB_AUDIO_BASE, &tmp_quadlet);
        printMessage("   Nb audio channels : %3d\n", tmp_quadlet);
        readRxReg(i, DICE_REGISTER_RX_MIDI_BASE, &tmp_quadlet);
        printMessage("   Nb midi channels  : %3d\n", tmp_quadlet);

        readRxReg(i, DICE_REGISTER_RX_AC3_CAPABILITIES_BASE, &tmp_quadlet);
        printMessage("   AC3 caps          : 0x%08"PRIX32"\n", tmp_quadlet);
        readRxReg(i, DICE_REGISTER_RX_AC3_ENABLE_BASE, &tmp_quadlet);
        printMessage("   AC3 enable        : 0x%08"PRIX32"\n", tmp_quadlet);

        stringlist channel_names = getRxNameString(i);
        printMessage("   Channel names     :\n");
        for ( stringlist::iterator it = channel_names.begin();
            it != channel_names.end();
            ++it )
        {
            printMessage("     %s\n", (*it).c_str());
        }
    }
    flushDebugOutput();
}


void
Device::setRXTXfuncs (const Streaming::Port::E_Direction direction) {
    if (direction == Streaming::Port::E_Capture) {
        // we are a receive processor
        audio_base_register = DICE_REGISTER_TX_NB_AUDIO_BASE;
        midi_base_register = DICE_REGISTER_TX_MIDI_BASE;
        writeFunc = &Device::writeTxReg;
        readFunc  = &Device::readTxReg;
        strcpy(dir, "TX");
    } else {
        // we are a transmit processor
        audio_base_register = DICE_REGISTER_RX_NB_AUDIO_BASE;
        midi_base_register  = DICE_REGISTER_RX_MIDI_BASE;
        writeFunc = &Device::writeRxReg;
        readFunc  = &Device::readRxReg;
        strcpy(dir, "RX");
    }
};

// Really prepare a stream processor. This function gets called by prepare() for each stream
// processor to create.
bool
Device::prepareSP(unsigned int i, const Streaming::Port::E_Direction direction_requested) {
    fb_quadlet_t nb_audio;
    fb_quadlet_t nb_midi;
    unsigned int nb_channels = 0;
    Streaming::Port::E_Direction direction = direction_requested;

    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    // get the device specific and/or global SP configuration
    Util::Configuration &config = getDeviceManager().getConfiguration();
    // base value is the config.h value
    float recv_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;
    float xmit_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;

    int xmit_max_cycles_early_transmit = AMDTP_MAX_CYCLES_TO_TRANSMIT_EARLY;
    int xmit_transfer_delay = AMDTP_TRANSMIT_TRANSFER_DELAY;
    int xmit_min_cycles_before_presentation = AMDTP_MIN_CYCLES_BEFORE_PRESENTATION;

    // we can override that globally
    config.getValueForSetting("streaming.common.recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForSetting("streaming.common.xmit_sp_dll_bw", xmit_sp_dll_bw);
    config.getValueForSetting("streaming.amdtp.xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForSetting("streaming.amdtp.xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForSetting("streaming.amdtp.xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    // or override in the device section
    uint32_t vendorid = getConfigRom().getNodeVendorId();
    uint32_t modelid = getConfigRom().getModelId();
    config.getValueForDeviceSetting(vendorid, modelid, "recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_sp_dll_bw", xmit_sp_dll_bw);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    stringlist names_audio;
    stringlist names_midi;

    Streaming::StreamProcessor *p;
    float dll_bw;

    // set function pointers and base IO for upcoming code
    setRXTXfuncs (direction_requested);

    if (direction == Streaming::Port::E_Capture) {
        // we are a receive processor
        names_audio = getCptrNameString(i);
    } else {
        names_audio = getPbckNameString(i);
    }
    

    if(!(*this.*readFunc)(i, audio_base_register, &nb_audio)) {
        debugError("Could not read DICE_REGISTER_%s_NB_AUDIO_BASE register for A%s%u\n",
                dir, dir, i);
        // non-fatal error, simply returning true. Only false is important for prepare();
        return true;
    }

    if(!(*this.*readFunc)(i, midi_base_register, &nb_midi)) {
        debugError("Could not read DICE_REGISTER_%s_MIDI_BASE register for A%s%u\n",
                dir, dir, i);
        // non-fatal error, simply returning true. Only false is important for prepare();
        return true;
    }

    // request the channel names
    if (names_audio.size() != nb_audio) {
        debugWarning("The audio channel name vector is incorrect, using default names\n");
        names_audio.clear();

        for (unsigned int j=0;j<nb_audio;j++) {
            std::ostringstream newname;
            newname << "input_" << j;
            names_audio.push_back(newname.str());
        }
    }

    nb_channels = nb_audio;
    if(nb_midi) nb_channels += 1; // midi-muxed counts as one

    // construct the MIDI names
    for (unsigned int j=0;j<nb_midi;j++) {
        std::ostringstream newname;
        newname << "midi " << j;
        names_midi.push_back(newname.str());
    }

    // construct the streamprocessor
    if (direction == Streaming::Port::E_Capture || snoopMode) {
        p = new Streaming::AmdtpReceiveStreamProcessor(*this, nb_channels);
        dll_bw = recv_sp_dll_bw;
        direction = Streaming::Port::E_Capture;
    } else {
        p = new Streaming::AmdtpTransmitStreamProcessor(*this, nb_channels);
        dll_bw = xmit_sp_dll_bw;

#if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
        // the DICE-II cannot handle payload in the NO-DATA packets.
        // the other DICE chips don't need payload. Therefore
        // we disable it.
        ((Streaming::AmdtpTransmitStreamProcessor*)p)->sendPayloadForNoDataPackets(false);
#endif

        // transmit control parameters
        ((Streaming::AmdtpTransmitStreamProcessor*)p)->setMaxCyclesToTransmitEarly(xmit_max_cycles_early_transmit);
        ((Streaming::AmdtpTransmitStreamProcessor*)p)->setTransferDelay(xmit_transfer_delay);
        ((Streaming::AmdtpTransmitStreamProcessor*)p)->setMinCyclesBeforePresentation(xmit_min_cycles_before_presentation);
    }


    if(!p->init()) {
        debugFatal("Could not initialize %s processor!\n", dir);
        delete p;
        // non-fatal error, simply returning true. Only false is important for prepare();
        return true;
    }

    // add audio ports to the processor
    for (unsigned int j=0;j<nb_audio;j++) {
        diceChannelInfo channelInfo;
        channelInfo.name=names_audio.at(j);
        channelInfo.portType=ePT_Analog;
        channelInfo.streamPosition=j;
        channelInfo.streamLocation=0;

        if (!addChannelToProcessor(&channelInfo, p, direction)) {
            debugError("Could not add channel %s to StreamProcessor\n",
                    channelInfo.name.c_str());
            continue;
        }
    }

    // add midi ports to the processor
    for (unsigned int j=0;j<nb_midi;j++) {
        diceChannelInfo channelInfo;
        channelInfo.name=names_midi.at(j);
        channelInfo.portType=ePT_MIDI;
        channelInfo.streamPosition=nb_audio;
        channelInfo.streamLocation=j;

        if (!addChannelToProcessor(&channelInfo, p, direction)) {
            debugError("Could not add channel %s to StreamProcessor\n",
                    channelInfo.name.c_str());
            continue;
        }
    }

    if(!p->setDllBandwidth(dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete p;
        return false;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) %s SP on channel [%d audio, %d midi]\n",
            this, dir, nb_audio, nb_midi);
    // add the SP to the vector
    if (direction_requested == Streaming::Port::E_Capture) {
        m_receiveProcessors.push_back(p);
    } else {
        // we put this SP into the transmit SP vector,
        // no matter if we are in snoop mode or not
        // this allows us to find out what direction
        // a certain stream should have.
        m_transmitProcessors.push_back(p);
    }

    return true;
}

// NOTE on bandwidth calculation
// FIXME: The bandwidth allocation calculation can probably be
// refined somewhat since this is currently based on a rudimentary
// understanding of the iso protocol.
// Currently we assume the following.
//   * Ack/iso gap = 0.05 us
//   * DATA_PREFIX = 0.16 us1
//   * DATA_END    = 0.26 us
// These numbers are the worst-case figures given in the ieee1394
// standard.  This gives approximately 0.5 us of overheads per
// packet - around 25 bandwidth allocation units (from the ieee1394
// standard 1 bandwidth allocation unit is 125/6144 us).  We further
// assume the device is running at S400 (which it should be) so one
// allocation unit is equivalent to 1 transmitted byte; thus the
// bandwidth allocation required for the packets themselves is just
// the size of the packet.
bool
Device::prepare() {

    bool exit_code = true;

    // prepare receive SP's
    for (unsigned int i=0; i<m_nb_tx; i++) {
        exit_code &= prepareSP (i, Streaming::Port::E_Capture);
    }

    for (unsigned int i=0; i<m_nb_rx; i++) {
        exit_code &= prepareSP (i, Streaming::Port::E_Playback);
    }

    return exit_code;
}

bool
Device::addChannelToProcessor(
    diceChannelInfo *channelInfo,
    Streaming::StreamProcessor *processor,
    Streaming::Port::E_Direction direction) {
    std::string dev_name;

    std::string id=std::string("dev?");
    dev_name = getNickname();
    if(!getOption("id", id) && dev_name.size() == 0) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

    if (dev_name.size() == 0) dev_name = id; 
    std::ostringstream portname;
    portname << dev_name << "/" << channelInfo->name;

    Streaming::Port *p=NULL;
    switch(channelInfo->portType) {
    case ePT_Analog:
        p=new Streaming::AmdtpAudioPort(
                *processor,
                portname.str(),
                direction,
                channelInfo->streamPosition,
                channelInfo->streamLocation,
                Streaming::AmdtpPortInfo::E_MBLA
        );
        break;

    case ePT_MIDI:
        p=new Streaming::AmdtpMidiPort(
                *processor,
                portname.str(),
                direction,
                channelInfo->streamPosition,
                channelInfo->streamLocation,
                Streaming::AmdtpPortInfo::E_Midi
        );

        break;
    default:
    // unsupported
        break;
    }

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",channelInfo->name.c_str());
    }

    return true;
}

bool
Device::lock() {
    fb_octlet_t result;

    debugOutput(DEBUG_LEVEL_VERBOSE, "Locking device at node %d\n", getNodeId());

    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        debugWarning("Lock not supported in snoop mode\n");
        return true; //FIXME: this should be false
    } else {

    // get a notifier to handle device notifications
        nodeaddr_t notify_address;
        notify_address = get1394Service().findFreeARMBlock(
                            DICE_NOTIFIER_BASE_ADDRESS,
                            DICE_NOTIFIER_BLOCK_LENGTH,
                            DICE_NOTIFIER_BLOCK_LENGTH);
    
        if (notify_address == 0xFFFFFFFFFFFFFFFFLLU) {
            debugError("Could not find free ARM block for notification\n");
            return false;
        }
    
        m_notifier = new Device::Notifier(*this, notify_address);
    
        if(!m_notifier) {
            debugError("Could not allocate notifier\n");
            return false;
        }
    
        if (!get1394Service().registerARMHandler(m_notifier)) {
            debugError("Could not register notifier\n");
            delete m_notifier;
            m_notifier=NULL;
            return false;
        }
    
        // register this notifier
        fb_nodeaddr_t addr = DICE_REGISTER_BASE
                        + m_global_reg_offset
                        + DICE_REGISTER_GLOBAL_OWNER;
    
        // registry offsets should always be smaller than 0x7FFFFFFF
        // because otherwise base + offset > 64bit
        if(m_global_reg_offset & 0x80000000) {
            debugError("register offset not initialized yet\n");
            return false;
        }
    
        fb_nodeaddr_t swap_value = ((0xFFC0) | get1394Service().getLocalNodeId());
        swap_value = swap_value << 48;
        swap_value |= m_notifier->getStart();
    
        if (!get1394Service().lockCompareSwap64(getNodeId() | 0xFFC0,
                                                addr, DICE_OWNER_NO_OWNER,
                                                swap_value, &result )) {
            debugWarning("Could not register ourselves as device owner\n");
            return false;
        }
    
        if (result != DICE_OWNER_NO_OWNER && result != swap_value) {
            debugWarning("Could not register ourselves as device owner, unexpected register value: 0x%016"PRIX64"\n", result);
            return false;
        }
    
        return true;
    }
}


bool
Device::unlock() {
    fb_octlet_t result;

    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        debugWarning("Unlock not supported in snoop mode\n");
        return true; //FIXME: this should be false
    } else {
        if(!m_notifier) {
            debugWarning("Request to unlock, but no notifier present!\n");
            return false;
        }
    
        fb_nodeaddr_t addr = DICE_REGISTER_BASE
                        + m_global_reg_offset
                        + DICE_REGISTER_GLOBAL_OWNER;
    
        // registry offsets should always be smaller than 0x7FFFFFFF
        // because otherwise base + offset > 64bit
        if(m_global_reg_offset & 0x80000000) {
            debugError("register offset not initialized yet\n");
            return false;
        }
    
        fb_nodeaddr_t compare_value = ((0xFFC0) | get1394Service().getLocalNodeId());
        compare_value <<= 48;
        compare_value |= m_notifier->getStart();
    
        if (!get1394Service().lockCompareSwap64(  getNodeId() | 0xFFC0, addr, compare_value,
                                        DICE_OWNER_NO_OWNER, &result )) {
            debugWarning("Could not unregister ourselves as device owner\n");
            return false;
        }
    
        get1394Service().unregisterARMHandler(m_notifier);
        delete m_notifier;
        m_notifier=NULL;
    
        return true;
    }
}

bool
Device::enableStreaming() {
    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        debugWarning("Stream should be already running for snoop mode\n");
        return true;
    } else {
        return enableIsoStreaming();
    }
}

bool
Device::disableStreaming() {
    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (snoopMode) {
        debugWarning("Won't disable stream in snoop mode\n");
        return true;
    } else {
        return disableIsoStreaming();
    }
}

int
Device::getStreamCount() {
    return m_receiveProcessors.size() + m_transmitProcessors.size();
}

Streaming::StreamProcessor *
Device::getStreamProcessorByIndex(int i) {

    if (i<(int)m_receiveProcessors.size()) {
        return m_receiveProcessors.at(i);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        return m_transmitProcessors.at(i-m_receiveProcessors.size());
    }

    return NULL;
}

bool
Device::startstopStreamByIndex(int i, const bool start) {
    bool snoopMode = false;
    fb_nodeaddr_t base_address;   // holds DICE_REGISTER_TX_ISOC_BASE or DICE_REGISTER_RX_ISOC_BASE
    int n;                       // number of streaming processor

    Streaming::StreamProcessor *p;

    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (!snoopMode && isIsoStreamingEnabled()) {
        debugError("Cannot start streams while streaming is enabled\n");
        return false;
    }

    if (i<(int)m_receiveProcessors.size()) {
        n=i;
        p = m_receiveProcessors.at(n);
        base_address = DICE_REGISTER_TX_ISOC_BASE;
        setRXTXfuncs (Streaming::Port::E_Capture);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        n=i-m_receiveProcessors.size();
        p=m_transmitProcessors.at(n);
        base_address = DICE_REGISTER_RX_ISOC_BASE;
        setRXTXfuncs (Streaming::Port::E_Playback);
    } else {
        debugError("SP index %d out of range!\n",i);
        return false;
    }

    if (start == true) {
        if(snoopMode) { // a stream from the device to another host
            fb_quadlet_t reg_isoch;
            // check value of ISO_CHANNEL register
            if(!(*this.*readFunc)(n, base_address, &reg_isoch)) {
                debugError("Could not read ISO_CHANNEL register for A%s %d\n", dir, n);
                p->setChannel(-1);
                return false;
            }
            int isochannel = reg_isoch;
            debugOutput(DEBUG_LEVEL_VERBOSE,
                    "(%p) Snooping %s from channel %d\n",
                    this, dir, isochannel);
            p->setChannel(isochannel);
        } else {
            // allocate ISO channel
            int isochannel = allocateIsoChannel(p->getMaxPacketSize());
            if(isochannel<0) {
                debugError("Could not allocate iso channel for SP %d (A%s %d)\n", i, dir, n);
                return false;
            }
            debugOutput(DEBUG_LEVEL_VERBOSE,
                    "(%p) Allocated channel %u for %s\n",
                    this, isochannel, dir);
            p->setChannel(isochannel);

            fb_quadlet_t reg_isoch;
            // check value of ISO_CHANNEL register
            if(!(*this.*readFunc)(n, base_address, &reg_isoch)) {
                debugError("Could not read ISO_CHANNEL register for A%s %d\n", dir, n);
                p->setChannel(-1);
                deallocateIsoChannel(isochannel);
                return false;
            }
            if(reg_isoch != 0xFFFFFFFFUL) {
                debugWarning("ISO_CHANNEL register != 0xFFFFFFFF (=0x%08"PRIX32") for A%s %d\n", reg_isoch, dir, n);
                /* The ISO channel has already been registered, probably
                 * because the device was running before and jackd just
                 * crashed. Let's simply reuse the previously selected
                 * ISO channel.
                 *
                 * FIXME: try to reset the channel register and
                 * return to a clean state
                 */
                deallocateIsoChannel(isochannel);
                p->setChannel(reg_isoch);
#if 0
                /* FIXME: Looks like it's not necessary to ask the IRM.
                 * Just use the already registered ISO channel.
                 */
                // ask the IRM to use this channel
                if (get1394Service().allocateFixedIsoChannelGeneric(reg_isoch,p->getMaxPacketSize()) < 0) {
                    debugError("Cannot allocate iso channel (0x%08"PRIX32") for ATX %d\n", reg_isoch, n); 
                }
#endif
                isochannel=reg_isoch;
            }

            // write value of ISO_CHANNEL register
            reg_isoch = isochannel;
            if(!(*this.*writeFunc)(n, base_address, reg_isoch)) {
                debugError("Could not write ISO_CHANNEL register for A%s %d\n", dir, n);
                p->setChannel(-1);
                deallocateIsoChannel(isochannel);
                return false;
            }
        }
        return true;
    } else {
        // stop
        if(!snoopMode) {
            unsigned int isochannel = p->getChannel();

            fb_quadlet_t reg_isoch;
            // check value of ISO_CHANNEL register
            if(!(*this.*readFunc)(n, base_address, &reg_isoch)) {
                debugError("Could not read ISO_CHANNEL register for A%s %d\n", dir, n);
                return false;
            }
            if(reg_isoch != isochannel) {
                debugError("ISO_CHANNEL register != 0x%08"PRIX32" (=0x%08"PRIX32") for A%s %d\n", isochannel, reg_isoch, dir, n);
                return false;
            }

            // write value of ISO_CHANNEL register
            reg_isoch=0xFFFFFFFFUL;
            if(!writeTxReg(n, base_address, reg_isoch)) {
                debugError("Could not write ISO_CHANNEL register for A%s %d\n", dir, n);
                return false;
            }

            // deallocate ISO channel
            if(!deallocateIsoChannel(isochannel)) {
                debugError("Could not deallocate iso channel for SP %d (A%s %d)\n",i, dir, n);
                return false;
            }
        }
        p->setChannel(-1);
        return true;
    }
}

bool
Device::startStreamByIndex(int i) {
    return startstopStreamByIndex(i, true); // start stream
}

bool
Device::stopStreamByIndex(int i) {
    return startstopStreamByIndex(i, false); // stop stream
}

// helper routines

// allocate ISO resources for the SP's
int Device::allocateIsoChannel(unsigned int packet_size) {
    unsigned int bandwidth=8+packet_size;

    int ch=get1394Service().allocateIsoChannelGeneric(bandwidth);

    debugOutput(DEBUG_LEVEL_VERBOSE, "allocated channel %d, bandwidth %d\n",
        ch, bandwidth);

    return ch;
}
// deallocate ISO resources
bool Device::deallocateIsoChannel(int channel) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "freeing channel %d\n",channel);
    return get1394Service().freeIsoChannel(channel);
}

bool
Device::enableIsoStreaming() {
    return writeGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, DICE_ISOSTREAMING_ENABLE);
}

bool
Device::disableIsoStreaming() {
    return writeGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, DICE_ISOSTREAMING_DISABLE);
}

bool
Device::isIsoStreamingEnabled() {
    fb_quadlet_t result;
    readGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, &result);
    // I don't know what exactly is 'enable',
    // but disable is definately == 0
    return (result != DICE_ISOSTREAMING_DISABLE);
}

/**
 * @brief performs a masked bit register equals 0 check on the global parameter space
 * @return true if readGlobalReg(offset) & mask == 0
 */
bool
Device::maskedCheckZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask) {
    fb_quadlet_t result;
    readGlobalReg(offset, &result);
    return ((result & mask) == 0);
}
/**
 * @brief performs a masked bit register not equal to 0 check on the global parameter space
 * @return true if readGlobalReg(offset) & mask != 0
 */
bool
Device::maskedCheckNotZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask) {
    return !maskedCheckZeroGlobalReg(offset, mask);
}

stringlist
Device::getTxNameString(unsigned int i) {
    stringlist names;
    char namestring[DICE_TX_NAMES_SIZE+1];

    if (!readTxRegBlock(i, DICE_REGISTER_TX_NAMES_BASE,
                        (fb_quadlet_t *)namestring, DICE_TX_NAMES_SIZE)) {
        debugError("Could not read TX name string \n");
        return names;
    }

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)namestring, DICE_TX_NAMES_SIZE/4);
    #endif
    namestring[DICE_TX_NAMES_SIZE]='\0';
    return splitNameString(std::string(namestring));
}

stringlist
Device::getRxNameString(unsigned int i) {
    stringlist names;
    char namestring[DICE_RX_NAMES_SIZE+1];

    if (!readRxRegBlock(i, DICE_REGISTER_RX_NAMES_BASE,
                        (fb_quadlet_t *)namestring, DICE_RX_NAMES_SIZE)) {
        debugError("Could not read RX name string \n");
        return names;
    }

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)namestring, DICE_RX_NAMES_SIZE/4);
    #endif
    namestring[DICE_RX_NAMES_SIZE]='\0';
    return splitNameString(std::string(namestring));
}

stringlist
Device::getCptrNameString(unsigned int i) {
    if (m_eap) return m_eap->getCptrNameString(i);
    else return getTxNameString(i);
}

stringlist
Device::getPbckNameString(unsigned int i) {
    if (m_eap) return m_eap->getPbckNameString(i);
    return getRxNameString(i);
}

stringlist
Device::getClockSourceNameString() {
    stringlist names;
    char namestring[DICE_CLOCKSOURCENAMES_SIZE+1];

    if (!readGlobalRegBlock(DICE_REGISTER_GLOBAL_CLOCKSOURCENAMES,
                      (fb_quadlet_t *)namestring, DICE_CLOCKSOURCENAMES_SIZE)) {
        debugError("Could not read CLOCKSOURCE name string \n");
        return names;
    }

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)namestring, DICE_CLOCKSOURCENAMES_SIZE/4);
    #endif
    namestring[DICE_CLOCKSOURCENAMES_SIZE]='\0';
    return splitNameString(std::string(namestring));
}


stringlist
Device::splitNameString(std::string in) {
    in = in.substr(0,in.find("\\\\"));
    return stringlist::splitString(in, "\\");
}


// I/O routines
bool
Device::initIoFunctions() {

    // offsets and sizes are returned in quadlets, but we use byte values
    if(!readReg(DICE_REGISTER_GLOBAL_PAR_SPACE_OFF, &m_global_reg_offset)) {
        debugError("Could not initialize m_global_reg_offset\n");
        return false;
    }
    m_global_reg_offset*=4;

    if(!readReg(DICE_REGISTER_GLOBAL_PAR_SPACE_SZ, &m_global_reg_size)) {
        debugError("Could not initialize m_global_reg_size\n");
        return false;
    }
    m_global_reg_size*=4;

    if(!readReg(DICE_REGISTER_TX_PAR_SPACE_OFF, &m_tx_reg_offset)) {
        debugError("Could not initialize m_tx_reg_offset\n");
        return false;
    }
    m_tx_reg_offset*=4;

    if(!readReg(DICE_REGISTER_TX_PAR_SPACE_SZ, &m_tx_reg_size)) {
        debugError("Could not initialize m_tx_reg_size\n");
        return false;
    }
    m_tx_reg_size*=4;

    if(!readReg(DICE_REGISTER_RX_PAR_SPACE_OFF, &m_rx_reg_offset)) {
        debugError("Could not initialize m_rx_reg_offset\n");
        return false;
    }
    m_rx_reg_offset*=4;

    if(!readReg(DICE_REGISTER_RX_PAR_SPACE_SZ, &m_rx_reg_size)) {
        debugError("Could not initialize m_rx_reg_size\n");
        return false;
    }
    m_rx_reg_size*=4;

    if(!readReg(DICE_REGISTER_UNUSED1_SPACE_OFF, &m_unused1_reg_offset)) {
        debugError("Could not initialize m_unused1_reg_offset\n");
        return false;
    }
    m_unused1_reg_offset*=4;

    if(!readReg(DICE_REGISTER_UNUSED1_SPACE_SZ, &m_unused1_reg_size)) {
        debugError("Could not initialize m_unused1_reg_size\n");
        return false;
    }
    m_unused1_reg_size*=4;

    if(!readReg(DICE_REGISTER_UNUSED2_SPACE_OFF, &m_unused2_reg_offset)) {
        debugError("Could not initialize m_unused2_reg_offset\n");
        return false;
    }
    m_unused2_reg_offset*=4;

    if(!readReg(DICE_REGISTER_UNUSED2_SPACE_SZ, &m_unused2_reg_size)) {
        debugError("Could not initialize m_unused2_reg_size\n");
        return false;
    }
    m_unused2_reg_size*=4;

    if(!readReg(m_tx_reg_offset + DICE_REGISTER_TX_NB_TX, &m_nb_tx)) {
        debugError("Could not initialize m_nb_tx\n");
        return false;
    }
    if(!readReg(m_tx_reg_offset + DICE_REGISTER_TX_SZ_TX, &m_tx_size)) {
        debugError("Could not initialize m_tx_size\n");
        return false;
    }
    m_tx_size*=4;

    if(!readReg(m_tx_reg_offset + DICE_REGISTER_RX_NB_RX, &m_nb_rx)) {
        debugError("Could not initialize m_nb_rx\n");
        return false;
    }

    if(!readReg(m_tx_reg_offset + DICE_REGISTER_RX_SZ_RX, &m_rx_size)) {
        debugError("Could not initialize m_rx_size\n");
        return false;
    }
    m_rx_size*=4;

    // FIXME: verify this and clean it up.
    /* special case for Alesis io14, which announces two receive transmitters,
     * but only has one. Same is true for Alesis Multimix16.
     */
    if (FW_VENDORID_ALESIS == getConfigRom().getNodeVendorId()) {
        switch (getConfigRom().getModelId()) {
            case 0x00000001:
            case 0x00000000:
                m_nb_rx = 1;
                break;
        }
    }

    // FIXME: after a crash, the device might still be streaming. We
    // simply force a stop now (unless in snoopMode) to return to a
    // clean state.
    bool snoopMode = false;
    if(!getOption("snoopMode", snoopMode)) {
        //debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    if (!snoopMode) {
        disableIsoStreaming();
    }

    debugOutput(DEBUG_LEVEL_VERBOSE,"DICE Parameter Space info:\n");
    debugOutput(DEBUG_LEVEL_VERBOSE," Global  : offset=%04X size=%04d\n", m_global_reg_offset, m_global_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE," TX      : offset=%04X size=%04d\n", m_tx_reg_offset, m_tx_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"               nb=%4d size=%04d\n", m_nb_tx, m_tx_size);
    debugOutput(DEBUG_LEVEL_VERBOSE," RX      : offset=%04X size=%04d\n", m_rx_reg_offset, m_rx_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"               nb=%4d size=%04d\n", m_nb_rx, m_rx_size);
    debugOutput(DEBUG_LEVEL_VERBOSE," UNUSED1 : offset=%04X size=%04d\n", m_unused1_reg_offset, m_unused1_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE," UNUSED2 : offset=%04X size=%04d\n", m_unused2_reg_offset, m_unused2_reg_size);

    return true;
}

bool
Device::readReg(fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08"PRIX64"\n", offset);

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016"PRIX64"\n", offset);
        return false;
    }

    fb_nodeaddr_t addr = DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;

    if(!get1394Service().read_quadlet( nodeId, addr, result ) ) {
        debugError("Could not read from node 0x%04X addr 0x%12"PRIX64"\n", nodeId, addr);
        return false;
    }

    *result = CondSwapFromBus32(*result);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Read result: 0x%08"PRIX32"\n", *result);

    return true;
}

bool
Device::writeReg(fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08"PRIX64", data: 0x%08"PRIX32"\n",
        offset, data);

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%012"PRIX64"\n", offset);
        return false;
    }

    fb_nodeaddr_t addr = DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;

    if(!get1394Service().write_quadlet( nodeId, addr, CondSwapToBus32(data) ) ) {
        debugError("Could not write to node 0x%04X addr 0x%12"PRIX64"\n", nodeId, addr);
        return false;
    }
    return true;
}

bool
Device::readRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Reading base register offset 0x%08"PRIX64", length %zd, to %p\n",
                offset, length, data);
    const int blocksize_quads = 512/4;

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%012"PRIX64"\n", offset);
        return false;
    }

    fb_nodeaddr_t addr = DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;
    int quads_done = 0;
    // round to next full quadlet
    int length_quads = (length+3)/4;
    while(quads_done < length_quads) {
        fb_nodeaddr_t curr_addr = addr + quads_done*4;
        fb_quadlet_t *curr_data = data + quads_done;
        int quads_todo = length_quads - quads_done;
        debugOutput(DEBUG_LEVEL_VERBOSE, "reading addr: 0x%012"PRIX64", %d quads to %p\n", curr_addr, quads_todo, curr_data);
        
        if (quads_todo > blocksize_quads) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Truncating read from %d to %d quadlets\n", quads_todo, blocksize_quads);
            quads_todo = blocksize_quads;
        }
        #ifdef DEBUG
        if (quads_todo < 0) {
            debugError("BUG: quads_todo < 0: %d\n", quads_todo);
        }
        #endif

        if(!get1394Service().read( nodeId, curr_addr, quads_todo, curr_data ) ) {
            debugError("Could not read %d quadlets from node 0x%04X addr 0x%012"PRIX64"\n", quads_todo, nodeId, curr_addr);
            return false;
        }
        quads_done += quads_todo;
    }

    byteSwapFromBus(data, length/4);
    return true;
}

bool
Device::writeRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08"PRIX64", length: %zd\n",
        offset, length);
    const int blocksize_quads = 512/4;

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%012"PRIX64"\n", offset);
        return false;
    }

    fb_quadlet_t data_out[length/4];
    memcpy(data_out, data, length);
    byteSwapToBus(data_out, length/4);

    fb_nodeaddr_t addr = DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId = getNodeId() | 0xFFC0;
    int quads_done = 0;
    int length_quads = (length+3)/4;
    while(quads_done < length_quads) {
        fb_nodeaddr_t curr_addr = addr + quads_done*4;
        fb_quadlet_t *curr_data = data_out + quads_done;
        int quads_todo = length_quads - quads_done;
        if (quads_todo > blocksize_quads) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Truncating write from %d to %d quadlets\n", quads_todo, blocksize_quads);
            quads_todo = blocksize_quads;
        }
        #ifdef DEBUG
        if (quads_todo < 0) {
            debugError("BUG: quads_todo < 0: %d\n", quads_todo);
        }
        #endif

        if(!get1394Service().write( nodeId, addr, quads_todo, curr_data ) ) {
            debugError("Could not write %d quadlets to node 0x%04X addr 0x%012"PRIX64"\n", quads_todo, nodeId, curr_addr);
            return false;
        }
        quads_done += quads_todo;
    }

    return true;
}

bool
Device::readGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading global register offset 0x%04"PRIX64"\n", offset);

    fb_nodeaddr_t offset_gl = globalOffsetGen(offset, sizeof(fb_quadlet_t));
    return readReg(m_global_reg_offset + offset_gl, result);
}

bool
Device::writeGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing global register offset 0x%08"PRIX64", data: 0x%08"PRIX32"\n",
        offset, data);

    fb_nodeaddr_t offset_gl = globalOffsetGen(offset, sizeof(fb_quadlet_t));
    return writeReg(m_global_reg_offset + offset_gl, data);
}

bool
Device::readGlobalRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading global register block offset 0x%04"PRIX64", length %zd bytes\n",
        offset, length);

    fb_nodeaddr_t offset_gl = globalOffsetGen(offset, length);
    return readRegBlock(m_global_reg_offset + offset_gl, data, length);
}

bool
Device::writeGlobalRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing global register block offset 0x%04"PRIX64", length: %zd bytes\n",
        offset, length);

    fb_nodeaddr_t offset_gl = globalOffsetGen(offset, length);
    return writeRegBlock(m_global_reg_offset + offset_gl, data, length);
}

fb_nodeaddr_t
Device::globalOffsetGen(fb_nodeaddr_t offset, size_t length) {

    // registry offsets should always be smaller than 0x7FFFFFFF
    // because otherwise base + offset > 64bit
    if(m_global_reg_offset & 0x80000000) {
        debugError("register offset not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    // out-of-range check
    if(offset+length > m_global_reg_offset+m_global_reg_size) {
        debugError("register offset+length too large: 0x%04"PRIX64"\n", offset + length);
        return DICE_INVALID_OFFSET;
    }

    return offset;
}

bool
Device::readTxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Reading tx %d register offset 0x%04"PRIX64"\n", i, offset);

    fb_nodeaddr_t offset_tx = txOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return readReg(m_tx_reg_offset + offset_tx, result);
}

bool
Device::writeTxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing tx %d register offset 0x%08"PRIX64", data: 0x%08"PRIX32"\n",
        i, offset, data);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return writeReg(m_tx_reg_offset + offset_tx, data);
}

bool
Device::readTxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading rx register block offset 0x%04"PRIX64", length: %zd bytes\n",
        offset, length);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, length);
    return readRegBlock(m_tx_reg_offset + offset_tx, data, length);
}

bool
Device::writeTxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing rx register block offset 0x%04"PRIX64", length: %zd bytes\n",
        offset, length);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, length);
    return writeRegBlock(m_tx_reg_offset + offset_tx, data, length);
}

fb_nodeaddr_t
Device::txOffsetGen(unsigned int i, fb_nodeaddr_t offset, size_t length) {
    // registry offsets should always be smaller than 0x7FFFFFFF
    // because otherwise base + offset > 64bit
    if(m_tx_reg_offset & 0x80000000) {
        debugError("register offset not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    if(m_nb_tx & 0x80000000) {
        debugError("m_nb_tx not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    if(m_tx_size & 0x80000000) {
        debugError("m_tx_size not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    if(i >= m_nb_tx) {
        debugError("TX index out of range\n");
        return DICE_INVALID_OFFSET;
    }

    fb_nodeaddr_t offset_tx = DICE_REGISTER_TX_PARAM(m_tx_size, i, offset);

    // out-of-range check
    if(offset_tx + length > m_tx_reg_offset+4+m_tx_reg_size*m_nb_tx) {
        debugError("register offset+length too large: 0x%04"PRIX64"\n", offset_tx + length);
        return DICE_INVALID_OFFSET;
    }

    return offset_tx;
}

bool
Device::readRxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading rx %d register offset 0x%04"PRIX64"\n", i, offset);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return readReg(m_rx_reg_offset + offset_rx, result);
}

bool
Device::writeRxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing rx register offset 0x%08"PRIX64", data: 0x%08"PRIX32"\n",
        offset, data);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return writeReg(m_rx_reg_offset + offset_rx, data);
}

bool
Device::readRxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading rx register block offset 0x%04"PRIX64", length: %zd bytes\n",
        offset, length);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, length);
    return readRegBlock(m_rx_reg_offset + offset_rx, data, length);
}

bool
Device::writeRxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing rx register block offset 0x%04"PRIX64", length: %zd bytes\n",
        offset, length);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, length);
    return writeRegBlock(m_rx_reg_offset + offset_rx, data, length);
}

fb_nodeaddr_t
Device::rxOffsetGen(unsigned int i, fb_nodeaddr_t offset, size_t length) {
    // registry offsets should always be smaller than 0x7FFFFFFF
    // because otherwise base + offset > 64bit
    if(m_rx_reg_offset & 0x80000000) {
        debugError("register offset not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    if(m_nb_rx & 0x80000000) {
        debugError("m_nb_rx not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    if(m_rx_size & 0x80000000) {
        debugError("m_rx_size not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    if(i >= m_nb_rx) {
        debugError("RX index out of range\n");
        return DICE_INVALID_OFFSET;
    }

    fb_nodeaddr_t offset_rx = DICE_REGISTER_RX_PARAM(m_rx_size, i, offset);

    // out-of-range check
    if(offset_rx + length > m_rx_reg_offset+4+m_rx_reg_size*m_nb_rx) {
        debugError("register offset+length too large: 0x%04"PRIX64"\n", offset_rx + length);
        return DICE_INVALID_OFFSET;
    }
    return offset_rx;
}


// the notifier

Device::Notifier::Notifier(Device &d, nodeaddr_t start)
 : ARMHandler(d.get1394Service(), start, DICE_NOTIFIER_BLOCK_LENGTH,
              RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
              RAW1394_ARM_WRITE, 0)
 , m_device(d)
{
    // switch over the debug module to that of this device instead of the 1394 service
    m_debugModule = d.m_debugModule;
}

Device::Notifier::~Notifier()
{

}

}
