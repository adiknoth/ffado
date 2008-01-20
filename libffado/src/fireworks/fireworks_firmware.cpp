/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "fireworks_device.h"
#include "fireworks_firmware.h"
#include "efc/efc_avc_cmd.h"
#include "efc/efc_cmd.h"
#include "efc/efc_cmds_flash.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

template <class T>
bool from_string(T& t, 
                 const std::string& s, 
                 std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}

// These classes provide support for reading/writing the firmware on
// echo fireworks based devices
namespace FireWorks {

IMPL_DEBUG_MODULE( Firmware, Firmware, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( FirmwareUtil, FirmwareUtil, DEBUG_LEVEL_NORMAL );

// the firmware class

// some generic string generation functions
const char *Firmware::eDatTypeToString(const enum Firmware::eDatType type) {
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

const enum Firmware::eDatType Firmware::intToeDatType(int type) {
    switch (type) {
        case (int)eDT_DspCode: return eDT_DspCode;
        case (int)eDT_IceLynxCode: return eDT_IceLynxCode;
        case (int)eDT_Data: return eDT_Data;
        case (int)eDT_FPGACode: return eDT_FPGACode;
        case (int)eDT_DeviceName: return eDT_DeviceName;
        default:
            return eDT_Invalid;
    }
}

Firmware::Firmware()
: m_Type ( eDT_Invalid )
, m_flash_offset_address ( 0 )
, m_length_quads ( 0 )
, m_CRC32 ( 0 )
, m_checksum ( 0 )
, m_version ( 0 )
, m_append_crc ( false )
, m_footprint_quads ( 0 )
, m_data( NULL )
{
}

Firmware::~Firmware()
{
    if (m_data) delete[] m_data;
}

void
Firmware::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Firmware from %s\n", m_filename.c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, " Type                 : %s\n", eDatTypeToString(m_Type));
    if (m_Type == eDT_Invalid) return;

    unsigned int version_major = (m_version & 0xFF000000) >> 24;
    unsigned int version_minor = (m_version & 0x00FF0000) >> 16;
    unsigned int version_build = (m_version & 0x0000FFFF);
    debugOutput(DEBUG_LEVEL_NORMAL, " Address Offset       : 0x%08lX\n", m_flash_offset_address);
    debugOutput(DEBUG_LEVEL_NORMAL, " Lenght (Quadlets)    : 0x%08lX\n", m_length_quads);
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC 32               : 0x%08lX\n", m_CRC32);
    debugOutput(DEBUG_LEVEL_NORMAL, " Checksum             : 0x%08lX\n", m_checksum);
    debugOutput(DEBUG_LEVEL_NORMAL, " Firmware version     : %02u.%02u.%02u (0x%08X)\n", 
                                    version_major, version_minor, version_build, m_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Append CRC           : %s\n", (m_append_crc?"Yes":"No"));
    debugOutput(DEBUG_LEVEL_NORMAL, " Footprint (Quadlets) : 0x%08lX\n", m_footprint_quads);
}

bool
Firmware::loadFile(std::string filename)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Loading firmware from file %s\n", filename.c_str());
    fstream fwfile;
    
    debugOutput(DEBUG_LEVEL_VERBOSE, " Loading file...\n");
    fwfile.open( filename.c_str(), ios::in | ios::ate);
    if ( !fwfile.is_open() ) {
        debugError("Could not open file.\n");
        return false;
    }
    // get file size
    int size;
    size = (int)fwfile.tellg();
    
    if( size > ECHO_FIRMWARE_FILE_MAX_LENGTH_BYTES) {
        debugError("File too large (%d bytes).\n", size);
        return false;
    }
    
    debugOutput(DEBUG_LEVEL_VERBOSE, " Checking magic...\n");
    // read magic
    if( size < ECHO_FIRMWARE_MAGIC_LENGTH_BYTES) {
        debugError("File too small (%d bytes) to contain the magic header.\n", size);
        return false;
    }
    
    fwfile.seekg (0, ios::beg);
    getline(fwfile, m_magic);
    // get rid of the DOS-Style end of line
    string::size_type loc = m_magic.find( '\r' );
    if( loc != string::npos ) {
        m_magic.erase(loc);
    }
    loc = m_magic.find( '\n' );
    if( loc != string::npos ) {
        m_magic.erase(loc);
    }
    
    // check the magic
    if (m_magic != ECHO_FIRMWARE_MAGIC) {
        debugError("Magic was '%s' but should have been '%s'\n", 
                    m_magic.c_str(), ECHO_FIRMWARE_MAGIC);
        return false;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "   magic OK...\n");
    
    debugOutput(DEBUG_LEVEL_VERBOSE, " Reading header...\n");
    // read header
    if( size < ECHO_FIRMWARE_MAGIC_LENGTH_BYTES + ECHO_FIRMWARE_HEADER_LENGTH_BYTES) {
        debugError("File too small to contain the header.\n");
        return false;
    }
    
    for (int i=0; i < ECHO_FIRMWARE_HEADER_LENGTH_QUADLETS; i++) {
        std::string buffer;
        getline(fwfile, buffer);
        // get rid of the DOS-Style end of line
        string::size_type loc = buffer.find( '\r' );
        if( loc != string::npos ) {
            buffer.erase(loc);
        }
        loc = buffer.find( '\n' );
        if( loc != string::npos ) {
            buffer.erase(loc);
        }
        
        if (!from_string<uint32_t>(m_header[i], buffer, std::hex)) {
            debugWarning("Could not convert '%s' to uint32_t\n", buffer.c_str());
            return false;
        }
        
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "   Header %02d: %08lX\n", 
                    i, m_header[i]);
    }

    m_Type                  = intToeDatType(m_header[0]);
    m_flash_offset_address  = m_header[1];
    m_length_quads          = m_header[2];
    m_CRC32                 = m_header[3];
    m_checksum              = m_header[4];
    m_version               = m_header[5];
    m_append_crc            = m_header[6] != 0;
    m_footprint_quads       = m_header[7];
    debugOutput(DEBUG_LEVEL_VERBOSE, "  header ok...\n");

    debugOutput(DEBUG_LEVEL_VERBOSE, " Reading data...\n");
    delete m_data;
    m_data = new uint32_t[m_length_quads];
    for (uint32_t i=0; i < m_length_quads; i++) {
        std::string buffer;
        getline(fwfile, buffer);
        // get rid of the DOS-Style end of line
        string::size_type loc = buffer.find( '\r' );
        if( loc != string::npos ) {
            buffer.erase(loc);
        }
        loc = buffer.find( '\n' );
        if( loc != string::npos ) {
            buffer.erase(loc);
        }
        
        if (!from_string<uint32_t>(m_data[i], buffer, std::hex)) {
            debugWarning("Could not convert '%s' to uint32_t\n", buffer.c_str());
            return false;
        }
        
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "   Data %02d: %08lX\n", 
                    i, m_data[i]);
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "  data ok...\n");
    fwfile.close();

    m_filename =  filename;
    return false;
}

// the firmware loader helper class
FirmwareUtil::FirmwareUtil(FireWorks::Device& p)
: m_Parent(p)
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
