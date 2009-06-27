/*
 * Copyright (C) 2009 by Jonathan Woithe
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

/* This file implements the flash memory methods of the Device object */

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"

#include "debugmodule/debugmodule.h"

#define MAX_FLASH_BUSY_RETRIES    25

namespace Rme {

signed int 
Device::wait_while_busy(unsigned int init_delay_ms) 
{
    signed int i;
    quadlet_t status;

    // Wait for the device to become available for a new command.  A delay
    // of init_delay_ms is executed prior to each test of the device status.
    for (i=0; i<MAX_FLASH_BUSY_RETRIES; i++) {
        usleep(init_delay_ms*1000);
        if (m_rme_model == RME_MODEL_FIREFACE400) {
            status = readRegister(RME_FF400_FLASH_STAT_REG);
            if (status == 0)
                break;
        } else {
            status = readRegister(RME_FF_STATUS_REG1);
            if (status & 0x40000000)
                break;
        }
    }

    if (i == MAX_FLASH_BUSY_RETRIES)
        return -1;
    return 0;
}

signed int
Device::get_revision(unsigned int *revision)
{
    signed int err = 0;

    if (m_rme_model == RME_MODEL_FIREFACE800) {
        *revision = readRegister(RME_FF800_REVISION_REG);
        return 0;
    }

    err = writeRegister(RME_FF400_FLASH_CMD_REG, RME_FF400_FLASH_CMD_GET_REVISION);
    err |= wait_while_busy(2);
    if (!err)
      *revision = readRegister(RME_FF400_FLASH_READ_BUFFER);

    return err?-1:0;
}

signed int 
Device::read_flash(fb_nodeaddr_t addr, quadlet_t *buf, unsigned int n_quads)
{
    // Read "n_quads" quadlets from the Fireface Flash starting at address
    // addr.  The result is written to "buf" which is assumed big enough to
    // hold the result.  Return 0 on success, -1 on error.  The caller must ensure
    // that the flash source address makes sense for the device in use.

    unsigned int xfer_size;
    unsigned int err = 0;
    quadlet_t block_desc[2];
    quadlet_t ff400_addr = (addr & 0xffffffff);

    if (m_rme_model == RME_MODEL_FIREFACE800) {
        return readBlock(addr, buf, n_quads);
    }
    // FF400 case follows
    do {
        xfer_size = (n_quads > 32)?32:n_quads;
        block_desc[0] = ff400_addr;
        block_desc[1] = xfer_size * sizeof(quadlet_t);
        // Program the read address and size
        err |= writeBlock(RME_FF400_FLASH_BLOCK_ADDR_REG, block_desc, 2);
        // Execute the read and wait for its completion
        err |= writeRegister(RME_FF400_FLASH_CMD_REG, RME_FF400_FLASH_CMD_READ);
        if (!err)
            wait_while_busy(2);
        // Read from bounce buffer into final destination
        err |= readBlock(RME_FF400_FLASH_READ_BUFFER, buf, xfer_size);

        n_quads -= xfer_size;
        ff400_addr += xfer_size*sizeof(quadlet_t);
        buf += xfer_size;
    } while (n_quads>0 && !err);

    return err?-1:0;
}

signed int
Device::erase_flash(unsigned int flags)
{
    // Erase the requested flash block.  "flags" should be one of the 
    // RME_FF_FLASH_ERASE_* flags.  Return 0 on success, -1 on error.

    fb_nodeaddr_t addr;
    quadlet_t data;
    unsigned int err = 0;

    if (m_rme_model == RME_MODEL_FIREFACE800) {
        switch (flags) {
            case RME_FF_FLASH_ERASE_VOLUME:
                addr = RME_FF800_FLASH_ERASE_VOLUME_REG; break;
            case RME_FF_FLASH_ERASE_SETTINGS:
                addr = RME_FF800_FLASH_ERASE_SETTINGS_REG; break;
            case RME_FF_FLASH_ERASE_CONFIG:
                addr = RME_FF800_FLASH_ERASE_CONFIG_REG; break;
            default:
                debugOutput(DEBUG_LEVEL_WARNING, "unknown flag %d\n", flags);
                return -1;
        }
        data = 0;
    } else {
        addr = RME_FF400_FLASH_CMD_REG;
        switch (flags) {
            case RME_FF_FLASH_ERASE_VOLUME:
                data = RME_FF400_FLASH_CMD_ERASE_VOLUME; break;
            case RME_FF_FLASH_ERASE_SETTINGS:
                data = RME_FF400_FLASH_CMD_ERASE_SETTINGS; break;
            case RME_FF_FLASH_ERASE_CONFIG:
                data = RME_FF400_FLASH_CMD_ERASE_CONFIG; break;
            default:
                debugOutput(DEBUG_LEVEL_WARNING, "unknown flag %d\n", flags);
                return -1;
        }
    }

    err |= writeRegister(addr, data);
    if (!err) {
        wait_while_busy(500);
        // After the device is ready, wait a further 20 milliseconds.  The purpose
        // of this is unclear.  Drivers from other OSes do it, so we should too.
        usleep(20000);
    }

    return err?-1:0;
}

signed int 
Device::write_flash(fb_nodeaddr_t addr, quadlet_t *buf, unsigned int n_quads)
{
    // Write "n_quads" quadlets to the Fireface Flash starting at address
    // addr.  Return 0 on success, -1 on error.  The caller must ensure the
    // supplied address is appropriate for the device in use.

    unsigned int xfer_size;
    unsigned int err = 0;
    quadlet_t block_desc[2];
    quadlet_t ff400_addr = (addr & 0xffffffff);

    if (m_rme_model == RME_MODEL_FIREFACE800) {
        err |= readBlock(addr, buf, n_quads);
        if (!err)
            wait_while_busy(5);
    }
    // FF400 case follows
    do {
        xfer_size = (n_quads > 32)?32:n_quads;
        // Send data to flash buffer
        err |= writeBlock(RME_FF400_FLASH_WRITE_BUFFER, buf, xfer_size);
        // Program the destination address and size
        block_desc[0] = ff400_addr;
        block_desc[1] = xfer_size * sizeof(quadlet_t);
        err |= writeBlock(RME_FF400_FLASH_BLOCK_ADDR_REG, block_desc, 2);
        // Execute the write and wait for its completion
        err |= writeRegister(RME_FF400_FLASH_CMD_REG, RME_FF400_FLASH_CMD_WRITE);
        if (!err)
            wait_while_busy(2);

        n_quads -= xfer_size;
        ff400_addr += xfer_size*sizeof(quadlet_t);
        buf += xfer_size;
    } while (n_quads>0 && !err);

    return err?-1:0;
}


signed int 
Device::read_device_flash_settings(FF_software_settings_t *settings) 
{
    // Read the device's configuration flash RAM and use this to set up 
    // the given settings structure.

    FF_device_flash_settings_t hw_settings;
    signed int i, err = 0;
    unsigned int rev;

    // FIXME: this is mostly for testing at present.  Still need to interface from
    // hw_settings to the object's "settings" field.

    i = get_revision(&rev);
    if (i != 0) {
        debugOutput(DEBUG_LEVEL_WARNING, "Error reading hardware revision: %d\n", i);
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Hardware revision: 0x%08x\n", rev);
    }

    // Read settings flash ram block
    err = read_flash(m_rme_model==RME_MODEL_FIREFACE800?
      RME_FF800_FLASH_SETTINGS_ADDR:RME_FF400_FLASH_SETTINGS_ADDR, 
        (quadlet_t *)&hw_settings, sizeof(hw_settings)/sizeof(uint32_t));
    if (err != 0) {
        debugOutput(DEBUG_LEVEL_WARNING, "Error reading device flash settings: %d\n", i);
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Device flash settings:\n");
        if (hw_settings.clock_mode == FF_DEV_FLASH_INVALID) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Clock mode: not set in device flash\n");
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Clock mode: %s\n",
              hw_settings.clock_mode==FF_DEV_FLASH_CLOCK_MODE_MASTER?"Master":"Slave");
        }
        if (hw_settings.sample_rate == FF_DEV_FLASH_INVALID) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Sample rate: not set in device flash\n");
        } else
        if (hw_settings.sample_rate == 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Sample rate: DDS not active\n");
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Sample rate: %d Hz (DDS active)\n", hw_settings.sample_rate);
        }
    }

    if (settings != NULL) {
        memset(settings, 0, sizeof(settings));
        // Copy hardware details to the software settings structure as
        // appropriate.
        for (i=0; i<4; i++)
            settings->mic_phantom[i] = hw_settings.mic_phantom[i];
        settings->spdif_input_mode = hw_settings.spdif_input_mode;
        settings->spdif_output_emphasis = hw_settings.spdif_output_emphasis;
        settings->spdif_output_pro = hw_settings.spdif_output_pro;
        settings->spdif_output_nonaudio = hw_settings.spdif_output_nonaudio;
        settings->spdif_output_mode = hw_settings.spdif_output_mode;
        settings->clock_mode = hw_settings.clock_mode;
        settings->sync_ref = hw_settings.sync_ref;
        settings->tms = hw_settings.tms;
        settings->limit_bandwidth = hw_settings.limit_bandwidth;
        settings->stop_on_dropout = hw_settings.stop_on_dropout;
        settings->input_level = hw_settings.input_level;
        settings->output_level = hw_settings.output_level;
        settings->filter = hw_settings.filter;
        settings->fuzz = hw_settings.fuzz;
        settings->limiter_disable = (hw_settings.p12db_an[0] == 0)?1:0;
        settings->sample_rate = hw_settings.sample_rate;
        settings->word_clock_single_speed = hw_settings.word_clock_single_speed;

        // The FF800 has front/rear selectors for the "instrument" input 
        // (aka channel 1) and the two "mic" channels (aka channels 7 and 8).
        // The FF400 does not.  The FF400 borrows the mic0 selector field 
        // in the flash configuration structure to use for the "phones"
        // level which the FF800 doesn't have.
        // The offset of 1 here is to maintain consistency with the values
        // used in the flash by other operating systems.
        if (m_rme_model == RME_MODEL_FIREFACE400)
            settings->phones_level = hw_settings.mic_plug_select[0] + 1;
        else {
            settings->input_opt[0] = hw_settings.instrument_plug_select + 1;
            settings->input_opt[1] = hw_settings.mic_plug_select[0] + 1;
            settings->input_opt[2] = hw_settings.mic_plug_select[1] + 1;
        }
    }

{
quadlet_t buf[4];
signed int i;
  i = readBlock(RME_FF_STATUS_REG0, buf, 4);
  fprintf(stderr, "Status read: %d\n", i);
  for (i=0; i<4; i++)
    fprintf(stderr,"0x%08x ", buf[i]);
  fprintf(stderr,"\n");
}

#if 0
{
// Read mixer volume flash ram block
quadlet_t buf[0x800];
  memset(buf, 0xdb, sizeof(buf));
  i = read_flash(m_rme_model==RME_MODEL_FIREFACE800?
        RME_FF800_FLASH_MIXER_VOLUME_ADDR:RME_FF400_FLASH_MIXER_VOLUME_ADDR, buf, 32);
fprintf(stderr,"result=%d\n", i);
for (i=0; i<32; i++) {
  fprintf(stderr, "%d: 0x%08x\n", i, buf[i]);
}
}
#endif

    return err!=0?-1:0;
}

signed int 
Device::write_device_flash_settings(FF_software_settings_t *settings) 
{
    // Write the given device settings to the device's configuration flash.

    // FIXME: fairly obviously the detail needs to be filled in here.
    return 0;
}

}
