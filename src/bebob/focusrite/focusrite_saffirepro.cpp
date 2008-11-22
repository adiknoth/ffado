/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "focusrite_saffirepro.h"
#include "focusrite_cmd.h"

#include "devicemanager.h"
#include "libutil/SystemTimeSource.h"

#include "libutil/ByteSwap.h"
#include <cstring>

namespace BeBoB {
namespace Focusrite {

SaffireProDevice::SaffireProDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FocusriteDevice( d, configRom )
    , m_MixerContainer( NULL )
    , m_ControlContainer( NULL )
    , m_deviceNameControl( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Focusrite::SaffireProDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    addOption(Util::OptionContainer::Option("rebootOnSamplerateChange", true));

    updateClockSources();
}

SaffireProDevice::~SaffireProDevice()
{
    destroyMixer();
}

bool
SaffireProDevice::buildMixer()
{
    bool result=true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a Focusrite SaffirePro mixer...\n");

    destroyMixer();

    // create the mixer object container
    m_MixerContainer = new Control::Container(this, "Mixer");

    if (!m_MixerContainer) {
        debugError("Could not create mixer container...\n");
        return false;
    }

    // output mute controls
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT12, FR_SAFFIREPRO_CMD_BITFIELD_BIT_MUTE,
                "Out12Mute", "Out1/2 Mute", "Output 1/2 Mute"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT34, FR_SAFFIREPRO_CMD_BITFIELD_BIT_MUTE,
                "Out34Mute", "Out3/4 Mute", "Output 3/4 Mute"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT56, FR_SAFFIREPRO_CMD_BITFIELD_BIT_MUTE,
                "Out56Mute", "Out5/6 Mute", "Output 5/6 Mute"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT78, FR_SAFFIREPRO_CMD_BITFIELD_BIT_MUTE,
                "Out78Mute", "Out7/8 Mute", "Output 7/8 Mute"));

    // output front panel hw volume control
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT12, FR_SAFFIREPRO_CMD_BITFIELD_BIT_HWCTRL,
                "Out12HwCtrl", "Out1/2 HwCtrl", "Output 1/2 Front Panel Hardware volume control"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT34, FR_SAFFIREPRO_CMD_BITFIELD_BIT_HWCTRL,
                "Out34HwCtrl", "Out3/4 HwCtrl", "Output 3/4 Front Panel Hardware volume control"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT56, FR_SAFFIREPRO_CMD_BITFIELD_BIT_HWCTRL,
                "Out56HwCtrl", "Out5/6 HwCtrl", "Output 5/6 Front Panel Hardware volume control"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT78, FR_SAFFIREPRO_CMD_BITFIELD_BIT_HWCTRL,
                "Out78HwCtrl", "Out7/8 HwCtrl", "Output 7/8 Front Panel Hardware volume control"));

    // output active monitor padding
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT12, FR_SAFFIREPRO_CMD_BITFIELD_BIT_PAD,
                "Out12Pad", "Out1/2 Pad", "Output 1/2 Active Monitor Pad"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT34, FR_SAFFIREPRO_CMD_BITFIELD_BIT_PAD,
                "Out34Pad", "Out3/4 Pad", "Output 3/4 Active Monitor Pad"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT56, FR_SAFFIREPRO_CMD_BITFIELD_BIT_PAD,
                "Out56Pad", "Out5/6 Pad", "Output 5/6 Active Monitor Pad"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT78, FR_SAFFIREPRO_CMD_BITFIELD_BIT_PAD,
                "Out78Pad", "Out7/8 Pad", "Output 7/8 Active Monitor Pad"));

    // output level dim
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT12, FR_SAFFIREPRO_CMD_BITFIELD_BIT_DIM,
                "Out12Dim", "Out1/2 Dim", "Output 1/2 Level Dim"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT34, FR_SAFFIREPRO_CMD_BITFIELD_BIT_DIM,
                "Out34Dim", "Out3/4 Dim", "Output 3/4 Level Dim"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT56, FR_SAFFIREPRO_CMD_BITFIELD_BIT_DIM,
                "Out56Dim", "Out5/6 Dim", "Output 5/6 Level Dim"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT78, FR_SAFFIREPRO_CMD_BITFIELD_BIT_DIM,
                "Out78Dim", "Out7/8 Dim", "Output 7/8 Level Dim"));

    // front panel dial position
    result &= m_MixerContainer->addElement(
        new DialPositionControl(*this, 
                FR_SAFFIREPRO_CMD_ID_MONITOR_DIAL, 0,
                "MonitorDial", "Monitor Dial", "Monitor Dial Value"));

    // direct monitoring controls
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH1,
                "DirectMonitorCH1", "Direct Monitor CH1", "Enable Direct Monitor on Channel 1"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH2,
                "DirectMonitorCH2", "Direct Monitor CH2", "Enable Direct Monitor on Channel 2"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH3,
                "DirectMonitorCH3", "Direct Monitor CH3", "Enable Direct Monitor on Channel 3"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH4,
                "DirectMonitorCH4", "Direct Monitor CH4", "Enable Direct Monitor on Channel 4"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH5,
                "DirectMonitorCH5", "Direct Monitor CH5", "Enable Direct Monitor on Channel 5"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH6,
                "DirectMonitorCH6", "Direct Monitor CH6", "Enable Direct Monitor on Channel 6"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH7,
                "DirectMonitorCH7", "Direct Monitor CH7", "Enable Direct Monitor on Channel 7"));
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_BITFIELD, FR_SAFFIREPRO_CMD_ID_DIRECT_MONITORING_CH8,
                "DirectMonitorCH8", "Direct Monitor CH8", "Enable Direct Monitor on Channel 8"));

    // output level controls
    result &= m_MixerContainer->addElement(
        new VolumeControlLowRes(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT12, 0,
                "Out12Level", "Out1/2 Level", "Output 1/2 Level"));
    result &= m_MixerContainer->addElement(
        new VolumeControlLowRes(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT34, 0,
                "Out34Level", "Out3/4 Level", "Output 3/4 Level"));
    result &= m_MixerContainer->addElement(
        new VolumeControlLowRes(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT56, 0,
                "Out56Level", "Out5/6 Level", "Output 5/6 Level"));
    result &= m_MixerContainer->addElement(
        new VolumeControlLowRes(*this, 
                FR_SAFFIREPRO_CMD_ID_BITFIELD_OUT78, 0,
                "Out78Level", "Out7/8 Level", "Output 7/8 Level"));

    // indicators
    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_MUTE_INDICATOR, 0,
                "Out12MuteInd", "Out1/2 Mute Ind", "Output 1/2 Mute Indicator"));

    result &= m_MixerContainer->addElement(
        new BinaryControl(*this, 
                FR_SAFFIREPRO_CMD_ID_DIM_INDICATOR, 0,
                "Out12DimInd", "Out1/2 Dim Ind", "Output 1/2 Level Dim Indicator"));

    // matrix mix controls
    result &= m_MixerContainer->addElement(
        new SaffireProMatrixMixer(*this, SaffireProMatrixMixer::eMMT_InputMix, "InputMix"));

    result &= m_MixerContainer->addElement(
        new SaffireProMatrixMixer(*this, SaffireProMatrixMixer::eMMT_OutputMix, "OutputMix"));

    if (!result) {
        debugWarning("One or more mixer control elements could not be created.");
        // clean up those that couldn't be created
        destroyMixer();
        return false;
    }
    if (!addElement(m_MixerContainer)) {
        debugWarning("Could not register mixer to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    // special controls
    m_ControlContainer = new Control::Container(this, "Control");
    if (!m_ControlContainer) {
        debugError("Could not create mixer container...\n");
        return false;
    }

    // create control objects for the saffire pro
    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_PHANTOM14, 0,
                 "Phantom_1to4", "Phantom 1-4", "Switch Phantom Power on channels 1-4"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_PHANTOM58, 0,
                 "Phantom_5to8", "Phantom 5-8", "Switch Phantom Power on channels 5-8"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_INSERT1, 0,
                "Insert1", "Insert 1", "Switch Insert on Channel 1"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_INSERT2, 0,
                "Insert2", "Insert 2", "Switch Insert on Channel 2"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_AC3_PASSTHROUGH, 0,
                "AC3pass", "AC3 Passtrough", "Enable AC3 Passthrough"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_MIDI_TRU, 0,
                "MidiTru", "Midi Tru", "Enable Midi Tru"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_AVC_MODEL, 0,
                "ADATDisable", "ADAT Disable", "Disable the ADAT I/O's"));

    result &= m_ControlContainer->addElement(
        new BinaryControl(*this, FR_SAFFIREPRO_CMD_ID_AVC_MODEL_MIDI, 0,
                "MIDIEnable", "MIDI Enable", "Enable the MIDI I/O's"));

    result &= m_ControlContainer->addElement(
        new SaffireProDeviceStandaloneEnum(*this,
                "StandaloneConfig", "Standalone Config", "Choose Standalone Configuration"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_Reboot,
            "Reboot", "Reboot", "Reboot Device"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_FlashLed,
            "FlashLed", "Flash Led", "Flash power led"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_UseHighVoltageRail,
            "UseHighVoltageRail", "Use High Supply", "Prefer the high voltage power supply rail"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_ExitStandalone,
            "ExitStandalone", "Exit Standalone mode", "Try to leave standalonbe mode"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_PllLockRange,
            "PllLockRange", "PLL Lock Range", "Get/Set PLL Lock range"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_SaveSettings,
            "SaveSettings", "Save settings to Flash", "Save the current mixer settings to flash memory"));

    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_EnableADAT1,
            "EnableAdat1", "Enable ADAT 1", "Enable/disable ADAT channel 1"));
    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_EnableADAT2,
            "EnableAdat2", "Enable ADAT 2", "Enable/disable ADAT channel 2"));
    result &= m_ControlContainer->addElement(
        new SaffireProMultiControl(*this, SaffireProMultiControl::eTCT_EnableSPDIF,
            "EnableSPDIF1", "Enable S/PDIF 1", "Enable/disable S/PDIF channel"));

    m_deviceNameControl = new SaffireProDeviceNameControl(*this,
            "DeviceName", "Flash Device Name", "Device name stored in flash memory");
    result &= m_ControlContainer->addElement(m_deviceNameControl);

    // add a direct register access element
    result &= addElement(new RegisterControl(*this, "Register", "Register Access", "Direct register access"));

    if (!result) {
        debugWarning("One or more device control elements could not be created.");
        // clean up those that couldn't be created
        destroyMixer();
        return false;
    }
    if (!addElement(m_ControlContainer)) {
        debugWarning("Could not register controls to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    return true;
}

bool
SaffireProDevice::destroyMixer()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "destroy mixer...\n");
    
    if (m_MixerContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no mixer to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_MixerContainer)) {
        debugError("Mixer present but not registered to the avdevice\n");
        return false;
    }
    
    // remove and delete (as in free) child control elements
    m_MixerContainer->clearElements(true);
    delete m_MixerContainer;
    m_MixerContainer = NULL;

    // remove control container
    if (m_ControlContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no controls to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_ControlContainer)) {
        debugError("Controls present but not registered to the avdevice\n");
        return false;
    }
    
    // remove and delete (as in free) child control elements
    m_ControlContainer->clearElements(true);
    delete m_ControlContainer;
    m_ControlContainer = NULL;

    return true;
}

void
SaffireProDevice::updateClockSources() {
    m_active_clocksource = &m_internal_clocksource;

    m_internal_clocksource.type = FFADODevice::eCT_Internal;
    m_internal_clocksource.active = false;
    m_internal_clocksource.valid = true;
    m_internal_clocksource.locked = true;
    m_internal_clocksource.id = FR_SAFFIREPRO_CMD_SYNC_CONFIG_INTERNAL;
    m_internal_clocksource.slipping = false;
    m_internal_clocksource.description = "Internal";

    m_spdif_clocksource.type = FFADODevice::eCT_SPDIF;
    m_spdif_clocksource.active = false;
    m_spdif_clocksource.valid = true;
    m_spdif_clocksource.locked = false;
    m_spdif_clocksource.id = FR_SAFFIREPRO_CMD_SYNC_CONFIG_SPDIF;
    m_spdif_clocksource.slipping = false;
    m_spdif_clocksource.description = "S/PDIF";

    m_wordclock_clocksource.type = FFADODevice::eCT_WordClock;
    m_wordclock_clocksource.active = false;
    m_wordclock_clocksource.valid = true;
    m_wordclock_clocksource.locked = false;
    m_wordclock_clocksource.id = FR_SAFFIREPRO_CMD_SYNC_CONFIG_WORDCLOCK;
    m_wordclock_clocksource.slipping = false;
    m_wordclock_clocksource.description = "WordClock";

    if(isPro26()) {
        m_adat1_clocksource.type = FFADODevice::eCT_ADAT;
        m_adat1_clocksource.active = false;
        m_adat1_clocksource.valid = true;
        m_adat1_clocksource.locked = false;
        m_adat1_clocksource.id = FR_SAFFIREPRO_CMD_SYNC_CONFIG_ADAT1;
        m_adat1_clocksource.slipping = false;
        m_adat1_clocksource.description = "ADAT 1";

        m_adat2_clocksource.type = FFADODevice::eCT_ADAT;
        m_adat2_clocksource.active = false;
        m_adat2_clocksource.valid = true;
        m_adat2_clocksource.locked = false;
        m_adat2_clocksource.id = FR_SAFFIREPRO_CMD_SYNC_CONFIG_ADAT2;
        m_adat2_clocksource.slipping = false;
        m_adat2_clocksource.description = "ADAT 2";
    }

    // figure out the active source
    uint32_t sync;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_SYNC_CONFIG, &sync ) ){
        debugError( "getSpecificValue failed\n" );
        m_internal_clocksource.active=true;
        return;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "SYNC_CONFIG field value: %08lX\n", sync );

    switch(sync & FR_SAFFIREPRO_CMD_ID_SYNC_CONFIG_MASK) {
        default:
            debugWarning( "Unexpected SYNC_CONFIG field value: %08lX\n", sync );
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_INTERNAL:
            m_internal_clocksource.active=true;
            m_active_clocksource = &m_internal_clocksource;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_SPDIF:
            m_spdif_clocksource.active=true;
            m_active_clocksource = &m_spdif_clocksource;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_ADAT1:
            m_adat1_clocksource.active=true;
            m_active_clocksource = &m_adat1_clocksource;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_ADAT2:
            m_adat2_clocksource.active=true;
            m_active_clocksource = &m_adat2_clocksource;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_WORDCLOCK:
            m_wordclock_clocksource.active=true;
            m_active_clocksource = &m_wordclock_clocksource;
            break;
    }
    switch((sync && FR_SAFFIREPRO_CMD_ID_SYNC_LOCK_MASK) >> 8) {
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_INTERNAL:
            // always locked
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_SPDIF:
            m_spdif_clocksource.locked=true;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_ADAT1:
            m_adat1_clocksource.locked=true;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_ADAT2:
            m_adat2_clocksource.locked=true;
            break;
        case FR_SAFFIREPRO_CMD_SYNC_CONFIG_WORDCLOCK:
            m_wordclock_clocksource.locked=true;
            break;
        default:
            debugWarning( "Unexpected SYNC_CONFIG_STATE field value: %08lX\n", sync );
    }
}

FFADODevice::ClockSource
SaffireProDevice::getActiveClockSource()
{
    updateClockSources();
    return *m_active_clocksource;
}

bool
SaffireProDevice::setActiveClockSource(ClockSource s)
{
    // prevent busresets from being handled immediately
    getDeviceManager().lockBusResetHandler();
    unsigned int gen_before = get1394Service().getGeneration();

    debugOutput(DEBUG_LEVEL_VERBOSE, "set active source to %d...\n", s.id);
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_SYNC_CONFIG, s.id ) ){
        debugError( "setSpecificValue failed\n" );
        getDeviceManager().unlockBusResetHandler();
        return false;
    }

    // the device can do a bus reset at this moment
    Util::SystemTimeSource::SleepUsecRelative(1000 * 1000);
    if(!get1394Service().waitForBusResetStormToEnd(10, 2000)) {
        debugWarning("Device doesn't stop bus-resetting\n");
    }
    unsigned int gen_after = get1394Service().getGeneration();
    debugOutput(DEBUG_LEVEL_VERBOSE, " gen: %d=>%d\n", gen_before, gen_after);

    getDeviceManager().unlockBusResetHandler();
    return true;
}

FFADODevice::ClockSourceVector
SaffireProDevice::getSupportedClockSources()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "listing...\n");
    FFADODevice::ClockSourceVector r;
    r.push_back(m_internal_clocksource);
    r.push_back(m_spdif_clocksource);
    r.push_back(m_wordclock_clocksource);
    if(isPro26()) {
        r.push_back(m_adat1_clocksource);
        r.push_back(m_adat2_clocksource);
    }
    return r;
}

std::vector<int>
SaffireProDevice::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    frequencies.push_back(44100);
    frequencies.push_back(48000);
    frequencies.push_back(88200);
    frequencies.push_back(96000);
    frequencies.push_back(176400);
    frequencies.push_back(192000);
    return frequencies;
}

uint16_t
SaffireProDevice::getConfigurationIdSyncMode()
{
    uint32_t sync;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_SYNC_CONFIG, &sync ) ){
        debugError( "getSpecificValue failed\n" );
        return 0xFFFF;
    }
    return sync & 0xFFFF;
}

uint64_t
SaffireProDevice::getConfigurationId()
{
    // have the generic mechanism create a unique configuration id.
    uint64_t id = BeBoB::AvDevice::getConfigurationId();

    // there are some parts that can be enabled/disabled and
    // that have influence on the AV/C model and channel config
    // so add them to the config id
    #if 0
    // FIXME: doesn't seem to be working, but the channel count
    //        makes that it's not that important
    if(getEnableDigitalChannel(eDC_SPDIF)) {
        id |= 1ULL << 40;
    }
    if(isPro26()) {
        if(getEnableDigitalChannel(eDC_ADAT1)) {
            id |= 1ULL << 41;
        }
        if(getEnableDigitalChannel(eDC_ADAT2)) {
            id |= 1ULL << 42;
        }
    }
    #endif
    return id;
}

bool
SaffireProDevice::setNickname( std::string name)
{
    if(m_deviceNameControl) {
        return m_deviceNameControl->setValue(name);
    } else return false;
}

std::string
SaffireProDevice::getNickname()
{
    if(m_deviceNameControl) {
        return m_deviceNameControl->getValue();
    } else return "Unknown";
}

bool
SaffireProDevice::canChangeNickname()
{
    return true;
}

void
SaffireProDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a BeBoB::Focusrite::SaffireProDevice\n");
    FocusriteDevice::showDevice();
}

void
SaffireProDevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );

    if (m_MixerContainer) m_MixerContainer->setVerboseLevel(l);

    // FIXME: add the other elements here too

    FocusriteDevice::setVerboseLevel(l);
}

int
SaffireProDevice::getSamplingFrequency( ) {
    uint32_t sr;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_SAMPLERATE, &sr ) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    }
    
    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "getSampleRate: %d\n", sr );

    return convertDefToSr(sr);
}

bool
SaffireProDevice::setSamplingFrequencyDo( uint32_t value )
{
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_SAMPLERATE, value) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    }
    return true;
}

bool
SaffireProDevice::setSamplingFrequencyDoNoReboot( uint32_t value )
{
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_SAMPLERATE_NOREBOOT, value) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    }
    return true;
}

bool
SaffireProDevice::setSamplingFrequency( int s )
{
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    bool rebootOnSamplerateChange=false;
    if(!getOption("rebootOnSamplerateChange", rebootOnSamplerateChange)) {
        debugWarning("Could not retrieve rebootOnSamplerateChange parameter, defauling to false\n");
    }

    if(snoopMode) {
        if (s != getSamplingFrequency()) {
            debugError("In snoop mode it is impossible to set the sample rate.\n");
            debugError("Please start the client with the correct setting.\n");
            return false;
        }
        return true;
    } else {
        uint32_t value = convertSrToDef(s);
        if ( value == 0 ) {
            debugError("Unsupported samplerate: %u\n", s);
            return false;
        }
    
        if (s == getSamplingFrequency()) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "No need to change samplerate\n");
            return true;
        }

        const int max_tries = 2;
        int ntries = max_tries+1;

        // the device behaves like a pig when changing samplerate,
        // generating a bunch of bus-resets.
        // we don't want the busreset handler to run while we are
        // changing the samplerate. however it has to run after the
        // device finished, since the bus resets might have influenced
        // other attached devices.
        getDeviceManager().lockBusResetHandler();
        unsigned int gen_before = get1394Service().getGeneration();

        while(--ntries) {
            if (rebootOnSamplerateChange) {
                debugOutput( DEBUG_LEVEL_VERBOSE, "Setting samplerate with reboot\n");
                if(!setSamplingFrequencyDo( value )) {
                    debugWarning("setSamplingFrequencyDo failed\n");
                }

                debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for device to finish rebooting...\n");

                // the device needs quite some time to reboot
                Util::SystemTimeSource::SleepUsecRelative(2 * 1000 * 1000);

                int timeout = 5; // multiples of 1s
                // wait for a busreset to occur
                while ((gen_before == get1394Service().getGeneration())
                       && --timeout)
                {
                    // wait for a while
                    Util::SystemTimeSource::SleepUsecRelative(1000 * 1000);
                }
                if (!timeout) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, "Device did not reset itself, forcing reboot...\n");
                    rebootDevice();

                    // the device needs quite some time to reboot
                    Util::SystemTimeSource::SleepUsecRelative(6 * 1000 * 1000);

                    // wait for the device to finish the reboot
                    timeout = 10; // multiples of 1s
                    while ((gen_before == get1394Service().getGeneration())
                           && --timeout)
                    {
                        // wait for a while
                        Util::SystemTimeSource::SleepUsecRelative(1000 * 1000);
                    }
                    if (!timeout) {
                        debugError( "Device did not reset itself after forced reboot...\n");
                        getDeviceManager().unlockBusResetHandler();
                        return false;
                    }
                }

                // so we know the device is rebooting
                // now wait until it stops generating busresets
                if(!get1394Service().waitForBusResetStormToEnd(20, 4000)) {
                    debugError("The device keeps behaving like a pig...\n");
                    getDeviceManager().unlockBusResetHandler();
                    return false;
                }
                debugOutput(DEBUG_LEVEL_VERBOSE, "Device available (gen: %u => %u)...\n", 
                    gen_before, get1394Service().getGeneration());

                // wait some more
                Util::SystemTimeSource::SleepUsecRelative(1 * 1000 * 1000);

                // update the generation of the 1394 service
                get1394Service().updateGeneration();

                // update our config rom since it might have changed
                // if this fails it means we have disappeared from the bus
                // that's bad.
                if(!getConfigRom().updatedNodeId()) {
                    debugError("Could not update node id\n");
                    getDeviceManager().unlockBusResetHandler();
                    return false;
                }

                // we have to rediscover the device
                if (discover()) break;
            } else {
                debugOutput( DEBUG_LEVEL_VERBOSE, "Setting samplerate without reboot\n");
                if(!setSamplingFrequencyDoNoReboot( value )) {
                    debugWarning("setSamplingFrequencyDoNoReboot failed\n");
                }
            }

            int verify = getSamplingFrequency();
            debugOutput( DEBUG_LEVEL_VERBOSE,
                        "setSampleRate (try %d): requested samplerate %d, device now has %d\n", 
                        max_tries-ntries, s, verify );

            if (s == verify) {
                break;
            }
            debugOutput( DEBUG_LEVEL_VERBOSE, "setSampleRate (try %d) failed. Try again...\n" );
        }

        // make the busreset handlers run
        getDeviceManager().unlockBusResetHandler();

        if (ntries==0) {
            debugError("Setting samplerate failed...\n");
            return false;
        }
        return true;
    }
    // not executable
    return false;
}

void
SaffireProDevice::rebootDevice() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "rebooting device...\n" );
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_REBOOT, 
                           FR_SAFFIREPRO_CMD_REBOOT_CODE ) ) {
        debugError( "setSpecificValue failed\n" );
    }
}

void
SaffireProDevice::exitStandalone() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "exit standalone mode...\n" );
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_EXIT_STANDALONE, 
                           FR_SAFFIREPRO_CMD_EXIT_STANDALONE_CODE ) ) {
        debugError( "setSpecificValue failed\n" );
    }
}

void
SaffireProDevice::saveSettings() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "saving settings on device...\n" );
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_SAVE_SETTINGS,
                           FR_SAFFIREPRO_CMD_REBOOT_CODE ) ) { // FIXME: is this correct?
        debugError( "setSpecificValue failed\n" );
    }
}

void
SaffireProDevice::flashLed() {
    int ledFlashDuration = 2;
    if(!getOption("ledFlashDuration", ledFlashDuration)) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Could not retrieve ledFlashDuration parameter, defaulting to 2sec\n");
    }
    int ledFlashFrequency = 10;
    if(!getOption("ledFlashFrequency", ledFlashFrequency)) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Could not retrieve ledFlashFrequency parameter, defaulting to 10Hz\n");
    }

    uint32_t reg = 0;
    debugOutput( DEBUG_LEVEL_VERBOSE, "flashing led ...\n" );
    
    reg = FR_SAFFIREPRO_CMD_SET_FLASH_SECS(reg, ledFlashDuration);
    reg = FR_SAFFIREPRO_CMD_SET_FLASH_FREQ(reg, ledFlashFrequency);
    
    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_FLASH_LED, 
                           reg ) ) {
        debugError( "setSpecificValue failed\n" );
    }
}

bool
SaffireProDevice::isAudioOn() {
    uint32_t ready;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_AUDIO_ON, &ready ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "isAudioOn: %d\n", ready!=0 );
    return ready != 0;
}

bool
SaffireProDevice::isExtClockLocked() {
    uint32_t ready;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_EXT_CLOCK_LOCK, &ready ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "isExtClockLocked: %d\n", ready!=0 );
    return ready != 0;
}

uint32_t
SaffireProDevice::getCount32() {
    uint32_t v;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_PLAYBACK_COUNT, &v ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "getCount32: %08lX\n", v );
    return v;
}

void
SaffireProDevice::useHighVoltageRail(bool useIt) {
    uint32_t reg=useIt;
    debugOutput( DEBUG_LEVEL_VERBOSE, "%s high voltage rail ...\n",
        (useIt?"Using":"Not using") );

    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_USE_HIGHVOLTAGE_RAIL, 
                           reg ) ) {
        debugError( "setSpecificValue failed\n" );
    }
}

bool
SaffireProDevice::usingHighVoltageRail() {
    uint32_t retval;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_USING_HIGHVOLTAGE_RAIL, &retval ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "usingHighVoltageRail: %d\n", retval!=0 );
    return retval != 0;
}

void
SaffireProDevice::setPllLockRange(unsigned int i) {
    uint32_t reg=i;
    debugOutput( DEBUG_LEVEL_VERBOSE, "set PLL lock range: %d ...\n", i );

    if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_PLL_LOCK_RANGE, 
                           reg ) ) {
        debugError( "setSpecificValue failed\n" );
    }
}

unsigned int
SaffireProDevice::getPllLockRange() {
    uint32_t retval;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_PLL_LOCK_RANGE, &retval ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "PLL lock range: %d\n", retval );
    return retval;
}

bool
SaffireProDevice::isMidiEnabled() {
    uint32_t ready;
    if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_AVC_MODEL_MIDI, &ready ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "isMidiEnabled: %d\n", ready != 0 );
    return ready != 0;
}

unsigned int
SaffireProDevice::getEnableDigitalChannel(enum eDigitalChannel c) {
    uint32_t retval;
    unsigned int id;
    switch(c) {
        default:
        case eDC_ADAT1: id=FR_SAFFIREPRO_CMD_ID_ENABLE_ADAT1_INPUT; break;
        case eDC_ADAT2: id=FR_SAFFIREPRO_CMD_ID_ENABLE_ADAT2_INPUT; break;
        case eDC_SPDIF: id=FR_SAFFIREPRO_CMD_ID_ENABLE_SPDIF_INPUT; break;
    }
    if ( !getSpecificValue(id, &retval ) ) {
        debugError( "getSpecificValue failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "get dig channel %d: %d\n", c, retval);
    return retval;
}

void
SaffireProDevice::setEnableDigitalChannel(enum eDigitalChannel c, unsigned int i) {
    uint32_t reg=i;
    unsigned int id;
    switch(c) {
        default:
        case eDC_ADAT1: id=FR_SAFFIREPRO_CMD_ID_ENABLE_ADAT1_INPUT; break;
        case eDC_ADAT2: id=FR_SAFFIREPRO_CMD_ID_ENABLE_ADAT2_INPUT; break;
        case eDC_SPDIF: id=FR_SAFFIREPRO_CMD_ID_ENABLE_SPDIF_INPUT; break;
    }
    debugOutput( DEBUG_LEVEL_VERBOSE, "set dig channel %d: %d...\n", c, i );

    if ( !setSpecificValue(id, reg ) ) {
        debugError( "setSpecificValue failed\n" );
    }
}

bool
SaffireProDevice::setDeviceName(std::string n) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "set device name : %s ...\n", n.c_str() );

    uint32_t tmp;
    char name[16]; // the device name field length is fixed
    memset(name, 0, 16);
    
    unsigned int nb_chars = n.size();
    if(nb_chars > 16) {
        debugWarning("Specified name too long: %s\n", n.c_str());
        nb_chars = 16;
    }
    
    unsigned int i;
    for(i=0; i<nb_chars; i++) {
        name[i] = n.at(i);
    }

    for (i=0; i<4; i++) {
        char *ptr = (char *) &name[i*4];
        tmp = *((uint32_t *)ptr);
        tmp = CondSwapToBus32(tmp);
        if ( !setSpecificValue(FR_SAFFIREPRO_CMD_ID_DEVICE_NAME_1 + i, tmp ) ) {
            debugError( "setSpecificValue failed\n" );
            return false;
        }
    }
    return true;
}

std::string
SaffireProDevice::getDeviceName() {
    std::string retval="";
    uint32_t tmp;
    unsigned int i;
    for (i=0; i<4; i++) {
        if ( !getSpecificValue(FR_SAFFIREPRO_CMD_ID_DEVICE_NAME_1 + i, &tmp ) ) {
            debugError( "getSpecificValue failed\n" );
            return "";
        }
        tmp = CondSwapFromBus32(tmp);
        unsigned int j;
        char *ptr = (char *) &tmp;
        for (j=0; j<4; j++) {
            retval += *ptr;
            ptr++;
        }
    }
    debugOutput( DEBUG_LEVEL_VERBOSE,
                     "device name: %s\n",  retval.c_str() );
    return retval;
}

// swiss army knife control element
SaffireProMultiControl::SaffireProMultiControl(SaffireProDevice& parent, enum eMultiControlType t)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_type ( t )
{}
SaffireProMultiControl::SaffireProMultiControl(SaffireProDevice& parent, enum eMultiControlType t,
                std::string name, std::string label, std::string descr)
: Control::Discrete(&parent)
, m_Parent(parent)
, m_type ( t )
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
SaffireProMultiControl::setValue(int v)
{
    switch (m_type) {
        case eTCT_Reboot: m_Parent.rebootDevice(); return true;
        case eTCT_FlashLed: m_Parent.flashLed(); return true;
        case eTCT_UseHighVoltageRail: m_Parent.useHighVoltageRail(v); return true;
        case eTCT_ExitStandalone: m_Parent.exitStandalone(); return true;
        case eTCT_PllLockRange: m_Parent.setPllLockRange(v); return true;
        case eTCT_SaveSettings: m_Parent.saveSettings(); return true;
        case eTCT_EnableADAT1: m_Parent.setEnableDigitalChannel(SaffireProDevice::eDC_ADAT1, v); return true;
        case eTCT_EnableADAT2: m_Parent.setEnableDigitalChannel(SaffireProDevice::eDC_ADAT2, v); return true;
        case eTCT_EnableSPDIF: m_Parent.setEnableDigitalChannel(SaffireProDevice::eDC_SPDIF, v); return true;
    }
    return false;
}

int
SaffireProMultiControl::getValue()
{
    switch (m_type) {
        case eTCT_Reboot: return 0;
        case eTCT_FlashLed: return 0;
        case eTCT_UseHighVoltageRail: return m_Parent.usingHighVoltageRail();
        case eTCT_ExitStandalone: return 0;
        case eTCT_PllLockRange: return m_Parent.getPllLockRange();
        case eTCT_SaveSettings: return 0;
        case eTCT_EnableADAT1: return m_Parent.getEnableDigitalChannel(SaffireProDevice::eDC_ADAT1);
        case eTCT_EnableADAT2: return m_Parent.getEnableDigitalChannel(SaffireProDevice::eDC_ADAT2);
        case eTCT_EnableSPDIF: return m_Parent.getEnableDigitalChannel(SaffireProDevice::eDC_SPDIF);
    }
    return -1;
}

// -- device name

SaffireProDeviceNameControl::SaffireProDeviceNameControl(SaffireProDevice& parent)
: Control::Text(&parent)
, m_Parent(parent)
{}
SaffireProDeviceNameControl::SaffireProDeviceNameControl(SaffireProDevice& parent,
                std::string name, std::string label, std::string descr)
: Control::Text(&parent)
, m_Parent(parent)
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
SaffireProDeviceNameControl::setValue(std::string v)
{
    return m_Parent.setDeviceName(v);
}

std::string
SaffireProDeviceNameControl::getValue()
{
    return m_Parent.getDeviceName();
}

// -- standalone config
SaffireProDeviceStandaloneEnum::SaffireProDeviceStandaloneEnum(SaffireProDevice& parent)
: Control::Enum(&parent)
, m_Parent(parent)
{}
SaffireProDeviceStandaloneEnum::SaffireProDeviceStandaloneEnum(SaffireProDevice& parent,
                std::string name, std::string label, std::string descr)
: Control::Enum(&parent)
, m_Parent(parent)
{
    setName(name);
    setLabel(label);
    setDescription(descr);
}

bool
SaffireProDeviceStandaloneEnum::select(int idx)
{
    if(idx>1) {
        debugError("Index (%d) out of range\n", idx);
        return false;
    }
    if(!m_Parent.setSpecificValue(FR_SAFFIREPRO_CMD_ID_STANDALONE_MODE, idx)) {
        debugError("Could not set selected mode\n");
        return false;
    } else {
        return true;
    }
}

int
SaffireProDeviceStandaloneEnum::selected()
{
    uint32_t sel=0;
    if(!m_Parent.getSpecificValue(FR_SAFFIREPRO_CMD_ID_STANDALONE_MODE, &sel)) {
        debugError("Could not get selected mode\n");
        return -1;
    } else {
        return sel;
    }
}

int
SaffireProDeviceStandaloneEnum::count()
{
    return 2;
}

std::string
SaffireProDeviceStandaloneEnum::getEnumLabel(int idx)
{
    if(idx>1) {
        debugError("Index (%d) out of range\n", idx);
        return false;
    }
    switch(idx) {
        case 0: return "Mixing";
        case 1: return "Tracking";
        default:
            debugError("Index (%d) out of range\n", idx);
            return "Error";
    }
}

// Saffire pro matrix mixer element

SaffireProMatrixMixer::SaffireProMatrixMixer(SaffireProDevice& p, 
                                             enum eMatrixMixerType type)
: FocusriteMatrixMixer(p, "MatrixMixer")
, m_type(type)
{
    init();
}

SaffireProMatrixMixer::SaffireProMatrixMixer(SaffireProDevice& p, 
                                             enum eMatrixMixerType type, std::string n)
: FocusriteMatrixMixer(p, n)
, m_type(type)
{
    init();
}

void SaffireProMatrixMixer::init()
{
    if (m_type==eMMT_OutputMix) {
        addSignalInfo(m_RowInfo, "PC1", "PC 1", "PC Channel 1");
        addSignalInfo(m_RowInfo, "PC2", "PC 2", "PC Channel 2");
        addSignalInfo(m_RowInfo, "PC3", "PC 3", "PC Channel 3");
        addSignalInfo(m_RowInfo, "PC4", "PC 4", "PC Channel 4");
        addSignalInfo(m_RowInfo, "PC5", "PC 5", "PC Channel 5");
        addSignalInfo(m_RowInfo, "PC6", "PC 6", "PC Channel 6");
        addSignalInfo(m_RowInfo, "PC7", "PC 7", "PC Channel 7");
        addSignalInfo(m_RowInfo, "PC8", "PC 8", "PC Channel 8");
        addSignalInfo(m_RowInfo, "PC9", "PC 9", "PC Channel 9");
        addSignalInfo(m_RowInfo, "PC10", "PC 10", "PC Channel 10");
        addSignalInfo(m_RowInfo, "IMIXL", "IMix L", "Input Mix Left");
        addSignalInfo(m_RowInfo, "IMIXR", "IMix R", "Input Mix Right");
        
        addSignalInfo(m_ColInfo, "OUT1", "OUT 1", "Output Channel 1");
        addSignalInfo(m_ColInfo, "OUT2", "OUT 2", "Output Channel 2");
        addSignalInfo(m_ColInfo, "OUT3", "OUT 3", "Output Channel 3");
        addSignalInfo(m_ColInfo, "OUT4", "OUT 4", "Output Channel 4");
        addSignalInfo(m_ColInfo, "OUT5", "OUT 5", "Output Channel 5");
        addSignalInfo(m_ColInfo, "OUT6", "OUT 6", "Output Channel 6");
        addSignalInfo(m_ColInfo, "OUT7", "OUT 7", "Output Channel 7");
        addSignalInfo(m_ColInfo, "OUT8", "OUT 8", "Output Channel 8");
        addSignalInfo(m_ColInfo, "OUT9", "OUT 9", "Output Channel 9");
        addSignalInfo(m_ColInfo, "OUT10", "OUT 10", "Output Channel 10");
        
        // init the cell matrix
        #define FOCUSRITE_SAFFIRE_PRO_OUTMIX_NB_COLS 10
        #define FOCUSRITE_SAFFIRE_PRO_OUTMIX_NB_ROWS 12
        
        std::vector<struct sCellInfo> tmp_cols( FOCUSRITE_SAFFIRE_PRO_OUTMIX_NB_COLS );
        std::vector< std::vector<struct sCellInfo> > tmp_all(FOCUSRITE_SAFFIRE_PRO_OUTMIX_NB_ROWS, tmp_cols);
        m_CellInfo = tmp_all;
    
        struct sCellInfo c;
        c.row=-1;
        c.col=-1;
        c.valid=false;
        c.address=0;
        
        for (int i=0;i<FOCUSRITE_SAFFIRE_PRO_OUTMIX_NB_ROWS;i++) {
            for (int j=0;j<FOCUSRITE_SAFFIRE_PRO_OUTMIX_NB_COLS;j++) {
                m_CellInfo[i][j]=c;
            }
        }
    
        // now set the cells that are valid
        setCellInfo(0,0,FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT1, true);
        setCellInfo(1,1,FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT2, true);
        setCellInfo(10,0,FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT1, true);
        setCellInfo(11,1,FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT2, true);
        setCellInfo(0,2,FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT3, true);
        setCellInfo(1,3,FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT4, true);
        setCellInfo(2,2,FR_SAFFIREPRO_CMD_ID_PC3_TO_OUT3, true);
        setCellInfo(3,3,FR_SAFFIREPRO_CMD_ID_PC4_TO_OUT4, true);
        setCellInfo(10,2,FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT3, true);
        setCellInfo(11,3,FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT4, true);
        setCellInfo(0,4,FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT5, true);
        setCellInfo(1,5,FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT6, true);
        setCellInfo(4,4,FR_SAFFIREPRO_CMD_ID_PC5_TO_OUT5, true);
        setCellInfo(5,5,FR_SAFFIREPRO_CMD_ID_PC6_TO_OUT6, true);
        setCellInfo(10,4,FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT5, true);
        setCellInfo(11,5,FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT6, true);
        setCellInfo(0,6,FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT7, true);
        setCellInfo(1,7,FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT8, true);
        setCellInfo(6,6,FR_SAFFIREPRO_CMD_ID_PC7_TO_OUT7, true);
        setCellInfo(7,7,FR_SAFFIREPRO_CMD_ID_PC8_TO_OUT8, true);
        setCellInfo(10,6,FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT7, true);
        setCellInfo(11,7,FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT8, true);
        setCellInfo(0,8,FR_SAFFIREPRO_CMD_ID_PC1_TO_OUT9, true);
        setCellInfo(1,9,FR_SAFFIREPRO_CMD_ID_PC2_TO_OUT10, true);
        setCellInfo(8,8,FR_SAFFIREPRO_CMD_ID_PC9_TO_OUT9, true);
        setCellInfo(9,9,FR_SAFFIREPRO_CMD_ID_PC10_TO_OUT10, true);
        setCellInfo(10,8,FR_SAFFIREPRO_CMD_ID_MIX1_TO_OUT9, true);
        setCellInfo(11,9,FR_SAFFIREPRO_CMD_ID_MIX2_TO_OUT10, true);

    } else if (m_type==eMMT_InputMix) {
        addSignalInfo(m_RowInfo, "AN1", "Analog 1", "Analog Input 1");
        addSignalInfo(m_RowInfo, "AN2", "Analog 2", "Analog Input 2");
        addSignalInfo(m_RowInfo, "AN3", "Analog 3", "Analog Input 3");
        addSignalInfo(m_RowInfo, "AN4", "Analog 4", "Analog Input 4");
        addSignalInfo(m_RowInfo, "AN5", "Analog 5", "Analog Input 5");
        addSignalInfo(m_RowInfo, "AN6", "Analog 6", "Analog Input 6");
        addSignalInfo(m_RowInfo, "AN7", "Analog 7", "Analog Input 7");
        addSignalInfo(m_RowInfo, "AN8", "Analog 8", "Analog Input 8");
        addSignalInfo(m_RowInfo, "SPDIFL", "SPDIF L", "SPDIF Left Input");
        addSignalInfo(m_RowInfo, "SPDIFR", "SPDIF R", "SPDIF Right Input");
        
        addSignalInfo(m_RowInfo, "ADAT11", "ADAT1 1", "ADAT1 Channel 1");
        addSignalInfo(m_RowInfo, "ADAT12", "ADAT1 2", "ADAT1 Channel 2");
        addSignalInfo(m_RowInfo, "ADAT13", "ADAT1 3", "ADAT1 Channel 3");
        addSignalInfo(m_RowInfo, "ADAT14", "ADAT1 4", "ADAT1 Channel 4");
        addSignalInfo(m_RowInfo, "ADAT15", "ADAT1 5", "ADAT1 Channel 5");
        addSignalInfo(m_RowInfo, "ADAT16", "ADAT1 6", "ADAT1 Channel 6");
        addSignalInfo(m_RowInfo, "ADAT17", "ADAT1 7", "ADAT1 Channel 7");
        addSignalInfo(m_RowInfo, "ADAT18", "ADAT1 8", "ADAT1 Channel 8");
        
        addSignalInfo(m_RowInfo, "ADAT21", "ADAT2 1", "ADAT2 Channel 1");
        addSignalInfo(m_RowInfo, "ADAT22", "ADAT2 2", "ADAT2 Channel 2");
        addSignalInfo(m_RowInfo, "ADAT23", "ADAT2 3", "ADAT2 Channel 3");
        addSignalInfo(m_RowInfo, "ADAT24", "ADAT2 4", "ADAT2 Channel 4");
        addSignalInfo(m_RowInfo, "ADAT25", "ADAT2 5", "ADAT2 Channel 5");
        addSignalInfo(m_RowInfo, "ADAT26", "ADAT2 6", "ADAT2 Channel 6");
        addSignalInfo(m_RowInfo, "ADAT27", "ADAT2 7", "ADAT2 Channel 7");
        addSignalInfo(m_RowInfo, "ADAT28", "ADAT2 8", "ADAT2 Channel 8");
        
        addSignalInfo(m_ColInfo, "IMIXL", "IMix L", "Input Mix Left");
        addSignalInfo(m_ColInfo, "IMIXR", "IMix R", "Input Mix Right");
        
        // init the cell matrix
        #define FOCUSRITE_SAFFIRE_PRO_INMIX_NB_COLS 2
        #define FOCUSRITE_SAFFIRE_PRO_INMIX_NB_ROWS 26
        
        std::vector<struct sCellInfo> tmp_cols( FOCUSRITE_SAFFIRE_PRO_INMIX_NB_COLS );
        std::vector< std::vector<struct sCellInfo> > tmp_all(FOCUSRITE_SAFFIRE_PRO_INMIX_NB_ROWS,tmp_cols);
        m_CellInfo = tmp_all;
    
        struct sCellInfo c;
        c.row=-1;
        c.col=-1;
        c.valid=false;
        c.address=0;
        
        for (int i=0;i<FOCUSRITE_SAFFIRE_PRO_INMIX_NB_ROWS;i++) {
            for (int j=0;j<FOCUSRITE_SAFFIRE_PRO_INMIX_NB_COLS;j++) {
                m_CellInfo[i][j]=c;
            }
        }
    
        // now set the cells that are valid
        setCellInfo(0,0,FR_SAFFIREPRO_CMD_ID_AN1_TO_IMIXL, true);
        setCellInfo(0,1,FR_SAFFIREPRO_CMD_ID_AN1_TO_IMIXR, true);
        setCellInfo(1,0,FR_SAFFIREPRO_CMD_ID_AN2_TO_IMIXL, true);
        setCellInfo(1,1,FR_SAFFIREPRO_CMD_ID_AN2_TO_IMIXR, true);
        setCellInfo(2,0,FR_SAFFIREPRO_CMD_ID_AN3_TO_IMIXL, true);
        setCellInfo(2,1,FR_SAFFIREPRO_CMD_ID_AN3_TO_IMIXR, true);
        setCellInfo(3,0,FR_SAFFIREPRO_CMD_ID_AN4_TO_IMIXL, true);
        setCellInfo(3,1,FR_SAFFIREPRO_CMD_ID_AN4_TO_IMIXR, true);
        setCellInfo(4,0,FR_SAFFIREPRO_CMD_ID_AN5_TO_IMIXL, true);
        setCellInfo(4,1,FR_SAFFIREPRO_CMD_ID_AN5_TO_IMIXR, true);
        setCellInfo(5,0,FR_SAFFIREPRO_CMD_ID_AN6_TO_IMIXL, true);
        setCellInfo(5,1,FR_SAFFIREPRO_CMD_ID_AN6_TO_IMIXR, true);
        setCellInfo(6,0,FR_SAFFIREPRO_CMD_ID_AN7_TO_IMIXL, true);
        setCellInfo(6,1,FR_SAFFIREPRO_CMD_ID_AN7_TO_IMIXR, true);
        setCellInfo(7,0,FR_SAFFIREPRO_CMD_ID_AN8_TO_IMIXL, true);
        setCellInfo(7,1,FR_SAFFIREPRO_CMD_ID_AN8_TO_IMIXR, true);
        setCellInfo(8,0,FR_SAFFIREPRO_CMD_ID_SPDIFL_TO_IMIXL, true);
        setCellInfo(8,1,FR_SAFFIREPRO_CMD_ID_SPDIFL_TO_IMIXR, true);
        setCellInfo(9,0,FR_SAFFIREPRO_CMD_ID_SPDIFR_TO_IMIXL, true);
        setCellInfo(9,1,FR_SAFFIREPRO_CMD_ID_SPDIFR_TO_IMIXR, true);

        setCellInfo(10,0,FR_SAFFIREPRO_CMD_ID_ADAT11_TO_IMIXL, true);
        setCellInfo(10,1,FR_SAFFIREPRO_CMD_ID_ADAT11_TO_IMIXR, true);
        setCellInfo(11,0,FR_SAFFIREPRO_CMD_ID_ADAT12_TO_IMIXL, true);
        setCellInfo(11,1,FR_SAFFIREPRO_CMD_ID_ADAT12_TO_IMIXR, true);
        setCellInfo(12,0,FR_SAFFIREPRO_CMD_ID_ADAT13_TO_IMIXL, true);
        setCellInfo(12,1,FR_SAFFIREPRO_CMD_ID_ADAT13_TO_IMIXR, true);
        setCellInfo(13,0,FR_SAFFIREPRO_CMD_ID_ADAT14_TO_IMIXL, true);
        setCellInfo(13,1,FR_SAFFIREPRO_CMD_ID_ADAT14_TO_IMIXR, true);
        setCellInfo(14,0,FR_SAFFIREPRO_CMD_ID_ADAT15_TO_IMIXL, true);
        setCellInfo(14,1,FR_SAFFIREPRO_CMD_ID_ADAT15_TO_IMIXR, true);
        setCellInfo(15,0,FR_SAFFIREPRO_CMD_ID_ADAT16_TO_IMIXL, true);
        setCellInfo(15,1,FR_SAFFIREPRO_CMD_ID_ADAT16_TO_IMIXR, true);
        setCellInfo(16,0,FR_SAFFIREPRO_CMD_ID_ADAT17_TO_IMIXL, true);
        setCellInfo(16,1,FR_SAFFIREPRO_CMD_ID_ADAT17_TO_IMIXR, true);
        setCellInfo(17,0,FR_SAFFIREPRO_CMD_ID_ADAT18_TO_IMIXL, true);
        setCellInfo(17,1,FR_SAFFIREPRO_CMD_ID_ADAT18_TO_IMIXR, true);

        setCellInfo(18,0,FR_SAFFIREPRO_CMD_ID_ADAT21_TO_IMIXL, true);
        setCellInfo(18,1,FR_SAFFIREPRO_CMD_ID_ADAT21_TO_IMIXR, true);
        setCellInfo(19,0,FR_SAFFIREPRO_CMD_ID_ADAT22_TO_IMIXL, true);
        setCellInfo(19,1,FR_SAFFIREPRO_CMD_ID_ADAT22_TO_IMIXR, true);
        setCellInfo(20,0,FR_SAFFIREPRO_CMD_ID_ADAT23_TO_IMIXL, true);
        setCellInfo(20,1,FR_SAFFIREPRO_CMD_ID_ADAT23_TO_IMIXR, true);
        setCellInfo(21,0,FR_SAFFIREPRO_CMD_ID_ADAT24_TO_IMIXL, true);
        setCellInfo(21,1,FR_SAFFIREPRO_CMD_ID_ADAT24_TO_IMIXR, true);
        setCellInfo(22,0,FR_SAFFIREPRO_CMD_ID_ADAT25_TO_IMIXL, true);
        setCellInfo(22,1,FR_SAFFIREPRO_CMD_ID_ADAT25_TO_IMIXR, true);
        setCellInfo(23,0,FR_SAFFIREPRO_CMD_ID_ADAT26_TO_IMIXL, true);
        setCellInfo(23,1,FR_SAFFIREPRO_CMD_ID_ADAT26_TO_IMIXR, true);
        setCellInfo(24,0,FR_SAFFIREPRO_CMD_ID_ADAT27_TO_IMIXL, true);
        setCellInfo(24,1,FR_SAFFIREPRO_CMD_ID_ADAT27_TO_IMIXR, true);
        setCellInfo(25,0,FR_SAFFIREPRO_CMD_ID_ADAT28_TO_IMIXL, true);
        setCellInfo(25,1,FR_SAFFIREPRO_CMD_ID_ADAT28_TO_IMIXR, true);

    } else {
        debugError("Invalid mixer type\n");
    }
}

void SaffireProMatrixMixer::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Saffire Pro Matrix mixer type %d\n");
}

} // Focusrite
} // BeBoB
