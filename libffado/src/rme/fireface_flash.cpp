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

#include <unistd.h>
#include <math.h>
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
        } else 
        if (m_rme_model == RME_MODEL_FIREFACE800) {
            status = readRegister(RME_FF_STATUS_REG1);
            if (status & 0x40000000)
                break;
        } else {
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            return -1;
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
        do {
            xfer_size = (n_quads > RME_FF_FLASH_SECTOR_SIZE_QUADS)?RME_FF_FLASH_SECTOR_SIZE_QUADS:n_quads;
            err |= readBlock(addr, buf, xfer_size);
            n_quads =- xfer_size;
            buf += xfer_size;
            addr += xfer_size*sizeof(quadlet_t);
        } while (n_quads>0 && !err);
    } else {
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
    }

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
    } else 
    if (m_rme_model == RME_MODEL_FIREFACE400) {
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
    } else {
        debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
        return -1;
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
        do {
            xfer_size = (n_quads > RME_FF_FLASH_SECTOR_SIZE_QUADS)?RME_FF_FLASH_SECTOR_SIZE_QUADS:n_quads;
            err |= writeBlock(addr, buf, xfer_size);
            if (!err) {
                err = wait_while_busy(5) != 0;
                if (err)
                    debugOutput(DEBUG_LEVEL_WARNING, "device still busy after flash write\n");
            } else
                debugOutput(DEBUG_LEVEL_WARNING, "flash writeBlock() failed\n");
            n_quads -= xfer_size;
            buf += xfer_size;
            addr += xfer_size*sizeof(quadlet_t);
        } while (n_quads>0 && !err);
        return err?-1:0;
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
    long long int addr;
    quadlet_t status_buf[4];

    // FIXME: the debug output in this function is mostly for testing at
    // present.

    i = get_revision(&rev);
    if (i != 0) {
        debugOutput(DEBUG_LEVEL_WARNING, "Error reading hardware revision: %d\n", i);
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Hardware revision: 0x%08x\n", rev);
    }

    // Read settings flash ram block
    if (m_rme_model == RME_MODEL_FIREFACE800)
        addr = RME_FF800_FLASH_SETTINGS_ADDR;
    else
    if (m_rme_model == RME_MODEL_FIREFACE400)
        addr = RME_FF400_FLASH_SETTINGS_ADDR;
    else {
        debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
        return -1;
    }
    err = read_flash(addr, 
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
        if (hw_settings.sample_rate == FF_DEV_FLASH_SRATE_DDS_INACTIVE) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Sample rate: DDS not active\n");
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  Sample rate: %d Hz (DDS active)\n", hw_settings.sample_rate);
        }
    }

    if (settings != NULL) {
        memset(settings, 0, sizeof(*settings));
        // Copy hardware details to the software settings structure as
        // appropriate.
        for (i=0; i<2; i++)
            settings->mic_phantom[i] = hw_settings.mic_phantom[i];

        if (m_rme_model == RME_MODEL_FIREFACE800) {
            for (i=2; i<4; i++)
                settings->mic_phantom[i] = hw_settings.mic_phantom[i];
        } else 
        if (m_rme_model == RME_MODEL_FIREFACE400) {
            // TODO: confirm this is true
            for (i=2; i<4; i++)
                settings->ff400_input_pad[i-2] = hw_settings.mic_phantom[i];
        } else {
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            return -1;
        }

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
        if (m_rme_model == RME_MODEL_FIREFACE800) {
            settings->filter = hw_settings.filter;
            settings->fuzz = hw_settings.fuzz;
        } else 
        if (m_rme_model == RME_MODEL_FIREFACE400) {
            // TODO: confirm this is true
            settings->ff400_instr_input[0] = hw_settings.fuzz;
            settings->ff400_instr_input[1] = hw_settings.filter;
        }
        settings->limiter = (hw_settings.p12db_an[0] == 0)?1:0;
        settings->sample_rate = hw_settings.sample_rate;
        settings->word_clock_single_speed = hw_settings.word_clock_single_speed;

        // The FF800 has front/rear selectors for the "instrument" input 
        // (aka channel 1) and the two "mic" channels (aka channels 7 and 8).
        // The FF400 does not.  The FF400 borrows the mic0 selector field 
        // in the flash configuration structure to use for the "phones"
        // level which the FF800 doesn't have.
        if (m_rme_model == RME_MODEL_FIREFACE400)
            settings->phones_level = hw_settings.mic_plug_select[0];
        else 
        if (m_rme_model == RME_MODEL_FIREFACE800) {
            // The value for the front/rear selectors coming from the flash
            // is an indexed value: 0=rear, 1=front, 2=front and rear.  By
            // adding one to this we can treat input_opt as a bitmask with
            // bit 0 being "rear" and bit 1 being "front".  This follows the
            // approach used in drivers for other operating systems and
            // simplifies certain logic expressions within the driver.
            settings->input_opt[0] = hw_settings.instrument_plug_select + 1;
            settings->input_opt[1] = hw_settings.mic_plug_select[0] + 1;
            settings->input_opt[2] = hw_settings.mic_plug_select[1] + 1;
        }
    }

    /* If debug is enabled, show what's been read from the flash */
    debugOutput(DEBUG_LEVEL_VERBOSE, "Settings read from flash:\n");
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "  Phantom: %d %d %d %d\n",
            settings->mic_phantom[0], settings->mic_phantom[1],
            settings->mic_phantom[2], settings->mic_phantom[2]);

    } else 
    if (m_rme_model == RME_MODEL_FIREFACE400) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "  Phantom: %d %d\n",
            settings->mic_phantom[0], settings->mic_phantom[1]);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  Input pad: %d %d\n",
            settings->ff400_input_pad[0], settings->ff400_input_pad[1]);
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "  spdif input mode: %d\n", settings->spdif_input_mode);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  spdif output emphasis: %d\n", settings->spdif_output_emphasis);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  spdif output pro: %d\n", settings->spdif_output_pro);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  spdif output nonaudio: %d\n", settings->spdif_output_nonaudio);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  spdif output mode: %d\n", settings->spdif_output_mode);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  clock mode: %d\n", settings->clock_mode);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  sync ref: %d\n", settings->sync_ref);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  tms: %d\n", settings->tms);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  limit bandwidth: %d\n", settings->limit_bandwidth);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  stop on dropout: %d\n", settings->stop_on_dropout);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  input level: %d\n", settings->input_level);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  output level: %d\n", settings->output_level);
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "  filter: %d\n", settings->filter);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  fuzz: %d\n", settings->fuzz);
    } else
    if (m_rme_model == RME_MODEL_FIREFACE400) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "  instr input 0: %d\n", settings->ff400_instr_input[0]);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  instr input 1: %d\n", settings->ff400_instr_input[1]);
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "  limiter: %d\n", settings->limiter);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  sample rate: %d\n", settings->sample_rate);
    debugOutput(DEBUG_LEVEL_VERBOSE, "  word clock single speed: %d\n", settings->word_clock_single_speed);
    if (m_rme_model == RME_MODEL_FIREFACE400) {
      debugOutput(DEBUG_LEVEL_VERBOSE, "  phones level: %d\n", settings->phones_level);
    } else
    if (m_rme_model == RME_MODEL_FIREFACE800) {
      debugOutput(DEBUG_LEVEL_VERBOSE, "  input opts: %d %d %d\n",
        settings->input_opt[0], settings->input_opt[1],
        settings->input_opt[2]);
    }

    i = readBlock(RME_FF_STATUS_REG0, status_buf, 4);
    debugOutput(DEBUG_LEVEL_VERBOSE, "Status read: %d: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
        status_buf[0], status_buf[1], status_buf[2], status_buf[3]);

#if 0
{
// Read mixer volume flash ram block
quadlet_t buf[0x800];
quadlet_t addr = 0;
  memset(buf, 0xdb, sizeof(buf));
  if (m_rme_model == RME_MODEL_FIREFACE400)
      addr = RME_FF400_FLASH_MIXER_VOLUME_ADDR;
  else
  if (m_rme_model == RME_MODEL_FIREFACE800)
      addr = RME_FF800_FLASH_MIXER_VOLUME_ADDR;
  if (addr != 0) {
    i = read_flash(addr, buf, 32);
    fprintf(stderr,"result=%d\n", i);
    for (i=0; i<32; i++) {
      fprintf(stderr, "%d: 0x%08x\n", i, buf[i]);
    }
  }
}
#endif

    return err!=0?-1:0;
}

signed int 
Device::write_device_flash_settings(FF_software_settings_t *settings) 
{
    // Write the given device settings to the device's configuration flash.

    FF_device_flash_settings_t hw_settings;
    signed int i, err = 0;

    if (settings == NULL) {
        debugOutput(DEBUG_LEVEL_WARNING, "NULL settings parameter\n");
        return -1;
    }

    memset(&hw_settings, 0, sizeof(hw_settings));

    // Copy software settings to the hardware structure as appropriate.
    for (i=0; i<4; i++)
        hw_settings.mic_phantom[i] = settings->mic_phantom[i];
    hw_settings.spdif_input_mode = settings->spdif_input_mode;
    hw_settings.spdif_output_emphasis = settings->spdif_output_emphasis;
    hw_settings.spdif_output_pro = settings->spdif_output_pro;
    hw_settings.spdif_output_nonaudio = settings->spdif_output_nonaudio;
    hw_settings.spdif_output_mode = settings->spdif_output_mode;
    hw_settings.clock_mode = settings->clock_mode;
    hw_settings.sync_ref = settings->sync_ref;
    hw_settings.tms = settings->tms;
    hw_settings.limit_bandwidth = settings->limit_bandwidth;
    hw_settings.stop_on_dropout = settings->stop_on_dropout;
    hw_settings.input_level = settings->input_level;
    hw_settings.output_level = settings->output_level;
    hw_settings.filter = settings->filter;
    hw_settings.fuzz = settings->fuzz;
    // The limiter can only be disabled if channel 1 uses the "front" input.
    // Note that p12db_an (as passed to the flash) seems to be a "limiter
    // disabled" flag.
    if (m_rme_model==RME_MODEL_FIREFACE800 && settings->limiter==0 && 
            settings->input_opt[0]==FF_SWPARAM_FF800_INPUT_OPT_FRONT)
        hw_settings.p12db_an[0] = 1;
    else
        hw_settings.p12db_an[0] = 0;
    hw_settings.sample_rate = settings->sample_rate;
    hw_settings.word_clock_single_speed = settings->word_clock_single_speed;

    // The FF800 has front/rear selectors for the "instrument" input 
    // (aka channel 1) and the two "mic" channels (aka channels 7 and 8).
    // The FF400 does not.  The FF400 borrows the mic0 selector field 
    // in the flash configuration structure to use for the "phones"
    // level which the FF800 doesn't have.
    if (m_rme_model == RME_MODEL_FIREFACE400)
        hw_settings.mic_plug_select[0] = settings->phones_level;
    else 
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        // The offset of 1 follows the convention used internally in drivers
        // for other operating systems.  It permits input_opt to be treated
        // as a bitmask with bit 0 being "rear" and bit 1 being "front".
        // In the flash, the corresponding value is the index of the active
        // option in the list rear, front, front and rear.  See also the
        // related section of read_device_flash_settings().
        hw_settings.instrument_plug_select = settings->input_opt[0] - 1;
        hw_settings.mic_plug_select[0] = settings->input_opt[1] - 1;
        hw_settings.mic_plug_select[1] = settings->input_opt[2] - 1;
    }

    // The configuration flash block must be erased before we can write to it
    err = erase_flash(RME_FF_FLASH_ERASE_SETTINGS) != 0;
    if (err != 0)
        debugOutput(DEBUG_LEVEL_WARNING, "Error erasing settings flash block: %d\n", i);
    else {
        long long int addr;
        if (m_rme_model == RME_MODEL_FIREFACE800)
            addr = RME_FF800_FLASH_SETTINGS_ADDR;
        else
        if (m_rme_model == RME_MODEL_FIREFACE400)
            addr = RME_FF400_FLASH_SETTINGS_ADDR;
        else {
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            return -1;
        }
        err = write_flash(addr, 
                  (quadlet_t *)&hw_settings, sizeof(hw_settings)/sizeof(uint32_t));

        if (err != 0)
            debugOutput(DEBUG_LEVEL_WARNING, "Error writing device flash settings: %d\n", i);
    }

    return err!=0?-1:0;
}

static float
fader2flashvol(signed int fader)
{
    return (1023.0/3) * log(fader*(exp(3.0)-1.0)/0x10000 + 1);
}

static float
flashvol2fader(signed int flash_vol)
{
    return 0x10000 * (exp(3.0*flash_vol/1023.0)-1) / (exp(3)-1.0);
}

static void
faders2flash(signed int fader0, signed int fader1, unsigned short int *flash_vol, unsigned short int *flash_pan)
{
    signed int v = fader0 + fader1;
    *flash_pan = 256.0 * fader1 / v;
    *flash_vol = fader2flashvol(v);
}

static void
flash2faders(signed int flash_vol, signed int flash_pan, signed int *fader0, signed int *fader1)
{
    float v = flashvol2fader(flash_vol);
    *fader0 = v * (flash_pan/256.0);
    *fader1 = v * (1 - flash_pan/256.0);
}

signed int
Device::read_device_mixer_settings(FF_software_settings_t *settings)
{
    unsigned short int vbuf[RME_FF_FLASH_MIXER_ARRAY_SIZE/2];
    unsigned short int pbuf[RME_FF_FLASH_MIXER_ARRAY_SIZE/2];
    unsigned short int obuf[RME_FF_FLASH_SECTOR_SIZE/2];
    fb_nodeaddr_t addr = 0;
    signed int i, in, out;
    signed int nch = 0;

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        addr = RME_FF400_FLASH_MIXER_VOLUME_ADDR;
        nch = 18;
    } else
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        addr = RME_FF800_FLASH_MIXER_VOLUME_ADDR;
        nch = 32;
    }
    if (addr == 0)
        return -1;

    i = read_flash(addr, (quadlet_t *)(vbuf), RME_FF_FLASH_MIXER_ARRAY_SIZE/4);
    debugOutput(DEBUG_LEVEL_VERBOSE, "read_flash(%lld) returned %d\n", addr, i);

    addr += RME_FF_FLASH_MIXER_ARRAY_SIZE;
    i = read_flash(addr, (quadlet_t *)(pbuf), RME_FF_FLASH_MIXER_ARRAY_SIZE/4);
    debugOutput(DEBUG_LEVEL_VERBOSE, "read_flash(%lld) returned %d\n", addr, i);

    addr += RME_FF_FLASH_MIXER_ARRAY_SIZE;
    i = read_flash(addr, (quadlet_t *)obuf, RME_FF_FLASH_SECTOR_SIZE_QUADS);
    debugOutput(DEBUG_LEVEL_VERBOSE, "read_flash(%lld) returned %d\n", addr, i);

    for (out=0; out<nch/2; out++) {
        for (in=0; in<nch; in++) {
            flash2faders(vbuf[in+out*nch*2], pbuf[in+out*nch*2],
              &settings->input_faders[getMixerGainIndex(in,out*2)],
              &settings->input_faders[getMixerGainIndex(in,out*2+1)]);
        }
    }
    for (out=0; out<nch/2; out++) {
        for (in=0; in<nch; in++) {
            flash2faders(vbuf[in+nch*(out*2+1)], pbuf[in+nch*(out*2+1)],
              &settings->playback_faders[getMixerGainIndex(in,out*2)],
              &settings->playback_faders[getMixerGainIndex(in,out*2+1)]);
        }
    }
    // Elements 30 and 31 of obuf[] are not output fader values: [30] 
    // indicates MIDI control is active while [31] is a submix number.
    // It's suspected that neither of these are used by the device directly,
    // and that these elements are just a convenient place for computer
    // control applications to store things.
    for (out=0; out<30; out++) {
      settings->output_faders[out] = flashvol2fader(obuf[out]);
    }

    return 0;
}

signed int
Device::write_device_mixer_settings(FF_software_settings_t *settings)
{
    quadlet_t shadow[RME_FF800_FLASH_MIXER_SHADOW_SIZE/4];
    unsigned short int vbuf[RME_FF_FLASH_MIXER_ARRAY_SIZE/2];
    unsigned short int pbuf[RME_FF_FLASH_MIXER_ARRAY_SIZE/2];
    unsigned short int obuf[RME_FF_FLASH_SECTOR_SIZE/2];
    fb_nodeaddr_t addr = 0;
    signed int i, in, out;
    signed int nch = 0;

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        addr = RME_FF400_FLASH_MIXER_VOLUME_ADDR;
        nch = 18;
    } else
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        addr = RME_FF800_FLASH_MIXER_SHADOW_ADDR;
        nch = 32;
    }
    if (addr == 0)
        return -1;

    // The mixer flash block must be erased before we can write to it
    i = erase_flash(RME_FF_FLASH_ERASE_VOLUME) != 0;
    if (i) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "erase_flash() failed\n");
        return -1;
    }

    /* Write the shadow mixer array if the device is a ff800 */
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        memset(shadow, 0, sizeof(shadow));
        for (out=0; out<nch; out++) {
            for (in=0; in<nch; in++) {
                shadow[in+out*0x40] = settings->input_faders[getMixerGainIndex(in,out)];
                shadow[in+out*0x40+0x20] = settings->playback_faders[getMixerGainIndex(in,out)];
            }
        }
        for (out=0; out<nch; out++) {
            shadow[0x1f80+out] = settings->output_faders[out];
        }
        i = write_flash(addr, shadow, RME_FF800_FLASH_MIXER_SHADOW_SIZE/4);
        debugOutput(DEBUG_LEVEL_VERBOSE, "write_flash(%lld) returned %d\n", addr, i);
        addr = RME_FF800_FLASH_MIXER_VOLUME_ADDR;
    }

    for (out=0; out<nch/2; out++) {
        for (in=0; in<nch; in++) {
            faders2flash(settings->input_faders[getMixerGainIndex(in,out*2)],
              settings->input_faders[getMixerGainIndex(in,out*2+1)],
              &vbuf[in+out*nch*2], &pbuf[in+out*nch*2]);
        }
    }
    for (out=0; out<nch/2; out++) {
        for (in=0; in<nch; in++) {
            faders2flash(settings->playback_faders[getMixerGainIndex(in,out*2)],
              settings->playback_faders[getMixerGainIndex(in,out*2+1)],
              &vbuf[in+nch*(out*2+1)], &pbuf[in+nch*(out*2+1)]);
        }
    }

    // Elements 30 and 31 of obuf[] are not output fader values.  See
    // comments in read_device_mixer_settings().
    memset(obuf, 0, sizeof(obuf));
    for (out=0; out<30; out++) {
      obuf[out] = fader2flashvol(settings->output_faders[out]);
    }

    i = write_flash(addr, (quadlet_t *)(vbuf), RME_FF_FLASH_MIXER_ARRAY_SIZE/4);
    debugOutput(DEBUG_LEVEL_VERBOSE, "write_flash(%lld) returned %d\n", addr, i);

    addr += RME_FF_FLASH_MIXER_ARRAY_SIZE;
    i = write_flash(addr, (quadlet_t *)(pbuf), RME_FF_FLASH_MIXER_ARRAY_SIZE/4);
    debugOutput(DEBUG_LEVEL_VERBOSE, "write_flash(%lld) returned %d\n", addr, i);

    addr += RME_FF_FLASH_MIXER_ARRAY_SIZE;
    i = write_flash(addr, (quadlet_t *)obuf, RME_FF_FLASH_SECTOR_SIZE_QUADS);
    debugOutput(DEBUG_LEVEL_VERBOSE, "write_flash(%lld) returned %d\n", addr, i);

    return 0;
}

}
