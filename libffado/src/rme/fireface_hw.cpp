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

/* This file implements miscellaneous lower-level hardware functions for the Fireface */

#include <math.h>

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"

#include "debugmodule/debugmodule.h"

namespace Rme {

unsigned int
Device::multiplier_of_freq(unsigned int freq) 
{
    if (freq > MIN_QUAD_SPEED)
      return 4;
    if (freq > MIN_DOUBLE_SPEED)
      return 2;
    return 1;
}

void
Device::config_lock(void) {
    rme_shm_lock(dev_config);
}

void
Device::config_unlock(void) {
    rme_shm_unlock(dev_config);
}

signed int
Device::init_hardware(void)
{
    signed int ret = 0;
    signed int src, dest;
    signed int n_channels;
    signed int have_mixer_settings = 0;

    switch (m_rme_model) {
        case RME_MODEL_FIREFACE400: n_channels = RME_FF400_MAX_CHANNELS; break;
        case RME_MODEL_FIREFACE800: n_channels = RME_FF800_MAX_CHANNELS; break;
        default:
          debugOutput(DEBUG_LEVEL_ERROR, "unknown model %d\n", m_rme_model);
          return -1;
    }

    // Initialises the device's settings structure to a known state and then
    // sets the hardware to reflect this state.

    config_lock();

    // If the software state is not yet valid, attempt to obtain settings
    // from the device flash.  If that fails for some reason, initialise
    // with a static setup.
    if (dev_config->settings_valid == 0) {
        dev_config->settings_valid = read_device_flash_settings(settings) == 0;
        // If valid settings were read, write them to the device so we can
        // be sure that this mirrors how the device is currently configured.
        // This is also needed so the "host" LED is extinguished on first
        // use after power up.  Also use the stored sample rate as the
        // operational rate.
        if (dev_config->settings_valid) {
            dev_config->software_freq = settings->sample_rate;
            // dds_freq can be used to run the audio clock at a slightly
            // different frequency to what the software requests (to allow
            // for drop-frame rates for example).  For the moment FFADO 
            // provides no explicit support for this, so dds_freq should
            // always be zero.
            //
            // If user access to dds_freq is implemented in future, it may
            // or may not be desireable to set dds_freq from
            // settings->sample_rate.  This will probably be determined by
            // the nature of the user interface to dds_freq.
            //
            // See also comments in getSamplingFrequency() and
            // setDDSFrequency().
            dev_config->dds_freq = 0;
            set_hardware_params(settings);
        }
    }

    // If no valid flash settings, configure with a static setup.
    if (dev_config->settings_valid == 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "flash settings unavailable or invalid; using defaults\n");
        memset(settings, 0, sizeof(*settings));
        settings->spdif_input_mode = FF_SWPARAM_SPDIF_INPUT_COAX;
        settings->spdif_output_mode = FF_SWPARAM_SPDIF_OUTPUT_COAX;
        settings->clock_mode = FF_SWPARAM_CLOCK_MODE_MASTER;
        settings->sync_ref = FF_SWPARAM_SYNCREF_WORDCLOCK;
        settings->input_level = FF_SWPARAM_ILEVEL_LOGAIN;
        settings->output_level = FF_SWPARAM_OLEVEL_HIGAIN;
        settings->phones_level = FF_SWPARAM_PHONESLEVEL_HIGAIN;
        settings->limit_bandwidth = FF_SWPARAM_BWLIMIT_SEND_ALL_CHANNELS;

        // A default sampling rate.  An explicit DDS frequency is not enabled
        // by default (FFADO doesn't currently make explicit use of this - see
        // above).
        dev_config->software_freq = 44100;
        dev_config->dds_freq = 0;
        settings->sample_rate = dev_config->software_freq;

        // TODO: set input amplifier gains to a value other than 0?

        // TODO: store and manipulate channel mute/rec flags

        // The FF800 needs the input source set via the input options.
        // The device's default has the limiter enabled so we'll follow
        // that convention.
        if (m_rme_model == RME_MODEL_FIREFACE800) {
            settings->input_opt[0] = settings->input_opt[1] = 
              settings->input_opt[2] = FF_SWPARAM_FF800_INPUT_OPT_FRONT;
            settings->limiter = 1;
        }

        // Configure the hardware to match the current software status. 
        // This is only done if the settings valid flag is 0; if it is 1 it
        // indicates that something has already set the device up to match
        // the software settings so there's no need to do it again.

        if (set_hardware_params(settings) != 0)
            ret = -1;

        if (ret==0) {
            signed freq = dev_config->software_freq;
            if (dev_config->dds_freq > 0)
                freq = dev_config->dds_freq;
            if (set_hardware_dds_freq(freq) != 0)
                ret = -1;
        }

        if (m_rme_model == RME_MODEL_FIREFACE400) {
            signed int i;
            for (i=FF400_AMPGAIN_MIC1; i<=FF400_AMPGAIN_INPUT4; i++) {
                set_hardware_ampgain(i, settings->amp_gains[i]);
            }
        }

        dev_config->settings_valid = 1;
    }

    have_mixer_settings = read_device_mixer_settings(settings) == 0;

    // Matrix mixer settings
    for (dest=0; dest<n_channels; dest++) {
        for (src=0; src<n_channels; src++) {
            if (!have_mixer_settings)
                settings->input_faders[getMixerGainIndex(src, dest)] = 0;
            set_hardware_mixergain(RME_FF_MM_INPUT, src, dest, settings->input_faders[getMixerGainIndex(src, dest)]);
        }
        for (src=0; src<n_channels; src++) {
            if (!have_mixer_settings)
                settings->playback_faders[getMixerGainIndex(src, dest)] =
                  src==dest?0x8000:0;
            set_hardware_mixergain(RME_FF_MM_PLAYBACK, src, dest, 
              settings->playback_faders[getMixerGainIndex(src, dest)]);
        }
    }
    for (src=0; src<n_channels; src++) {
        if (!have_mixer_settings)
            settings->output_faders[src] = 0x8000;
        set_hardware_mixergain(RME_FF_MM_OUTPUT, src, 0, settings->output_faders[src]);
    }

    set_hardware_output_rec(0);

    if (ret==0 && m_rme_model==RME_MODEL_FIREFACE400 && provide_midi) {
        // Precisely mirror the method used under other operating systems
        // to set the high quadlet of the MIDI ARM address, even though it
        // is a little inflexible.  We can refine this later if need be.
        unsigned int node_id = getConfigRom().getNodeId();
        unsigned int midi_hi_addr;
        midi_hi_addr = 0x01;
        if (writeRegister(RME_FF400_MIDI_HIGH_ADDR, (node_id<<16) | midi_hi_addr) != 0) {
            debugOutput(DEBUG_LEVEL_ERROR, "failed to write MIDI high address register\n");
            ret = -1;
        }
    }

    // Also configure the TCO (Time Code Option) settings for those devices
    // which have a TCO.
    if (ret==0 && dev_config->tco_settings_valid==0) {
        if (dev_config->tco_present) {
            memset(tco_settings, 0, sizeof(*tco_settings));
            if (write_tco_settings(tco_settings) != 0) {
                debugOutput(DEBUG_LEVEL_ERROR, "failed to write TCO settings\n");
            }
        }
        dev_config->tco_settings_valid = 1;
    }

    config_unlock();

    return ret;
}

signed int
Device::get_hardware_status(unsigned int *stat0, unsigned int *stat1)
{
    unsigned int buf[2];
    if (readBlock(RME_FF_STATUS_REG0, buf, 2) != 0)
        return -1;
    *stat0 = buf[0];
    *stat1 = buf[1];
    return 0;
}

signed int
Device::get_hardware_streaming_status(unsigned int *stat, unsigned int n)
{
    // Get the hardware status as it applies to the streaming system.  This
    // involves a request of 4 quadlets from the status register.  It
    // appears that the first register's definition is slightly different in
    // this situation compared to when only 2 quadlets are requested as is
    // done in get_hardware_status().
    //
    // "n" is the size of the passed-in stat array.  It must be >= 4.
    if (n < 4)
        return -1;
    if (readBlock(RME_FF_STATUS_REG0, stat, 4) != 0)
        return -1;
    return 0;
}

signed int
Device::get_hardware_state(FF_state_t *state)
{
    // Retrieve the hardware status and deduce the device state.  Return
    // -1 on error, 0 on success.  The given state structure will be 
    // cleared by this call.
    static signed int call_count = 0;
    unsigned int stat0, stat1;
    memset(state, 0, sizeof(*state));
    if (get_hardware_status(&stat0, &stat1) != 0)
        return -1;

    state->is_streaming = dev_config->is_streaming;

    state->clock_mode = (settings->clock_mode == FF_SWPARAM_CLOCK_MODE_MASTER)?FF_STATE_CLOCKMODE_MASTER:FF_STATE_CLOCKMODE_AUTOSYNC;

    switch (stat0 & SR0_AUTOSYNC_SRC_MASK) {
        case SR0_AUTOSYNC_SRC_ADAT1:
            state->autosync_source = FF_STATE_AUTOSYNC_SRC_ADAT1;
            break;
        case SR0_AUTOSYNC_SRC_ADAT2:
            state->autosync_source = FF_STATE_AUTOSYNC_SRC_ADAT2;
            break;
        case SR0_AUTOSYNC_SRC_SPDIF:
            state->autosync_source = FF_STATE_AUTOSYNC_SRC_SPDIF;
            break;
        case SR0_AUTOSYNC_SRC_WCLK:
            state->autosync_source = FF_STATE_AUTOSYNC_SRC_WCLK;
            break;
        case SR0_AUTOSYNC_SRC_TCO:
            state->autosync_source = FF_STATE_AUTOSYNC_SRC_TCO;
            break;
        default: state->autosync_source = FF_STATE_AUTOSYNC_SRC_NOLOCK;
    }

    switch (stat0 & SR0_AUTOSYNC_FREQ_MASK) {
        case SR0_AUTOSYNC_FREQ_32k:  state->autosync_freq = 32000; break;
        case SR0_AUTOSYNC_FREQ_44k1: state->autosync_freq = 44100; break;
        case SR0_AUTOSYNC_FREQ_48k:  state->autosync_freq = 48000; break;
        case SR0_AUTOSYNC_FREQ_64k:  state->autosync_freq = 64000; break;
        case SR0_AUTOSYNC_FREQ_88k2: state->autosync_freq = 88200; break;
        case SR0_AUTOSYNC_FREQ_96k:  state->autosync_freq = 96000; break;
        case SR0_AUTOSYNC_FREQ_128k: state->autosync_freq = 128000; break;
        case SR0_AUTOSYNC_FREQ_176k4:state->autosync_freq = 176400; break;
        case SR0_AUTOSYNC_FREQ_192k: state->autosync_freq = 192000; break;
    }

    switch (stat0 & SR0_SPDIF_FREQ_MASK) {
        case SR0_SPDIF_FREQ_32k:  state->spdif_freq = 32000; break;
        case SR0_SPDIF_FREQ_44k1: state->spdif_freq = 41000; break;
        case SR0_SPDIF_FREQ_48k:  state->spdif_freq = 48000; break;
        case SR0_SPDIF_FREQ_64k:  state->spdif_freq = 64000; break;
        case SR0_SPDIF_FREQ_88k2: state->spdif_freq = 88200; break;
        case SR0_SPDIF_FREQ_96k:  state->spdif_freq = 96000; break;
        case SR0_SPDIF_FREQ_128k: state->spdif_freq = 128000; break;
        case SR0_SPDIF_FREQ_176k4:state->spdif_freq = 176400; break;
        case SR0_SPDIF_FREQ_192k: state->spdif_freq = 192000; break;
    }

    switch (stat0 & SR0_ADAT1_STATUS_MASK) {
        case SR0_ADAT1_STATUS_NOLOCK:
            state->adat1_sync_status = FF_STATE_SYNC_NOLOCK; break;
        case SR0_ADAT1_STATUS_LOCK:
            state->adat1_sync_status = FF_STATE_SYNC_LOCKED; break;
        case SR0_ADAT1_STATUS_SYNC: 
            state->adat1_sync_status = FF_STATE_SYNC_SYNCED; break;
    }
    switch (stat0 & SR0_ADAT2_STATUS_MASK) {
        case SR0_ADAT2_STATUS_NOLOCK:
            state->adat2_sync_status = FF_STATE_SYNC_NOLOCK; break;
        case SR0_ADAT2_STATUS_LOCK:
            state->adat2_sync_status = FF_STATE_SYNC_LOCKED; break;
        case SR0_ADAT2_STATUS_SYNC:
            state->adat2_sync_status = FF_STATE_SYNC_SYNCED; break;
    }
    switch (stat0 & SR0_SPDIF_STATUS_MASK) {
        case SR0_SPDIF_STATUS_NOLOCK:
            state->spdif_sync_status = FF_STATE_SYNC_NOLOCK; break;
        case SR0_SPDIF_STATUS_LOCK:
            state->spdif_sync_status = FF_STATE_SYNC_LOCKED; break;
        case SR0_SPDIF_STATUS_SYNC:
            state->spdif_sync_status = FF_STATE_SYNC_SYNCED; break;
    }
    switch (stat0 & SR0_WCLK_STATUS_MASK) {
        case SR0_WCLK_STATUS_NOLOCK:
            state->wclk_sync_status = FF_STATE_SYNC_NOLOCK; break;
        case SR0_WCLK_STATUS_LOCK:
            state->wclk_sync_status = FF_STATE_SYNC_LOCKED; break;
        case SR0_WCLK_STATUS_SYNC:
            state->wclk_sync_status = FF_STATE_SYNC_SYNCED; break;
    }
    switch (stat1 & SR1_TCO_STATUS_MASK) {
       case SR1_TCO_STATUS_NOLOCK:
           state->tco_sync_status = FF_STATE_SYNC_NOLOCK; break;
       case SR1_TCO_STATUS_LOCK:
           state->tco_sync_status = FF_STATE_SYNC_LOCKED; break;
       case SR1_TCO_STATUS_SYNC:
           state->tco_sync_status = FF_STATE_SYNC_SYNCED; break;
    }

    // Report the state reported by the hardware if debug output is active.
    // Only do this for the first few calls.
    if (call_count < 2) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "State reported by hardware:\n");
        debugOutput(DEBUG_LEVEL_VERBOSE, "  is_streaming: %d\n", state->is_streaming);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  clock_mode: %s\n", state->clock_mode==FF_STATE_CLOCKMODE_MASTER?"master":"autosync/slave");
        debugOutput(DEBUG_LEVEL_VERBOSE, "  autosync source: %d\n", state->autosync_source);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  autosync freq: %d\n", state->autosync_freq);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  spdif freq: %d\n", state->spdif_freq);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  ADAT 1/2 status: %x, %x\n", state->adat1_sync_status, state->adat2_sync_status);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  SDPIF status: %x\n", state->spdif_sync_status);
        debugOutput(DEBUG_LEVEL_VERBOSE, "  Wclk/tco status: %x, %x\n", state->wclk_sync_status, state->tco_sync_status);
        call_count++;
    }
    return 0;
}

signed int 
Device::set_hardware_params(FF_software_settings_t *use_settings)
{
    // Initialises the hardware to the state defined by the supplied
    // software settings structure (which will usually be the device's
    // "settings" structure).  This has the side effect of extinguishing the
    // "Host" LED on the FF400 when done for the first time after the
    // interface has been powered up.
    //
    // If use_settings is NULL, the device's current settings structure will
    // be used to source the configuration information.

    FF_software_settings_t *sw_settings;
    quadlet_t data[3] = {0, 0, 0};
    unsigned int conf_reg;

    if (use_settings == NULL)
      sw_settings = settings;
    else
      sw_settings = use_settings;

    if (sw_settings->mic_phantom[0])
      data[0] |= CR0_PHANTOM_MIC0;
    if (sw_settings->mic_phantom[1])
      data[0] |= CR0_PHANTOM_MIC1;
    switch (m_rme_model) {
        case RME_MODEL_FIREFACE800:
            if (sw_settings->mic_phantom[2])
                data[0] |= CR0_FF800_PHANTOM_MIC9;
            if (sw_settings->mic_phantom[3])
                data[0] |= CR0_FF800_PHANTOM_MIC10;
            break;
        case RME_MODEL_FIREFACE400:
            if (sw_settings->ff400_input_pad[0])
                data[0] |= CR0_FF400_CH3_PAD;
            if (sw_settings->ff400_input_pad[1])
                data[0] |= CR0_FF400_CH4_PAD;
            break;
        default:
            break;
    }

    /* Phones level */
    if (m_rme_model == RME_MODEL_FIREFACE400) {
        switch (sw_settings->phones_level) {
            case FF_SWPARAM_PHONESLEVEL_HIGAIN:
                data[0] |= CRO_PHLEVEL_HIGAIN;
                break;
            case FF_SWPARAM_PHONESLEVEL_4dBU:
                data[0] |= CR0_PHLEVEL_4dBU;
                break;
            case FF_SWPARAM_PHONESLEVEL_m10dBV:
                data[0] |= CRO_PHLEVEL_m10dBV;
                break;
        }
    }

    /* Input level */
    switch (sw_settings->input_level) {
        case FF_SWPARAM_ILEVEL_LOGAIN: // Low gain
            data[1] |= CR1_ILEVEL_CPLD_LOGAIN;    // CPLD
            data[0] |= CR0_ILEVEL_FPGA_LOGAIN;    // LED control (used on FF800 only)
            break;
        case FF_SWPARAM_ILEVEL_4dBU:   // +4 dBu
            data[1] |= CR1_ILEVEL_CPLD_4dBU;
            data[0] |= CR0_ILEVEL_FPGA_4dBU;
            break;
        case FF_SWPARAM_ILEVEL_m10dBV: // -10 dBV
            data[1] |= CR1_ILEVEL_CPLD_m10dBV;
            data[0] |= CR0_ILEVEL_FPGA_m10dBV;
            break;
    }

    /* Output level */
    switch (sw_settings->output_level) {
        case FF_SWPARAM_OLEVEL_HIGAIN: // High gain
            data[1] |= CR1_OLEVEL_CPLD_HIGAIN;   // CPLD
            data[0] |= CR0_OLEVEL_FPGA_HIGAIN;   // LED control (used on FF800 only)
            break;
        case FF_SWPARAM_OLEVEL_4dBU:   // +4 dBu
            data[1] |= CR1_OLEVEL_CPLD_4dBU;
            data[0] |= CR0_OLEVEL_FPGA_4dBU;
            break;
        case FF_SWPARAM_OLEVEL_m10dBV: // -10 dBV
            data[1] |= CR1_OLEVEL_CPLD_m10dBV;
            data[0] |= CR0_OLEVEL_FPGA_m10dBV;
            break;
    }

    /* Set input options.  The meaning of the options differs between
     * devices, so we use the generic identifiers here.
     */
    data[1] |= (sw_settings->input_opt[1] & FF_SWPARAM_INPUT_OPT_A) ? CR1_INPUT_OPT1_A : 0;
    data[1] |= (sw_settings->input_opt[1] & FF_SWPARAM_INPUT_OPT_B) ? CR1_INPUT_OPT1_B : 0;
    data[1] |= (sw_settings->input_opt[2] & FF_SWPARAM_INPUT_OPT_A) ? CR1_INPUT_OPT2_A : 0;
    data[1] |= (sw_settings->input_opt[2] & FF_SWPARAM_INPUT_OPT_B) ? CR1_INPUT_OPT2_B : 0;

    // Drive the speaker emulation / filter LED via FPGA in FF800.  In FF400
    // the same bit controls the channel 4 "instrument" option.
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        data[0] |= (sw_settings->filter) ? CR0_FF800_FILTER_FPGA : 0;
    } else
    if (m_rme_model == RME_MODEL_FIREFACE400) {
        data[0] |= (sw_settings->ff400_instr_input[1]) ? CR0_FF400_CH4_INSTR : 0;
    }

    // Set the "rear" option for input 0 if selected
    data[1] |= (sw_settings->input_opt[0] & FF_SWPARAM_FF800_INPUT_OPT_REAR) ? CR1_FF800_INPUT1_REAR : 0;

    // The input 0 "front" option is activated using one of two bits
    // depending on whether the filter (aka "speaker emulation") setting is
    // active.
    if (sw_settings->input_opt[0] & FF_SWPARAM_FF800_INPUT_OPT_FRONT) {
        data[1] |= (sw_settings->filter) ? CR1_FF800_INPUT1_FRONT_WITH_FILTER : CR1_FF800_INPUT1_FRONT;
    }

    data[2] |= (sw_settings->spdif_output_emphasis==FF_SWPARAM_SPDIF_OUTPUT_EMPHASIS_ON) ? CR2_SPDIF_OUT_EMP : 0;
    data[2] |= (sw_settings->spdif_output_pro==FF_SWPARAM_SPDIF_OUTPUT_PRO_ON) ? CR2_SPDIF_OUT_PRO : 0;
    data[2] |= (sw_settings->spdif_output_nonaudio==FF_SWPARAM_SPDIF_OUTPUT_NONAUDIO_ON) ? CR2_SPDIF_OUT_NONAUDIO : 0;
    data[2] |= (sw_settings->spdif_output_mode==FF_SWPARAM_SPDIF_OUTPUT_OPTICAL) ? CR2_SPDIF_OUT_ADAT2 : 0;
    data[2] |= (sw_settings->clock_mode==FF_SWPARAM_CLOCK_MODE_AUTOSYNC) ? CR2_CLOCKMODE_AUTOSYNC : CR2_CLOCKMODE_MASTER;
    data[2] |= (sw_settings->spdif_input_mode==FF_SWPARAM_SPDIF_INPUT_COAX) ? CR2_SPDIF_IN_COAX : CR2_SPDIF_IN_ADAT2;
    data[2] |= (sw_settings->word_clock_single_speed=FF_SWPARAM_WORD_CLOCK_1x) ? CR2_WORD_CLOCK_1x : 0;

    /* TMS / TCO toggle bits in CR2 are not set by other drivers */

    /* Drive / fuzz in FF800.  In FF400, the CR0 bit used by "Drive" controls
     * the channel 3 "instrument" option.
     */
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        if (sw_settings->fuzz)
            data[0] |= CR0_FF800_DRIVE_FPGA; // FPGA LED control
        else
            data[1] |= CR1_INSTR_DRIVE;      // CPLD
    } else 
    if (m_rme_model == RME_MODEL_FIREFACE400) {
        data[0] |= (sw_settings->ff400_instr_input[0]) ? CR0_FF400_CH3_INSTR : 0;
    }

    /* Drop-and-stop is hardwired on in other drivers */
    data[2] |= CR2_DROP_AND_STOP;

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        data[2] |= CR2_FF400_BIT;
    }

    switch (sw_settings->sync_ref) {
        case FF_SWPARAM_SYNCREF_WORDCLOCK:
            data[2] |= CR2_SYNC_WORDCLOCK;
            break;
        case FF_SWPARAM_SYNCREF_ADAT1:
            data[2] |= CR2_SYNC_ADAT1;
            break;
        case FF_SWPARAM_SYNCREF_ADAT2:
            data[2] |= CR2_SYNC_ADAT2;
            break;
        case FF_SWPARAM_SYNCREF_SPDIF:
            data[2] |= CR2_SYNC_SPDIF;
            break;
        case FF_SWPARAM_SYNCREC_TCO:
            data[2] |= CR2_SYNC_TCO;
            break;
    }

    // This is hardwired in other drivers
    data[2] |= (CR2_FREQ0 + CR2_FREQ1 + CR2_DSPEED + CR2_QSSPEED);

    // The FF800 limiter can only be disabled if the front panel instrument
    // input is in use, so it only makes sense that it is disabled when that
    // input is in use.
    data[2] |= (sw_settings->limiter==0 && 
                (sw_settings->input_opt[0]==FF_SWPARAM_FF800_INPUT_OPT_FRONT)) ? 
                CR2_DISABLE_LIMITER : 0;

//This is just for testing - it's a known consistent configuration
//data[0] = 0x00020810;      // Phantom off
//data[0] = 0x00020811;      // Phantom on
//data[1] = 0x0000031e;
//data[2] = 0xc400101f;
    debugOutput(DEBUG_LEVEL_VERBOSE, "set hardware registers: 0x%08x 0x%08x 0x%08x\n",
      data[0], data[1], data[2]);

    switch (m_rme_model) {
        case RME_MODEL_FIREFACE800: conf_reg = RME_FF800_CONF_REG; break;
        case RME_MODEL_FIREFACE400: conf_reg = RME_FF400_CONF_REG; break;
        default:
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            return -1;
    }
    if (writeBlock(conf_reg, data, 3) != 0) {
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write device settings\n");
        return -1;
    }

    return 0;
}

signed int 
Device::read_tco(quadlet_t *tco_data, signed int size)
{
    // Read the TCO registers and return the respective values in *tco_data. 
    // Return value is 0 on success, or -1 if there is no TCO present. 
    // "size" is the size (in quadlets) of the array pointed to by tco_data. 
    // To obtain all TCO data "size" should be at least 4.  If the caller
    // doesn't care about the data returned by the TCO, tco_data can be
    // NULL.
    quadlet_t buf[4];
    signed int i;

    // Only the Fireface 800 can have the TCO fitted
    if (m_rme_model != RME_MODEL_FIREFACE800)
        return -1;

    if (readBlock(RME_FF_TCO_READ_REG, buf, 4) != 0)
        return -1;

    if (tco_data != NULL) {
        for (i=0; i<(size<4)?size:4; i++)
            tco_data[i] = buf[i];
    }

    if ( (buf[0] & 0x80808080) == 0x80808080 &&
         (buf[1] & 0x80808080) == 0x80808080 &&
         (buf[2] & 0x80808080) == 0x80808080 &&
         (buf[3] & 0x8000FFFF) == 0x80008000) {
        // A TCO is present
        return 0;
    }

    return -1;
}

signed int 
Device::write_tco(quadlet_t *tco_data, signed int size)
{
    // Writes data to the TCO.  No check is made as to whether a TCO is
    // present in the current device.  Return value is 0 on success or -1 on
    // error.  "size" is the size (in quadlets) of the data pointed to by
    // "tco_data".  The first 4 quadlets of tco_data are significant; all
    // others are ignored.  If fewer than 4 quadlets are supplied (as
    // indicated by the "size" parameter, -1 will be returned.
    if (size < 4)
        return -1;

    // Don't bother trying to write if the device is not a FF800 since the
    // TCO can only be fitted to a FF800.
    if (m_rme_model != RME_MODEL_FIREFACE800)
        return -1;

    if (writeBlock(RME_FF_TCO_WRITE_REG, tco_data, 4) != 0)
        return -1;

    return 0;
}

signed int
Device::hardware_is_streaming(void)
{
    // Return 1 if the hardware is streaming, 0 if not.
    return dev_config->is_streaming;
}

signed int 
Device::read_tco_state(FF_TCO_state_t *tco_state)
{
    // Reads the current TCO state into the supplied state structure

    quadlet_t tc[4];
    unsigned int PLL_phase;

    if (read_tco(tc, 4) != 0)
      return -1;

    // The timecode is stored in BCD (binary coded decimal) in register 0.
    tco_state->frames = (tc[0] & 0xf) + ((tc[0] & 0x30) >> 4)*10;
    tco_state->seconds = ((tc[0] & 0xf00) >> 8) + ((tc[0] & 0x7000) >> 12)*10;
    tco_state->minutes = ((tc[0] & 0xf0000) >> 16) + ((tc[0] & 0x700000) >> 20)*10;
    tco_state->hours = ((tc[0] & 0xf000000) >> 24) + ((tc[0] & 0x30000000) >> 28)*10;

    tco_state->locked = (tc[1] & FF_TCO1_TCO_lock) != 0;
    tco_state->ltc_valid = (tc[1] & FF_TCO1_LTC_INPUT_VALID) != 0;

    switch (tc[1] & FF_TCO1_LTC_FORMAT_MASK) {
        case FF_TC01_LTC_FORMAT_24fps: 
          tco_state->frame_rate = FF_TCOSTATE_FRAMERATE_24fps; break;
        case FF_TCO1_LTC_FORMAT_25fps: 
          tco_state->frame_rate = FF_TCOSTATE_FRAMERATE_25fps; break;
        case FF_TC01_LTC_FORMAT_29_97fps: 
          tco_state->frame_rate = FF_TCOSTATE_FRAMERATE_29_97fps; break;
        case FF_TCO1_LTC_FORMAT_30fps: 
          tco_state->frame_rate = FF_TCOSTATE_FRAMERATE_30fps; break;
    }

    tco_state->drop_frame = (tc[1] & FF_TCO1_SET_DROPFRAME) != 0;

    switch (tc[1] & FF_TCO1_VIDEO_INPUT_MASK) {
        case FF_TCO1_VIDEO_INPUT_NTSC:
            tco_state->video_input = FF_TCOSTATE_VIDEO_NTSC; break;
        case FF_TCO1_VIDEO_INPUT_PAL:
            tco_state->video_input = FF_TCOSTATE_VIDEO_PAL; break;
        default:
            tco_state->video_input = FF_TCOSTATE_VIDEO_NONE;
    }

    if ((tc[1] & FF_TCO1_WORD_CLOCK_INPUT_VALID) == 0) {
        tco_state->word_clock_state = FF_TCOSTATE_WORDCLOCK_NONE;
    } else {
        switch (tc[1] & FF_TCO1_WORD_CLOCK_INPUT_MASK) {
            case FF_TCO1_WORD_CLOCK_INPUT_1x:
                tco_state->word_clock_state = FF_TCOSTATE_WORDCLOCK_1x; break;
            case FF_TCO1_WORD_CLOCK_INPUT_2x:
                tco_state->word_clock_state = FF_TCOSTATE_WORDCLOCK_2x; break;
            case FF_TCO1_WORD_CLOCK_INPUT_4x:
                tco_state->word_clock_state = FF_TCOSTATE_WORDCLOCK_4x; break;
        }
    }

    PLL_phase = (tc[2] & 0x7f) + ((tc[2] & 0x7f00) >> 1);
    tco_state->sample_rate = (25000000.0 * 16.0)/PLL_phase;

    return 0;
}

signed int 
Device::write_tco_settings(FF_TCO_settings_t *tco_settings)
{
    // Writes the supplied application-level settings to the device's TCO
    // (Time Code Option).  Don't bother doing anything if the device doesn't
    // have a TCO fitted.  Returns 0 on success, -1 on error.

    quadlet_t tc[4] = {0, 0, 0, 0};

    if (!dev_config->tco_present) {
        return -1;
    }

    if (tco_settings->MTC)
        tc[0] |= FF_TCO0_MTC;

    switch (tco_settings->input) {
        case FF_TCOPARAM_INPUT_LTC:
            tc[2] |= FF_TCO2_INPUT_LTC; break;
        case FF_TCOPARAM_INPUT_VIDEO:
            tc[2] |= FF_TCO2_INPUT_VIDEO; break;
        case FF_TCOPARAM_INPUT_WCK:
            tc[2] |= FF_TCO2_INPUT_WORD_CLOCK; break;
    }

    switch (tco_settings->frame_rate) {
        case FF_TCOPARAM_FRAMERATE_24fps:
            tc[1] |= FF_TC01_LTC_FORMAT_24fps; break;
        case FF_TCOPARAM_FRAMERATE_25fps:
            tc[1] |= FF_TCO1_LTC_FORMAT_25fps; break;
        case FF_TCOPARAM_FRAMERATE_29_97fps:
            tc[1] |= FF_TC01_LTC_FORMAT_29_97fps; break;
        case FF_TCOPARAM_FRAMERATE_29_97dfps:
            tc[1] |= FF_TCO1_LTC_FORMAT_29_97dpfs; break;
        case FF_TCOPARAM_FRAMERATE_30fps:
            tc[1] |= FF_TCO1_LTC_FORMAT_30fps; break;
        case FF_TCOPARAM_FRAMERATE_30dfps:
            tc[1] |= FF_TCO1_LTC_FORMAT_30dfps; break;
    }

    switch (tco_settings->word_clock) {
        case FF_TCOPARAM_WORD_CLOCK_CONV_1_1:
            tc[2] |= FF_TCO2_WORD_CLOCK_CONV_1_1; break;
        case FF_TCOPARAM_WORD_CLOCK_CONV_44_48:
            tc[2] |= FF_TCO2_WORD_CLOCK_CONV_44_48; break;
        case FF_TCOPARAM_WORD_CLOCK_CONV_48_44:
            tc[2] |= FF_TCO2_WORD_CLOCK_CONV_48_44; break;
    }

    switch (tco_settings->sample_rate) {
        case FF_TCOPARAM_SRATE_44_1:
            tc[2] |= FF_TCO2_SRATE_44_1; break;
        case FF_TCOPARAM_SRATE_48:
            tc[2] |= FF_TCO2_SRATE_48; break;
        case FF_TCOPARAM_SRATE_FROM_APP:
            tc[2] |= FF_TCO2_SRATE_FROM_APP; break;
    }

    switch (tco_settings->pull) {
        case FF_TCPPARAM_PULL_NONE:
            tc[2] |= FF_TCO2_PULL_0; break;
        case FF_TCOPARAM_PULL_UP_01:
            tc[2] |= FF_TCO2_PULL_UP_01; break;
        case FF_TCOPARAM_PULL_DOWN_01:
            tc[2] |= FF_TCO2_PULL_DOWN_01; break;
        case FF_TCOPARAM_PULL_UP_40:
            tc[2] |= FF_TCO2_PULL_UP_40; break;
        case FF_TCOPARAM_PULL_DOWN_40:
            tc[2] |= FF_TCO2_PULL_DOWN_40; break;
    }

    if (tco_settings->termination == FF_TCOPARAM_TERMINATION_ON)
        tc[2] |= FF_TCO2_SET_TERMINATION;

    return write_tco(tc, 4);
}

signed int
Device::set_hardware_dds_freq(signed int freq) 
{
    // Set the device's DDS to the given frequency (which in turn determines
    // the sampling frequency).  Returns 0 on success, -1 on error.

    unsigned int ret = 0;

    if (freq < MIN_SPEED || freq > MAX_SPEED)
        return -1;

    switch (m_rme_model) {
        case RME_MODEL_FIREFACE400:
            ret = writeRegister(RME_FF400_STREAM_SRATE, freq); break;
        case RME_MODEL_FIREFACE800:
            ret = writeRegister(RME_FF800_STREAM_SRATE, freq); break;
        default:
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            ret = -1;
    }
    if (ret == 0)
        dev_config->hardware_freq = freq;
    else
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write DDS register\n");

    return ret;
}

signed int
Device::hardware_init_streaming(unsigned int sample_rate, 
    unsigned int tx_channel)
{
    // tx_channel is the ISO channel the PC will transmit on.
    quadlet_t buf[5];
    fb_nodeaddr_t addr;
    unsigned int size;
    signed int ret;

debugOutput(DEBUG_LEVEL_VERBOSE, "*** stream init: %d, %d, %d\n",
  sample_rate, num_channels, tx_channel);

    buf[0] = sample_rate;
    buf[1] = (num_channels << 11) + tx_channel;
    buf[2] = num_channels;
    buf[3] = 0;
    buf[4] = 0;
    if (speed800) {
        buf[2] |= RME_FF800_STREAMING_SPEED_800;
    }

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        addr = RME_FF400_STREAM_INIT_REG;
        size = RME_FF400_STREAM_INIT_SIZE;
    } else 
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        addr = RME_FF800_STREAM_INIT_REG;
        size = RME_FF800_STREAM_INIT_SIZE;
    } else {
        debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
        return -1;
    }

    ret = writeBlock(addr, buf, size);
    if (ret != 0)
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write streaming parameters\n");
    return ret;
}

signed int
Device::hardware_start_streaming(unsigned int listen_channel)
{
    signed int ret = 0;
    // Listen_channel is the ISO channel the PC will listen on for data sent
    // by the Fireface.
    fb_nodeaddr_t addr;
    quadlet_t data = num_channels;

    config_lock();
    if (not(hardware_is_streaming())) {
debugOutput(DEBUG_LEVEL_VERBOSE,"*** starting: listen=%d, num_ch=%d\n", listen_channel, num_channels);
        if (m_rme_model == RME_MODEL_FIREFACE400) {
            addr = RME_FF400_STREAM_START_REG;
            data |= (listen_channel << 5);
        } else 
        if (m_rme_model == RME_MODEL_FIREFACE800) {
            addr = RME_FF800_STREAM_START_REG;
            if (speed800)
                data |= RME_FF800_STREAMING_SPEED_800; // Flag 800 Mbps speed
        } else {
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            return -1;
        }

debugOutput(DEBUG_LEVEL_VERBOSE, "start 0x%016"PRIx64" data: %08x\n", addr, data);
        ret = writeRegister(addr, data);
debugOutput(DEBUG_LEVEL_VERBOSE, "  ret=%d\n", ret);
        if (ret == 0) {
            dev_config->is_streaming = 1;
        } else
            debugOutput(DEBUG_LEVEL_ERROR, "failed to write for streaming start\n");

        set_hardware_channel_mute(-1, 0);

    } else
        ret = 0;
    config_unlock();

    return ret;
}

signed int
Device::hardware_stop_streaming(void)
{
    fb_nodeaddr_t addr;
    quadlet_t buf[4] = {0, 0, 0, 1};
    unsigned int size, ret = 0;

    config_lock();
    if (hardware_is_streaming()) {
        if (m_rme_model == RME_MODEL_FIREFACE400) {
            addr = RME_FF400_STREAM_END_REG;
            size = RME_FF400_STREAM_END_SIZE;
        } else 
        if (m_rme_model == RME_MODEL_FIREFACE800) {
            addr = RME_FF800_STREAM_END_REG;
            size = RME_FF800_STREAM_END_SIZE;
        } else {
            debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
            return -1;
        }

        ret = writeBlock(addr, buf, size);
        if (ret == 0) {
            dev_config->is_streaming = 0;
        } else
            debugOutput(DEBUG_LEVEL_ERROR, "failed to write for streaming stop\n");

        set_hardware_channel_mute(-1, 1);

    } else
        ret = 0;
    config_unlock();

    return ret;
}

signed int
Device::set_hardware_ampgain(unsigned int index, signed int val) {
// "index" indicates the hardware amplifier gain to set.  Values of 0-3
// correspond to input amplifier gains.  Values from 4 on relate to output
// volume.
//
// "val" is in dB except for inputs 3/4 where it's in units of 0.5 dB. This
// function is responsible for converting to/from the scale used by the
// device.
//
// Only the FF400 has the hardware gain register which is controlled by this
// function.
    quadlet_t regval = 0;
    signed int devval = 0;
    signed int ret;
    if (val > 120)
      val = 120;
    if (val < -120)
      val = -120;
    if (index <= FF400_AMPGAIN_MIC2) {
        if (val >= 10)
            devval = val;
        else
            devval = 0;
    } else
    if (index <= FF400_AMPGAIN_INPUT4) {
        devval = val;
    } else {
        devval = 6 - val;
        if (devval > 53)
            devval = 0x3f;  // Mute
    }
    regval |= devval;
    regval |= (index << 16);
    ret = writeRegister(RME_FF400_GAIN_REG, regval);
    if (ret != 0)
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write amp gains\n");
    return ret;
}

signed int
Device::set_hardware_mixergain(unsigned int ctype, unsigned int src_channel, 
  unsigned int dest_channel, signed int val) {
// Set the value of a matrix mixer control.  ctype is one of the RME_FF_MM_*
// defines:
//   RME_FF_MM_INPUT: source is a physical input
//   RME_FF_MM_PLAYBACK: source is playback from PC
//   RME_FF_MM_OUTPUT: source is the physical output whose gain is to be 
//     changed, destination is ignored
// Val is the integer value sent to the device.  The amount of gain (in dB)
// applied can be calculated using
//   dB = 20.log10(val/32768)
// The maximum value of val is 0x10000, corresponding to +6dB of gain.
// The minimum is 0x00000 corresponding to mute.

    unsigned int n_channels;
    signed int ram_output_block_size;
    unsigned int ram_addr;

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        n_channels = RME_FF400_MAX_CHANNELS;
        ram_output_block_size = 0x48;
    } else
    if (m_rme_model == RME_MODEL_FIREFACE800) {
        n_channels = RME_FF800_MAX_CHANNELS;
        ram_output_block_size = 0x80;
    } else {
        debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
        return -1;
    }

    if (src_channel>n_channels || dest_channel>n_channels)
        return -1;
    if (abs(val)>0x10000)
        return -1;

    ram_addr = RME_FF_MIXER_RAM;
    switch (ctype) {
        case RME_FF_MM_INPUT:
        case RME_FF_MM_PLAYBACK:
            ram_addr += (dest_channel*2*ram_output_block_size) + 4*src_channel;
            if (ctype == RME_FF_MM_PLAYBACK)
                 ram_addr += ram_output_block_size;
            break;
        case RME_FF_MM_OUTPUT:
            if (m_rme_model == RME_MODEL_FIREFACE400)
                ram_addr += 0x0f80;
            else
                ram_addr += 0x1f80;
            ram_addr += 4*src_channel;
            break;
    }

    if (writeRegister(ram_addr, val) != 0) {
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write mixer gain element\n");
    }

    // If setting the output volume and the device is the FF400, keep
    // the separate gain register in sync.
    if (ctype==RME_FF_MM_OUTPUT && m_rme_model==RME_MODEL_FIREFACE400) {
        signed int dB;
        if (val < 0)
          val = -val;
        if (val==0)
            dB = -90;
        else
            dB = roundl(20.0*log10(val/32768.0));
        set_hardware_ampgain(FF400_AMPGAIN_OUTPUT1+src_channel, dB);
    }

    return 0;
}

signed int
Device::set_hardware_channel_mute(signed int chan, signed int mute) {

// Mute hardware channels as instructed.  This mute probably relates to the
// sampled input channels as delivered to the PC.  If "chan" is -1 the
// supplied "mute" status is applied to all channels.  This is the only
// supported "chan" value for now.  Down the track, if there's a need,
// this could be extended to allow individual channel control.
    quadlet_t buf[28];
    signed int i;
    signed int n_channels;
    
    if (m_rme_model == RME_MODEL_FIREFACE400)
        n_channels = RME_FF400_MAX_CHANNELS;
    else
    if (m_rme_model == RME_MODEL_FIREFACE800)
        n_channels = RME_FF800_MAX_CHANNELS;
    else {
        debugOutput(DEBUG_LEVEL_ERROR, "unimplemented model %d\n", m_rme_model);
        return -1;
    }

    i = 0;
    if (chan < 0) {
        while (i<n_channels && i<28) {
            buf[i++] = (mute!=0);
        }
    } else {
      return 0;
    }

    while (i < 28) {
        buf[i++] = 0x00000001;
    }

    // Write 28 quadlets even for FF400
    i = writeBlock(RME_FF_CHANNEL_MUTE_MASK, buf, 28);
    if (i != 0)
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write channel mute\n");
    return i;
}

signed int
Device::set_hardware_output_rec(signed int rec) {
// Explicitly record (mute!=1) outputs, or not.
// TODO: fill the details in to allow individual outputs to be recorded as 
// required.
    quadlet_t buf[28];
    signed int i;

    for (i=0; i<28; i++)
        buf[i] = (rec!=0);

    // Write 28 quadlets even for FF400
    i = writeBlock(RME_FF_OUTPUT_REC_MASK, buf, 28);
    if (i != 0)
        debugOutput(DEBUG_LEVEL_ERROR, "failed to write output record flags\n");
    return i;
}

}
