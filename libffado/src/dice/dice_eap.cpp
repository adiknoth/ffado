/*
 * Copyright (C) 2005-2009 by Pieter Palmers
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

#include "dice_avdevice.h"

#include "dice_eap.h"
#include "dice_defines.h"

#include "libutil/SystemTimeSource.h"

#include <cstdio>

namespace Dice {

// ----------- helper functions -------------

static const char *
srcBlockToString(const char id)
{
    switch(id) {
        case 0: return "AES ";
        case 1: return "ADAT";
        case 2: return "MXR ";
        case 4: return "INS0";
        case 5: return "INS1";
        case 10: return "ARM ";
        case 11: return "AVS0";
        case 12: return "AVS1";
        case 15: return "MUTE";
        default : return "RSVD";
    }
}

static  const char *
dstBlockToString(const char id)
{
    switch(id) {
        case 0: return "AES ";
        case 1: return "ADAT";
        case 2: return "MXR0";
        case 3: return "MXR1";
        case 4: return "INS0";
        case 5: return "INS1";
        case 10: return "ARM ";
        case 11: return "AVS0";
        case 12: return "AVS1";
        case 15: return "MUTE";
        default : return "RSVD";
    }
}


Device::EAP::EAP(Device &d)
: Control::Container(&d, "EAP")
, m_debugModule(d.m_debugModule) // NOTE: has to be initialized before creating the config classes
                                 //       otherwise the reference content used by those is bogus
, m_device(d)
, m_mixer( NULL )
, m_router( NULL )
, m_current_cfg_routing_low( RouterConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_LOW_ROUTER) )
, m_current_cfg_routing_mid( RouterConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_MID_ROUTER) )
, m_current_cfg_routing_high( RouterConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_HIGH_ROUTER) )
, m_current_cfg_stream_low( StreamConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_LOW_STREAM) )
, m_current_cfg_stream_mid( StreamConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_MID_STREAM) )
, m_current_cfg_stream_high( StreamConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_HIGH_STREAM) )
{
}

Device::EAP::~EAP()
{
    // remove all control elements registered to this device (w/o free)
    clearElements(false);

    // delete the helper classes
    if(m_mixer) delete m_mixer;
    if(m_router) delete m_router;
}

// offsets and sizes are returned in quadlets, but we use byte values, hence the *= 4
#define DICE_EAP_READREG_AND_CHECK(base, addr, var) { \
    if(!readReg(base, addr, &var)) { \
        debugError("Could not initialize " #var "\n"); \
        return false; \
    } \
    var *= 4; \
}

bool
Device::EAP::init() {
    if(!supportsEAP(m_device)) {
        debugError("Device does not support EAP");
        return false;
    }

    // offsets and sizes are returned in quadlets, but we use byte values
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_CAPABILITY_SPACE_OFF, m_capability_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_CAPABILITY_SPACE_SZ, m_capability_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_CMD_SPACE_OFF, m_cmd_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_CMD_SPACE_SZ, m_cmd_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_MIXER_SPACE_OFF, m_mixer_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_MIXER_SPACE_SZ, m_mixer_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_PEAK_SPACE_OFF, m_peak_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_PEAK_SPACE_SZ, m_peak_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_NEW_ROUTING_SPACE_OFF, m_new_routing_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_NEW_ROUTING_SPACE_SZ, m_new_routing_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_NEW_STREAM_CFG_SPACE_OFF, m_new_stream_cfg_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_NEW_STREAM_CFG_SPACE_SZ, m_new_stream_cfg_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_CURR_CFG_SPACE_OFF, m_curr_cfg_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_CURR_CFG_SPACE_SZ, m_curr_cfg_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_STAND_ALONE_CFG_SPACE_OFF, m_standalone_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_STAND_ALONE_CFG_SPACE_SZ, m_standalone_size);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_APP_SPACE_OFF, m_app_offset);
    DICE_EAP_READREG_AND_CHECK(eRT_Base, DICE_EAP_APP_SPACE_SZ, m_app_size);

    // initialize the capability info
    quadlet_t tmp;
    if(!readReg(eRT_Capability, DICE_EAP_CAPABILITY_ROUTER, &tmp)) {
        debugError("Could not read router capabilities\n");
        return false;
    }
    m_router_exposed = (tmp >> DICE_EAP_CAP_ROUTER_EXPOSED) & 0x01;
    m_router_readonly = (tmp >> DICE_EAP_CAP_ROUTER_READONLY) & 0x01;
    m_router_flashstored = (tmp >> DICE_EAP_CAP_ROUTER_FLASHSTORED) & 0x01;
    m_router_nb_entries = (tmp >> DICE_EAP_CAP_ROUTER_MAXROUTES) & 0xFFFF;

    if(!readReg(eRT_Capability, DICE_EAP_CAPABILITY_MIXER, &tmp)) {
        debugError("Could not read mixer capabilities\n");
        return false;
    }
    m_mixer_exposed = (tmp >> DICE_EAP_CAP_MIXER_EXPOSED) & 0x01;
    m_mixer_readonly = (tmp >> DICE_EAP_CAP_MIXER_READONLY) & 0x01;
    m_mixer_flashstored = (tmp >> DICE_EAP_CAP_MIXER_FLASHSTORED) & 0x01;
    m_mixer_tx_id = (tmp >> DICE_EAP_CAP_MIXER_IN_DEV) & 0x000F;
    m_mixer_rx_id = (tmp >> DICE_EAP_CAP_MIXER_OUT_DEV) & 0x000F;
    m_mixer_nb_tx = (tmp >> DICE_EAP_CAP_MIXER_INPUTS) & 0x00FF;
    m_mixer_nb_rx = (tmp >> DICE_EAP_CAP_MIXER_OUTPUTS) & 0x00FF;

    if(!readReg(eRT_Capability, DICE_EAP_CAPABILITY_GENERAL, &tmp)) {
        debugError("Could not read general capabilities\n");
        return false;
    }
    m_general_support_dynstream = (tmp >> DICE_EAP_CAP_GENERAL_STRM_CFG_EN) & 0x01;
    m_general_support_flash = (tmp >> DICE_EAP_CAP_GENERAL_FLASH_EN) & 0x01;
    m_general_peak_enabled = (tmp >> DICE_EAP_CAP_GENERAL_PEAK_EN) & 0x01;
    m_general_max_tx = (tmp >> DICE_EAP_CAP_GENERAL_MAX_TX_STREAM) & 0x0F;
    m_general_max_rx = (tmp >> DICE_EAP_CAP_GENERAL_MAX_RX_STREAM) & 0x0F;
    m_general_stream_cfg_stored = (tmp >> DICE_EAP_CAP_GENERAL_STRM_CFG_FLS) & 0x01;
    m_general_chip = (tmp >> DICE_EAP_CAP_GENERAL_CHIP) & 0xFFFF;

    // update our view on the current configuration
    if(!updateConfigurationCache()) {
        debugError("Could not initialize configuration cache\n");
        return false;
    }

    // initialize the helper classes
    if (m_mixer_exposed) {
        // initialize the mixer
        m_mixer = new EAP::Mixer(*this);
        if(m_mixer == NULL) {
            debugError("Could not allocate memory for mixer\n");
            return false;
        }
        if(!m_mixer->init()) {
            debugError("Could not initialize mixer\n");
            delete m_mixer;
            m_mixer = NULL;
            return false;
        }
        // add the mixer to the EAP control container
        if(!addElement(m_mixer)) {
            debugWarning("Failed to add mixer to control tree\n");
        }
        
        // initialize the peak meter
        m_router = new EAP::Router(*this);
        if(m_router == NULL) {
            debugError("Could not allocate memory for router\n");
            return false;
        }

        // add the router to the EAP control container
        if(!addElement(m_router)) {
            debugWarning("Failed to add router to control tree\n");
        }
    }

    return true;
}

bool
Device::EAP::updateConfigurationCache()
{
    if(!m_current_cfg_routing_low.read()) {
        debugError("Could not initialize current routing configuration (low rates)\n");
        return false;
    }
    if(!m_current_cfg_routing_mid.read()) {
        debugError("Could not initialize current routing configuration (mid rates)\n");
        return false;
    }
    if(!m_current_cfg_routing_high.read()) {
        debugError("Could not initialize current routing configuration (high rates)\n");
        return false;
    }
    if(!m_current_cfg_stream_low.read()) {
        debugError("Could not initialize current stream configuration (low rates)\n");
        return false;
    }
    if(!m_current_cfg_stream_mid.read()) {
        debugError("Could not initialize current stream configuration (mid rates)\n");
        return false;
    }
    if(!m_current_cfg_stream_high.read()) {
        debugError("Could not initialize current stream configuration (high rates)\n");
        return false;
    }
    if(m_mixer) m_mixer->updateNameCache();
    return true;
}

/**
 * Returns the router configuration for the current rate mode
 * @return 
 */
Device::EAP::RouterConfig *
Device::EAP::getActiveRouterConfig()
{
    switch(m_device.getCurrentConfig()) {
        case eDC_Low: return &m_current_cfg_routing_low;
        case eDC_Mid: return &m_current_cfg_routing_mid;
        case eDC_High: return &m_current_cfg_routing_high;
        default:
            debugError("Unsupported configuration mode\n");
            return NULL;
    }
}

/**
 * Returns the stream configuration for the current rate mode
 * @return 
 */
Device::EAP::StreamConfig *
Device::EAP::getActiveStreamConfig()
{
    switch(m_device.getCurrentConfig()) {
        case eDC_Low: return &m_current_cfg_stream_low;
        case eDC_Mid: return &m_current_cfg_stream_mid;
        case eDC_High: return &m_current_cfg_stream_high;
        default:
            debugError("Unsupported configuration mode\n");
            return NULL;
    }
}

/**
 * Uploads a new router configuration to the device
 * @param  rcfg The new RouterConfig
 * @param low store as config for the low rates
 * @param mid store as config for the mid rates
 * @param high store as config for the high rates
 * @return true if successful, false otherwise
 */
bool
Device::EAP::updateRouterConfig(RouterConfig& rcfg, bool low, bool mid, bool high) {
    // write the router config to the appropriate memory space on the device
    if(!rcfg.write(eRT_NewRouting, 0)) {
        debugError("Could not write new router configuration\n");
        return false;
    }
    // perform the store operation
    if(!loadRouterConfig(low, mid, high)) {
        debugError("Could not activate new router configuration\n");
        updateConfigurationCache(); // for consistency
        return false;
    }
    return updateConfigurationCache();
}

/**
 * Uploads a new router configuration to replace the configuration
 * for the current rate.
 * @param  rcfg The new RouterConfig
 * @return true if successful, false otherwise
 */
bool
Device::EAP::updateCurrentRouterConfig(RouterConfig& rcfg) {
    switch(m_device.getCurrentConfig()) {
        case eDC_Low: return updateRouterConfig(rcfg, true, false, false);
        case eDC_Mid: return updateRouterConfig(rcfg, false, true, false);
        case eDC_High: return updateRouterConfig(rcfg, false, false, true);
        default:
            debugError("Unsupported configuration mode\n");
            return false;
    }
}

/**
 * Uploads a new stream configuration to the device
 * @param scfg The new StreamConfig
 * @param low store as config for the low rates
 * @param mid store as config for the mid rates
 * @param high store as config for the high rates
 * @return true if successful, false otherwise
 */
bool
Device::EAP::updateStreamConfig(StreamConfig& scfg, bool low, bool mid, bool high) {
    // write the stream config to the appropriate memory space on the device
    if(!scfg.write(eRT_NewStreamCfg, 0)) {
        debugError("Could not write new stream configuration\n");
        return false;
    }
    // perform the store operation
    if(!loadStreamConfig(low, mid, high)) {
        debugError("Could not activate new stream configuration\n");
        updateConfigurationCache(); // for consistency
        return false;
    }
    return updateConfigurationCache();
}

/**
 * Uploads a new router and stream configuration to the device
 * @param  rcfg The new RouterConfig
 * @param  scfg The new StreamConfig
 * @param low store as config for the low rates
 * @param mid store as config for the mid rates
 * @param high store as config for the high rates
 * @return true if successful, false otherwise
 */
bool
Device::EAP::updateStreamConfig(RouterConfig& rcfg, StreamConfig& scfg, bool low, bool mid, bool high) {
    // write the router config to the appropriate memory space on the device
    if(!rcfg.write(eRT_NewRouting, 0)) {
        debugError("Could not write new router configuration\n");
        return false;
    }
    // write the stream config to the appropriate memory space on the device
    if(!scfg.write(eRT_NewStreamCfg, 0)) {
        debugError("Could not write new stream configuration\n");
        return false;
    }
    // perform the store operation
    if(!loadRouterAndStreamConfig(low, mid, high)) {
        debugError("Could not activate new router/stream configuration\n");
        updateConfigurationCache(); // for consistency
        return false;
    }
    return updateConfigurationCache();
}


bool
Device::EAP::loadFlashConfig() {
    bool retval = true;
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_FLASH_CFG;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    if(!commandHelper(cmd)) {
        debugWarning("Command failed\n");
        retval = false;
    }
    retval &= updateConfigurationCache();
    return retval;
}

bool
Device::EAP::storeFlashConfig() {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_ST_FLASH_CFG;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

// helpers
void
Device::EAP::show()
{
    printMessage("== DICE EAP ==\n");
    debugOutput(DEBUG_LEVEL_VERBOSE, "Parameter Space info:\n");
    debugOutput(DEBUG_LEVEL_VERBOSE, " Capability        : offset=%04X size=%06d\n", m_capability_offset, m_capability_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Command           : offset=%04X size=%06d\n", m_cmd_offset, m_cmd_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Mixer             : offset=%04X size=%06d\n", m_mixer_offset, m_mixer_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Peak              : offset=%04X size=%06d\n", m_peak_offset, m_peak_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " New Routing Cfg   : offset=%04X size=%06d\n", m_new_routing_offset, m_new_routing_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " New Stream Cfg    : offset=%04X size=%06d\n", m_new_stream_cfg_offset, m_new_stream_cfg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Current Cfg       : offset=%04X size=%06d\n", m_curr_cfg_offset, m_curr_cfg_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Standalone Cfg    : offset=%04X size=%06d\n", m_standalone_offset, m_standalone_size);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Application Space : offset=%04X size=%06d\n", m_app_offset, m_app_size);

    debugOutput(DEBUG_LEVEL_VERBOSE, "Capabilities:\n");
    debugOutput(DEBUG_LEVEL_VERBOSE, " Router: %sexposed, %swritable, %sstored, %d routes\n",
                                     (m_router_exposed?"":"not "),
                                     (m_router_readonly?"not ":""),
                                     (m_router_flashstored?"":"not "),
                                     m_router_nb_entries);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Mixer : %sexposed, %swritable, %sstored\n",
                                     (m_mixer_exposed?"":"not "),
                                     (m_mixer_readonly?"not ":""),
                                     (m_mixer_flashstored?"":"not "));
    debugOutput(DEBUG_LEVEL_VERBOSE, "         tx id: %s [%d], rx id: %s [%d]\n", 
                                     dstBlockToString(m_mixer_tx_id), m_mixer_tx_id,
                                     srcBlockToString(m_mixer_rx_id), m_mixer_rx_id);
    debugOutput(DEBUG_LEVEL_VERBOSE, "         nb tx channels: %d, nb rx channels: %d\n", m_mixer_nb_tx, m_mixer_nb_rx);
    debugOutput(DEBUG_LEVEL_VERBOSE, " General: dynamic stream config %ssupported\n",
                                     (m_general_support_dynstream?"":"not "));
    debugOutput(DEBUG_LEVEL_VERBOSE, "          flash load and store %ssupported\n",
                                     (m_general_support_flash?"":"not "));
    debugOutput(DEBUG_LEVEL_VERBOSE, "          peak metering %s\n",
                                     (m_general_peak_enabled?"enabled":"disabled"));
    debugOutput(DEBUG_LEVEL_VERBOSE, "          stream config %sstored\n",
                                     (m_general_stream_cfg_stored?"":"not "));
    debugOutput(DEBUG_LEVEL_VERBOSE, "          max TX streams: %d, max RX streams: %d\n",
                                     m_general_max_tx, m_general_max_rx);

    if(m_general_chip == DICE_EAP_CAP_GENERAL_CHIP_DICEII) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "          Chip: DICE-II\n");
    } else if(m_general_chip == DICE_EAP_CAP_GENERAL_CHIP_DICEMINI) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "          Chip: DICE Mini (TCD2210)\n");
    } else if(m_general_chip == DICE_EAP_CAP_GENERAL_CHIP_DICEJR) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "          Chip: DICE Junior (TCD2220)\n");
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "--- Mixer configuration ---\n");
    if(m_mixer) {
        m_mixer->show();
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "--- Router/Peak space ---\n");
    if(m_router) {
        m_router->show();
    }

    RouterConfig *rcfg = getActiveRouterConfig();
    if(rcfg) {
        rcfg->show();
    }
    StreamConfig *scfg = getActiveStreamConfig();
    if(scfg) {
        scfg->show();
    }

// fixme
//     size_t len = 0x1000;
//     quadlet_t tmp[len];
//     if(!readRegBlock( eRT_CurrentCfg, DICE_EAP_CURRCFG_LOW_STREAM, tmp, len*4) ) {
//         debugError("Failed to read block\n");
//     } else {
//         hexDumpQuadlets(tmp, len);
//     }

}

// EAP load/store operations

enum Device::EAP::eWaitReturn
Device::EAP::operationBusy() {
    fb_quadlet_t tmp;
    if(!readReg(eRT_Command, DICE_EAP_COMMAND_OPCODE, &tmp)) {
        debugError("Could not read opcode register\n");
        return eWR_Error;
    }
    if( (tmp & DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE) == DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE) {
        return eWR_Busy;
    } else {
        return eWR_Done;
    }
}

enum Device::EAP::eWaitReturn
Device::EAP::waitForOperationEnd(int max_wait_time_ms) {
    int max_waits = max_wait_time_ms;

    while(max_waits--) {
        enum eWaitReturn retval = operationBusy();
        switch(retval) {
            case eWR_Busy:
                break; // not done yet, keep waiting
            case eWR_Done:
                return eWR_Done;
            case eWR_Error:
            case eWR_Timeout:
                debugError("Error while waiting for operation to end. (%d)\n", retval);
        }
        Util::SystemTimeSource::SleepUsecRelative(1000);
    }
    return eWR_Timeout;
}

bool
Device::EAP::commandHelper(fb_quadlet_t cmd) {
    // check whether another command is still running
    if(operationBusy() == eWR_Busy) {
        debugError("Other operation in progress\n");
        return false;
    }

    // execute the command
    if(!writeReg(eRT_Command, DICE_EAP_COMMAND_OPCODE, cmd)) {
        debugError("Could not write opcode register\n");
        return false;
    }

    // wait for the operation to end
    enum eWaitReturn retval = waitForOperationEnd();
    switch(retval) {
        case eWR_Done:
            break; // do nothing
        case eWR_Timeout:
            debugWarning("Time-out while waiting for operation to end. (%d)\n", retval);
            return false;
        case eWR_Error:
        case eWR_Busy: // can't be returned
            debugError("Error while waiting for operation to end. (%d)\n", retval);
            return false;
    }

    // check the return value
    if(!readReg(eRT_Command, DICE_EAP_COMMAND_RETVAL, &cmd)) {
        debugError("Could not read return value register\n");
        return false;
    }
    if(cmd != 0) {
        debugWarning("Command failed\n");
        return false;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Command successful\n");
        return true;
    }
}

bool
Device::EAP::loadRouterConfig(bool low, bool mid, bool high) {
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_ROUTER;
    if(low) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_LOW;
    if(mid) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_MID;
    if(high) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

bool
Device::EAP::loadStreamConfig(bool low, bool mid, bool high) {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_STRM_CFG;
    if(low) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_LOW;
    if(mid) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_MID;
    if(high) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

bool
Device::EAP::loadRouterAndStreamConfig(bool low, bool mid, bool high) {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_RTR_STRM_CFG;
    if(low) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_LOW;
    if(mid) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_MID;
    if(high) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

// internal I/O operations
bool
Device::EAP::readReg(enum eRegBase base, unsigned offset, fb_quadlet_t *result) {
    fb_nodeaddr_t addr = offsetGen(base, offset, 4);
    return m_device.readReg(addr, result);
}

bool
Device::EAP::writeReg(enum eRegBase base, unsigned offset, fb_quadlet_t data) {
    fb_nodeaddr_t addr = offsetGen(base, offset, 4);
    return m_device.writeReg(addr, data);
}

bool
Device::EAP::readRegBlock(enum eRegBase base, unsigned offset, fb_quadlet_t *data, size_t length) {
    fb_nodeaddr_t addr = offsetGen(base, offset, length);
    return m_device.readRegBlock(addr, data, length);
}

bool
Device::EAP::writeRegBlock(enum eRegBase base, unsigned offset, fb_quadlet_t *data, size_t length) {
    fb_nodeaddr_t addr = offsetGen(base, offset, length);
    return m_device.writeRegBlock(addr, data, length);
}

fb_nodeaddr_t
Device::EAP::offsetGen(enum eRegBase base, unsigned offset, size_t length) {
    fb_nodeaddr_t addr;
    fb_nodeaddr_t maxlen;
    switch(base) {
        case eRT_Base:
            addr = 0;
            maxlen = DICE_EAP_MAX_SIZE;
            break;
        case eRT_Capability:
            addr = m_capability_offset;
            maxlen = m_capability_size;
            break;
        case eRT_Command:
            addr = m_cmd_offset;
            maxlen = m_cmd_size;
            break;
        case eRT_Mixer:
            addr = m_mixer_offset;
            maxlen = m_mixer_size;
            break;
        case eRT_Peak:
            addr = m_peak_offset;
            maxlen = m_peak_size;
            break;
        case eRT_NewRouting:
            addr = m_new_routing_offset;
            maxlen = m_new_routing_size;
            break;
        case eRT_NewStreamCfg:
            addr = m_new_stream_cfg_offset;
            maxlen = m_new_stream_cfg_size;
            break;
        case eRT_CurrentCfg:
            addr = m_curr_cfg_offset;
            maxlen = m_curr_cfg_size;
            break;
        case eRT_Standalone:
            addr = m_standalone_offset;
            maxlen = m_standalone_size;
            break;
        case eRT_Application:
            addr = m_app_offset;
            maxlen = m_app_size;
            break;
        default:
            debugError("Unsupported base address\n");
            return 0;
    };

    // out-of-range check
    if(length > maxlen) {
        debugError("requested length too large: %d > %d\n", length, maxlen);
        return DICE_INVALID_OFFSET;
    }
    return DICE_EAP_BASE + addr + offset;
}

/**
 * Check whether a device supports eap
 * @param d the device to check
 * @return true if the device supports EAP
 */
bool
Device::EAP::supportsEAP(Device &d)
{
    DebugModule &m_debugModule = d.m_debugModule;
    quadlet_t tmp;
    if(!d.readReg(DICE_EAP_BASE, &tmp)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not read from DICE EAP base address\n");
        return false;
    }
    if(!d.readReg(DICE_EAP_BASE + DICE_EAP_ZERO_MARKER_1, &tmp)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not read from DICE EAP zero marker\n");
        return false;
    }
    if(tmp != 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "DICE EAP zero marker not zero\n");
        return false;
    }
    return true;
}

// ----------- Mixer -------------
Device::EAP::Mixer::Mixer(EAP &p)
: Control::MatrixMixer(&p.m_device, "MatrixMixer")
, m_eap(p)
, m_coeff(NULL)
, m_debugModule(p.m_debugModule)
{
}

Device::EAP::Mixer::~Mixer()
{
    if (m_coeff) {
        free(m_coeff);
        m_coeff = NULL;
    }
}

bool
Device::EAP::Mixer::init()
{
    if(!m_eap.m_mixer_exposed) {
        debugError("Device does not expose mixer\n");
        return false;
    }

    // remove previous coefficient array
    if(m_coeff) {
        free(m_coeff);
        m_coeff = NULL;
    }
    
    // allocate coefficient array
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int nb_outputs = m_eap.m_mixer_nb_rx;

    m_coeff = (fb_quadlet_t *)calloc(nb_outputs * nb_inputs, sizeof(fb_quadlet_t));

    // load initial values
    if(!loadCoefficients()) {
        debugWarning("Could not initialize coefficients\n");
        return false;
    }
    updateNameCache();
    return true;
}

bool
Device::EAP::Mixer::loadCoefficients()
{
    if(m_coeff == NULL) {
        debugError("Coefficient cache not initialized\n");
        return false;
    }
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int nb_outputs = m_eap.m_mixer_nb_rx;
    if(!m_eap.readRegBlock(eRT_Mixer, 4, m_coeff, nb_inputs * nb_outputs * 4)) {
        debugError("Failed to read coefficients\n");
        return false;
    }
    return true;
}

bool
Device::EAP::Mixer::storeCoefficients()
{
    if(m_coeff == NULL) {
        debugError("Coefficient cache not initialized\n");
        return false;
    }
    if(m_eap.m_mixer_readonly) {
        debugWarning("Mixer is read-only\n");
        return false;
    }
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int nb_outputs = m_eap.m_mixer_nb_rx;
    if(!m_eap.writeRegBlock(eRT_Mixer, 4, m_coeff, nb_inputs * nb_outputs * 4)) {
        debugError("Failed to read coefficients\n");
        return false;
    }
    return true;
}

void
Device::EAP::Mixer::updateNameCache()
{
    // figure out the number of i/o's
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int nb_outputs = m_eap.m_mixer_nb_rx;

    // clear the previous map
    m_input_route_map.clear();
    m_output_route_map.clear();

    // find the active router configuration
    RouterConfig * rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not get active routing info\n");
        return;
    }

    // find the inputs
    for(int i=0; i < nb_inputs; i++) {
        int ch = i;
        // the destination id of the mixer input
        int dest_int = m_eap.m_mixer_tx_id;

        // from the DICE mixer spec:
        // we can have 16 channels per "block"
        // if there are more, consecutive block id's are assumed
        while(ch > 15) {
            ch -= 16;
            dest_int += 1;
        }
        // the destination block and channel corresponding with this
        // mixer input is now known
        enum eRouteDestination dest = rcfg->intToRouteDestination(dest_int);

        // get the source for this mixer channel
        m_input_route_map[i] = rcfg->getRouteForDestination(dest, ch);

        debugOutput(DEBUG_LEVEL_VERBOSE, "Mixer input channel %2d source: %s\n", i,
                                          srcBlockToString(m_input_route_map[i].src),
                                          m_input_route_map[i].srcChannel);
    }

    // find where the outputs are connected to
    for(int i=0; i < nb_outputs; i++) {
        int ch = i;
        // the source id of the mixer input
        int src_int = m_eap.m_mixer_rx_id;

        // from the DICE mixer spec:
        // we can have 16 channels per "block"
        // if there are more, consecutive block id's are assumed
        while(ch > 15) {
            ch -= 16;
            src_int += 1;
        }

        // the source block and channel corresponding with this
        // mixer output is now known
        enum eRouteSource src = rcfg->intToRouteSource(src_int);

        // get the routing destinations for this mixer channel
        m_output_route_map[i] = rcfg->getRoutesForSource(src, ch);

        #ifdef DEBUG
        std::string destinations;
        for ( RouterConfig::RouteVectorIterator it = m_output_route_map[i].begin();
            it != m_output_route_map[i].end();
            ++it )
        {
            RouterConfig::Route r = *it;
            // check whether the destination is valid
            if((r.dst != eRD_Invalid) && (r.dstChannel >= 0)) {
                char tmp[128];
                snprintf(tmp, 128, "%s:%d,", dstBlockToString(r.dst), r.dstChannel);
                destinations += tmp;
            }
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "Mixer output channel %2d destinations: %s\n", i, destinations.c_str());
        #endif
    }
}

void
Device::EAP::Mixer::show()
{
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int nb_outputs = m_eap.m_mixer_nb_rx;

    updateNameCache();

    const size_t bufflen = 4096;
    char tmp[bufflen];
    int cnt;

    cnt = 0;
    for(int j=0; j < nb_inputs; j++) {
        cnt += snprintf(tmp+cnt, bufflen-cnt, "   %02d   ", j);
    }
    printMessage("%s\n", tmp);

    cnt = 0;
    for(int j=0; j < nb_inputs; j++) {
        cnt += snprintf(tmp+cnt, bufflen-cnt, "%s:%02d ",
                        srcBlockToString(m_input_route_map[j].src),
                        m_input_route_map[j].srcChannel);
    }
    printMessage("%s\n", tmp);

    // display coefficients
    for(int i=0; i < nb_outputs; i++) {
        cnt = 0;
        for(int j=0; j < nb_inputs; j++) {
            cnt += snprintf(tmp+cnt, bufflen-cnt, "%07d ", *(m_coeff + nb_inputs * i + j));
        }

        // construct the set of destinations
        std::string destinations;
        for ( RouterConfig::RouteVectorIterator it = m_output_route_map[i].begin();
            it != m_output_route_map[i].end();
            ++it )
        {
            RouterConfig::Route r = *it;
            // check whether the destination is valid
            if((r.dst != eRD_Invalid) && (r.dstChannel >= 0)) {
                char tmp[128];
                snprintf(tmp, 128, "%s:%d,", dstBlockToString(r.dst), r.dstChannel);
                destinations += tmp;
            }
        }

        cnt += snprintf(tmp+cnt, bufflen-cnt, "=[%02d]=> %s ", i, destinations.c_str());
        printMessage("%s\n", tmp);
    }

}

// The control interface to the mixer
std::string
Device::EAP::Mixer::getRowName( const int row )
{
    return "FIXME";
}

std::string
Device::EAP::Mixer::getColName( const int col )
{
    return "FIXME";
}

int
Device::EAP::Mixer::canWrite( const int row, const int col)
{
    if(m_eap.m_mixer_readonly) {
        return false;
    }
    return (row >= 0 && row < m_eap.m_mixer_nb_tx && col >= 0 && col < m_eap.m_mixer_nb_rx);
}

double
Device::EAP::Mixer::setValue( const int row, const int col, const double val)
{
    if(m_eap.m_mixer_readonly) {
        debugWarning("Mixer is read-only\n");
        return false;
    }
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int addr = ((nb_inputs * row) + col) * 4;
    quadlet_t tmp = (quadlet_t) val;
    if(!m_eap.writeRegBlock(eRT_Mixer, 4+addr, &tmp, 4)) {
        debugError("Failed to write coefficient\n");
        return 0;
    }
    return (double)(tmp);
}

double
Device::EAP::Mixer::getValue( const int row, const int col)
{
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int addr = ((nb_inputs * row) + col) * 4;
    quadlet_t tmp;
    if(!m_eap.readRegBlock(eRT_Mixer, 4+addr, &tmp, 4)) {
        debugError("Failed to read coefficient\n");
        return 0;
    }
    return (double)(tmp);
}

int
Device::EAP::Mixer::getRowCount()
{
    return m_eap.m_mixer_nb_tx;
}

int
Device::EAP::Mixer::getColCount()
{
    return m_eap.m_mixer_nb_rx;
}

// full map updates are unsupported
bool
Device::EAP::Mixer::getCoefficientMap(int &) {
    return false;
}

bool
Device::EAP::Mixer::storeCoefficientMap(int &) {
    if(m_eap.m_mixer_readonly) {
        debugWarning("Mixer is read-only\n");
        return false;
    }
    return false;
}

// ----------- Router -------------
// FIXME: some more efficient datastructure for the 
//        sources and destinations might be good


Device::EAP::Router::Router(EAP &p)
: Control::CrossbarRouter(&p.m_device, "Router")
, m_eap(p)
, m_peak( *(new PeakSpace(p)) )
, m_debugModule(p.m_debugModule)
{
    setupSources();
    setupDestinations();
}

Device::EAP::Router::~Router()
{
    delete &m_peak;
}

void
Device::EAP::Router::setupSourcesAddSource(const char *basename, enum eRouteSource srcid, 
                                           unsigned int base, unsigned int cnt)
{
    unsigned int i=0;
    char name[16];
    for (i=0; i<cnt; i++) {
        snprintf(name, 16, "%s:%02d", basename, base+i);
        struct Source s = {name, srcid, i};
        m_sources.push_back(s);
    }
}

void
Device::EAP::Router::setupDestinationsAddDestination(const char *basename, enum eRouteDestination dstid,
                                                     unsigned int base, unsigned int cnt)
{
    unsigned int i=0;
    char name[16];
    for (i=0; i<cnt; i++) {
        snprintf(name, 16, "%s:%02d", basename, base+i);
        struct Destination d = {name, dstid, i};
        m_destinations.push_back(d);
    }
}


void
Device::EAP::Router::setupSources() {
    // add the routing sources and destinations for a DICE chip
    switch(m_eap.m_general_chip) {
        case DICE_EAP_CAP_GENERAL_CHIP_DICEII:
            // router/EAP currently not supported
            break;
        case DICE_EAP_CAP_GENERAL_CHIP_DICEJR:
            // these are unique to the junior

            // second audio port
            setupSourcesAddSource("InS1", eRS_InS1, 0, 8);

        case DICE_EAP_CAP_GENERAL_CHIP_DICEMINI:
            // these are common to the mini and junior

            // the AES receiver
            setupSourcesAddSource("AES", eRS_AES, 0, 8);

            // the ADAT receiver
            setupSourcesAddSource("ADAT", eRS_ADAT, 0, 8);

            // the Mixer outputs
            setupSourcesAddSource("MixerOut", eRS_Mixer, 0, 16);

            // the first audio port
            setupSourcesAddSource("InS0", eRS_InS0, 0, 8);

            // the ARM audio port
            setupSourcesAddSource("ARM", eRS_ARM, 0, 8);

            // the 1394 stream receivers
            setupSourcesAddSource("1394_0", eRS_ARX0, 0, 16);
            setupSourcesAddSource("1394_1", eRS_ARX1, 0, 16);

            // mute
            setupSourcesAddSource("Mute", eRS_Muted, 0, 1);

            break;
        default:
            // this is an unsupported chip
            break;
    }
}

void
Device::EAP::Router::setupDestinations() {
    // add the routing sources and destinations for a DICE chip
    switch(m_eap.m_general_chip) {
        case DICE_EAP_CAP_GENERAL_CHIP_DICEII:
            // router/EAP currently not supported
            break;
        case DICE_EAP_CAP_GENERAL_CHIP_DICEJR:
            // these are unique to the junior

            // second audio port
            setupDestinationsAddDestination("InS1", eRD_InS1, 0, 8);

        case DICE_EAP_CAP_GENERAL_CHIP_DICEMINI:
            // these are common to the mini and junior

            // the AES receiver
            setupDestinationsAddDestination("AES", eRD_AES, 0, 8);

            // the ADAT receiver
            setupDestinationsAddDestination("ADAT", eRD_ADAT, 0, 8);

            // the Mixer outputs
            setupDestinationsAddDestination("MixerIn", eRD_Mixer0, 0, 16);
            setupDestinationsAddDestination("MixerIn", eRD_Mixer1, 16, 2);

            // the first audio port
            setupDestinationsAddDestination("InS0", eRD_InS0, 0, 8);

            // the ARM audio port
            setupDestinationsAddDestination("ARM", eRD_ARM, 0, 8);

            // the 1394 stream receivers
            setupDestinationsAddDestination("1394_0", eRD_ATX0, 0, 16);
            setupDestinationsAddDestination("1394_1", eRD_ATX1, 0, 16);

            // mute
            setupDestinationsAddDestination("Mute", eRD_Muted, 0, 1);

            break;
        default:
            // this is an unsupported chip
            break;
    }
}

std::string
Device::EAP::Router::getSourceName(const int srcid)
{
    if((unsigned)srcid < m_sources.size()) {
        return m_sources.at(srcid).name;
    } else {
        debugWarning("source id out of range (%d)\n", srcid);
        return "";
    }
}

std::string
Device::EAP::Router::getDestinationName(const int dstid)
{
    if((unsigned)dstid < m_destinations.size()) {
        return m_destinations.at(dstid).name;
    } else {
        debugWarning("destination id out of range (%d)\n", dstid);
        return "";
    }
}

int
Device::EAP::Router::getSourceIndex(std::string name)
{
    int i = 0;
    for ( SourceVectorIterator it = m_sources.begin();
        it != m_sources.end();
        ++it )
    {
        if(it->name == name) return i;
        i++;
    }
    return -1;
}

int
Device::EAP::Router::getDestinationIndex(std::string name)
{
    int i = 0;
    for ( DestinationVectorIterator it = m_destinations.begin();
        it != m_destinations.end();
        ++it )
    {
        if(it->name == name) return i;
        i++;
    }
    return -1;
}

int
Device::EAP::Router::getSourceIndex(enum eRouteSource srcid, int channel)
{
    int i = 0;
    for ( SourceVectorIterator it = m_sources.begin();
        it != m_sources.end();
        ++it )
    {
        if((it->src == srcid) && (it->srcChannel == channel)) return i;
        i++;
    }
    return -1;
}

int
Device::EAP::Router::getDestinationIndex(enum eRouteDestination dstid, int channel)
{
    int i = 0;
    for ( DestinationVectorIterator it = m_destinations.begin();
        it != m_destinations.end();
        ++it )
    {
        if((it->dst == dstid) && (it->dstChannel == channel)) return i;
        i++;
    }
    return -1;
}

Control::CrossbarRouter::NameVector
Device::EAP::Router::getSourceNames()
{
    Control::CrossbarRouter::NameVector n;
    for ( SourceVectorIterator it = m_sources.begin();
        it != m_sources.end();
        ++it )
    {
        n.push_back(it->name);
    }
    return n;
}

Control::CrossbarRouter::NameVector
Device::EAP::Router::getDestinationNames()
{
    Control::CrossbarRouter::NameVector n;
    for ( DestinationVectorIterator it = m_destinations.begin();
        it != m_destinations.end();
        ++it )
    {
        n.push_back(it->name);
    }
    return n;
}

Control::CrossbarRouter::Groups
Device::EAP::Router::getSources()
{
    debugError("Device::EAP::Router::getSources() is not yet implemented!");
    return Control::CrossbarRouter::Groups();
}

Control::CrossbarRouter::Groups
Device::EAP::Router::getDestinations()
{
    debugError("Device::EAP::Router::getDestinations() is not yet implemented!");
    return Control::CrossbarRouter::Groups();
}

Control::CrossbarRouter::IntVector
Device::EAP::Router::getDestinationsForSource(const int srcid)
{
    IntVector retval;
    if((unsigned)srcid < m_sources.size()) {
        Source s = m_sources.at(srcid);

        // get the routing configuration
        RouterConfig *rcfg = m_eap.getActiveRouterConfig();
        if(rcfg == NULL) {
            debugError("Could not request active router configuration\n");
            return retval;
        }
        // get the source routes
        RouterConfig::RouteVector v = rcfg->getRoutesForSource(s.src, s.srcChannel);

        for ( RouterConfig::RouteVectorIterator it = v.begin();
            it != v.end();
            ++it )
        {
            // FIXME: some more efficient datastructure might be good to
            // avoid this loop
            RouterConfig::Route &r = *it;
            int i = 0;
            for ( DestinationVectorIterator it = m_destinations.begin();
                it != m_destinations.end();
                ++it )
            {
                if((it->dst == r.dst) && (it->dstChannel == r.dstChannel)) {
                    retval.push_back(i);
                    break; // can only match once
                }
                i++;
            }
        }
        return retval;
    } else {
        debugWarning("source id out of range (%d)\n", srcid);
        return retval;
    }
}

int
Device::EAP::Router::getSourceForDestination(const int dstid)
{
    if((unsigned)dstid < m_destinations.size()) {
        Destination d = m_destinations.at(dstid);

        // get the routing configuration
        RouterConfig *rcfg = m_eap.getActiveRouterConfig();
        if(rcfg == NULL) {
            debugError("Could not request active router configuration\n");
            return false;
        }

        RouterConfig::Route r = rcfg->getRouteForDestination(d.dst, d.dstChannel);
        if(r.src == eRS_Invalid) {
            return -1;
        } else {
            // FIXME: some more efficient datastructure might be good to
            // avoid this loop
            int i = 0;
            for ( SourceVectorIterator it = m_sources.begin();
                it != m_sources.end();
                ++it )
            {
                if((it->src == r.src) && (it->srcChannel == r.srcChannel)) return i;
                i++;
            }
            return -1;
        }
    } else {
        debugWarning("destination id out of range (%d)\n", dstid);
        return -1;
    }
}

int
Device::EAP::Router::getNbSources()
{
    return m_sources.size();
}

int
Device::EAP::Router::getNbDestinations()
{
    return m_destinations.size();
}

bool
Device::EAP::Router::canConnect(const int source, const int dest)
{
    if((unsigned)source >= m_sources.size()) {
        debugWarning("source id out of range (%d)\n", source);
        return false;
    }
    Source s = m_sources.at(source);

    if((unsigned)dest >= m_destinations.size()) {
        debugWarning("destination id out of range (%d)\n", dest);
        return false;
    }
    Destination d = m_destinations.at(dest);

    // we can connect anything
    // FIXME: can we?
    return true;
}

bool
Device::EAP::Router::setConnectionState(const int source, const int dest, const bool enable)
{
    if((unsigned)source >= m_sources.size()) {
        debugWarning("source id out of range (%d)\n", source);
        return false;
    }
    Source s = m_sources.at(source);

    if((unsigned)dest >= m_destinations.size()) {
        debugWarning("destination id out of range (%d)\n", dest);
        return false;
    }
    Destination d = m_destinations.at(dest);

    // get the routing configuration
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return false;
    }

    // build a new routing configuration
    RouterConfig newcfg = Device::EAP::RouterConfig(*rcfg);

    // construct the routing entry to find
    RouterConfig::Route r = {s.src, s.srcChannel, d.dst, d.dstChannel};

    // find the appropriate entry
    int idx = newcfg.getRouteIndex(r);

    if (idx < 0) {
        // we have to add the route
        newcfg.insertRoute(r);
    } else {
        // the route is already present, so we can replace it
        if(enable) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "connection %d => %d already present\n", source, dest);
            return true;
        } else {
            // remove the route
            newcfg.removeRoute(idx);
        }
    }

    // if we get here it means we have to upload a new router config
    if(!m_eap.updateCurrentRouterConfig(newcfg)) {
        debugError("Could not update router config\n");
        return false;
    }

    return true;
}

bool
Device::EAP::Router::getConnectionState(const int source, const int dest)
{
    if((unsigned)source >= m_sources.size()) {
        debugWarning("source id out of range (%d)\n", source);
        return false;
    }
    Source s = m_sources.at(source);

    if((unsigned)dest >= m_destinations.size()) {
        debugWarning("destination id out of range (%d)\n", dest);
        return false;
    }
    Destination d = m_destinations.at(dest);

    // get the routing configuration
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return false;
    }

    // build a new routing configuration
    RouterConfig newcfg = Device::EAP::RouterConfig(*rcfg);

    // construct the routing entry to find
    RouterConfig::Route r = {s.src, s.srcChannel, d.dst, d.dstChannel};

    // find the appropriate entry
    int idx = newcfg.getRouteIndex(r);

    if (idx < 0) {
        // the route is not present
        return false;
    } else {
        // the route is already present
        return true;
    }
}

bool
Device::EAP::Router::canConnect(std::string src, std::string dst)
{
    int srcidx = getSourceIndex(src);
    int dstidx = getDestinationIndex(dst);
    return canConnect(srcidx, dstidx);
}

bool
Device::EAP::Router::setConnectionState(std::string src, std::string dst, const bool enable)
{
    int srcidx = getSourceIndex(src);
    int dstidx = getDestinationIndex(dst);
    return setConnectionState(srcidx, dstidx, enable);
}

bool
Device::EAP::Router::getConnectionState(std::string src, std::string dst)
{
    int srcidx = getSourceIndex(src);
    int dstidx = getDestinationIndex(dst);
    return getConnectionState(srcidx, dstidx);
}

// the map is organized as a matrix where the 
// rows are the destinations and the columns are
// the sources

// note that map as assumed to be big enough and
// allocated by the user
bool
Device::EAP::Router::getConnectionMap(int *map)
{
    unsigned int nb_sources = getNbSources();
    unsigned int nb_destinations = getNbDestinations();

    // clear the map
    memset(map, 0, nb_sources * nb_destinations * sizeof(int));

    // get the routing configuration
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return false;
    }

    unsigned int nb_routes = rcfg->getNbRoutes();

    for(unsigned int i=0; i<nb_routes; i++) {
        struct RouterConfig::Route r = rcfg->getRoute(i);
        int srcidx = getSourceIndex(r.src, r.srcChannel);
        int dstidx = getDestinationIndex(r.dst, r.dstChannel);
        if(srcidx < 0) {
            debugWarning("bogus source (%d, %d) in route table\n", r.src, r.srcChannel);
        } else if(dstidx < 0) {
            debugWarning("bogus destination (%d, %d) in route table\n", r.dst, r.dstChannel);
        } else {
            int *ptr = map;
            ptr += dstidx * nb_sources;
            ptr += srcidx;
            *ptr = 1; // route present
        }
    }
    return true;
}

bool
Device::EAP::Router::setConnectionMap(int *map)
{
    return false;
}

bool
Device::EAP::Router::clearAllConnections()
{
    // build a new empty routing configuration
    RouterConfig newcfg = Device::EAP::RouterConfig(m_eap);

    // upload the new router config
    if(!m_eap.updateCurrentRouterConfig(newcfg)) {
        debugError("Could not update router config\n");
        return false;
    }
    return true;
}

bool
Device::EAP::Router::hasPeakMetering()
{
    return m_eap.m_router_exposed;
}

double
Device::EAP::Router::getPeakValue(const int source, const int dest)
{
    if((unsigned)source >= m_sources.size()) {
        debugWarning("source id out of range (%d)\n", source);
        return false;
    }
    Source s = m_sources.at(source);

    if((unsigned)dest >= m_destinations.size()) {
        debugWarning("destination id out of range (%d)\n", dest);
        return false;
    }
    Destination d = m_destinations.at(dest);

    debugOutput(DEBUG_LEVEL_VERBOSE, "getting peak info for [%d] %s => [%d] %s\n", 
                                     source, s.name.c_str(),
                                     dest, d.name.c_str());

    // update the peak information
    m_peak.read();

    // construct the routing entry to find
    RouterConfig::Route r = {s.src, s.srcChannel, d.dst, d.dstChannel, 0};

    // find the appropriate entry
    int idx = m_peak.getRouteIndex(r);

    if (idx < 0) {
        // the route is not present
        return -1;
    } else {
        // the route is present
        r = m_peak.getRoute(idx);
        return r.peak;
    }

}

Control::CrossbarRouter::PeakValues
Device::EAP::Router::getPeakValues()
{
    m_peak.read();
    Control::CrossbarRouter::PeakValues values;
    for (unsigned int i=0; i<m_peak.getNbRoutes(); ++i) {
        Control::CrossbarRouter::PeakValue tmp;
        RouterConfig::Route route = m_peak.getRoute(i);
        tmp.destination = getDestinationIndex(route.dst, route.dstChannel);
        tmp.peakvalue = route.peak;
        values.push_back(tmp);
    }
    return values;
}

void
Device::EAP::Router::show()
{
    // print the peak space as it also contains the routing configuration
    printMessage("Active router config:\n");
    m_peak.read();
    m_peak.show();
}

// ----------- routing config -------------
Device::EAP::RouterConfig::RouterConfig(EAP &p)
: m_eap(p)
, m_base(eRT_None), m_offset(0)
, m_debugModule(p.m_debugModule)
{}

Device::EAP::RouterConfig::RouterConfig(EAP &p, enum eRegBase b, unsigned int o)
: m_eap(p)
, m_base(b), m_offset(o)
, m_debugModule(p.m_debugModule)
{}

Device::EAP::RouterConfig::~RouterConfig()
{}

bool
Device::EAP::RouterConfig::read(enum eRegBase base, unsigned offset)
{
    // first clear the current route vector
    m_routes.clear();

    uint32_t nb_routes;
    if(!m_eap.readRegBlock(base, offset, &nb_routes, 4)) {
        debugError("Failed to read number of entries\n");
        return false;
    }
    if(nb_routes == 0) {
        debugWarning("No routes found\n");
    }

    // read the route info
    uint32_t tmp_entries[nb_routes];
    if(!m_eap.readRegBlock(base, offset+4, tmp_entries, nb_routes*4)) {
        debugError("Failed to read router config block information\n");
        return false;
    }

    // decode into the routing vector
    for(unsigned int i=0; i < nb_routes; i++) {
        m_routes.push_back(decodeRoute(tmp_entries[i]));
    }
    return true;
}

bool
Device::EAP::RouterConfig::write(enum eRegBase base, unsigned offset)
{
    uint32_t nb_routes = m_routes.size();
    if(nb_routes == 0) {
        debugWarning("Writing 0 routes?\n");
    }
    uint32_t tmp_entries[nb_routes];

    // encode from the routing vector
    int i=0;
    for ( RouteVectorIterator it = m_routes.begin();
        it != m_routes.end();
        ++it )
    {
        tmp_entries[i] = encodeRoute( *it );
        i++;
    }

    // write the result to the device
    if(!m_eap.writeRegBlock(base, offset+4, tmp_entries, nb_routes*4)) {
        debugError("Failed to write router config block information\n");
        return false;
    }
    if(!m_eap.writeRegBlock(base, offset, &nb_routes, 4)) {
        debugError("Failed to write number of entries\n");
        return false;
    }
    return true;
}

bool
Device::EAP::RouterConfig::insertRoute(struct Route r, unsigned int index)
{
    unsigned int nb_routes = getNbRoutes();
    if(index > nb_routes) {
        debugError("Index out of range\n");
        return false;
    }
    if (index == nb_routes) { // append
        m_routes.push_back(r);
        return true;
    }
    // insert
    RouteVectorIterator pos = m_routes.begin() + index;
    m_routes.insert(pos, r);
    return true;
}

bool
Device::EAP::RouterConfig::replaceRoute(unsigned int old_index, struct Route new_route)
{
    if(old_index >= getNbRoutes()) {
        debugError("Index out of range\n");
        return false;
    }
    if(!removeRoute(old_index)) {
        debugError("Could not remove old route\n");
        return false;
    }
    return insertRoute(new_route, old_index);
}

bool
Device::EAP::RouterConfig::replaceRoute(struct Route old_route, struct Route new_route)
{
    int idx = getRouteIndex(old_route);
    if(idx < 0) {
        debugWarning("Route not found\n");
        return false;
    }
    return replaceRoute((unsigned int)idx, new_route);
}

bool
Device::EAP::RouterConfig::removeRoute(struct Route r)
{
    int idx = getRouteIndex(r);
    if(idx < 0) {
        debugWarning("Route not found\n");
        return false;
    }
    return removeRoute((unsigned int)idx);
}

bool
Device::EAP::RouterConfig::removeRoute(unsigned int index)
{
    if(index >= getNbRoutes()) {
        debugError("Index out of range\n");
        return false;
    }
    RouteVectorIterator pos = m_routes.begin() + index;
    m_routes.erase(pos);
    return true;
}

int
Device::EAP::RouterConfig::getRouteIndex(struct Route r)
{
    int i = 0;
    for ( RouteVectorIterator it = m_routes.begin();
        it != m_routes.end();
        ++it )
    {
        struct Route t = *it;
        if ((t.src == r.src) && (t.srcChannel == r.srcChannel) && (t.dst == r.dst) && (t.dstChannel == r.dstChannel)) return i;
        i++;
    }
    return -1;
}

struct Device::EAP::RouterConfig::Route
Device::EAP::RouterConfig::getRoute(unsigned int idx)
{
    if( (idx < 0) || (idx >= m_routes.size()) ) {
        debugWarning("Route index out of range (%d)\n", idx);
        Route r = {eRS_Invalid, -1, eRD_Invalid, -1, 0};
        return r;
    }
    return m_routes.at(idx);
}

#define CASE_INT_EQUAL_RETURN(_x) case (int)(_x): return _x;
enum Device::EAP::eRouteDestination
Device::EAP::RouterConfig::intToRouteDestination(int dst)
{
    switch(dst) {
        CASE_INT_EQUAL_RETURN(eRD_AES);
        CASE_INT_EQUAL_RETURN(eRD_ADAT);
        CASE_INT_EQUAL_RETURN(eRD_Mixer0);
        CASE_INT_EQUAL_RETURN(eRD_Mixer1);
        CASE_INT_EQUAL_RETURN(eRD_InS0);
        CASE_INT_EQUAL_RETURN(eRD_InS1);
        CASE_INT_EQUAL_RETURN(eRD_ARM);
        CASE_INT_EQUAL_RETURN(eRD_ATX0);
        CASE_INT_EQUAL_RETURN(eRD_ATX1);
        CASE_INT_EQUAL_RETURN(eRD_Muted);
        default: return eRD_Invalid;
    }
}

enum Device::EAP::eRouteSource
Device::EAP::RouterConfig::intToRouteSource(int src)
{
    switch(src) {
        CASE_INT_EQUAL_RETURN(eRS_AES);
        CASE_INT_EQUAL_RETURN(eRS_ADAT);
        CASE_INT_EQUAL_RETURN(eRS_Mixer);
        CASE_INT_EQUAL_RETURN(eRS_InS0);
        CASE_INT_EQUAL_RETURN(eRS_InS1);
        CASE_INT_EQUAL_RETURN(eRS_ARM);
        CASE_INT_EQUAL_RETURN(eRS_ARX0);
        CASE_INT_EQUAL_RETURN(eRS_ARX1);
        CASE_INT_EQUAL_RETURN(eRS_Muted);
        default: return eRS_Invalid;
    }
}

struct Device::EAP::RouterConfig::Route
Device::EAP::RouterConfig::decodeRoute(uint32_t val) {
    int routerval = val & 0xFFFF;
    int peak = (val >> 16) & 0x0FFF;
    int src_blk = (routerval >> 12) & 0xF;
    int src_ch = (routerval >> 8) & 0xF;
    int dst_blk = (routerval >> 4) & 0xF;
    int dst_ch = (routerval >> 0) & 0xF;
    struct Route r = {intToRouteSource(src_blk), src_ch, intToRouteDestination(dst_blk), dst_ch, peak};
    return r;
}

uint32_t
Device::EAP::RouterConfig::encodeRoute(struct Route r) {
    if(r.src == eRS_Invalid || r.dst == eRD_Invalid) {
        debugWarning("Encoding invalid source/dest (%d/%d)\n", r.src, r.dst);
//         return 0xFFFFFFFF;
    }
    unsigned int src_blk = ((unsigned int)r.src) & 0xF;
    unsigned int src_ch = ((unsigned int)r.srcChannel) & 0xF;
    unsigned int dst_blk = ((unsigned int)r.dst) & 0xF;
    unsigned int dst_ch = ((unsigned int)r.dstChannel) & 0xF;
    uint32_t routerval = 0;
    routerval |= (src_blk << 12);
    routerval |= (src_ch << 8);
    routerval |= (dst_blk << 4);
    routerval |= (dst_ch << 0);
    return routerval;
}

struct Device::EAP::RouterConfig::Route
Device::EAP::RouterConfig::getRouteForDestination(enum eRouteDestination dst, int channel)
{
    for ( RouteVectorIterator it = m_routes.begin();
        it != m_routes.end();
        ++it )
    {
        struct Route r = *it;
        if((r.dst == (int)dst) && (r.dstChannel == channel)) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "%s:%02d comes from %s:%02d\n",
                                                  dstBlockToString(r.dst), r.dstChannel,
                                                  srcBlockToString(r.src), r.srcChannel);
            return r;
        }
    }
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "%s:%02d source can't be found\n",
                                          dstBlockToString((int)dst), channel);
    struct Route r = {eRS_Invalid, -1, eRD_Invalid, 0, 0};
    return r;
}

std::vector<struct Device::EAP::RouterConfig::Route>
Device::EAP::RouterConfig::getRoutesForSource(enum eRouteSource src, int channel)
{
    std::vector<struct Route>routes;
    for ( RouteVectorIterator it = m_routes.begin();
        it != m_routes.end();
        ++it )
    {
        struct Route r = *it;
        if((r.src == (int)src) && (r.srcChannel == channel)) {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "%s:%02d goes to %s:%02d\n",
                                                  srcBlockToString(r.src), r.srcChannel,
                                                  dstBlockToString(r.dst), r.dstChannel);
            routes.push_back(r);
        }
    }
    return routes;
}

void
Device::EAP::RouterConfig::show()
{
    for ( RouteVectorIterator it = m_routes.begin();
        it != m_routes.end();
        ++it )
    {
        struct Route r = *it;
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "%s:%02d => %s:%02d\n",
                                              srcBlockToString(r.src), r.srcChannel,
                                              dstBlockToString(r.dst), r.dstChannel);
    }
}


// ----------- peak space -------------

bool
Device::EAP::PeakSpace::read(enum eRegBase base, unsigned offset)
{
    // first clear the current route vector
    m_routes.clear();

    uint32_t nb_routes;
    // we have to figure out the number of entries through the currently
    // active router config
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not get active router config\n");
        return false;
    }
    nb_routes = rcfg->getNbRoutes();

    // read the route info
    uint32_t tmp_entries[nb_routes];
    if(!m_eap.readRegBlock(base, offset, tmp_entries, nb_routes*4)) {
        debugError("Failed to read peak block information\n");
        return false;
    }

    // decode into the routing vector
    for(unsigned int i=0; i < nb_routes; i++) {
        m_routes.push_back(decodeRoute(tmp_entries[i]));
    }
//     show();
    return true;
}

bool
Device::EAP::PeakSpace::write(enum eRegBase base, unsigned offset)
{
    debugError("Peak space is read-only\n");
    return true;
}

void
Device::EAP::PeakSpace::show()
{
    for ( RouteVectorIterator it = m_routes.begin();
        it != m_routes.end();
        ++it )
    {
        struct Route r = *it;
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "%s:%02d => %s:%02d : %06d\n",
                                              srcBlockToString(r.src), r.srcChannel,
                                              dstBlockToString(r.dst), r.dstChannel,
                                              r.peak);
    }
}

// ----------- stream config block -------------
Device::EAP::StreamConfig::StreamConfig(EAP &p, enum eRegBase b, unsigned int o)
: m_eap(p)
, m_base(b), m_offset(o)
, m_nb_tx(0), m_nb_rx(0)
, m_tx_configs(NULL), m_rx_configs(NULL)
, m_debugModule(p.m_debugModule)
{

}

Device::EAP::StreamConfig::~StreamConfig()
{
    if(m_tx_configs) delete[]m_tx_configs;
    if(m_rx_configs) delete[]m_rx_configs;
}

bool
Device::EAP::StreamConfig::read(enum eRegBase base, unsigned offset)
{
    if(!m_eap.readRegBlock(base, offset, &m_nb_tx, 4)) {
        debugError("Failed to read number of tx entries\n");
        return false;
    }
    if(!m_eap.readRegBlock(base, offset+4, &m_nb_rx, 4)) {
        debugError("Failed to read number of rx entries\n");
        return false;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " Entries: TX: %lu, RX: %lu\n", m_nb_tx, m_nb_rx);

    if(m_tx_configs) {
        delete[]m_tx_configs;
        m_tx_configs = NULL;
    }
    if(m_rx_configs) {
        delete[]m_rx_configs;
        m_rx_configs = NULL;
    }
    
    offset += 8;
    if(m_nb_tx > 0) {
        m_tx_configs = new struct ConfigBlock[m_nb_tx];
        for(unsigned int i=0; i<m_nb_tx; i++) {
            fb_quadlet_t *ptr = reinterpret_cast<fb_quadlet_t *>(&(m_tx_configs[i]));
            if(!m_eap.readRegBlock(base, offset, ptr, sizeof(struct ConfigBlock))) {
                debugError("Failed to read tx entry %d\n", i);
                return false;
            }
            offset += sizeof(struct ConfigBlock);
        }
    }

    if(m_nb_rx > 0) {
        m_rx_configs = new struct ConfigBlock[m_nb_rx];
        for(unsigned int i=0; i<m_nb_rx; i++) {
            fb_quadlet_t *ptr = reinterpret_cast<fb_quadlet_t *>(&(m_rx_configs[i]));
            if(!m_eap.readRegBlock(base, offset, ptr, sizeof(struct ConfigBlock))) {
                debugError("Failed to read rx entry %d\n", i);
                return false;
            }
            offset += sizeof(struct ConfigBlock);
        }
    }
    return true;
}

bool
Device::EAP::StreamConfig::write(enum eRegBase base, unsigned offset)
{
    if(!m_eap.writeRegBlock(base, offset, &m_nb_tx, 4)) {
        debugError("Failed to write number of tx entries\n");
        return false;
    }
    if(!m_eap.writeRegBlock(base, offset+4, &m_nb_rx, 4)) {
        debugError("Failed to write number of rx entries\n");
        return false;
    }

    offset += 8;
    for(unsigned int i=0; i<m_nb_tx; i++) {
        fb_quadlet_t *ptr = reinterpret_cast<fb_quadlet_t *>(&(m_tx_configs[i]));
        if(!m_eap.writeRegBlock(base, offset, ptr, sizeof(struct ConfigBlock))) {
            debugError("Failed to write tx entry %d\n", i);
            return false;
        }
        offset += sizeof(struct ConfigBlock);
    }

    for(unsigned int i=0; i<m_nb_rx; i++) {
        fb_quadlet_t *ptr = reinterpret_cast<fb_quadlet_t *>(&(m_rx_configs[i]));
        if(!m_eap.writeRegBlock(base, offset, ptr, sizeof(struct ConfigBlock))) {
            debugError("Failed to write rx entry %d\n", i);
            return false;
        }
        offset += sizeof(struct ConfigBlock);
    }
    return true;
}

Device::diceNameVector
Device::EAP::StreamConfig::getNamesForBlock(struct ConfigBlock &b)
{
    diceNameVector names;
    char namestring[DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_BYTES+1];

    memcpy(namestring, b.names, DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_BYTES);

    // Strings from the device are always little-endian,
    // so byteswap for big-endian machines
    #if __BYTE_ORDER == __BIG_ENDIAN
    byteSwapBlock((quadlet_t *)namestring, DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS);
    #endif

    namestring[DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_BYTES]='\0';
    return m_eap.m_device.splitNameString(std::string(namestring));
}

void
Device::EAP::StreamConfig::showConfigBlock(struct ConfigBlock &b)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, " Channel count : %lu audio, %lu midi\n", b.nb_audio, b.nb_midi);
    debugOutput(DEBUG_LEVEL_VERBOSE, " AC3 Map       : 0x%08X\n", b.ac3_map);
    diceNameVector channel_names  = getNamesForBlock(b);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Channel names :\n");
    for ( diceNameVectorIterator it = channel_names.begin();
        it != channel_names.end();
        ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE,"     %s\n", (*it).c_str());
    }
}

void
Device::EAP::StreamConfig::show()
{
    for(unsigned int i=0; i<m_nb_tx; i++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "TX Config block %d\n", i);
        showConfigBlock(m_tx_configs[i]);
    }
    for(unsigned int i=0; i<m_nb_rx; i++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "RX Config block %d\n", i);
        showConfigBlock(m_rx_configs[i]);
    }
}

} // namespace Dice



