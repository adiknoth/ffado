/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "fireworks_device.h"
#include "fireworks_firmware.h"
#include "efc/efc_avc_cmd.h"
#include "efc/efc_cmd.h"
#include "efc/efc_cmds_flash.h"

#include <string>
#include <sstream>

using namespace std;

// These classes provide support for reading/writing the firmware on
// echo fireworks based devices
namespace FireWorks {
IMPL_DEBUG_MODULE( Firmware, Firmware, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( FirmwareUtil, FirmwareUtil, DEBUG_LEVEL_NORMAL );

// the firmware class

// some generic string generation functions
const char *Firmware::eDatTypeToString(const enum eDatType type) {
    switch (type) {
        case eDT_DspCode:
            return "Dsp Code";
        case eDT_IceLynxCode:
            return "IceLynx Code";
        case eDT_Data:
            return "Data";
        case eDT_FPGACode:
            return "FPGA Code";
        case eDT_DeviceName:
            return "Device Name";
        default:
            return "invalid";
    }
}

Firmware::Firmware()
{
}

Firmware::~Firmware()
{
}

void
Firmware::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Firmware from %s\n", m_filename.c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, " Type                 : %s\n", eDatTypeToString(m_Type));
    debugOutput(DEBUG_LEVEL_NORMAL, " Address Offset       : %08llX\n", m_flash_offset_address);
    debugOutput(DEBUG_LEVEL_NORMAL, " Lenght (Quadlets)    : %08llX\n", m_length_quads);
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC 32               : %08llX\n", m_CRC32);
    debugOutput(DEBUG_LEVEL_NORMAL, " Checksum             : %08llX\n", m_checksum);
    debugOutput(DEBUG_LEVEL_NORMAL, " Firmware version     : %08llX\n", m_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Append CRC           : %s\n", (m_append_crc?"Yes":"No"));
    debugOutput(DEBUG_LEVEL_NORMAL, " Footprint (Quadlets) : %08llX\n", m_footprint_quads);

}

bool
Firmware::loadFile(std::string filename)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Loading firmware from file %s\n", filename.c_str());
    
    
    m_filename =  filename;
    return false;
}

// the firmware loader helper class
FirmwareUtil::FirmwareUtil(FireWorks::Device& p, FireWorks::Firmware& f)
: m_Parent(p)
, m_Firmware(f)
{
}

FirmwareUtil::~FirmwareUtil()
{
}

void
FirmwareUtil::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "FirmwareUtil\n");
}

} // FireWorks
