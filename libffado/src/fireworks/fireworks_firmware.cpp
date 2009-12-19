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

#include "libieee1394/configrom.h"
#include "libieee1394/vendor_model_ids.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstring>

#define DAT_EXTENSION "dat"

// device id's
#define AUDIOFIRE2          0x000af2
#define AUDIOFIRE4          0x000af4
#define AUDIOFIRE8          0x000af8
#define AUDIOFIRE12         0x00af12
#define AUDIOFIRE12HD       0x0af12d
#define FWHDMI              0x00afd1
#define ONYX400F            0x00400f 
#define ONYX1200F           0x01200f
#define FIREWORKS8          0x0000f8

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
    debugOutput(DEBUG_LEVEL_NORMAL, " Address Offset       : 0x%08X\n", m_flash_offset_address);
    debugOutput(DEBUG_LEVEL_NORMAL, " Length (Quadlets)    : 0x%08X\n", m_length_quads);
    debugOutput(DEBUG_LEVEL_NORMAL, " CRC 32               : 0x%08X\n", m_CRC32);
    debugOutput(DEBUG_LEVEL_NORMAL, " Checksum             : 0x%08X\n", m_checksum);
    debugOutput(DEBUG_LEVEL_NORMAL, " Firmware version     : %02u.%02u.%02u (0x%08X)\n", 
                                    version_major, version_minor, version_build, m_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Append CRC           : %s\n", (m_append_crc?"Yes":"No"));
    debugOutput(DEBUG_LEVEL_NORMAL, " Footprint (Quadlets) : 0x%08X\n", m_footprint_quads);
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
        
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "   Header %02d: %08X\n", 
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
        
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "   Data %02d: %08X\n", 
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

uint32_t
Firmware::getWriteDataLen() {
    uint32_t retval = 0;
    if((m_append_crc != 0) && (m_length_quads < m_footprint_quads)) {
        retval += m_footprint_quads;
    } else {
        retval += m_length_quads;
    }
    return retval;
}

bool
Firmware::getWriteData(uint32_t *buff) {
    // copy the payload data
    memcpy(buff, m_data, m_length_quads*4);
    // if necessary, add crc/version
    if((m_append_crc != 0) && (m_length_quads < m_footprint_quads)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "appending CRC and version\n");
        buff[m_footprint_quads - 1] = m_CRC32;
        buff[m_footprint_quads - 2] = m_version;
    }
    return true;
}

void
Firmware::dumpData()
{
    debugWarning("-- char dump --");
    hexDump((unsigned char*)m_data, m_length_quads*4);
/*    
    debugWarning("-- quadlet dump --");
    hexDumpQuadlets(m_data, m_length_quads);*/

}


// the firmware loader helper class
const char *Af2Dats[] = 
{
    "Fireworks3"
};

const char *Af4Dats[] = 
{
    "Fireworks3"
};

const char *Af8Dats[] = 
{
    "bootstrap",
    "audiofire8",
    "audiofire8_E",
    "FireworksARM"
};

const char *Af12Dats[] = 
{
    "bootstrap",
    "audiofire12",
    "audiofire12_E",
    "FireworksARM"
};

FirmwareUtil::FirmwareUtil(FireWorks::Device& p)
: m_Parent(p)
{

    struct dat_list datlists[4] =
    {
            { FW_VENDORID_ECHO, AUDIOFIRE2,    0x04010000, 1, Af2Dats },
            { FW_VENDORID_ECHO, AUDIOFIRE4,    0x04010000, 1, Af4Dats },
            { FW_VENDORID_ECHO, AUDIOFIRE8,    0x04010000, 4, Af8Dats },
            { FW_VENDORID_ECHO, AUDIOFIRE12,    0x04010000, 4, Af12Dats }
    };

    assert(sizeof(datlists) <= sizeof(m_datlists));
    memset(&m_datlists, 0, sizeof(m_datlists));
    memcpy(&m_datlists, &datlists, sizeof(datlists));
}

FirmwareUtil::~FirmwareUtil()
{
}

bool
FirmwareUtil::isValidForDevice(Firmware f)
{
    std::string src = f.getSourceString();

    uint32_t vendor = m_Parent.getConfigRom().getNodeVendorId();
    uint32_t model = m_Parent.getConfigRom().getModelId();

    for (unsigned int i=0; i<ECHO_FIRMWARE_NUM_BOXTYPES; i++) {
        if(m_datlists[i].boxtype == model
           && m_datlists[i].vendorid == vendor)
        {
            for(int j=0; j<m_datlists[i].count; j++) {
                std::string cmpstring = m_datlists[i].filenames[j];
                cmpstring += ".dat";
                std::string::size_type loc = src.find( cmpstring, 0 );
                if( loc != std::string::npos ) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, "found filename\n");
                    return true;
                    break;
                 }
            }
        }
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "file not for this device\n");
    return false;
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
FirmwareUtil::writeFirmwareToDevice(Firmware f)
{
    uint32_t start_addr = f.getAddress();
    uint32_t writelen = f.getWriteDataLen();
    uint32_t buff[writelen * 4];
    if (!f.getWriteData(buff)) {
        debugError("Could not prepare data for writing to the device\n");
        return false;
    }
    if(!m_Parent.writeFlash(start_addr, writelen, buff)) {
        debugError("Writing to flash failed.\n");
        return false;
    }
    return true;
}

bool
FirmwareUtil::eraseBlocks(uint32_t start_address, unsigned int nb_quads)
{
    return m_Parent.eraseFlashBlocks(start_address, nb_quads);
}

void
FirmwareUtil::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "FirmwareUtil\n");
}

} // FireWorks
