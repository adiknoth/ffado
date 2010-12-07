/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#include "libstreaming/amdtp/AmdtpReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/Time.h"

#include "bounce_slave_avdevice.h"

#include <libraw1394/raw1394.h>
#include <libavc1394/rom1394.h>

namespace Bounce {

SlaveDevice::SlaveDevice( DeviceManager& d, std::auto_ptr< ConfigRom >( configRom ) )
    : Device( d, configRom )
{
    addOption(Util::OptionContainer::Option("isoTimeoutSecs",(int64_t)120));
}

SlaveDevice::~SlaveDevice() {

}

bool
SlaveDevice::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    // we are always capable of constructing a slave device
    return true;
}

FFADODevice *
SlaveDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new SlaveDevice(d, configRom );
}

bool
SlaveDevice::discover()
{
    return true;
}

bool SlaveDevice::initMemSpace() {
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
    m_original_config_rom = save_config_rom( get1394Service().getHandle() );

    if ( init_config_rom( get1394Service().getHandle() ) < 0 ) {
        debugError("Could not initalize local config rom\n");
        return false;
    }

    // refresh our config rom cache
    if ( !getConfigRom().initialize() ) {
        // \todo If a PHY on the bus is in power safe mode then
        // the config rom is missing. So this might be just
        // such this case and we can safely skip it. But it might
        // be there is a real software problem on our side.
        // This should be handled more carefuly.
        debugError( "Could not reread config rom from device (node id %d).\n",
                     getNodeId() );
        return false;
    }
    return true;
}

bool SlaveDevice::restoreMemSpace() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Restoring memory space...\n");
    restore_config_rom( get1394Service().getHandle(), m_original_config_rom);
    return true;
}

bool
SlaveDevice::lock() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Locking node %d\n", getNodeId());

    // get a notifier to handle device notifications
    nodeaddr_t notify_address;
    notify_address = get1394Service().findFreeARMBlock(
                        BOUNCE_REGISTER_BASE,
                        BOUNCE_REGISTER_LENGTH,
                        BOUNCE_REGISTER_LENGTH);

    if (notify_address == 0xFFFFFFFFFFFFFFFFLLU) {
        debugError("Could not find free ARM block for notification\n");
        return false;
    }

    m_Notifier=new SlaveDevice::BounceSlaveNotifier(*this, notify_address);

    if(!m_Notifier) {
        debugError("Could not allocate notifier\n");
        return false;
    }

    if (!get1394Service().registerARMHandler(m_Notifier)) {
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
SlaveDevice::unlock() {
    // (re)initialize the memory space
    if (!restoreMemSpace()) {
        debugError("Could not restore memory space\n");
        return false;
    }
    get1394Service().unregisterARMHandler(m_Notifier);
    delete m_Notifier;
    m_Notifier=NULL;

    return true;
}

bool
SlaveDevice::prepare() {
    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing SlaveDevice...\n" );

    // create & add streamprocessors
    Streaming::StreamProcessor *p;

    p = new Streaming::AmdtpReceiveStreamProcessor(*this,
                       BOUNCE_NB_AUDIO_CHANNELS+(BOUNCE_NB_MIDI_CHANNELS?1:0));

    if(!p->init()) {
        debugFatal("Could not initialize receive processor!\n");
        delete p;
        return false;
    }

    if (!addPortsToProcessor(p,
            Streaming::Port::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        delete p;
        return false;
    }

    m_receiveProcessors.push_back(p);

    // do the transmit processor
    p = new Streaming::AmdtpTransmitStreamProcessor(*this, 
                       BOUNCE_NB_AUDIO_CHANNELS+(BOUNCE_NB_MIDI_CHANNELS?1:0));

    if(!p->init()) {
        debugFatal("Could not initialize transmit processor!\n");
        delete p;
        return false;
    }

    if (!addPortsToProcessor(p,
        Streaming::Port::E_Playback)) {
        debugFatal("Could not add plug to processor!\n");
        delete p;
        return false;
    }
    m_transmitProcessors.push_back(p);

    return true;
}

// this has to wait until the ISO channel numbers are written
bool
SlaveDevice::startStreamByIndex(int i) {

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
        Streaming::StreamProcessor *p = m_transmitProcessors.at(n);

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
SlaveDevice::stopStreamByIndex(int i) {
    // nothing special to do I guess...
    return false;
}

// helpers
bool
SlaveDevice::waitForRegisterNotEqualTo(nodeaddr_t offset, fb_quadlet_t v) {
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
        SleepRelativeUsec(100000);
    }

    if(!wait_cycles) { // timout has occurred
        return false;
    }

    return true;
}

// configrom helpers
// FIXME: should be changed into a better framework


struct SlaveDevice::configrom_backup
SlaveDevice::save_config_rom(raw1394handle_t handle)
{
    int retval;
    struct configrom_backup tmp;
    /* get the current rom image */
    retval=raw1394_get_config_rom(handle, tmp.rom, 0x100, &tmp.rom_size, &tmp.rom_version);
//     tmp.rom_size=rom1394_get_size(tmp.rom);
//     printf("save_config_rom get_config_rom returned %d, romsize %d, rom_version %d:\n",retval,tmp.rom_size,tmp.rom_version);

    return tmp;
}

int
SlaveDevice::restore_config_rom(raw1394handle_t handle, struct SlaveDevice::configrom_backup old)
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
//         printf(" %08x", CondSwapFromBus32(current_rom[i]));
//     }
//     printf("\n");

    return retval;
}

int
SlaveDevice::init_config_rom(raw1394handle_t handle)
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
//         printf(" %08x", CondSwapFromBus32(rom[i]));
//     }
//     printf("\n");

    /* get the local directory */
    rom1394_get_directory( handle, raw1394_get_local_id(handle) & 0x3f, &dir);

    /* change the vendor description for kicks */
    i = strlen(dir.textual_leafs[0]);
    strncpy(dir.textual_leafs[0], FFADO_BOUNCE_SERVER_VENDORNAME "                                          ", i);

    dir.vendor_id=FFADO_BOUNCE_SERVER_VENDORID;
    dir.model_id=FFADO_BOUNCE_SERVER_MODELID;

    /* update the rom */
    retval = rom1394_set_directory(rom, &dir);
//     printf("rom1394_set_directory returned %d, romsize %d:",retval,rom_size);
//     for (i = 0; i < rom_size; i++)
//     {
//         if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
//         printf(" %08x", CondSwapFromBus32(rom[i]));
//     }
//     printf("\n");

    /* free the allocated mem for the textual leaves */
    rom1394_free_directory( &dir);

    /* add an AV/C unit directory */
    dir.unit_spec_id    = FFADO_BOUNCE_SERVER_SPECID;
    dir.unit_sw_version = 0x00010001;
    leaf = FFADO_BOUNCE_SERVER_MODELNAME;
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
//         printf(" %08x", CondSwapFromBus32(rom[i]));
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
//         printf(" %08x", CondSwapFromBus32(rom[i]));
//     }
//     printf("\n");

//     printf("You need to reload your ieee1394 modules to reset the rom.\n");

    return 0;
}


// the notifier

SlaveDevice::BounceSlaveNotifier::BounceSlaveNotifier(SlaveDevice &d, nodeaddr_t start)
 : ARMHandler(d.get1394Service(), start, BOUNCE_REGISTER_LENGTH,
              RAW1394_ARM_READ | RAW1394_ARM_WRITE, // allowed operations
              0, //RAW1394_ARM_READ | RAW1394_ARM_WRITE, // operations to be notified of
              0)                                    // operations that are replied to by us (instead of kernel)
 , m_bounceslavedevice(d)
{

}

SlaveDevice::BounceSlaveNotifier::~BounceSlaveNotifier()
{

}

} // end of namespace Bounce
