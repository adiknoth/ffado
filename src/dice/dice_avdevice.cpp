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

#include "dice/dice_avdevice.h"
#include "dice/dice_defines.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"
#include <libraw1394/csr.h>

#include <iostream>
#include <sstream>

#include <string>
using namespace std;

namespace Dice {

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    // vendor id, model id, vendor name, model name
    {FW_VENDORID_TCAT, 0x00000002, "TCAT", "DiceII EVM"},
    {FW_VENDORID_TCAT, 0x00000004, "TCAT", "DiceII EVM (vxx)"},
    {FW_VENDORID_TCAT, 0x00000021, "TC Electronic", "Konnekt 8"},
    {FW_VENDORID_TCAT, 0x00000023, "TC Electronic", "Konnekt Live"},
};

DiceAvDevice::DiceAvDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_model( NULL )
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
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::DiceAvDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

}

DiceAvDevice::~DiceAvDevice()
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
}

bool
DiceAvDevice::probe( ConfigRom& configRom )
{
    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            return true;
        }
    }

    return false;
}

FFADODevice *
DiceAvDevice::createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new DiceAvDevice( d, configRom );
}

bool
DiceAvDevice::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].model_id == modelId )
           )
        {
            m_model = &(supportedDeviceList[i]);
        }
    }

    if (m_model == NULL) {
        debugWarning("DiceAvDevice::discover() called for a non-dice device");
        return false;
    }

    if ( !initIoFunctions() ) {
        debugError("Could not initialize I/O functions\n");
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s (nick: %s)\n",
                m_model->vendor_name, m_model->model_name, getDeviceNickName().c_str());

    return true;
}

int
DiceAvDevice::getSamplingFrequency( ) {
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

int
DiceAvDevice::getConfigurationId()
{
    return 0;
}

bool
DiceAvDevice::setSamplingFrequency( int samplingFrequency )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Setting sample rate: %d\n",
        (samplingFrequency));

    bool supported=false;
    fb_quadlet_t select=0x0;

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

    return true;
}

FFADODevice::ClockSourceVector
DiceAvDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;

    quadlet_t clock_caps;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, &clock_caps);
    uint16_t clocks_supported = (clock_caps >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock caps: 0x%08X, supported=0x%04X\n",
                                    clock_caps, clocks_supported);

    quadlet_t clock_select;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clock_select);
    byte_t clock_selected = (clock_select) & 0xFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock select: 0x%08X, selected=0x%04X\n",
                                    clock_select, clock_selected);
    quadlet_t extended_status;
    readGlobalReg(DICE_REGISTER_GLOBAL_EXTENDED_STATUS, &extended_status);
    #ifdef DEBUG
    uint16_t clock_status = (extended_status) & 0xFFFF;
    uint16_t clock_slipping = (extended_status >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock status: 0x%08X, status=0x%04X, slip=0x%04X\n",
                                    extended_status, clock_status, clock_slipping);
    #endif

    diceNameVector names = getClockSourceNameString();
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
DiceAvDevice::isClockSourceIdLocked(unsigned int id, quadlet_t ext_status_reg) {
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
DiceAvDevice::isClockSourceIdSlipping(unsigned int id, quadlet_t ext_status_reg) {
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
DiceAvDevice::clockIdToType(unsigned int id) {
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
DiceAvDevice::setActiveClockSource(ClockSource s) {
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
DiceAvDevice::getActiveClockSource() {
    ClockSource s;
    quadlet_t clock_caps;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, &clock_caps);
    uint16_t clocks_supported = (clock_caps >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock caps: 0x%08X, supported=0x%04X\n",
                                    clock_caps, clocks_supported);

    quadlet_t clock_select;
    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &clock_select);
    byte_t id = (clock_select) & 0xFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock select: 0x%08X, selected=0x%04X\n",
                                    clock_select, id);
    quadlet_t extended_status;
    readGlobalReg(DICE_REGISTER_GLOBAL_EXTENDED_STATUS, &extended_status);
    #ifdef DEBUG
    uint16_t clock_status = (extended_status) & 0xFFFF;
    uint16_t clock_slipping = (extended_status >> 16) & 0xFFFF;
    debugOutput(DEBUG_LEVEL_VERBOSE," Clock status: 0x%08X, status=0x%04X, slip=0x%04X\n",
                                    extended_status, clock_status, clock_slipping);
    #endif

    diceNameVector names = getClockSourceNameString();
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
DiceAvDevice::setNickname( std::string name)
{
    return setDeviceNickName(name);
}

std::string
DiceAvDevice::getNickname()
{
    return getDeviceNickName();
}

void
DiceAvDevice::showDevice()
{
    fb_quadlet_t tmp_quadlet;
    fb_octlet_t tmp_octlet;

    debugOutput(DEBUG_LEVEL_NORMAL, "Device is a DICE device\n");
    FFADODevice::showDevice();

    debugOutput(DEBUG_LEVEL_VERBOSE," DICE Parameter Space info:\n");
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Global  : offset=0x%04X size=%04d\n", m_global_reg_offset, m_global_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  TX      : offset=0x%04X size=%04d\n", m_tx_reg_offset, m_tx_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"                nb=%4d size=%04d\n", m_nb_tx, m_tx_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  RX      : offset=0x%04X size=%04d\n", m_rx_reg_offset, m_rx_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"                nb=%4d size=%04d\n", m_nb_rx, m_rx_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  UNUSED1 : offset=0x%04X size=%04d\n", m_unused1_reg_offset, m_unused1_reg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  UNUSED2 : offset=0x%04X size=%04d\n", m_unused2_reg_offset, m_unused2_reg_size);

    debugOutput(DEBUG_LEVEL_VERBOSE," Global param space:\n");

    readGlobalRegBlock(DICE_REGISTER_GLOBAL_OWNER, reinterpret_cast<fb_quadlet_t *>(&tmp_octlet), sizeof(fb_octlet_t));
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Owner            : 0x%016X\n",tmp_octlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_NOTIFICATION, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Notification     : 0x%08X\n",tmp_quadlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_NOTIFICATION, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_NORMAL,"  Nick name        : %s\n",getDeviceNickName().c_str());

    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCK_SELECT, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_NORMAL,"  Clock Select     : 0x%02X 0x%02X\n",
        (tmp_quadlet>>8) & 0xFF, tmp_quadlet & 0xFF);

    readGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_NORMAL,"  Enable           : %s\n",
        (tmp_quadlet&0x1?"true":"false"));

    readGlobalReg(DICE_REGISTER_GLOBAL_STATUS, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_NORMAL,"  Clock Status     : %s 0x%02X\n",
        (tmp_quadlet&0x1?"locked":"not locked"), (tmp_quadlet>>8) & 0xFF);

    readGlobalReg(DICE_REGISTER_GLOBAL_EXTENDED_STATUS, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Extended Status  : 0x%08X\n",tmp_quadlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_SAMPLE_RATE, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_NORMAL,"  Samplerate       : 0x%08X (%lu)\n",tmp_quadlet,tmp_quadlet);

    readGlobalReg(DICE_REGISTER_GLOBAL_VERSION, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_NORMAL,"  Version          : 0x%08X (%u.%u.%u.%u)\n",
        tmp_quadlet,
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_A(tmp_quadlet),
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_B(tmp_quadlet),
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_C(tmp_quadlet),
        DICE_DRIVER_SPEC_VERSION_NUMBER_GET_D(tmp_quadlet)
        );

    readGlobalReg(DICE_REGISTER_GLOBAL_CLOCKCAPABILITIES, &tmp_quadlet);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Clock caps       : 0x%08X\n",tmp_quadlet);

    diceNameVector names=getClockSourceNameString();
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Clock sources    :\n");

    for ( diceNameVectorIterator it = names.begin();
          it != names.end();
          ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE,"    %s\n", (*it).c_str());
    }

    debugOutput(DEBUG_LEVEL_VERBOSE," TX param space:\n");
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Nb of xmit        : %1d\n", m_nb_tx);
    for (unsigned int i=0;i<m_nb_tx;i++) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"  Transmitter %d:\n",i);

        readTxReg(i, DICE_REGISTER_TX_ISOC_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   ISO channel       : %3d\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_TX_SPEED_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   ISO speed         : %3d\n", tmp_quadlet);

        readTxReg(i, DICE_REGISTER_TX_NB_AUDIO_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Nb audio channels : %3d\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_TX_MIDI_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Nb midi channels  : %3d\n", tmp_quadlet);

        readTxReg(i, DICE_REGISTER_TX_AC3_CAPABILITIES_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   AC3 caps          : 0x%08X\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_TX_AC3_ENABLE_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   AC3 enable        : 0x%08X\n", tmp_quadlet);

        diceNameVector channel_names=getTxNameString(i);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Channel names     :\n");
        for ( diceNameVectorIterator it = channel_names.begin();
            it != channel_names.end();
            ++it )
        {
            debugOutput(DEBUG_LEVEL_VERBOSE,"     %s\n", (*it).c_str());
        }
    }

    debugOutput(DEBUG_LEVEL_VERBOSE," RX param space:\n");
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Nb of recv        : %1d\n", m_nb_tx);
    for (unsigned int i=0;i<m_nb_rx;i++) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"  Receiver %d:\n",i);

        readTxReg(i, DICE_REGISTER_RX_ISOC_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   ISO channel       : %3d\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_RX_SEQ_START_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Sequence start    : %3d\n", tmp_quadlet);

        readTxReg(i, DICE_REGISTER_RX_NB_AUDIO_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Nb audio channels : %3d\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_RX_MIDI_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Nb midi channels  : %3d\n", tmp_quadlet);

        readTxReg(i, DICE_REGISTER_RX_AC3_CAPABILITIES_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   AC3 caps          : 0x%08X\n", tmp_quadlet);
        readTxReg(i, DICE_REGISTER_RX_AC3_ENABLE_BASE, &tmp_quadlet);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   AC3 enable        : 0x%08X\n", tmp_quadlet);

        diceNameVector channel_names=getRxNameString(i);
        debugOutput(DEBUG_LEVEL_VERBOSE,"   Channel names     :\n");
        for ( diceNameVectorIterator it = channel_names.begin();
            it != channel_names.end();
            ++it )
        {
            debugOutput(DEBUG_LEVEL_VERBOSE,"     %s\n", (*it).c_str());
        }
    }
    flushDebugOutput();
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
DiceAvDevice::prepare() {
    // prepare receive SP's
//     for (unsigned int i=0;i<1;i++) {
    for (unsigned int i=0;i<m_nb_tx;i++) {
        fb_quadlet_t nb_audio;
        fb_quadlet_t nb_midi;
        unsigned int nb_channels=0;

        if(!readTxReg(i, DICE_REGISTER_TX_NB_AUDIO_BASE, &nb_audio)) {
            debugError("Could not read DICE_REGISTER_TX_NB_AUDIO_BASE register for ATX%u\n",i);
            continue;
        }
        if(!readTxReg(i, DICE_REGISTER_TX_MIDI_BASE, &nb_midi)) {
            debugError("Could not read DICE_REGISTER_TX_MIDI_BASE register for ATX%u\n",i);
            continue;
        }

        // request the channel names
        diceNameVector names_audio=getTxNameString(i);

        if (names_audio.size() != nb_audio) {
            debugWarning("The audio channel name vector is incorrect, using default names\n");
            names_audio.clear();

            for (unsigned int j=0;j<nb_audio;j++) {
                std::ostringstream newname;
                newname << "input_" << j;
                names_audio.push_back(newname.str());
            }
        }

        nb_channels=nb_audio;
        if(nb_midi) nb_channels += 1; // midi-muxed counts as one

        // construct the MIDI names
        diceNameVector names_midi;
        for (unsigned int j=0;j<nb_midi;j++) {
            std::ostringstream newname;
            newname << "midi_in_" << j;
            names_midi.push_back(newname.str());
        }

        // construct the streamprocessor
        Streaming::AmdtpReceiveStreamProcessor *p;
        p=new Streaming::AmdtpReceiveStreamProcessor(*this,
                             nb_channels);

        if(!p->init()) {
            debugFatal("Could not initialize receive processor!\n");
            delete p;
            continue;
        }

        // add audio ports to the processor
        for (unsigned int j=0;j<nb_audio;j++) {
            diceChannelInfo channelInfo;
            channelInfo.name=names_audio.at(j);
            channelInfo.portType=ePT_Analog;
            channelInfo.streamPosition=j;
            channelInfo.streamLocation=0;

            if (!addChannelToProcessor(&channelInfo, p, Streaming::Port::E_Capture)) {
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

            if (!addChannelToProcessor(&channelInfo, p, Streaming::Port::E_Capture)) {
                debugError("Could not add channel %s to StreamProcessor\n",
                    channelInfo.name.c_str());
                continue;
            }
        }

        // add the SP to the vector
        m_receiveProcessors.push_back(p);
    }

    // prepare transmit SP's
    //for (unsigned int i=0;i<1;i++) {
    for (unsigned int i=0;i<m_nb_rx;i++) {
        fb_quadlet_t nb_audio;
        fb_quadlet_t nb_midi;
        unsigned int nb_channels=0;

        if(!readTxReg(i, DICE_REGISTER_RX_NB_AUDIO_BASE, &nb_audio)) {
            debugError("Could not read DICE_REGISTER_RX_NB_AUDIO_BASE register for ARX%u\n",i);
            continue;
        }
        if(!readTxReg(i, DICE_REGISTER_RX_MIDI_BASE, &nb_midi)) {
            debugError("Could not read DICE_REGISTER_RX_MIDI_BASE register for ARX%u\n",i);
            continue;
        }

        // request the channel names
        diceNameVector names_audio=getRxNameString(i);

        if (names_audio.size() != nb_audio) {
            debugWarning("The audio channel name vector is incorrect, using default names\n");
            names_audio.clear();

            for (unsigned int j=0;j<nb_audio;j++) {
                std::ostringstream newname;
                newname << "output_" << j;
                names_audio.push_back(newname.str());
            }
        }

        nb_channels=nb_audio;
        if(nb_midi) nb_channels += 1; // midi-muxed counts as one

        // construct the MIDI names
        diceNameVector names_midi;
        for (unsigned int j=0;j<nb_midi;j++) {
            std::ostringstream newname;
            newname << "midi_out_" << j;
            names_midi.push_back(newname.str());
        }

        // construct the streamprocessor
        Streaming::AmdtpTransmitStreamProcessor *p;
        p=new Streaming::AmdtpTransmitStreamProcessor(*this,
                             nb_channels);

        if(!p->init()) {
            debugFatal("Could not initialize transmit processor!\n");
            delete p;
            continue;
        }

#if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
        // the DICE-II cannot handle payload in the NO-DATA packets.
        // the other DICE chips don't need payload. Therefore
        // we disable it.
        p->sendPayloadForNoDataPackets(false);
#endif

        // add audio ports to the processor
        for (unsigned int j=0;j<nb_audio;j++) {
            diceChannelInfo channelInfo;
            channelInfo.name=names_audio.at(j);
            channelInfo.portType=ePT_Analog;
            channelInfo.streamPosition=j;
            channelInfo.streamLocation=0;

            if (!addChannelToProcessor(&channelInfo, p, Streaming::Port::E_Playback)) {
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

            if (!addChannelToProcessor(&channelInfo, p, Streaming::Port::E_Playback)) {
                debugError("Could not add channel %s to StreamProcessor\n",
                    channelInfo.name.c_str());
                continue;
            }
        }

        m_transmitProcessors.push_back(p);
    }
    return true;
}

bool
DiceAvDevice::addChannelToProcessor(
    diceChannelInfo *channelInfo,
    Streaming::StreamProcessor *processor,
    Streaming::Port::E_Direction direction) {

    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

    std::ostringstream portname;
    portname << id;
    if(direction == Streaming::Port::E_Playback) {
        portname << "p";
    } else {
        portname << "c";
    }

    portname << "_" << channelInfo->name;

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
DiceAvDevice::lock() {
    fb_octlet_t result;

    debugOutput(DEBUG_LEVEL_VERBOSE, "Locking %s %s at node %d\n",
        m_model->vendor_name, m_model->model_name, getNodeId());

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

    m_notifier=new DiceAvDevice::DiceNotifier(this, notify_address);

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

    if (result != DICE_OWNER_NO_OWNER) {
        debugWarning("Could not register ourselves as device owner, unexpected register value: 0x%016llX\n", result);
        return false;
    }

    return true;
}


bool
DiceAvDevice::unlock() {
    fb_octlet_t result;

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

bool
DiceAvDevice::enableStreaming() {
    return enableIsoStreaming();
}

bool
DiceAvDevice::disableStreaming() {
    return disableIsoStreaming();
}

int
DiceAvDevice::getStreamCount() {
    return m_receiveProcessors.size() + m_transmitProcessors.size();
}

Streaming::StreamProcessor *
DiceAvDevice::getStreamProcessorByIndex(int i) {

    if (i<(int)m_receiveProcessors.size()) {
        return m_receiveProcessors.at(i);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        return m_transmitProcessors.at(i-m_receiveProcessors.size());
    }

    return NULL;
}

bool
DiceAvDevice::startStreamByIndex(int i) {

    if (isIsoStreamingEnabled()) {
        debugError("Cannot start streams while streaming is enabled\n");
        return false;
    }

    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);

        // allocate ISO channel
        int isochannel=allocateIsoChannel(p->getMaxPacketSize());
        if(isochannel<0) {
            debugError("Could not allocate iso channel for SP %d (ATX %d)\n",i,n);
            return false;
        }
        p->setChannel(isochannel);

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readTxReg(n, DICE_REGISTER_TX_ISOC_BASE, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register for ATX %d\n", n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }
        if(reg_isoch != 0xFFFFFFFFUL) {
            debugError("ISO_CHANNEL register != 0xFFFFFFFF (=0x%08X) for ATX %d\n", reg_isoch, n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=isochannel;
        if(!writeTxReg(n, DICE_REGISTER_TX_ISOC_BASE, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register for ATX %d\n", n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        // allocate ISO channel
        int isochannel=allocateIsoChannel(p->getMaxPacketSize());
        if(isochannel<0) {
            debugError("Could not allocate iso channel for SP %d (ATX %d)\n",i,n);
            return false;
        }
        p->setChannel(isochannel);

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readRxReg(n, DICE_REGISTER_RX_ISOC_BASE, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register for ARX %d\n", n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }
        if(reg_isoch != 0xFFFFFFFFUL) {
            debugError("ISO_CHANNEL register != 0xFFFFFFFF (=0x%08X) for ARX %d\n", reg_isoch, n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=isochannel;
        if(!writeRxReg(n, DICE_REGISTER_RX_ISOC_BASE, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register for ARX %d\n", n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        return true;
    }

    debugError("SP index %d out of range!\n",i);

    return false;
}

bool
DiceAvDevice::stopStreamByIndex(int i) {

    if (isIsoStreamingEnabled()) {
        debugError("Cannot stop streams while streaming is enabled\n");
        return false;
    }

    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);
        unsigned int isochannel=p->getChannel();

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readTxReg(n, DICE_REGISTER_TX_ISOC_BASE, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register for ATX %d\n", n);
            return false;
        }
        if(reg_isoch != isochannel) {
            debugError("ISO_CHANNEL register != 0x%08X (=0x%08X) for ATX %d\n", isochannel, reg_isoch, n);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=0xFFFFFFFFUL;
        if(!writeTxReg(n, DICE_REGISTER_TX_ISOC_BASE, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register for ATX %d\n", n);
            return false;
        }

        // deallocate ISO channel
        if(!deallocateIsoChannel(isochannel)) {
            debugError("Could not deallocate iso channel for SP %d (ATX %d)\n",i,n);
            return false;
        }

        p->setChannel(-1);
        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        unsigned int isochannel=p->getChannel();

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readRxReg(n, DICE_REGISTER_RX_ISOC_BASE, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register for ARX %d\n", n);
            return false;
        }
        if(reg_isoch != isochannel) {
            debugError("ISO_CHANNEL register != 0x%08X (=0x%08X) for ARX %d\n", isochannel, reg_isoch, n);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=0xFFFFFFFFUL;
        if(!writeRxReg(n, DICE_REGISTER_RX_ISOC_BASE, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register for ARX %d\n", n);
            return false;
        }

        // deallocate ISO channel
        if(!deallocateIsoChannel(isochannel)) {
            debugError("Could not deallocate iso channel for SP %d (ARX %d)\n",i,n);
            return false;
        }

        p->setChannel(-1);
        return true;
    }

    debugError("SP index %d out of range!\n",i);

    return false;
}

// helper routines

// allocate ISO resources for the SP's
int DiceAvDevice::allocateIsoChannel(unsigned int packet_size) {
    unsigned int bandwidth=8+packet_size;

    int ch=get1394Service().allocateIsoChannelGeneric(bandwidth);

    debugOutput(DEBUG_LEVEL_VERBOSE, "allocated channel %d, bandwidth %d\n",
        ch, bandwidth);

    return ch;
}
// deallocate ISO resources
bool DiceAvDevice::deallocateIsoChannel(int channel) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "freeing channel %d\n",channel);
    return get1394Service().freeIsoChannel(channel);
}

bool
DiceAvDevice::enableIsoStreaming() {
    return writeGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, DICE_ISOSTREAMING_ENABLE);
}

bool
DiceAvDevice::disableIsoStreaming() {
    return writeGlobalReg(DICE_REGISTER_GLOBAL_ENABLE, DICE_ISOSTREAMING_DISABLE);
}

bool
DiceAvDevice::isIsoStreamingEnabled() {
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
DiceAvDevice::maskedCheckZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask) {
    fb_quadlet_t result;
    readGlobalReg(offset, &result);
    return ((result & mask) == 0);
}
/**
 * @brief performs a masked bit register not equal to 0 check on the global parameter space
 * @return true if readGlobalReg(offset) & mask != 0
 */
bool
DiceAvDevice::maskedCheckNotZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask) {
    return !maskedCheckZeroGlobalReg(offset, mask);
}

DiceAvDevice::diceNameVector
DiceAvDevice::getTxNameString(unsigned int i) {
    diceNameVector names;
    char namestring[DICE_TX_NAMES_SIZE+1];

    if (!readTxRegBlock(i, DICE_REGISTER_TX_NAMES_BASE,
                        (fb_quadlet_t *)namestring, DICE_TX_NAMES_SIZE)) {
        debugError("Could not read TX name string \n");
        return names;
    }

    namestring[DICE_TX_NAMES_SIZE]='\0';
    return splitNameString(std::string(namestring));
}

DiceAvDevice::diceNameVector
DiceAvDevice::getRxNameString(unsigned int i) {
    diceNameVector names;
    char namestring[DICE_RX_NAMES_SIZE+1];

    if (!readRxRegBlock(i, DICE_REGISTER_RX_NAMES_BASE,
                        (fb_quadlet_t *)namestring, DICE_RX_NAMES_SIZE)) {
        debugError("Could not read RX name string \n");
        return names;
    }

    namestring[DICE_RX_NAMES_SIZE]='\0';
    return splitNameString(std::string(namestring));
}

DiceAvDevice::diceNameVector
DiceAvDevice::getClockSourceNameString() {
    diceNameVector names;
    char namestring[DICE_CLOCKSOURCENAMES_SIZE+1];

    if (!readGlobalRegBlock(DICE_REGISTER_GLOBAL_CLOCKSOURCENAMES,
                        (fb_quadlet_t *)namestring, DICE_CLOCKSOURCENAMES_SIZE)) {
        debugError("Could not read CLOCKSOURCE name string \n");
        return names;
    }

    namestring[DICE_CLOCKSOURCENAMES_SIZE]='\0';
    return splitNameString(std::string(namestring));
}

std::string
DiceAvDevice::getDeviceNickName() {
    char namestring[DICE_NICK_NAME_SIZE+1];

    if (!readGlobalRegBlock(DICE_REGISTER_GLOBAL_NICK_NAME,
                        (fb_quadlet_t *)namestring, DICE_NICK_NAME_SIZE)) {
        debugError("Could not read nickname string \n");
        return std::string("(unknown)");
    }

    namestring[DICE_NICK_NAME_SIZE]='\0';
    return std::string(namestring);
}

bool
DiceAvDevice::setDeviceNickName(std::string name) {
    char namestring[DICE_NICK_NAME_SIZE+1];
    strncpy(namestring, name.c_str(), DICE_NICK_NAME_SIZE);

    if (!writeGlobalRegBlock(DICE_REGISTER_GLOBAL_NICK_NAME,
                        (fb_quadlet_t *)namestring, DICE_NICK_NAME_SIZE)) {
        debugError("Could not write nickname string \n");
        return false;
    }
    return true;
}

DiceAvDevice::diceNameVector
DiceAvDevice::splitNameString(std::string in) {
    diceNameVector names;

    // find the end of the string
    unsigned int end=in.find(string("\\\\"));
    // cut the end
    in=in.substr(0,end);

    unsigned int cut;
    while( (cut = in.find(string("\\"))) != in.npos ) {
        if(cut > 0) {
            names.push_back(in.substr(0,cut));
        }
        in = in.substr(cut+1);
    }
    if(in.length() > 0) {
        names.push_back(in);
    }
    return names;
}


// I/O routines
bool
DiceAvDevice::initIoFunctions() {

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
DiceAvDevice::readReg(fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08llX\n", offset);

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().read_quadlet( nodeId, addr, result ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012X\n", nodeId, addr);
        return false;
    }

    *result=CondSwap32(*result);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Read result: 0x%08X\n", *result);

    return true;
}

bool
DiceAvDevice::writeReg(fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08llX, data: 0x%08X\n",
        offset, data);

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().write_quadlet( nodeId, addr, CondSwap32(data) ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012X\n", nodeId, addr);
        return false;
    }
    return true;
}

bool
DiceAvDevice::readRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08llX, length %u\n",
        offset, length);

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().read( nodeId, addr, length/4, data ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012llX\n", nodeId, addr);
        return false;
    }

    for(unsigned int i=0;i<length/4;i++) {
        *(data+i)=CondSwap32(*(data+i));
    }

    return true;
}

bool
DiceAvDevice::writeRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08llX, length: %u\n",
        offset, length);

    if(offset >= DICE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=DICE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    fb_quadlet_t data_out[length/4];

    for(unsigned int i=0;i<length/4;i++) {
        data_out[i]=CondSwap32(*(data+i));
    }

    if(!get1394Service().write( nodeId, addr, length/4, data_out ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012llX\n", nodeId, addr);
        return false;
    }

    return true;
}

bool
DiceAvDevice::readGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading global register offset 0x%04llX\n", offset);

    fb_nodeaddr_t offset_gl=globalOffsetGen(offset, sizeof(fb_quadlet_t));
    return readReg(m_global_reg_offset + offset_gl, result);
}

bool
DiceAvDevice::writeGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing global register offset 0x%08llX, data: 0x%08X\n",
        offset, data);

    fb_nodeaddr_t offset_gl=globalOffsetGen(offset, sizeof(fb_quadlet_t));
    return writeReg(m_global_reg_offset + offset_gl, data);
}

bool
DiceAvDevice::readGlobalRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading global register block offset 0x%04llX, length %u bytes\n",
        offset, length);

    fb_nodeaddr_t offset_gl=globalOffsetGen(offset, length);
    return readRegBlock(m_global_reg_offset + offset_gl, data, length);
}

bool
DiceAvDevice::writeGlobalRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing global register block offset 0x%04llX, length %u bytes\n",
        offset, length);

    fb_nodeaddr_t offset_gl=globalOffsetGen(offset, length);
    return writeRegBlock(m_global_reg_offset + offset_gl, data, length);
}

fb_nodeaddr_t
DiceAvDevice::globalOffsetGen(fb_nodeaddr_t offset, size_t length) {

    // registry offsets should always be smaller than 0x7FFFFFFF
    // because otherwise base + offset > 64bit
    if(m_global_reg_offset & 0x80000000) {
        debugError("register offset not initialized yet\n");
        return DICE_INVALID_OFFSET;
    }
    // out-of-range check
    if(offset+length > m_global_reg_offset+m_global_reg_size) {
        debugError("register offset+length too large: 0x%0llX\n", offset + length);
        return DICE_INVALID_OFFSET;
    }

    return offset;
}

bool
DiceAvDevice::readTxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading tx %d register offset 0x%04llX\n", i, offset);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return readReg(m_tx_reg_offset + offset_tx, result);
}

bool
DiceAvDevice::writeTxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing tx %d register offset 0x%08llX, data: 0x%08X\n",
        i, offset, data);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return writeReg(m_tx_reg_offset + offset_tx, data);
}

bool
DiceAvDevice::readTxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading rx register block offset 0x%04llX, length %u bytes\n",
        offset, length);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, length);
    return readRegBlock(m_tx_reg_offset + offset_tx, data, length);
}

bool
DiceAvDevice::writeTxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing rx register block offset 0x%04llX, length %u bytes\n",
        offset, length);

    fb_nodeaddr_t offset_tx=txOffsetGen(i, offset, length);
    return writeRegBlock(m_tx_reg_offset + offset_tx, data, length);
}

fb_nodeaddr_t
DiceAvDevice::txOffsetGen(unsigned int i, fb_nodeaddr_t offset, size_t length) {
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
        debugError("register offset+length too large: 0x%0llX\n", offset_tx + length);
        return DICE_INVALID_OFFSET;
    }

    return offset_tx;
}

bool
DiceAvDevice::readRxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading rx %d register offset 0x%04llX\n", i, offset);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return readReg(m_rx_reg_offset + offset_rx, result);
}

bool
DiceAvDevice::writeRxReg(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing rx register offset 0x%08llX, data: 0x%08X\n",
        offset, data);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, sizeof(fb_quadlet_t));
    return writeReg(m_rx_reg_offset + offset_rx, data);
}

bool
DiceAvDevice::readRxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading rx register block offset 0x%04llX, length %u bytes\n",
        offset, length);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, length);
    return readRegBlock(m_rx_reg_offset + offset_rx, data, length);
}

bool
DiceAvDevice::writeRxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing rx register block offset 0x%04llX, length %u bytes\n",
        offset, length);

    fb_nodeaddr_t offset_rx=rxOffsetGen(i, offset, length);
    return writeRegBlock(m_rx_reg_offset + offset_rx, data, length);
}

fb_nodeaddr_t
DiceAvDevice::rxOffsetGen(unsigned int i, fb_nodeaddr_t offset, size_t length) {
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
        debugError("register offset+length too large: 0x%0llX\n", offset_rx + length);
        return DICE_INVALID_OFFSET;
    }
    return offset_rx;
}


// the notifier

DiceAvDevice::DiceNotifier::DiceNotifier(DiceAvDevice *d, nodeaddr_t start)
 : ARMHandler(start, DICE_NOTIFIER_BLOCK_LENGTH,
              RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
              RAW1394_ARM_WRITE, 0)
 , m_dicedevice(d)
{

}

DiceAvDevice::DiceNotifier::~DiceNotifier()
{

}

}
