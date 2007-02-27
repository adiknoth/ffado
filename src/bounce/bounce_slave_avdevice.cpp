/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "bounce_slave_avdevice.h"
#include "libfreebob/freebob_bounce.h"

#include <libraw1394/raw1394.h>
#include <libavc1394/rom1394.h>

namespace Bounce {

static VendorModelEntry supportedDeviceList[] =
{
  //{vendor_id, model_id, unit_specifier_id, vendor_name, model_name},
    {0x0B0001, 0x0B0001, 0x0B0001, "FreeBoB", "Bounce Slave"},
};

IMPL_DEBUG_MODULE( BounceSlaveDevice, BounceSlaveDevice, DEBUG_LEVEL_VERBOSE );

BounceSlaveDevice::BounceSlaveDevice( std::auto_ptr< ConfigRom >( configRom ),
                            Ieee1394Service& ieee1394service,
                            int verboseLevel )
    : BounceDevice( configRom,
                    ieee1394service,
                    ieee1394service.getLocalNodeId(),
//                     verboseLevel )
                    DEBUG_LEVEL_VERBOSE )
{
    addOption(Util::OptionContainer::Option("isoTimeoutSecs",(int64_t)120));
}

BounceSlaveDevice::~BounceSlaveDevice() {

}

bool
BounceSlaveDevice::probe( ConfigRom& configRom )
{
    // we are always capable of constructing a slave device
    return true;
}

bool
BounceSlaveDevice::discover()
{
    m_model = &(supportedDeviceList[0]);
    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
        return true;
    }
    return false;
}

bool BounceSlaveDevice::initMemSpace() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Initializing memory space...\n");
    fb_quadlet_t result=0xFFFFFFFFLU;
    
    // initialize the ISO channel registers
    // this will write to our own registers
    if (!writeReg(BOUNCE_REGISTER_TX_ISOCHANNEL, result)) {
        debugError("Could not initalize ISO channel register for TX\n");
        return false;
    }
    if (!writeReg(BOUNCE_REGISTER_RX_ISOCHANNEL, result)) {
        debugError("Could not initalize ISO channel register for TX\n");
        return false;
    }
    
    // set everything such that we can be discovered
    m_original_config_rom=save_config_rom( m_p1394Service->getHandle() );
    
    if ( init_config_rom( m_p1394Service->getHandle() ) < 0 ) {
        debugError("Could not initalize local config rom\n");
        return false;
    }
    
    // refresh our config rom cache
    if ( !m_configRom->initialize() ) {
        // \todo If a PHY on the bus is in power safe mode then
        // the config rom is missing. So this might be just
        // such this case and we can safely skip it. But it might
        // be there is a real software problem on our side.
        // This should be handled more carefuly.
        debugError( "Could not reread config rom from device (node id %d).\n",
                     m_nodeId );
        return false;
    }
    return true;
}

bool BounceSlaveDevice::restoreMemSpace() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Restoring memory space...\n");
    restore_config_rom( m_p1394Service->getHandle(), m_original_config_rom);
    return true;
}

bool
BounceSlaveDevice::lock() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Locking %s %s at node %d\n", 
        m_model->vendor_name, m_model->model_name, m_nodeId);
    
    // get a notifier to handle device notifications
    nodeaddr_t notify_address;
    notify_address = m_p1394Service->findFreeARMBlock(
                        BOUNCE_REGISTER_BASE,
                        BOUNCE_REGISTER_LENGTH,
                        BOUNCE_REGISTER_LENGTH);
    
    if (notify_address == 0xFFFFFFFFFFFFFFFFLLU) {
        debugError("Could not find free ARM block for notification\n");
        return false;
    }
    
    m_Notifier=new BounceSlaveDevice::BounceSlaveNotifier(this, notify_address);
    
    if(!m_Notifier) {
        debugError("Could not allocate notifier\n");
        return false;
    }
    
    if (!m_p1394Service->registerARMHandler(m_Notifier)) {
        debugError("Could not register notifier\n");
        delete m_Notifier;
        m_Notifier=NULL;
        return false;
    }
    
    // (re)initialize the memory space
    if (!initMemSpace()) {
        debugError("Could not initialize memory space\n");
        return false;
    }
    
    return true;
}

bool
BounceSlaveDevice::unlock() {
    // (re)initialize the memory space
    if (!restoreMemSpace()) {
        debugError("Could not restore memory space\n");
        return false;
    }
    m_p1394Service->unregisterARMHandler(m_Notifier);
    delete m_Notifier;
    m_Notifier=NULL;

    return true;
}

bool
BounceSlaveDevice::prepare() {
    // snooping does not make sense for a slave device
    setOption("snoopMode", false);
    
    // prepare the base class
    // FIXME: when doing proper discovery this won't work anymore
    //        as it relies on a completely symmetric transmit/receive
    if(!BounceDevice::prepare()) {
        debugError("Base class preparation failed\n");
        return false;
    }
    
    // do any customisations here
    
    return true;
}

// this has to wait until the ISO channel numbers are written
bool
BounceSlaveDevice::startStreamByIndex(int i) {
    
    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);
        
        // the other side sends on this channel
        nodeaddr_t iso_channel_offset = BOUNCE_REGISTER_RX_ISOCHANNEL;
        iso_channel_offset += ((unsigned)n)*4;
        
        if (!waitForRegisterNotEqualTo(iso_channel_offset, 0xFFFFFFFFLU)) {
            debugError("Timeout waiting for stream %d to get an ISO channel\n",i);
            return false;
        }
        
        fb_quadlet_t result;
        // this will read from our own registers
        if (!readReg(iso_channel_offset, &result)) {
            debugError("Could not read ISO channel register for stream %d\n",i);
            return false;
        }
        
        // set ISO channel
        p->setChannel(result);

        return true;
        
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);
        
        // the other side sends on this channel
        nodeaddr_t iso_channel_offset = BOUNCE_REGISTER_TX_ISOCHANNEL;
        iso_channel_offset += ((unsigned)n)*4;
        
        if (!waitForRegisterNotEqualTo(iso_channel_offset, 0xFFFFFFFF)) {
            debugError("Timeout waiting for stream %d to get an ISO channel\n",i);
            return false;
        }
        
        fb_quadlet_t result;
        // this will read from our own registers
        if (!readReg(iso_channel_offset, &result)) {
            debugError("Could not read ISO channel register for stream %d\n",i);
            return false;
        }
        
        // set ISO channel
        p->setChannel(result);

        return true;

    }
    
    debugError("SP index %d out of range!\n",i);
    
    return false;
}

bool
BounceSlaveDevice::stopStreamByIndex(int i) {
    // nothing special to do I guess...
    return false;
}

// helpers
bool
BounceSlaveDevice::waitForRegisterNotEqualTo(nodeaddr_t offset, fb_quadlet_t v) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for StreamProcessor streams to start running...\n");
    // we have to wait until all streamprocessors indicate that they are running
    // i.e. that there is actually some data stream flowing
    int timeoutSecs=120;
    if(!getOption("isoTimeoutSecs", timeoutSecs)) {
        debugWarning("Could not retrieve isoTimeoutSecs parameter, defauling to 120secs\n");
    }
    
    int wait_cycles=timeoutSecs*10; // two seconds
    
    fb_quadlet_t reg=v;
    
    while ((v == reg) && wait_cycles) {
        wait_cycles--;
        if (!readReg(offset,&reg)) {
            debugError("Could not read register\n");
            return false;
        }
        usleep(100000);
    }

    if(!wait_cycles) { // timout has occurred
        return false;
    }
    
    return true;
}

// configrom helpers
// FIXME: should be changed into a better framework


struct BounceSlaveDevice::configrom_backup 
BounceSlaveDevice::save_config_rom(raw1394handle_t handle)
{
    int retval;
    struct configrom_backup tmp;
    /* get the current rom image */
    retval=raw1394_get_config_rom(handle, tmp.rom, 0x100, &tmp.rom_size, &tmp.rom_version);
// 	tmp.rom_size=rom1394_get_size(tmp.rom);
//     printf("save_config_rom get_config_rom returned %d, romsize %d, rom_version %d:\n",retval,tmp.rom_size,tmp.rom_version);

    return tmp;
}

int 
BounceSlaveDevice::restore_config_rom(raw1394handle_t handle, struct BounceSlaveDevice::configrom_backup old)
{
    int retval;
//     int i;
    
    quadlet_t current_rom[0x100];
    size_t current_rom_size;
    unsigned char current_rom_version;

    retval=raw1394_get_config_rom(handle, current_rom, 0x100, &current_rom_size, &current_rom_version);
//     printf("restore_config_rom get_config_rom returned %d, romsize %d, rom_version %d:\n",retval,current_rom_size,current_rom_version);

//     printf("restore_config_rom restoring to romsize %d, rom_version %d:\n",old.rom_size,old.rom_version);

    retval = raw1394_update_config_rom(handle, old.rom, old.rom_size, current_rom_version);
//     printf("restore_config_rom update_config_rom returned %d\n",retval);

    /* get the current rom image */
    retval=raw1394_get_config_rom(handle, current_rom, 0x100, &current_rom_size, &current_rom_version);
    current_rom_size = rom1394_get_size(current_rom);
//     printf("get_config_rom returned %d, romsize %d, rom_version %d:",retval,current_rom_size,current_rom_version);
//     for (i = 0; i < current_rom_size; i++)
//     {
//         if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
//         printf(" %08x", ntohl(current_rom[i]));
//     }
//     printf("\n");

    return retval;
}

int 
BounceSlaveDevice::init_config_rom(raw1394handle_t handle)
{
    int retval, i;
    quadlet_t rom[0x100];
    size_t rom_size;
    unsigned char rom_version;
    rom1394_directory dir;
    char *leaf;
    
    /* get the current rom image */
    retval=raw1394_get_config_rom(handle, rom, 0x100, &rom_size, &rom_version);
    rom_size = rom1394_get_size(rom);
//     printf("get_config_rom returned %d, romsize %d, rom_version %d:",retval,rom_size,rom_version);
//     for (i = 0; i < rom_size; i++)
//     {
//         if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
//         printf(" %08x", ntohl(rom[i]));
//     }
//     printf("\n");
    
    /* get the local directory */
    rom1394_get_directory( handle, raw1394_get_local_id(handle) & 0x3f, &dir);
    
    /* change the vendor description for kicks */
    i = strlen(dir.textual_leafs[0]);
    strncpy(dir.textual_leafs[0], FREEBOB_BOUNCE_SERVER_VENDORNAME "                                          ", i);
    
    dir.vendor_id=FREEBOB_BOUNCE_SERVER_VENDORID;
    dir.model_id=FREEBOB_BOUNCE_SERVER_MODELID;
    
    /* update the rom */
    retval = rom1394_set_directory(rom, &dir);
//     printf("rom1394_set_directory returned %d, romsize %d:",retval,rom_size);
//     for (i = 0; i < rom_size; i++)
//     {
//         if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
//         printf(" %08x", ntohl(rom[i]));
//     }
//     printf("\n");
    
    /* free the allocated mem for the textual leaves */
    rom1394_free_directory( &dir);
    
    /* add an AV/C unit directory */
    dir.unit_spec_id    = FREEBOB_BOUNCE_SERVER_SPECID;
    dir.unit_sw_version = 0x00010001;
    leaf = FREEBOB_BOUNCE_SERVER_MODELNAME;
    dir.nr_textual_leafs = 1;
    dir.textual_leafs = &leaf;
    
    /* manipulate the rom */
    retval = rom1394_add_unit( rom, &dir);
    
    /* get the computed size of the rom image */
    rom_size = rom1394_get_size(rom);
    
//     printf("rom1394_add_unit_directory returned %d, romsize %d:",retval,rom_size);
//     for (i = 0; i < rom_size; i++)
//     {
//         if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
//         printf(" %08x", ntohl(rom[i]));
//     }
//     printf("\n");
//     
    /* convert computed rom size from quadlets to bytes before update */
    rom_size *= sizeof(quadlet_t);
    retval = raw1394_update_config_rom(handle, rom, rom_size, rom_version);
//     printf("update_config_rom returned %d\n",retval);
    
    retval=raw1394_get_config_rom(handle, rom, 0x100, &rom_size, &rom_version);
//     printf("get_config_rom returned %d, romsize %d, rom_version %d:",retval,rom_size,rom_version);
//     for (i = 0; i < rom_size; i++)
//     {
//         if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
//         printf(" %08x", ntohl(rom[i]));
//     }
//     printf("\n");
    
// 	printf("You need to reload your ieee1394 modules to reset the rom.\n");
    
    return 0;
}


// the notifier

BounceSlaveDevice::BounceSlaveNotifier::BounceSlaveNotifier(BounceSlaveDevice *d, nodeaddr_t start)
 : ARMHandler(start, BOUNCE_REGISTER_LENGTH, 
              RAW1394_ARM_READ | RAW1394_ARM_WRITE, // allowed operations
              0, //RAW1394_ARM_READ | RAW1394_ARM_WRITE, // operations to be notified of
              0)                                    // operations that are replied to by us (instead of kernel)
 , m_bounceslavedevice(d)
{

}

BounceSlaveDevice::BounceSlaveNotifier::~BounceSlaveNotifier() 
{

}

} // end of namespace Bounce
