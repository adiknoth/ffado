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
#include "libutil/ByteSwap.h"

#include <cstdio>

namespace Dice {

// ----------- helper functions -------------

#if 0
static const char *
srcBlockToString(const char id)
{
    switch(id) {
        case eRS_AES:   return "AES ";
        case eRS_ADAT:  return "ADAT";
        case eRS_Mixer: return "MXR ";
        case eRS_InS0:  return "INS0";
        case eRS_InS1:  return "INS1";
        case eRS_ARM:   return "ARM ";
        case eRS_ARX0:  return "AVS0";
        case eRS_ARX1:  return "AVS1";
        case eRS_Muted: return "MUTE";
        default :       return "RSVD";
    }
}

static  const char *
dstBlockToString(const char id)
{
    switch(id) {
        case eRD_AES:    return "AES ";
        case eRD_ADAT:   return "ADAT";
        case eRD_Mixer0: return "MXR0";
        case eRD_Mixer1: return "MXR1";
        case eRD_InS0:   return "INS0";
        case eRD_InS1:   return "INS1";
        case eRD_ARM:    return "ARM ";
        case eRD_ATX0:   return "AVS0";
        case eRD_ATX1:   return "AVS1";
        case eRD_Muted:  return "MUTE";
        default : return "RSVD";
    }
}
#endif

IMPL_DEBUG_MODULE( EAP, EAP, DEBUG_LEVEL_NORMAL );

EAP::EAP(Device &d)
: Control::Container(&d, "EAP")
, m_device(d)
, m_mixer( NULL )
, m_router( NULL )
, m_current_cfg_routing_low ( RouterConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_LOW_ROUTER ) )
, m_current_cfg_routing_mid ( RouterConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_MID_ROUTER ) )
, m_current_cfg_routing_high( RouterConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_HIGH_ROUTER) )
, m_current_cfg_stream_low  ( StreamConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_LOW_STREAM ) )
, m_current_cfg_stream_mid  ( StreamConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_MID_STREAM ) )
, m_current_cfg_stream_high ( StreamConfig(*this, eRT_CurrentCfg, DICE_EAP_CURRCFG_HIGH_STREAM) )
{
}

EAP::~EAP()
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
EAP::init() {
    if(!supportsEAP(m_device)) {
        debugWarning("no EAP mixer (device does not support EAP)\n");
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
        m_router->update();

        // add the router to the EAP control container
        if(!addElement(m_router)) {
            debugWarning("Failed to add router to control tree\n");
        }
    }

    return true;
}


void
EAP::update()
{
    // update EAP from the last init
    //   update router sources and destinations
    if (m_router) {
        m_router->update();
    }
}

void
EAP::setupSources() {
    // define router sources (possibly depending on the samplerate)
    switch(m_device.getCurrentConfig()) {
        case Device::eDC_Low: setupSources_low(); return;
        case Device::eDC_Mid: setupSources_mid(); return;
        case Device::eDC_High: setupSources_high(); return;
        default:
            debugError("Unsupported configuration mode\n");
            return;
    }
}

void
EAP::setupSources_low() {
    // add the routing sources for a DICE chip
    switch(m_general_chip) {
        case DICE_EAP_CAP_GENERAL_CHIP_DICEII:
            // router/EAP currently not supported
            break;
        case DICE_EAP_CAP_GENERAL_CHIP_DICEJR:
            // second audio port (unique to the junior)
            addSource("InS1", 0, 8, eRS_InS1);
        case DICE_EAP_CAP_GENERAL_CHIP_DICEMINI:
            /// these are common to the mini and junior
            // the AES receiver
            addSource("AES", 0, 8, eRS_AES);
            // the ADAT receiver
            addSource("ADAT", 0, 8, eRS_ADAT);
            // the Mixer outputs
            addSource("MixerOut", 0, 16, eRS_Mixer);
            // the first audio port
            addSource("InS0", 0, 8, eRS_InS0);
            // the ARM audio port
            addSource("ARM", 0, 8, eRS_ARM);
            // the 1394 stream receivers
            addSource("1394_0", 0, 16, eRS_ARX0);
            addSource("1394_1", 0, 16, eRS_ARX1);
            // mute
            addSource("Mute", 0, 1, eRS_Muted);
            break;
        default:
            // this is an unsupported chip
            break;
    }
}

void
EAP::setupSources_mid() {
    setupSources_low();
}

void
EAP::setupSources_high() {
    setupSources_low();
}

unsigned int
EAP::getSMuteId() {
    return m_router->getSourceIndex("Mute:00");
}

void
EAP::setupDestinations() {
    switch(m_device.getCurrentConfig()) {
        case Device::eDC_Low: setupDestinations_low(); return;
        case Device::eDC_Mid: setupDestinations_mid(); return;
        case Device::eDC_High: setupDestinations_high(); return;
        default:
            debugError("Unsupported configuration mode\n");
            return;
    }
}

void
EAP::setupDestinations_low() {
    // add the routing destinations for a DICE chip
    switch(m_general_chip) {
        case DICE_EAP_CAP_GENERAL_CHIP_DICEII:
            // router/EAP currently not supported
            break;
        case DICE_EAP_CAP_GENERAL_CHIP_DICEJR:
            // second audio port (unique to the junior)
            addDestination("InS1", 0, 8, eRD_InS1);
        case DICE_EAP_CAP_GENERAL_CHIP_DICEMINI:
            /// these are common to the mini and junior
            // the AES receiver
            addDestination("AES", 0, 8, eRD_AES);
            // the ADAT receiver
            addDestination("ADAT", 0, 8, eRD_ADAT);
            // the Mixer outputs
            addDestination("MixerIn", 0, 16, eRD_Mixer0);
            addDestination("MixerIn", 0, 2, eRD_Mixer1, 16);
            // the first audio port
            addDestination("InS0", 0, 8, eRD_InS0);
            // the ARM audio port
            addDestination("ARM", 0, 8, eRD_ARM);
            // the 1394 stream receivers
            addDestination("1394_0", 0, 16, eRD_ATX0);
            addDestination("1394_1", 0, 16, eRD_ATX1);
            // mute
            addDestination("Mute", 0, 1, eRD_Muted);
            break;
        default:
            // this is an unsupported chip
            break;
    }
}
void
EAP::setupDestinations_mid() {
    setupDestinations_low();
}
void
EAP::setupDestinations_high() {
    setupDestinations_low();
}

void
EAP::addSource(const std::string name, unsigned int base, unsigned int count,
               enum eRouteSource srcid, unsigned int offset)
{
    m_router->addSource(name, srcid, base, count, offset);
}
void
EAP::addDestination(const std::string name, unsigned int base, unsigned int count,
                    enum eRouteDestination destid, unsigned int offset)
{
    m_router->addDestination(name, destid, base, count, offset);
}

bool
EAP::updateConfigurationCache()
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

// Get capture and playback names
//   If the device has a router, capture and playback are destinations and sources, respectively,
//     as defined above
//   If no router is found, transmitters and receivers names are returned

stringlist
EAP::getCptrNameString(unsigned int i) {
    std::vector<unsigned int> destid;
    unsigned int destid_0;
    stringlist cptr_names;
    std::string dest_name, src_name;
    
    if (m_router) {
        switch (i) {
          case 0: destid_0 = (eRD_ATX0<<4); break;
          case 1: destid_0 = (eRD_ATX1<<4); break;
          // Only 2 transmitter possible (?)
          default: return cptr_names;
        }
        // At most 16 destinations per eRD
        for (int j=0; j<16; j++) {
          destid.push_back(destid_0+j);
        }
        for (unsigned it=0; it<destid.size(); it++) {
          // If destination identifier is not part of the router, destination name will be empty
          dest_name = m_router->getDestinationName(destid.at(it));
          if (dest_name.size() > 0) {
            // get a source possibly routed to the destination (only one source per destination)
            src_name = m_router->getSourceForDestination(dest_name);
            if (src_name.size() > 0) {
              dest_name.append(" ("); dest_name.append(src_name); dest_name.append(")");
            }
            cptr_names.push_back(dest_name);
          }
        }

    } else {
        StreamConfig *scfg = getActiveStreamConfig();
        if(scfg) {
          cptr_names = scfg->getTxNamesString(i);
        }
    }
    return cptr_names;
}

stringlist
EAP::getPbckNameString(unsigned int i) {
    std::vector<unsigned int> srcid;
    unsigned int srcid_0;
    stringlist pbck_names, dest_names;
    std::string src_name;
    
    if (m_router) {
        switch (i) {
           case 0: srcid_0 = (eRS_ARX0<<4); break;
           case 1: srcid_0 = (eRS_ARX1<<4); break;
            // Only 2 receiver possible (?)
          default: return pbck_names;
        }
        // At most 16 destinations per eRD
        for (int j=0; j<16; j++) {
          srcid.push_back(srcid_0+j);
        }
        for (unsigned it=0; it<srcid.size(); it++) {
          // If source identifier is not part of the router, source name will be empty
          src_name = m_router->getSourceName(srcid.at(it));
          if (src_name.size() > 0) {
            // Search for destinations routed to this source
            //   Multiple destinations for a single source are possible
            dest_names = m_router->getDestinationsForSource(src_name);
            if (dest_names.size() > 0) {
              src_name.append(" (");
              stringlist::iterator it_d = dest_names.begin();
              stringlist::iterator it_d_end_m1 = dest_names.end(); --it_d_end_m1;
              while (it_d != it_d_end_m1) {
                src_name.append((*it_d).c_str()); src_name.append("; ");
                it_d++;
              }
              src_name.append((*it_d).c_str()); src_name.append(")");
            }
            pbck_names.push_back(src_name);
          }
        }
    } else {
        StreamConfig *scfg = getActiveStreamConfig();
        if(scfg) {
          pbck_names = scfg->getRxNamesString(i);
        }
    }
    return pbck_names;
}

/**
 * Returns the router configuration for the current rate mode
 */
EAP::RouterConfig *
EAP::getActiveRouterConfig()
{
    switch(m_device.getCurrentConfig()) {
        case Device::eDC_Low: return &m_current_cfg_routing_low;
        case Device::eDC_Mid: return &m_current_cfg_routing_mid;
        case Device::eDC_High: return &m_current_cfg_routing_high;
        default:
            debugError("Unsupported configuration mode\n");
            return NULL;
    }
}

/**
 * Returns the stream configuration for the current rate mode
 */
EAP::StreamConfig *
EAP::getActiveStreamConfig()
{
    switch(m_device.getCurrentConfig()) {
        case Device::eDC_Low: return &m_current_cfg_stream_low;
        case Device::eDC_Mid: return &m_current_cfg_stream_mid;
        case Device::eDC_High: return &m_current_cfg_stream_high;
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
EAP::updateRouterConfig(RouterConfig& rcfg, bool low, bool mid, bool high) {
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
EAP::updateCurrentRouterConfig(RouterConfig& rcfg) {
    switch(m_device.getCurrentConfig()) {
        case Device::eDC_Low: return updateRouterConfig(rcfg, true, false, false);
        case Device::eDC_Mid: return updateRouterConfig(rcfg, false, true, false);
        case Device::eDC_High: return updateRouterConfig(rcfg, false, false, true);
        default:
            debugError("Unsupported configuration mode\n");
            return false;
    }
}

/**
 * Add (create) a route from source to destination
 */
bool
EAP::addRoute(enum eRouteSource srcid, unsigned int base_src, enum eRouteDestination dstid,
              unsigned int base_dst)
{
    RouterConfig *rcfg = getActiveRouterConfig();
    return rcfg->createRoute((srcid<<4) + base_src, (dstid<<4) + base_dst);
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
EAP::updateStreamConfig(StreamConfig& scfg, bool low, bool mid, bool high) {
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
EAP::updateStreamConfig(RouterConfig& rcfg, StreamConfig& scfg, bool low, bool mid, bool high) {
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
EAP::loadFlashConfig() {
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
EAP::storeFlashConfig() {
    //debugWarning("Untested code\n") // Works. -Arnold;
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_ST_FLASH_CFG;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

// helpers
void
EAP::show()
{
    printMessage("== DICE EAP ==\n");
    printMessage("Parameter Space info:\n");
    printMessage(" Capability        : offset=%04X size=%06d\n", m_capability_offset, m_capability_size);
    printMessage(" Command           : offset=%04X size=%06d\n", m_cmd_offset, m_cmd_size);
    printMessage(" Mixer             : offset=%04X size=%06d\n", m_mixer_offset, m_mixer_size);
    printMessage(" Peak              : offset=%04X size=%06d\n", m_peak_offset, m_peak_size);
    printMessage(" New Routing Cfg   : offset=%04X size=%06d\n", m_new_routing_offset, m_new_routing_size);
    printMessage(" New Stream Cfg    : offset=%04X size=%06d\n", m_new_stream_cfg_offset, m_new_stream_cfg_size);
    printMessage(" Current Cfg       : offset=%04X size=%06d\n", m_curr_cfg_offset, m_curr_cfg_size);
    printMessage(" Standalone Cfg    : offset=%04X size=%06d\n", m_standalone_offset, m_standalone_size);
    printMessage(" Application Space : offset=%04X size=%06d\n", m_app_offset, m_app_size);

    printMessage("Capabilities:\n");
    printMessage(" Router: %sexposed, %swritable, %sstored, %d routes\n",
                                     (m_router_exposed?"":"not "),
                                     (m_router_readonly?"not ":""),
                                     (m_router_flashstored?"":"not "),
                                     m_router_nb_entries);
    printMessage(" Mixer : %sexposed, %swritable, %sstored\n",
                                     (m_mixer_exposed?"":"not "),
                                     (m_mixer_readonly?"not ":""),
                                     (m_mixer_flashstored?"":"not "));
    printMessage("         tx id: (%d==eRD_Mixer0) ? %s, rx id: (%d==eRS_Mixer) ? %s\n", 
                                     m_mixer_tx_id, (m_mixer_tx_id == eRD_Mixer0)?"true":"false",
                                     m_mixer_rx_id, (m_mixer_rx_id == eRS_Mixer) ?"true":"false");
    printMessage("         nb tx channels: %d, nb rx channels: %d\n", m_mixer_nb_tx, m_mixer_nb_rx);
    printMessage(" General: dynamic stream config %ssupported\n",
                                     (m_general_support_dynstream?"":"not "));
    printMessage("          flash load and store %ssupported\n",
                                     (m_general_support_flash?"":"not "));
    printMessage("          peak metering %s\n",
                                     (m_general_peak_enabled?"enabled":"disabled"));
    printMessage("          stream config %sstored\n",
                                     (m_general_stream_cfg_stored?"":"not "));
    printMessage("          max TX streams: %d, max RX streams: %d\n",
                                     m_general_max_tx, m_general_max_rx);

    if(m_general_chip == DICE_EAP_CAP_GENERAL_CHIP_DICEII) {
        printMessage("          Chip: DICE-II\n");
    } else if(m_general_chip == DICE_EAP_CAP_GENERAL_CHIP_DICEMINI) {
        printMessage("          Chip: DICE Mini (TCD2210)\n");
    } else if(m_general_chip == DICE_EAP_CAP_GENERAL_CHIP_DICEJR) {
        printMessage("          Chip: DICE Junior (TCD2220)\n");
    }

    printMessage("--- Mixer configuration ---\n");
    if(m_mixer) {
        m_mixer->show();
    }
    printMessage("--- Router/Peak space ---\n");
    if(m_router) {
        m_router->show();
    }

    printMessage("--- Active Router ---\n");
    RouterConfig *rcfg = getActiveRouterConfig();
    if(rcfg) {
        rcfg->show();
    }
    printMessage("--- Active Stream ---\n");
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
void
EAP::showApplication()
{
    printMessage("--- Application space ---\n");
    fb_quadlet_t* tmp = (fb_quadlet_t *)calloc(128, sizeof(fb_quadlet_t));
    unsigned int appsize = m_app_size; /// m_app_size is rather big. Start with the first four block of 128 quadlets...
    unsigned int offset = 0;
    while ( appsize > 0 ) {
        if ( ! readRegBlock( eRT_Application, offset, tmp, ((appsize<128)?appsize:128)*sizeof(fb_quadlet_t) ) )
            appsize = 0;
        else {
            hexDumpQuadlets(tmp, 128);
            offset += 128*sizeof(fb_quadlet_t);
            appsize -= 128*sizeof(fb_quadlet_t);
        }
    }
}

void
EAP::showFullRouter()
{
    printMessage("--- Full router content ---\n");

    printMessage(" %d entries to read\n", m_router_nb_entries);

    unsigned int offset;
    switch(m_device.getCurrentConfig()) {
        case Device::eDC_Low: offset = DICE_EAP_CURRCFG_LOW_ROUTER; break;
        case Device::eDC_Mid: offset = DICE_EAP_CURRCFG_MID_ROUTER; break;
        case Device::eDC_High: offset = DICE_EAP_CURRCFG_HIGH_ROUTER; break;
        default: offset = 0; break;
    }

    // Current config
    printMessage("  Current Configuration:\n");
    // First bloc is the effective number of routes
    uint32_t nb_routes;
    if(!readRegBlock(eRT_CurrentCfg, offset, &nb_routes, 4)) {
        printMessage("Failed to read number of entries\n");
        return;
    }
    printMessage("   %d routes\n", nb_routes);

    // read the route info
    uint32_t tmp_entries[m_router_nb_entries];
    if(!readRegBlock(eRT_CurrentCfg, offset+4, tmp_entries, m_router_nb_entries*4)) {
        printMessage("Failed to read router config block information\n");
        return;
    }

    // decode and print
    for(unsigned int i=0; i < m_router_nb_entries; i++) {
        printMessage("    %d: 0x%02x <- 0x%02x;\n", i, tmp_entries[i]&0xff, (tmp_entries[i]>>8)&0xff);
    }

    // New config
    printMessage("  New Configuration:\n");
    // First bloc is the effective number of routes
    if(!readRegBlock(eRT_NewRouting, 0, &nb_routes, 4)) {
        printMessage("Failed to read number of entries\n");
        return;
    }
    printMessage("   %d routes\n", nb_routes);

    // read the route info
    if(!readRegBlock(eRT_NewRouting, 4, tmp_entries, m_router_nb_entries*4)) {
        printMessage("Failed to read router config block information\n");
        return;
    }

    // decode and print
    for(unsigned int i=0; i < m_router_nb_entries; i++) {
        printMessage("    %d: 0x%02x <- 0x%02x;\n", i, tmp_entries[i]&0xff, (tmp_entries[i]>>8)&0xff);
    }

    return;
}

void
EAP::showFullPeakSpace()
{
    printMessage("--- Full Peak space content ---\n");

    // read the peak/route info
    uint32_t tmp_entries[m_router_nb_entries];
    if(!readRegBlock(eRT_Peak, 0, tmp_entries, m_router_nb_entries*4)) {
        debugError("Failed to read peak block information\n");
        return;
    }
    // decode and print
    for (unsigned int i=0; i<m_router_nb_entries; ++i) {
        printMessage("  %d: 0x%02x: %d;\n", i, tmp_entries[i]&0xff, (tmp_entries[i]&0xfff0000)>>16);
    }
    return;
}

// EAP load/store operations

enum EAP::eWaitReturn
EAP::operationBusy() {
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

enum EAP::eWaitReturn
EAP::waitForOperationEnd(int max_wait_time_ms) {
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
EAP::commandHelper(fb_quadlet_t cmd) {
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
EAP::loadRouterConfig(bool low, bool mid, bool high) {
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_ROUTER;
    if(low) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_LOW;
    if(mid) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_MID;
    if(high) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

bool
EAP::loadStreamConfig(bool low, bool mid, bool high) {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_STRM_CFG;
    if(low) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_LOW;
    if(mid) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_MID;
    if(high) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

bool
EAP::loadRouterAndStreamConfig(bool low, bool mid, bool high) {
    debugWarning("Untested code\n");
    fb_quadlet_t cmd = DICE_EAP_CMD_OPCODE_LD_RTR_STRM_CFG;
    if(low) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_LOW;
    if(mid) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_MID;
    if(high) cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH;
    cmd |= DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE;
    return commandHelper(cmd);
}

/*
  I/O operations
  */
bool
EAP::readReg(enum eRegBase base, unsigned offset, fb_quadlet_t *result) {
    fb_nodeaddr_t addr = offsetGen(base, offset, 4);
    return m_device.readReg(addr, result);
}

bool
EAP::writeReg(enum eRegBase base, unsigned offset, fb_quadlet_t data) {
    fb_nodeaddr_t addr = offsetGen(base, offset, 4);
    return m_device.writeReg(addr, data);
}

bool
EAP::readRegBlock(enum eRegBase base, unsigned offset, fb_quadlet_t *data, size_t length) {
    fb_nodeaddr_t addr = offsetGen(base, offset, length);
    return m_device.readRegBlock(addr, data, length);
}

bool
EAP::writeRegBlock(enum eRegBase base, unsigned offset, fb_quadlet_t *data, size_t length) {
    fb_nodeaddr_t addr = offsetGen(base, offset, length);
    return m_device.writeRegBlock(addr, data, length);
}

fb_nodeaddr_t
EAP::offsetGen(enum eRegBase base, unsigned offset, size_t length) {
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
        debugError("requested length too large: %zd > %"PRIu64"\n", length, maxlen);
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
EAP::supportsEAP(Device &d)
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

// -------------------------------- Mixer ----------------------------------
//   Dice matrix-mixer is a one-dimensional array with inputs index varying
//    first
//
//   "ffado convention" is that inputs are rows and outputs are columns
//
EAP::Mixer::Mixer(EAP &p)
: Control::MatrixMixer(&p.m_device, "MatrixMixer")
, m_eap(p)
, m_coeff(NULL)
, m_debugModule(p.m_debugModule)
{
}

EAP::Mixer::~Mixer()
{
    if (m_coeff) {
        free(m_coeff);
        m_coeff = NULL;
    }
}

bool
EAP::Mixer::init()
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
EAP::Mixer::loadCoefficients()
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
EAP::Mixer::storeCoefficients()
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
EAP::Mixer::updateNameCache()
{
    debugWarning("What is this function about?\n");
#if 0
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

        debugOutput(DEBUG_LEVEL_VERBOSE, "Mixer input channel %2d source: %s (%d)\n", i,
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
#endif
}

void
EAP::Mixer::show()
{
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int nb_outputs = m_eap.m_mixer_nb_rx;

    updateNameCache();

    const size_t bufflen = 4096;
    char tmp[bufflen];
    int cnt;

    //
    // Caution the user that, displaying as further, because inputs index varies first
    //  inputs will appears as columns at the opposite of the "ffado convention"
    printMessage("   -- inputs index -->>\n");
    cnt = 0;
    for(int j=0; j < nb_inputs; j++) {
        cnt += snprintf(tmp+cnt, bufflen-cnt, "   %02d   ", j);
    }
    printMessage("%s\n", tmp);

    cnt = 0;
    for(int j=0; j < nb_inputs; j++) {
        cnt += snprintf(tmp+cnt, bufflen-cnt, "%s ", getRowName(j).data());
    }
    printMessage("%s\n", tmp);

    // display coefficients
    for(int i=0; i < nb_outputs; i++) {
        cnt = 0;
        for(int j=0; j < nb_inputs; j++) {
            cnt += snprintf(tmp+cnt, bufflen-cnt, "%07d ", *(m_coeff + nb_inputs * i + j));
        }
 
        // Display destinations name
        cnt += snprintf(tmp+cnt, bufflen-cnt, "=[%02d]=> %s", i, getColName(i).data());
        printMessage("%s\n", tmp);
    }

}

int
EAP::Mixer::canWrite( const int row, const int col)
{
    if(m_eap.m_mixer_readonly) {
        return false;
    }
    return (row >= 0 && row < m_eap.m_mixer_nb_tx && col >= 0 && col < m_eap.m_mixer_nb_rx);
}

double
EAP::Mixer::setValue( const int row, const int col, const double val)
{
    if(m_eap.m_mixer_readonly) {
        debugWarning("Mixer is read-only\n");
        return false;
    }
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int addr = ((nb_inputs * col) + row) * 4;
    quadlet_t tmp = (quadlet_t) val;
    if(!m_eap.writeRegBlock(eRT_Mixer, 4+addr, &tmp, 4)) {
        debugError("Failed to write coefficient\n");
        return 0;
    }
    return (double)(tmp);
}

double
EAP::Mixer::getValue( const int row, const int col)
{
    int nb_inputs = m_eap.m_mixer_nb_tx;
    int addr = ((nb_inputs * col) + row) * 4;
    quadlet_t tmp;
    if(!m_eap.readRegBlock(eRT_Mixer, 4+addr, &tmp, 4)) {
        debugError("Failed to read coefficient\n");
        return 0;
    }
    return (double)(tmp);
}

int
EAP::Mixer::getRowCount()
{
    return m_eap.m_mixer_nb_tx;
}

int
EAP::Mixer::getColCount()
{
    return m_eap.m_mixer_nb_rx;
}

// full map updates are unsupported
bool
EAP::Mixer::getCoefficientMap(int &) {
    return false;
}

bool
EAP::Mixer::storeCoefficientMap(int &) {
    if(m_eap.m_mixer_readonly) {
        debugWarning("Mixer is read-only\n");
        return false;
    }
    return false;
}

// Names
std::string
EAP::Mixer::getRowName(const int row) {
    std::string mixer_src;
    if (row < 0 || row > m_eap.m_mixer_nb_tx) return "Invalid";
    unsigned int dstid = (eRD_Mixer0<<4) + row; // Mixer has consecutive ID's
    debugOutput(DEBUG_LEVEL_VERBOSE, "EAP::Mixer::getRowName( %d ): ID's %d\n", row, dstid);
    if (m_eap.m_router){ 
      std::string mixer_dst = m_eap.m_router->getDestinationName(dstid);
      mixer_src = m_eap.m_router->getSourceForDestination(mixer_dst);
      debugOutput(DEBUG_LEVEL_VERBOSE, "EAP::Mixer::found %s as source for %s\n", mixer_src.data(),
                  mixer_dst.data());
    }
    else {
      char tmp[32];
      snprintf(tmp, 32, "MixIn:%d", row); 
      mixer_src = tmp;
    }

    return mixer_src;
}

std::string
EAP::Mixer::getColName(const int col) {
    std::string mixer_dst;
    stringlist dest_names;
    
    // invalid col index
    if (col < 0 || col > m_eap.m_mixer_nb_rx) {
      mixer_dst.append("Invalid");
      return mixer_dst;
    }
    
    unsigned int srcid = (eRS_Mixer<<4) + col; // Mixer has consecutive ID's
    debugOutput(DEBUG_LEVEL_VERBOSE, "EAP::Mixer::getColName( %d ): ID's %d\n", col, srcid);
    if (m_eap.m_router){ 
      std::string mixer_src = m_eap.m_router->getSourceName(srcid);
      dest_names = m_eap.m_router->getDestinationsForSource(mixer_src);
      if (dest_names.size() > 0) {
        stringlist::iterator it_d = dest_names.begin();
        stringlist::iterator it_d_end_m1 = dest_names.end(); --it_d_end_m1;
        while (it_d != it_d_end_m1) {
          mixer_dst.append((*it_d).c_str()); mixer_dst.append(";\n");
          it_d++;
        }
        mixer_dst.append((*it_d).c_str());
      }
    } else {
      char tmp[32];
      snprintf(tmp, 32, "MixOut:%d", col); 
      mixer_dst.append(tmp);
    }

    return mixer_dst;
}

//
// ----------- Router -------------
//

EAP::Router::Router(EAP &p)
: Control::CrossbarRouter(&p.m_device, "Router")
, m_eap(p)
, m_peak( *(new PeakSpace(p)) )
, m_debugModule(p.m_debugModule)
{
}

EAP::Router::~Router()
{
    delete &m_peak;
}

void
EAP::Router::update()
{
    // Clear and
    //   define new sources and destinations for the router
    m_sources.clear();
    m_eap.setupSources();
    m_destinations.clear();
    m_eap.setupDestinations();
    return;
}

void
EAP::Router::addSource(const std::string& basename, enum eRouteSource srcid,
                       unsigned int base, unsigned int cnt, unsigned int offset)
{
    std::string name = basename + ":";
    char tmp[4];
    for (unsigned int i=0; i<cnt; i++) {
        snprintf(tmp, 4, "%02d", offset+i);
        m_sources[name+tmp] = (srcid<<4) + base+i;
    }
}

void
EAP::Router::addDestination(const std::string& basename, enum eRouteDestination dstid,
                            unsigned int base, unsigned int cnt, unsigned int offset)
{
    std::string name = basename + ":";
    char tmp[4];
    for (unsigned int i=0; i<cnt; i++) {
        snprintf(tmp, 4, "%02d", offset+i);
        m_destinations[name+tmp] = (dstid<<4) + base+i;
    }
}

std::string
EAP::Router::getSourceName(const int srcid)
{
    for (std::map<std::string, int>::iterator it=m_sources.begin(); it!=m_sources.end(); ++it) {
        if (it->second == srcid) {
            return it->first;
        }
    }
    return "";
}

std::string
EAP::Router::getDestinationName(const int dstid)
{
    for (std::map<std::string, int>::iterator it=m_destinations.begin(); it!=m_destinations.end(); ++it) {
        if (it->second == dstid) {
            return it->first;
        }
    }
    return "";
}

int
EAP::Router::getSourceIndex(std::string name)
{
    if (m_sources.count(name) < 1)
        return -1;
    return m_sources[name];
}

int
EAP::Router::getDestinationIndex(std::string name)
{
    if (m_destinations.count(name) < 1)
        return -1;
    return m_destinations[name];
}

stringlist
EAP::Router::getSourceNames()
{
    stringlist n;

    for (std::map<std::string, int>::iterator it=m_sources.begin(); it!=m_sources.end(); ++it)
        n.push_back(it->first);
    return n;
}

stringlist
EAP::Router::getDestinationNames()
{
    stringlist n;
    for (std::map<std::string, int>::iterator it=m_destinations.begin(); it!=m_destinations.end(); ++it)
        n.push_back(it->first);
    return n;
}

stringlist
EAP::Router::getDestinationsForSource(const std::string& srcname) {
    RouterConfig* rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return stringlist();
    }
    stringlist ret;
    std::vector<unsigned char> dests = rcfg->getDestinationsForSource(m_sources[srcname]);
    std::string name;
    for (unsigned int i=0; i<dests.size(); ++i) {
        if ((name = getDestinationName(dests[i])) != "") {
          ret.push_back(name);
        }
    }
    return ret;
}
std::string
EAP::Router::getSourceForDestination(const std::string& dstname) {
    RouterConfig* rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return "";
    }
    int source = rcfg->getSourceForDestination(m_destinations[dstname]);
    return getSourceName(source);
}


bool
EAP::Router::canConnect(const int source, const int dest)
{
    debugWarning("TODO: Implement canConnect(0x%02x, 0x%02x)\n", source, dest);

    // we can connect anything
    // FIXME: can we?
    return true;
}

bool
EAP::Router::setConnectionState(const int source, const int dest, const bool enable)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,"Router::setConnectionState(0x%02x -> 0x%02x ? %i)\n", source, dest, enable);
    // get the routing configuration
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return false;
    }

    bool ret = false;
    if (enable) {
        ret = rcfg->setupRoute(source, dest);
    } else {
        ret = rcfg->muteRoute(dest);
        // FixMe:
        //   Not all devices are intended to work correctly with a variable number of router entries
        //     so muting the destination is preferable
        //   Now, this might be useful for some other ones, but is it the right place for this ?
        //   ret = rcfg->removeRoute(source, dest);
    }
    m_eap.updateCurrentRouterConfig(*rcfg);
    return ret;
}

bool
EAP::Router::getConnectionState(const int source, const int dest)
{
    // get the routing configuration
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not request active router configuration\n");
        return false;
    }
    if (rcfg->getSourceForDestination(dest) == source) {
        return true;
    }
    return false;
}

bool
EAP::Router::canConnect(const std::string& src, const std::string& dst)
{
    int srcidx = getSourceIndex(src);
    int dstidx = getDestinationIndex(dst);
    return canConnect(srcidx, dstidx);
}

bool
EAP::Router::setConnectionState(const std::string& src, const std::string& dst, const bool enable)
{
    int srcidx = getSourceIndex(src);
    int dstidx = getDestinationIndex(dst);
    return setConnectionState(srcidx, dstidx, enable);
}

bool
EAP::Router::getConnectionState(const std::string& src, const std::string& dst)
{
    int srcidx = getSourceIndex(src);
    int dstidx = getDestinationIndex(dst);
    return getConnectionState(srcidx, dstidx);
}


bool
EAP::Router::clearAllConnections()
{
    // build a new empty routing configuration
    RouterConfig newcfg = EAP::RouterConfig(m_eap);

    // upload the new router config
    if(!m_eap.updateCurrentRouterConfig(newcfg)) {
        debugError("Could not update router config\n");
        return false;
    }
    return true;
}

bool
EAP::Router::hasPeakMetering()
{
    return m_eap.m_router_exposed;
}

double
EAP::Router::getPeakValue(const std::string& dest)
{
    m_peak.read();
    unsigned char dst = m_destinations[dest];
    return m_peak.getPeak(dst);
}

std::map<std::string, double>
EAP::Router::getPeakValues()
{
    m_peak.read();
    std::map<std::string, double> ret;
    std::map<unsigned char, int> peaks = m_peak.getPeaks();
    for (std::map<unsigned char, int>::iterator it=peaks.begin(); it!=peaks.end(); ++it) {
        ret[getDestinationName(it->first)] = it->second;
    }
    return ret;
}

void
EAP::Router::show()
{
    // print the peak space as it also contains the routing configuration
    printMessage("Router sources:\n");
    printMessage(" %llu sources:\n", (unsigned long long)m_sources.size());
    for ( std::map<std::string, int>::iterator it=m_sources.begin(); it!=m_sources.end(); ++it ) {
        printMessage(" 0x%02x : %s\n", (*it).second, (*it).first.c_str());
    }
    printMessage("Router destinations:\n");
    printMessage(" %llu destinations:\n", (unsigned long long)m_destinations.size());
    for ( std::map<std::string, int>::iterator it=m_destinations.begin(); it!=m_destinations.end(); ++it ) {
        printMessage(" 0x%02x : %s\n", (*it).second, (*it).first.c_str());
    }
    printMessage("Router connections:\n");
    stringlist sources = getSourceNames();
    stringlist destinations = getDestinationNames();
    for (stringlist::iterator it1=sources.begin(); it1!=sources.end(); ++it1) {
        for (stringlist::iterator it2=destinations.begin(); it2!=destinations.end(); ++it2) {
            if (getConnectionState(*it1, *it2)) {
                printMessage(" %s -> %s\n", it1->c_str(), it2->c_str());
            }
        }
    }
    printMessage("Active router config:\n");
    m_eap.getActiveRouterConfig()->show();
    printMessage("Active peak config:\n");
    m_peak.read();
    m_peak.show();
}

// ----------- routing config -------------
EAP::RouterConfig::RouterConfig(EAP &p)
: m_eap(p)
, m_base(eRT_None), m_offset(0)
, m_debugModule(p.m_debugModule)
{}

EAP::RouterConfig::RouterConfig(EAP &p, enum eRegBase b, unsigned int o)
: m_eap(p)
, m_base(b), m_offset(o)
, m_debugModule(p.m_debugModule)
{}

EAP::RouterConfig::~RouterConfig()
{}

bool
EAP::RouterConfig::read(enum eRegBase base, unsigned offset)
{
    // first clear the current route vector
    clearRoutes();

    uint32_t nb_routes;
    if(!m_eap.readRegBlock(base, offset, &nb_routes, 4)) {
        debugError("Failed to read number of entries\n");
        return false;
    }
    if(nb_routes == 0) {
        debugWarning("No routes found. Base 0x%x, offset 0x%x\n", base, offset);
    }

    // read the route info
    uint32_t tmp_entries[nb_routes];
    if(!m_eap.readRegBlock(base, offset+4, tmp_entries, nb_routes*4)) {
        debugError("Failed to read router config block information\n");
        return false;
    }

    // decode into the routing map
    for(unsigned int i=0; i < nb_routes; i++) {
        m_routes2.push_back(std::make_pair(tmp_entries[i]&0xff, (tmp_entries[i]>>8)&0xff));
    }
    return true;
}

bool
EAP::RouterConfig::write(enum eRegBase base, unsigned offset)
{
    uint32_t nb_routes = m_routes2.size();
    if(nb_routes == 0) {
        debugWarning("Writing 0 routes? This will deactivate routing and make the device very silent...\n");
    }
    if (nb_routes > 128) {
        debugError("More then 128 are not possible, only the first 128 routes will get saved!\n");
        nb_routes = 128;
    }
    uint32_t tmp_entries[nb_routes];

    // encode from the routing vector
    int i=0;
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        tmp_entries[i] = ((it->second<<8) + it->first)&0xffff;
        ++i;
    }

    unsigned int nb_routes_max = m_eap.getMaxNbRouterEntries();
    uint32_t zeros[nb_routes_max+1];

    for (unsigned int i=0; i<nb_routes_max+1; ++i) zeros[i] = 0;
    if(!m_eap.writeRegBlock(base, offset, zeros, (nb_routes_max+1)*4)) {
        debugError("Failed to write zeros to router config block\n");
        return false;
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

// Clear the route vector;
bool
EAP::RouterConfig::clearRoutes() {
    m_routes2.clear();
    return true;
}

// Create a destination with a prescribed source
bool
EAP::RouterConfig::createRoute(unsigned char src, unsigned char dest) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"RouterConfig::createRoute( 0x%02x, 0x%02x )\n", src, dest);
    m_routes2.push_back(std::make_pair(dest, src));
    return true;
}

bool
EAP::RouterConfig::setupRoute(unsigned char src, unsigned char dest) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"RouterConfig::setupRoute( 0x%02x, 0x%02x )\n", src, dest);
    // modify exisiting routing
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        if (it->first == dest) {
          it->second = src;
          return true;
        }
    }
    // destination does not yet exist; create it
    return createRoute(src, dest);
}


bool
EAP::RouterConfig::removeRoute(unsigned char src, unsigned char dest) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"RouterConfig::removeRoute( 0x%02x, 0x%02x )\n", src, dest);
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        if (it->first == dest) {
          if (it->second != src) {
            return false;
          }
          m_routes2.erase(it);
          return true;
        }
    }
    return false;
}

bool
EAP::RouterConfig::muteRoute(unsigned char dest) {
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        if (it->first == dest) {
          it->second = m_eap.getSMuteId();
          return true;
        }
    }
    return false;
}

bool
EAP::RouterConfig::removeRoute(unsigned char dest) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"RouterConfig::removeRoute( 0x%02x )\n", dest);
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        if (it->first == dest) {
          m_routes2.erase(it);
          return true;
        }
    }
    return false;
}

unsigned char
EAP::RouterConfig::getSourceForDestination(unsigned char dest) {
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        if (it->first == dest) {
          return it->second;
        }
    }
    return -1;
}

std::vector<unsigned char>
EAP::RouterConfig::getDestinationsForSource(unsigned char source) {
    std::vector<unsigned char> ret;
    for (RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it) {
        if (it->second == source) {
            ret.push_back(it->first);
        }
    }
    return ret;
}

void
EAP::RouterConfig::show()
{
    printMessage("%llu routes\n", (unsigned long long)m_routes2.size());
    for ( RouteVectorV2::iterator it=m_routes2.begin(); it!=m_routes2.end(); ++it ) {
        printMessage("0x%02x -> 0x%02x\n", it->second, it->first);
    }
}

//
// ----------- peak space -------------
//

bool
EAP::PeakSpace::read(enum eRegBase base, unsigned offset)
{
    uint32_t nb_routes;
    // we have to figure out the number of entries through the currently
    // active router config
    RouterConfig *rcfg = m_eap.getActiveRouterConfig();
    if(rcfg == NULL) {
        debugError("Could not get active router config\n");
        return false;
    }
    nb_routes = rcfg->getNbRoutes();

    // read the peak/route info
    uint32_t tmp_entries[nb_routes];
    if(!m_eap.readRegBlock(base, offset, tmp_entries, nb_routes*4)) {
        debugError("Failed to read peak block information\n");
        return false;
    }
    // parse the peaks into the map
    for (unsigned int i=0; i<nb_routes; ++i) {
        unsigned char dest = tmp_entries[i]&0xff;
        int peak = (tmp_entries[i]&0xfff0000)>>16;
        if (m_peaks.count(dest) == 0 || m_peaks[dest] < peak) {
            m_peaks[dest] = peak;
        }
    }
    return true;
}


void
EAP::PeakSpace::show()
{
    printMessage("  %zi peaks\n", m_peaks.size());
    for (std::map<unsigned char, int>::iterator it=m_peaks.begin(); it!=m_peaks.end(); ++it) {
        printMessage("0x%02x : %i\n", it->first, it->second);
    }
}

int
EAP::PeakSpace::getPeak(unsigned char dst) {
    int ret = m_peaks[dst];
    m_peaks.erase(dst);
    return ret;
}

std::map<unsigned char, int>
EAP::PeakSpace::getPeaks() {
    // Create a new empty map
    std::map<unsigned char, int> ret;
    // Swap the peak map with the new and empty one
    ret.swap(m_peaks);
    // Return the now filled map of the peaks :-)
    return ret;
}

// ----------- stream config block -------------
EAP::StreamConfig::StreamConfig(EAP &p, enum eRegBase b, unsigned int o)
: m_eap(p)
, m_base(b), m_offset(o)
, m_nb_tx(0), m_nb_rx(0)
, m_tx_configs(NULL), m_rx_configs(NULL)
, m_debugModule(p.m_debugModule)
{

}

EAP::StreamConfig::~StreamConfig()
{
    if(m_tx_configs) delete[]m_tx_configs;
    if(m_rx_configs) delete[]m_rx_configs;
}

bool
EAP::StreamConfig::read(enum eRegBase base, unsigned offset)
{
    if(!m_eap.readRegBlock(base, offset, &m_nb_tx, 4)) {
        debugError("Failed to read number of tx entries\n");
        return false;
    }
    if(!m_eap.readRegBlock(base, offset+4, &m_nb_rx, 4)) {
        debugError("Failed to read number of rx entries\n");
        return false;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " Entries: TX: %u, RX: %u\n", m_nb_tx, m_nb_rx);

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
EAP::StreamConfig::write(enum eRegBase base, unsigned offset)
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

stringlist
EAP::StreamConfig::getNamesForBlock(struct ConfigBlock &b)
{
    stringlist names;
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

stringlist
EAP::StreamConfig::getTxNamesString(unsigned int i)
{
    return getNamesForBlock(m_tx_configs[i]);
}

stringlist
EAP::StreamConfig::getRxNamesString(unsigned int i)
{
    return getNamesForBlock(m_rx_configs[i]);
}

void
EAP::StreamConfig::showConfigBlock(struct ConfigBlock &b)
{
    printMessage(" Channel count : %u audio, %u midi\n", b.nb_audio, b.nb_midi);
    printMessage(" AC3 Map       : 0x%08X\n", b.ac3_map);
    stringlist channel_names  = getNamesForBlock(b);
    printMessage("  Channel names :\n");
    for ( stringlist::iterator it = channel_names.begin();
        it != channel_names.end();
        ++it )
    {
        printMessage("     %s\n", (*it).c_str());
    }
}

void
EAP::StreamConfig::show()
{
    for(unsigned int i=0; i<m_nb_tx; i++) {
        printMessage("TX Config block %d\n", i);
        showConfigBlock(m_tx_configs[i]);
    }
    for(unsigned int i=0; i<m_nb_rx; i++) {
        printMessage("RX Config block %d\n", i);
        showConfigBlock(m_rx_configs[i]);
    }
}

} // namespace Dice


// vim: et
