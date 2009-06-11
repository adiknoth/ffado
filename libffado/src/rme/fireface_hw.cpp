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

#include "rme/rme_avdevice.h"
#include "rme/fireface_def.h"

#include "debugmodule/debugmodule.h"

namespace Rme {

signed int 
Device::init_hardware(void)
{
    // Initialises the hardware the state defined by the Device's settings
    // structure.  This has the side effect of extinguishing the "Host" LED
    // on the FF400 when done for the first time after the interface has
    // been powered up.
    //
    // FIXME: currently this is a minimum skeleton to prove basic
    // functionality.  It requires details to be filled in.
    quadlet_t data[3] = {0, 0, 0};
    unsigned int conf_reg;

    if (settings.mic_phantom[0])
      data[0] |= CR0_PHANTOM_MIC0;
    if (settings.mic_phantom[1])
      data[0] |= CR0_PHANTOM_MIC1;
    if (settings.mic_phantom[2])
      data[0] |= CR0_PHANTOM_MIC2;
    if (settings.mic_phantom[3])
      data[0] |= CR0_PHANTOM_MIC3;

    /* Phones level */
    switch (settings.phones_level) {
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

    /* Input level */
    switch (settings.input_level) {
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
    switch (settings.output_level) {
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
    data[1] |= (settings.input_opt[1] & FF_SWPARAM_INPUT_OPT_A) ? CR1_INPUT_OPT1_A : 0;
    data[1] |= (settings.input_opt[1] & FF_SWPARAM_INPUT_OPT_B) ? CR1_INPUT_OPT1_B : 0;
    data[1] |= (settings.input_opt[2] & FF_SWPARAM_INPUT_OPT_A) ? CR1_INPUT_OPT2_A : 0;
    data[1] |= (settings.input_opt[2] & FF_SWPARAM_INPUT_OPT_B) ? CR1_INPUT_OPT2_B : 0;

    // Drive the speaker emulation / filter LED via FPGA
    data[0] |= (settings.filter) ? CR0_FILTER_FPGA : 0;

    // Set the "rear" option for input 0 if selected
    data[1] |= (settings.input_opt[0] & FF_SWPARAM_FF800_INPUT_OPT_REAR) ? CR1_FF800_INPUT1_REAR : 0;

    // The input 0 "front" option is activated using one of two bits
    // depending on whether the filter (aka "speaker emulation") setting is
    // active.
    if (settings.input_opt[0] & FF_SWPARAM_FF800_INPUT_OPT_FRONT) {
        data[1] |= (settings.filter) ? CR1_FF800_INPUT1_FRONT_WITH_FILTER : CR1_FF800_INPUT1_FRONT;
    }

    data[2] |= (settings.spdif_output_emphasis==FF_SWPARAM_SPDIF_OUTPUT_EMPHASIS_ON) ? CR2_SPDIF_OUT_EMP : 0;
    data[2] |= (settings.spdif_output_pro==FF_SWPARAM_SPDIF_OUTPUT_PRO_ON) ? CR2_SPDIF_OUT_PRO : 0;
    data[2] |= (settings.spdif_output_nonaudio==FF_SWPARAM_SPDIF_OUTPUT_NONAUDIO_ON) ? CR2_SPDIF_OUT_NONAUDIO : 0;
    data[2] |= (settings.spdif_output_mode==FF_SWPARAM_SPDIF_OUTPUT_OPTICAL) ? CR2_SPDIF_OUT_ADAT2 : 0;
    data[2] |= (settings.clock_mode==FF_SWPARAM_SPDIF_CLOCK_MODE_AUTOSYNC) ? CR2_CLOCKMODE_AUTOSYNC : CR2_CLOCKMODE_MASTER;
    data[2] |= (settings.spdif_input_mode==FF_SWPARAM_SPDIF_INPUT_COAX) ? CR2_SPDIF_IN_COAX : CR2_SPDIF_IN_ADAT2;
    data[2] |= (settings.word_clock_single_speed=FF_SWPARAM_WORD_CLOCK_1x) ? CR2_WORD_CLOCK_1x : 0;

    /* TMS / TCO toggle bits in CR2 are not set by other drivers */

    /* Drive / fuzz */
    if (settings.fuzz)
      data[0] |= CR0_INSTR_DRIVE_FPGA; // FPGA LED control
    else
      data[1] |= CR1_INSTR_DRIVE;      // CPLD

    /* Drop-and-stop is hardwired on in other drivers */
    data[2] |= CR2_DROP_AND_STOP;

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        data[2] |= CR2_FF400_BIT;
    }

    switch (settings.sync_ref) {
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

    // The FF800 limiter applies to the front panel instrument input, so it
    // only makes sense that it is disabled when that input is in use.
    data[2] |= (settings.limiter_disable && 
                (settings.input_opt[0] & FF_SWPARAM_FF800_INPUT_OPT_FRONT)) ? 
                CR2_DISABLE_LIMITER : 0;

//This is just for testing - it's a known consistent configuration
//data[0] = 0x00020811;      // Phantom off
data[0] = 0x00020811;      // Phantom on
data[1] = 0x0000031e;
data[2] = 0xc400101f;

    conf_reg = (m_rme_model==RME_MODEL_FIREFACE800)?RME_FF800_CONF_REG:RME_FF400_CONF_REG;
    if (writeBlock(conf_reg, data, 3) != 0)
        return -1;

    return -0;
}

signed int Device::read_tco(quadlet_t *tco_data, signed int size)
{
    // Read the TCO registers and return the respective values in *tco_data. 
    // Return value is 0 on success, or -1 if there is no TCO present. 
    // "size" is the size (in quadlets) of the array pointed to by tco_data. 
    // To obtain all TCO data "size" should be at least 4.  If the caller
    // doesn't care about the data returned by the TCO, tco_data can be
    // NULL.
    quadlet_t buf[4];
    signed int i;

    // The Fireface 400 can't have the TCO fitted
    if (m_rme_model==RME_MODEL_FIREFACE400)
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

signed int Device::write_tco(quadlet_t *tco_data, signed int size)
{
    // Writes data to the TCO.  No check is made as to whether a TCO
    // is present in the current device.  Return value is 0 on success
    // or -1 on error.  The first 4 quadlets of tco_data are significant;
    // all others are ignored.  If fewer than 4 quadlets are supplied (as
    // indicated by the "size" parameter, -1 will be returned.
    if (size < 4)
        return -1;

    // Don't bother trying to write if the device is a FF400 since the TCO
    // can't be fitted to this device.
    if (m_rme_model==RME_MODEL_FIREFACE400)
        return -1;

    if (writeBlock(RME_FF_TCO_WRITE_REG, tco_data, 4) != 0)
        return -1;

    return 0;
}

}
