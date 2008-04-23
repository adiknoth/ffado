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

#define ECHO_FLASH_TIMEOUT_MILLISECS 2000

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
: m_source( "none" )
, m_Type ( eDT_Invalid )
, m_flash_offset_address ( 0 )
, m_length_quads ( 0 )
, m_CRC32 ( 0 )
, m_checksum ( 0 )
, m_version ( 0 )
, m_append_crc ( false )
, m_footprint_quads ( 0 )
, m_data( NULL )
, m_valid( false )
{
}

Firmware::Firmware(const Firmware& f) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "copy constructor\n");
    m_source = f.m_source;
    m_Type = f.m_Type;
    m_flash_offset_address = f.m_flash_offset_address;
    m_length_quads = f.m_length_quads;
    m_CRC32 = f.m_CRC32;
    m_checksum = f.m_checksum;
    m_version = f.m_version;
    m_append_crc = f.m_append_crc;
    m_footprint_quads = f.m_footprint_quads;
    m_valid = f.m_valid;
    m_data = new uint32_t[m_length_quads];

    memcpy(m_data, f.m_data, m_length_quads*sizeof(uint32_t));
}

Firmware& Firmware::operator=(const Firmware& f) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "assignment\n");
    if (this != &f) {  // make sure not same object
        // assign new vars
        m_source = f.m_source;
        m_Type = f.m_Type;
        m_flash_offset_address = f.m_flash_offset_address;
        m_length_quads = f.m_length_quads;
        m_CRC32 = f.m_CRC32;
        m_checksum = f.m_checksum;
        m_version = f.m_version;
        m_append_crc = f.m_append_crc;
        m_footprint_quads = f.m_footprint_quads;
        m_valid = f.m_valid;

        // replace dynamic data
        delete [] m_data;
        m_data = new uint32_t[m_length_quads];
        memcpy(m_data, f.m_data, m_length_quads*sizeof(uint32_t));
    }
    return *this;    // Return ref for multiple assignment
}

Firmware::~Firmware()
{
    if (m_data) delete[] m_data;
}

void
Firmware::show()
{
    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_NORMAL, "Firmware from %s\n", m_source.c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, " Valid?               : %s\n", (m_valid?"Yes":"No"));
    debugOutput(DEBUG_LEVEL_NORMAL, " Type                 : %s\n", eDatTypeToString(m_Type));
    if (m_Type == eDT_Invalid) return;

    unsigned int version_major = (m_version & 0xFF000000) >> 24;
    unsigned int version_minor = (m_version & 0x00FF0000) >> 16;
    unsigned int version_build = (m_version & 0x0000FFFF);
    debugOutput(DEBUG_LEVEL_NORMAL, " Address Offset       : 0x%08lX\n", m_flash_offset_address);
    debugOutput(DEBUG_LEVEL_NORMAL, " Length (Quadlets)    : 0x%08lX\n", m_length_quads);
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC 32               : 0x%08lX\n", m_CRC32);
    debugOutput(DEBUG_LEVEL_NORMAL, " Checksum             : 0x%08lX\n", m_checksum);
    debugOutput(DEBUG_LEVEL_NORMAL, " Firmware version     : %02u.%02u.%02u (0x%08X)\n", 
                                    version_major, version_minor, version_build, m_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Append CRC           : %s\n", (m_append_crc?"Yes":"No"));
    debugOutput(DEBUG_LEVEL_NORMAL, " Footprint (Quadlets) : 0x%08lX\n", m_footprint_quads);
    #endif
}

bool
Firmware::operator==(const Firmware& f)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Comparing header...\n");
    if(m_flash_offset_address != f.m_flash_offset_address) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "Flash address differs: %08X != %08X\n",
                    m_flash_offset_address, f.m_flash_offset_address);
        return false;
    }
    if(m_length_quads != f.m_length_quads) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "Flash length differs: %08X != %08X\n",
                    m_length_quads, f.m_length_quads);
        return false;
    }
    if(m_data == NULL && f.m_data == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "both firmwares have no data\n");
        return true;
    }
    if(m_data == NULL || f.m_data == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "one of the firmwares has no data: %p != %p\n",
                    m_data, f.m_data);
        return false;
    }
    
    debugOutput(DEBUG_LEVEL_VERBOSE, "Comparing data...\n");
    bool retval = true;
    for(unsigned int i=0; i<m_length_quads; i++) {
        if(m_data[i] != f.m_data[i]) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        " POS 0x%08X: %08X != %08X\n",
                        i, m_data[i], f.m_data[i]);
            retval = false;
        }
    }
    return retval;
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
    delete[] m_data;
    m_data = new uint32_t[m_length_quads];
    if(m_data == NULL) {
        debugError("could not allocate memory for firmware\n");
        return false;
    }
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

    m_source = filename;
    m_valid = true;
    return true;
}

bool
Firmware::loadFromMemory(uint32_t *data, uint32_t addr, uint32_t len) {
    m_valid = false;

    // mark it as invalid for now
    m_Type                  = eDT_Invalid;

    // set some values (FIXME)
    m_flash_offset_address  = addr;
    m_length_quads          = len;
    m_CRC32                 = 0;
    m_checksum              = 0;
    m_version               = 0;
    m_append_crc            = false;
    m_footprint_quads       = 0;

    // delete any old data
    delete[] m_data;
    m_data = new uint32_t[len];
    if(m_data == NULL) {
        debugError("could not allocate memory for firmware\n");
        return false;
    }
    // copy data
    memcpy(m_data, data, len*sizeof(uint32_t));

    return true;
}

void
Firmware::dumpData()
{
    debugWarning("-- char dump --");
    hexDump((unsigned char*)m_data, m_length_quads*4);
    
    debugWarning("-- quadlet dump --");
    hexDumpQuadlets(m_data, m_length_quads);

}

// the firmware loader helper class
FirmwareUtil::FirmwareUtil(FireWorks::Device& p)
: m_Parent(p)
{
}

FirmwareUtil::~FirmwareUtil()
{
}

Firmware
FirmwareUtil::getFirmwareFromDevice(uint32_t start, uint32_t len)
{
    if(len == 0) {
        debugError("Invalid length: %u\n", len);
        return Firmware();
    }

    uint32_t data[len];
    Firmware f = Firmware();

    if(!m_Parent.readFlash(start, len, data)) {
        debugError("Flash read failed\n");
        return f;
    }

    if(!f.loadFromMemory(data, start, len)) {
        debugError("Could not load firmware from memory dump\n");
    }

    return f;
}

bool
FirmwareUtil::waitForFlash()
{
    bool ready;

    EfcFlashGetStatusCmd statusCmd;
    const unsigned int time_to_sleep_usecs = 10000;
    int wait_cycles = ECHO_FLASH_TIMEOUT_MILLISECS * 1000 / time_to_sleep_usecs;

    do {
        if (!m_Parent.doEfcOverAVC(statusCmd)) {
            debugError("Could not read flash status\n");
            return false;
        }
        if (statusCmd.m_header.retval == EfcCmd::eERV_FlashBusy) {
            ready = false;
        } else {
            ready = statusCmd.m_ready;
        }
        usleep(time_to_sleep_usecs);
    } while (!ready && wait_cycles--);

    if(wait_cycles == 0) {
        debugError("Timeout while waiting for flash\n");
        return false;
    }

    return ready;
}

bool
FirmwareUtil::eraseBlocks(uint32_t start_address, unsigned int nb_quads)
{
    EfcFlashEraseCmd eraseCmd;
    uint32_t blocksize_bytes;
    uint32_t blocksize_quads;
    unsigned int quads_left = nb_quads;
    bool success = true;

    const unsigned int max_nb_tries = 10;
    unsigned int nb_tries = 0;

    do {
        // the erase block size is fixed by the HW, and depends
        // on the flash section we're in
        if (start_address < MAINBLOCKS_BASE_OFFSET_BYTES)
                blocksize_bytes = PROGRAMBLOCK_SIZE_BYTES;
        else
                blocksize_bytes = MAINBLOCK_SIZE_BYTES;
        start_address &= ~(blocksize_bytes - 1);
        blocksize_quads = blocksize_bytes / 4;

        // corner case: requested to erase less than one block
        if (blocksize_quads > quads_left) {
            blocksize_quads = quads_left;
        }

        // do the actual erase
        eraseCmd.m_address = start_address;
        if (!m_Parent.doEfcOverAVC(eraseCmd)) {
            debugWarning("Could not erase flash block at 0x%08X\n", start_address);
            success = false;
        } else {
            // verify that the block is empty as an extra precaution
            EfcFlashReadCmd readCmd;
            readCmd.m_address = start_address;
            readCmd.m_nb_quadlets = blocksize_quads;
            uint32_t verify_quadlets_read = 0;
            do {
                if (!m_Parent.doEfcOverAVC(readCmd)) {
                    debugError("Could not read flash block at 0x%08X\n", start_address);
                    return false;
                }

                // everything should be 0xFFFFFFFF if the erase was successful
                for (unsigned int i = 0; i < readCmd.m_nb_quadlets; i++) {
                    if (0xFFFFFFFF != readCmd.m_data[i]) {
                        debugWarning("Flash erase verification failed.\n");
                        success = false;
                        break;
                    }
                }
 
                // maybe not everything was read at once
                verify_quadlets_read += readCmd.m_nb_quadlets;
                readCmd.m_address += start_address + verify_quadlets_read * 4;
                readCmd.m_nb_quadlets = blocksize_quads - verify_quadlets_read;
            } while(verify_quadlets_read < blocksize_quads);
        }

        if (success) {
            start_address += blocksize_bytes;
            quads_left -= blocksize_quads;
            nb_tries = 0;
        } else {
            nb_tries++;
        }
        if (nb_tries > max_nb_tries) {
            debugError("Needed too many tries to erase flash at 0x%08X", start_address);
            return false;
        }
    } while (quads_left > 0);
    
    return true;
}


void
FirmwareUtil::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "FirmwareUtil\n");
}

} // FireWorks
