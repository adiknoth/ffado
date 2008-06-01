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

#include "fireworks_device.h"
#include "efc/efc_avc_cmd.h"
#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"
#include "efc/efc_cmds_hardware_ctrl.h"
#include "efc/efc_cmds_flash.h"

#include "audiofire/audiofire_device.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "config.h"

#include "fireworks/fireworks_control.h"

#include "libutil/PosixMutex.h"

#include <sstream>
using namespace std;

// FireWorks is the platform used and developed by ECHO AUDIO
namespace FireWorks {

Device::Device(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : GenericAVC::AvDevice( d, configRom)
    , m_poll_lock( new Util::PosixMutex() )
    , m_efc_discovery_done ( false )
    , m_MixerContainer ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created FireWorks::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{
    destroyMixer();
}

void
Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a FireWorks::Device\n");
    if ( !m_efc_discovery_done) {
        if (!discoverUsingEFC()) {
            debugError("EFC discovery failed\n");
        }
    }
    m_HwInfo.showEfcCmd();
    GenericAVC::AvDevice::showDevice();
}

bool
Device::probe( ConfigRom& configRom, bool generic )
{
    if(generic) {
        // try an EFC command
        EfcOverAVCCmd cmd( configRom.get1394Service() );
        cmd.setCommandType( AVC::AVCCommand::eCT_Control );
        cmd.setNodeId( configRom.getNodeId() );
        cmd.setSubunitType( AVC::eST_Unit  );
        cmd.setSubunitId( 0xff );
        cmd.setVerbose( configRom.getVerboseLevel() );

        EfcHardwareInfoCmd hwInfo;
        hwInfo.setVerboseLevel(configRom.getVerboseLevel());
        cmd.m_cmd = &hwInfo;

        if ( !cmd.fire()) {
            return false;
        }

        if ( cmd.getResponse() != AVC::AVCCommand::eR_Accepted) {
            return false;
        }
        if ( hwInfo.m_header.retval != EfcCmd::eERV_Ok
             && hwInfo.m_header.retval != EfcCmd::eERV_FlashBusy) {
             debugError( "EFC command failed\n" );
             return false;
        }
        return true;
    } else {
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();
    
        GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_fireworks.txt" );
        if ( vendorModel.parse() ) {
            return vendorModel.isPresent( vendorId, modelId );
        }
        return false;
    }
}

bool
Device::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    GenericAVC::VendorModel vendorModel( SHAREDIR "/ffado_driver_fireworks.txt" );
    if ( vendorModel.parse() ) {
        m_model = vendorModel.find( vendorId, modelId );
    }

    if (!GenericAVC::VendorModel::isValid(m_model)) {
        debugWarning("Using generic ECHO Audio FireWorks support for unsupported device '%s %s'\n",
            getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    } else {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model.vendor_name.c_str(), m_model.model_name.c_str());
    }

    // get the info from the EFC
    if ( !discoverUsingEFC() ) {
        debugError( "Could not discover using EFC\n" );
        return false;
    }

    // discover AVC-wise
    if ( !GenericAVC::AvDevice::discover() ) {
        debugError( "Could not discover GenericAVC::AvDevice\n" );
        return false;
    }

    if(!buildMixer()) {
        debugWarning("Could not build mixer\n");
    }

    return true;
}

bool
Device::discoverUsingEFC()
{
    m_efc_discovery_done = false;
    m_HwInfo.setVerboseLevel(getDebugLevel());

    if (!doEfcOverAVC(m_HwInfo)) {
        debugError("Could not read hardware capabilities\n");
        return false;
    }

    // save the EFC version, since some stuff
    // depends on this
    m_efc_version = m_HwInfo.m_header.version;

    if (!updatePolledValues()) {
        debugError("Could not update polled values\n");
        return false;
    }

    m_efc_discovery_done = true;
    return true;
}

FFADODevice *
Device::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();
//     unsigned int modelId = configRom->getModelId();

    switch(vendorId) {
        case FW_VENDORID_ECHO: return new ECHO::AudioFire(d, configRom );
        default: return new Device(d, configRom );
    }
}

bool 
Device::doEfcOverAVC(EfcCmd &c) {
    EfcOverAVCCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );

    cmd.setVerbose( getDebugLevel() );
    cmd.m_cmd = &c;

    if ( !cmd.fire()) {
        debugError( "EfcOverAVCCmd command failed\n" );
        c.showEfcCmd();
        return false;
    }

    if ( cmd.getResponse() != AVC::AVCCommand::eR_Accepted) {
        debugError( "EfcOverAVCCmd not accepted\n" );
        return false;
    }

    if (   c.m_header.retval != EfcCmd::eERV_Ok
        && c.m_header.retval != EfcCmd::eERV_FlashBusy) {
        debugError( "EFC command failed\n" );
        c.showEfcCmd();
        return false;
    }

    return true;
}

bool
Device::buildMixer()
{
    bool result=true;
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a FireWorks mixer...\n");
    
    destroyMixer();
    
    // create the mixer object container
    m_MixerContainer = new Control::Container(this, "Mixer");

    if (!m_MixerContainer) {
        debugError("Could not create mixer container...\n");
        return false;
    }

    // create control objects for the audiofire

    // matrix mix controls
    result &= m_MixerContainer->addElement(
        new MonitorControl(*this, MonitorControl::eMC_Gain, "MonitorGain"));

    result &= m_MixerContainer->addElement(
        new MonitorControl(*this, MonitorControl::eMC_Mute, "MonitorMute"));

    result &= m_MixerContainer->addElement(
        new MonitorControl(*this, MonitorControl::eMC_Solo, "MonitorSolo"));

    result &= m_MixerContainer->addElement(
        new MonitorControl(*this, MonitorControl::eMC_Pan, "MonitorPan"));

    // Playback mix controls
    for (unsigned int ch=0;ch<m_HwInfo.m_nb_1394_playback_channels;ch++) {
        std::ostringstream node_name;
        node_name << "PC" << ch;
        
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, eMT_PlaybackMix, eMC_Mute, ch, 0, node_name.str()+"Mute"));
        result &= m_MixerContainer->addElement(
            new SimpleControl(*this, eMT_PlaybackMix, eMC_Gain, ch, node_name.str()+"Gain"));
    }
    
    // Physical output mix controls
    for (unsigned int ch=0;ch<m_HwInfo.m_nb_phys_audio_out;ch++) {
        std::ostringstream node_name;
        node_name << "OUT" << ch;
        
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, eMT_PhysicalOutputMix, eMC_Mute, ch, 0, node_name.str()+"Mute"));
        result &= m_MixerContainer->addElement(
            new BinaryControl(*this, eMT_PhysicalOutputMix, eMC_Nominal, ch, 1, node_name.str()+"Nominal"));
        result &= m_MixerContainer->addElement(
            new SimpleControl(*this, eMT_PhysicalOutputMix, eMC_Gain, ch, node_name.str()+"Gain"));
    }
    
    // check for IO config controls and add them if necessary
    if(m_HwInfo.hasMirroring()) {
        result &= m_MixerContainer->addElement(
            new IOConfigControl(*this, eCR_Mirror, "ChannelMirror"));
    }
    if(m_HwInfo.hasSoftwarePhantom()) {
        result &= m_MixerContainer->addElement(
            new IOConfigControl(*this, eCR_Phantom, "PhantomPower"));
    }

    if (!result) {
        debugWarning("One or more control elements could not be created.");
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

    return true;
}

bool
Device::destroyMixer()
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
    return true;
}


bool
Device::updatePolledValues() {
    Util::MutexLockHelper lock(*m_poll_lock);
    return doEfcOverAVC(m_Polled);
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;

    if (!m_efc_discovery_done) {
        debugError("EFC discovery not done yet!\n");
        return r;
    }

    uint32_t active_clock=getClock();

    if(EFC_CMD_HW_CHECK_FLAG(m_HwInfo.m_supported_clocks, EFC_CMD_HW_CLOCK_INTERNAL)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Internal clock supported\n");
        ClockSource s=clockIdToClockSource(EFC_CMD_HW_CLOCK_INTERNAL);
        s.active=(active_clock == EFC_CMD_HW_CLOCK_INTERNAL);
        if (s.type != eCT_Invalid) r.push_back(s);
    }
    if(EFC_CMD_HW_CHECK_FLAG(m_HwInfo.m_supported_clocks, EFC_CMD_HW_CLOCK_SYTMATCH)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Syt Match clock supported\n");
        ClockSource s=clockIdToClockSource(EFC_CMD_HW_CLOCK_SYTMATCH);
        s.active=(active_clock == EFC_CMD_HW_CLOCK_SYTMATCH);
        if (s.type != eCT_Invalid) r.push_back(s);
    }
    if(EFC_CMD_HW_CHECK_FLAG(m_HwInfo.m_supported_clocks, EFC_CMD_HW_CLOCK_WORDCLOCK)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "WordClock supported\n");
        ClockSource s=clockIdToClockSource(EFC_CMD_HW_CLOCK_WORDCLOCK);
        s.active=(active_clock == EFC_CMD_HW_CLOCK_WORDCLOCK);
        if (s.type != eCT_Invalid) r.push_back(s);
    }
    if(EFC_CMD_HW_CHECK_FLAG(m_HwInfo.m_supported_clocks, EFC_CMD_HW_CLOCK_SPDIF)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "SPDIF clock supported\n");
        ClockSource s=clockIdToClockSource(EFC_CMD_HW_CLOCK_SPDIF);
        s.active=(active_clock == EFC_CMD_HW_CLOCK_SPDIF);
        if (s.type != eCT_Invalid) r.push_back(s);
    }
    if(EFC_CMD_HW_CHECK_FLAG(m_HwInfo.m_supported_clocks, EFC_CMD_HW_CLOCK_ADAT_1)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ADAT 1 clock supported\n");
        ClockSource s=clockIdToClockSource(EFC_CMD_HW_CLOCK_ADAT_1);
        s.active=(active_clock == EFC_CMD_HW_CLOCK_ADAT_1);
        if (s.type != eCT_Invalid) r.push_back(s);
    }
    if(EFC_CMD_HW_CHECK_FLAG(m_HwInfo.m_supported_clocks, EFC_CMD_HW_CLOCK_ADAT_2)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "ADAT 2 clock supported\n");
        ClockSource s=clockIdToClockSource(EFC_CMD_HW_CLOCK_ADAT_2);
        s.active=(active_clock == EFC_CMD_HW_CLOCK_ADAT_2);
        if (s.type != eCT_Invalid) r.push_back(s);
    }
    return r;
}

bool
Device::isClockValid(uint32_t id) {
    // always valid
    if (id==EFC_CMD_HW_CLOCK_INTERNAL) return true;

    // the polled values are used to detect
    // whether clocks are valid
    if (!updatePolledValues()) {
        debugError("Could not update polled values\n");
        return false;
    }
    return EFC_CMD_HW_CHECK_FLAG(m_Polled.m_status,id);
}

bool
Device::setActiveClockSource(ClockSource s) {
    bool result;

    debugOutput(DEBUG_LEVEL_VERBOSE, "setting clock source to id: %d\n",s.id);

    if(!isClockValid(s.id)) {
        debugError("Clock not valid\n");
        return false;
    }

    result=setClock(s.id);

    // From the ECHO sources:
    // "If this is a 1200F and the sample rate is being set via EFC, then
    // send the "phy reconnect command" so the device will vanish and reappear 
    // with a new descriptor."

//     EfcPhyReconnectCmd rccmd;
//     if(!doEfcOverAVC(rccmd)) {
//         debugError("Phy reconnect failed\n");
//     } else {
//         // sleep for one second such that the phy can get reconnected
//         sleep(1);
//     }

    return result;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    ClockSource s;
    uint32_t active_clock=getClock();
    s=clockIdToClockSource(active_clock);
    s.active=true;
    return s;
}

FFADODevice::ClockSource
Device::clockIdToClockSource(uint32_t clockid) {
    ClockSource s;
    debugOutput(DEBUG_LEVEL_VERBOSE, "clock id: %lu\n", clockid);

    // the polled values are used to detect
    // whether clocks are valid
    if (!updatePolledValues()) {
        debugError("Could not update polled values\n");
        return s;
    }

    switch (clockid) {
        case EFC_CMD_HW_CLOCK_INTERNAL:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Internal clock\n");
            s.type=eCT_Internal;
            s.description="Internal sync";
            break;

        case EFC_CMD_HW_CLOCK_SYTMATCH:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Syt Match\n");
            s.type=eCT_SytMatch;
            s.description="SYT Match";
            break;

        case EFC_CMD_HW_CLOCK_WORDCLOCK:
            debugOutput(DEBUG_LEVEL_VERBOSE, "WordClock\n");
            s.type=eCT_WordClock;
            s.description="Word Clock";
            break;

        case EFC_CMD_HW_CLOCK_SPDIF:
            debugOutput(DEBUG_LEVEL_VERBOSE, "SPDIF clock\n");
            s.type=eCT_SPDIF;
            s.description="SPDIF";
            break;

        case EFC_CMD_HW_CLOCK_ADAT_1:
            debugOutput(DEBUG_LEVEL_VERBOSE, "ADAT 1 clock\n");
            s.type=eCT_ADAT;
            s.description="ADAT 1";
            break;

        case EFC_CMD_HW_CLOCK_ADAT_2:
            debugOutput(DEBUG_LEVEL_VERBOSE, "ADAT 2 clock\n");
            s.type=eCT_ADAT;
            s.description="ADAT 2";
            break;

        default:
            debugError("Invalid clock id: %d\n",clockid);
            return s; // return an invalid ClockSource
    }

    s.id=clockid;
    s.valid=isClockValid(clockid);

    return s;
}

uint32_t
Device::getClock() {
    EfcGetClockCmd gccmd;
    if (!doEfcOverAVC(gccmd)) {
        debugError("Could not get clock info\n");
        return EFC_CMD_HW_CLOCK_UNSPECIFIED;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "Active clock: 0x%08lX\n",gccmd.m_clock);
    gccmd.showEfcCmd();

    return gccmd.m_clock;
}

bool
Device::setClock(uint32_t id) {
    EfcGetClockCmd gccmd;
    if (!doEfcOverAVC(gccmd)) {
        debugError("Could not get clock info\n");
        return false;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "Set clock: 0x%08lX\n", id);

    EfcSetClockCmd sccmd;
    sccmd.m_clock=id;
    sccmd.m_samplerate=gccmd.m_samplerate;
    sccmd.m_index=0;
    if (!doEfcOverAVC(sccmd)) {
        debugError("Could not set clock info\n");
        return false;
    }
    return true;
}

bool
Device::lockFlash(bool lock) {
    EfcFlashLockCmd cmd;
    cmd.m_lock = lock;

    if(!doEfcOverAVC(cmd)) {
        debugError("Flash lock failed\n");
        return false;
    }
    return true;
}

bool
Device::writeFlash(uint32_t start, uint32_t len, uint32_t* buffer) {

    if(len <= 0 || 0xFFFFFFFF - len*4 < start) {
        debugError("bogus start/len: 0x%08X / %u\n", start, len);
        return false;
    }
    if(start & 0x03) {
        debugError("start address not quadlet aligned: 0x%08X\n", start);
        return false;
    }

    uint32_t start_addr = start;
    uint32_t stop_addr = start + len*4;
    uint32_t *target_buffer = buffer;

    EfcFlashWriteCmd cmd;
    // write EFC_FLASH_SIZE_BYTES at a time
    for(start_addr = start; start_addr < stop_addr; start_addr += EFC_FLASH_SIZE_BYTES) {
        cmd.m_address = start_addr;
        unsigned int quads_to_write = (stop_addr - start_addr)/4;
        if (quads_to_write > EFC_FLASH_SIZE_QUADS) {
            quads_to_write = EFC_FLASH_SIZE_QUADS;
        }
        cmd.m_nb_quadlets = quads_to_write;
        for(unsigned int i=0; i<quads_to_write; i++) {
            cmd.m_data[i] = *target_buffer;
            target_buffer++;
        }
        if(!doEfcOverAVC(cmd)) {
            debugError("Flash write failed for block 0x%08X (%d quadlets)\n", start_addr, quads_to_write);
            return false;
        }
    }
    return true;
}

bool
Device::readFlash(uint32_t start, uint32_t len, uint32_t* buffer) {

    if(len <= 0 || 0xFFFFFFFF - len*4 < start) {
        debugError("bogus start/len: 0x%08X / %u\n", start, len);
        return false;
    }
    if(start & 0x03) {
        debugError("start address not quadlet aligned: 0x%08X\n", start);
        return false;
    }

    uint32_t start_addr = start;
    uint32_t stop_addr = start + len*4;
    uint32_t *target_buffer = buffer;

    EfcFlashReadCmd cmd;
    // read EFC_FLASH_SIZE_BYTES at a time
    for(start_addr = start; start_addr < stop_addr; start_addr += EFC_FLASH_SIZE_BYTES) {
        unsigned int quads_to_read = (stop_addr - start_addr)/4;
        if (quads_to_read > EFC_FLASH_SIZE_QUADS) {
            quads_to_read = EFC_FLASH_SIZE_QUADS;
        }
        uint32_t quadlets_read = 0;
        int ntries = 10000;
        do {
            cmd.m_address = start_addr + quadlets_read*4;
            unsigned int new_to_read = quads_to_read - quadlets_read;
            cmd.m_nb_quadlets = new_to_read;
            if(!doEfcOverAVC(cmd)) {
                debugError("Flash read failed for block 0x%08X (%d quadlets)\n", start_addr, quads_to_read);
                return false;
            }
            if(cmd.m_nb_quadlets != new_to_read) {
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "Flash read didn't return enough data (%u/%u) \n",
                            cmd.m_nb_quadlets, new_to_read);
                // continue trying
            }
            quadlets_read += cmd.m_nb_quadlets;

            // copy content
            for(unsigned int i=0; i<cmd.m_nb_quadlets; i++) {
                *target_buffer = cmd.m_data[i];
                target_buffer++;
            }
        } while(quadlets_read < quads_to_read && ntries--);
        if(ntries==0) {
            debugError("deadlock while reading flash\n");
            return false;
        }
    }
    return true;
}

bool
Device::eraseFlash(uint32_t addr) {
    if(addr & 0x03) {
        debugError("start address not quadlet aligned: 0x%08X\n", addr);
        return false;
    }
    EfcFlashEraseCmd cmd;
    cmd.m_address = addr;
    if(!doEfcOverAVC(cmd)) {
        if (cmd.m_header.retval == EfcCmd::eERV_FlashBusy) {
            return true;
        }
        debugError("Flash erase failed for block 0x%08X\n", addr);
        return false;
    }
    return true;
}

bool
Device::waitForFlash(unsigned int msecs)
{
    bool ready;

    EfcFlashGetStatusCmd statusCmd;
    const unsigned int time_to_sleep_usecs = 10000;
    int wait_cycles = msecs * 1000 / time_to_sleep_usecs;

    do {
        if (!doEfcOverAVC(statusCmd)) {
            debugError("Could not read flash status\n");
            return false;
        }
        if (statusCmd.m_header.retval == EfcCmd::eERV_FlashBusy) {
            ready = false;
        } else {
            ready = statusCmd.m_ready;
        }
        usleep(time_to_sleep_usecs);
    } while (!ready && wait_cycles--);

    if(wait_cycles == 0) {
        debugError("Timeout while waiting for flash\n");
        return false;
    }

    return ready;
}

} // FireWorks
