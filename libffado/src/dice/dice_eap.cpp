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

Device::EAP::EAP(Device &d)
: m_device(d)
, m_debugModule(d.m_debugModule)
{
}

Device::EAP::~EAP()
{
}

bool
Device::EAP::init() {
    if(!supportsEAP(m_device)) {
        debugError("Device does not support EAP");
        return false;
    }

    // offsets and sizes are returned in quadlets, but we use byte values
    if(!readReg(eRT_Base, DICE_EAP_CAPABILITY_SPACE_OFF, &m_capability_offset)) {
        debugError("Could not initialize m_capability_offset\n");
        return false;
    }
    m_capability_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_CAPABILITY_SPACE_SZ, &m_capability_size)) {
        debugError("Could not initialize m_capability_size\n");
        return false;
    }
    m_capability_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_CMD_SPACE_OFF, &m_cmd_offset)) {
        debugError("Could not initialize m_cmd_offset\n");
        return false;
    }
    m_cmd_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_CMD_SPACE_SZ, &m_cmd_size)) {
        debugError("Could not initialize m_cmd_size\n");
        return false;
    }
    m_cmd_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_MIXER_SPACE_OFF, &m_mixer_offset)) {
        debugError("Could not initialize m_mixer_offset\n");
        return false;
    }
    m_mixer_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_MIXER_SPACE_SZ, &m_mixer_size)) {
        debugError("Could not initialize m_mixer_size\n");
        return false;
    }
    m_mixer_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_PEAK_SPACE_OFF, &m_peak_offset)) {
        debugError("Could not initialize m_peak_offset\n");
        return false;
    }
    m_peak_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_PEAK_SPACE_SZ, &m_peak_size)) {
        debugError("Could not initialize m_peak_size\n");
        return false;
    }
    m_peak_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_NEW_ROUTING_SPACE_OFF, &m_new_routing_offset)) {
        debugError("Could not initialize m_new_routing_offset\n");
        return false;
    }
    m_new_routing_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_NEW_ROUTING_SPACE_SZ, &m_new_routing_size)) {
        debugError("Could not initialize m_new_routing_size\n");
        return false;
    }
    m_new_routing_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_NEW_STREAM_CFG_SPACE_OFF, &m_new_stream_cfg_offset)) {
        debugError("Could not initialize m_new_stream_cfg_offset\n");
        return false;
    }
    m_new_stream_cfg_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_NEW_STREAM_CFG_SPACE_SZ, &m_new_stream_cfg_size)) {
        debugError("Could not initialize m_new_stream_cfg_size\n");
        return false;
    }
    m_new_stream_cfg_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_CURR_CFG_SPACE_OFF, &m_curr_cfg_offset)) {
        debugError("Could not initialize m_curr_cfg_offset\n");
        return false;
    }
    m_curr_cfg_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_CURR_CFG_SPACE_SZ, &m_curr_cfg_size)) {
        debugError("Could not initialize m_curr_cfg_size\n");
        return false;
    }
    m_curr_cfg_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_STAND_ALONE_CFG_SPACE_OFF, &m_standalone_offset)) {
        debugError("Could not initialize m_standalone_offset\n");
        return false;
    }
    m_standalone_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_STAND_ALONE_CFG_SPACE_SZ, &m_standalone_size)) {
        debugError("Could not initialize m_standalone_size\n");
        return false;
    }
    m_standalone_size *= 4;
    
    if(!readReg(eRT_Base, DICE_EAP_APP_SPACE_OFF, &m_app_offset)) {
        debugError("Could not initialize m_app_offset\n");
        return false;
    }
    m_app_offset *= 4;

    if(!readReg(eRT_Base, DICE_EAP_APP_SPACE_SZ, &m_app_size)) {
        debugError("Could not initialize m_app_size\n");
        return false;
    }
    m_app_size *= 4;

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
    m_mixer_exposed = (tmp >> DICE_EAP_CAP_ROUTER_EXPOSED) & 0x01;
    m_mixer_readonly = (tmp >> DICE_EAP_CAP_ROUTER_EXPOSED) & 0x01;
    m_mixer_flashstored = (tmp >> DICE_EAP_CAP_ROUTER_EXPOSED) & 0x01;
    m_mixer_input_id = (tmp >> DICE_EAP_CAP_MIXER_IN_DEV) & 0x000F;
    m_mixer_output_id = (tmp >> DICE_EAP_CAP_MIXER_OUT_DEV) & 0x000F;
    m_mixer_nb_inputs = (tmp >> DICE_EAP_CAP_MIXER_INPUTS) & 0x00FF;
    m_mixer_nb_outputs = (tmp >> DICE_EAP_CAP_MIXER_OUTPUTS) & 0x00FF;

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

    return true;
}

void
Device::EAP::show() {
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
    debugOutput(DEBUG_LEVEL_VERBOSE, "         input id: %d, output id: %d\n", m_mixer_input_id, m_mixer_output_id);
    debugOutput(DEBUG_LEVEL_VERBOSE, "         nb inputs: %d, nb outputs: %d\n", m_mixer_nb_inputs, m_mixer_nb_outputs);
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
    if(operationBusy()) {
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
    debugWarning("Untested code\n");
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

bool
Device::EAP::loadFlashConfig() {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_FLASH_CFG;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

bool
Device::EAP::storeFlashConfig() {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_ST_FLASH_CFG;
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
: m_parent(p)
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
    if(!m_parent.m_mixer_exposed) {
        debugError("Device does not expose mixer\n");
        return false;
    }

    // remove previous coefficient array
    if(m_coeff) {
        free(m_coeff);
        m_coeff = NULL;
    }
    
    // allocate coefficient array
    int nb_inputs = m_parent.m_mixer_nb_inputs;
    int nb_outputs = m_parent.m_mixer_nb_outputs;

    m_coeff = (fb_quadlet_t *)calloc(nb_outputs * nb_inputs, sizeof(fb_quadlet_t));

    // load initial values
    if(!updateCoefficients()) {
        debugWarning("Could not initialize coefficients\n");
        return false;
    }
    
    return true;
}

bool
Device::EAP::Mixer::updateCoefficients()
{
    if(m_coeff == NULL) {
        debugError("Coefficients not initialized\n");
        return false;
    }
    int nb_inputs = m_parent.m_mixer_nb_inputs;
    int nb_outputs = m_parent.m_mixer_nb_outputs;
    if(!m_parent.readRegBlock(eRT_Mixer, 4, m_coeff, nb_inputs * nb_outputs * 4)) {
        debugError("Failed to read coefficients\n");
        return false;
    }
    return true;
}

void
Device::EAP::Mixer::show()
{
    int nb_inputs = m_parent.m_mixer_nb_inputs;
    int nb_outputs = m_parent.m_mixer_nb_outputs;

    const size_t bufflen = 4096;
    char tmp[bufflen];
    int cnt;

    cnt = 0;
    cnt += snprintf(tmp+cnt, bufflen-cnt, "%3s ", "");
    for(int j=0; j<nb_inputs; j++) {
        cnt += snprintf(tmp+cnt, bufflen-cnt, "%8d ", j);
    }
    printMessage("%s\n", tmp);

    // display coefficients
    for(int i=0; i<nb_outputs; i++) {
        cnt = 0;
        cnt += snprintf(tmp+cnt, bufflen-cnt, "%03d: ", i);
        for(int j=0; j<nb_inputs; j++) {
            cnt += snprintf(tmp+cnt, bufflen-cnt, "%08X ", *(m_coeff + nb_inputs * i + j));
        }
        printMessage("%s\n", tmp);
    }

}

// ----------- Router -------------
Device::EAP::Router::Router(EAP &p)
: m_parent(p)
, m_debugModule(p.m_debugModule)
{

}

Device::EAP::Router::~Router()
{
}

bool
Device::EAP::Router::init()
{
    return false;
}

void
Device::EAP::Router::show()
{

}

} // namespace Dice



