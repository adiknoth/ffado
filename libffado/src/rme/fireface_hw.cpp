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

    /* Input level */
    switch (settings.input_level) {
        case 1: // Low gain
            data[1] |= CR1_ILEVEL_CPLD_LOGAIN;    // CPLD
            data[0] |= CR0_ILEVEL_FPGA_LOGAIN;    // LED control (used on FF800 only)
            break;
        case 2: // +4 dBu
            data[1] |= CR1_ILEVEL_CPLD_4dBU;
            data[0] |= CR0_ILEVEL_FPGA_4dBU;
            break;
        case 3: // -10 dBV
            data[1] |= CR1_ILEVEL_CPLD_m10dBV;
            data[0] |= CR0_ILEVEL_FPGA_m10dBV;
            break;
    }

    /* Output level */
    switch (settings.output_level) {
        case 1: // High gain
            data[1] |= CR1_OLEVEL_CPLD_HIGAIN;   // CPLD
            data[0] |= CR0_OLEVEL_FPGA_HIGAIN;   // LED control (used on FF800 only)
            break;
        case 2: // +4 dBu
            data[1] |= CR1_OLEVEL_CPLD_4dBU;
            data[0] |= CR0_OLEVEL_FPGA_4dBU;
            break;
        case 3: // -10 dBV
            data[1] |= CR1_OLEVEL_CPLD_m10dBV;
            data[0] |= CR0_OLEVEL_FPGA_m10dBV;
            break;
    }

    /* Speaker emulation / filter  FIXME: needs filling out, is tied in
     * with analog input settings. 
     */
    data[1] = 0xf;

    /* Drive / fuzz */
    if (settings.fuzz)
      data[0] |= CR0_INSTR_DRIVE_FPGA; // FPGA LED control
    else
      data[1] |= CR1_INSTR_DRIVE;      // CPLD

    /* Drop-and-stop is hardwired on */
    data[2] |= CR2_DROP_AND_STOP;

    if (m_rme_model == RME_MODEL_FIREFACE400) {
        data[2] |= CR2_FF400_BIT;
    }

    data[2] |= (CR2_FREQ0 + CR2_FREQ1 + CR2_DSPEED + CR2_QSSPEED);

//data[0] = 0x00020811;      // Phantom off
data[0] = 0x00020811;      // Phantom on
data[1] = 0x0000031e;
data[2] = 0xc400101f;

    conf_reg = (m_rme_model==RME_MODEL_FIREFACE800)?RME_FF800_CONF_REG:RME_FF400_CONF_REG;
    if (writeBlock(conf_reg, data, 3) != 0)
        return -1;

    return -0;
}

}
