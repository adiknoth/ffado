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

#ifndef FIREWORKS_FIRMWARE_H
#define FIREWORKS_FIRMWARE_H

#include "debugmodule/debugmodule.h"

#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"
#include "efc/efc_cmds_flash.h"

#include "IntelFlashMap.h"

#include <string>

class ConfigRom;
class Ieee1394Service;

namespace FireWorks {

#define ECHO_FIRMWARE_MAGIC                     "1651 1 0 0 0"
#define ECHO_FIRMWARE_MAGIC_LENGTH_BYTES        14

// the number of quadlets in the file
#define ECHO_FIRMWARE_HEADER_LENGTH_QUADLETS    64
// note that the dat files are not binary files but have the quadlets
// as "0x0ABCDE12\n" lines
#define ECHO_FIRMWARE_HEADER_LENGTH_BYTES       ( 12 * ECHO_FIRMWARE_HEADER_LENGTH_QUADLETS )

#define ECHO_FIRMWARE_FILE_MAX_LENGTH_QUADLETS  ((384 * 1024) / 4)
#define ECHO_FIRMWARE_FILE_MAX_LENGTH_BYTES     (ECHO_FIRMWARE_FILE_MAX_LENGTH_QUADLETS * 12 + ECHO_FIRMWARE_HEADER_LENGTH_BYTES)

#define ECHO_FIRMWARE_NUM_BOXTYPES  4

class Firmware
{
public:
    enum eDatType {
        eDT_DspCode         = 0,
        eDT_IceLynxCode     = 1,
        eDT_Data            = 2,
        eDT_FPGACode        = 3,
        eDT_DeviceName      = 4,
        eDT_Invalid         = 0xFF,
    };
    static const char *eDatTypeToString(const enum eDatType target);
    static const enum eDatType intToeDatType(int type);
public:
    Firmware();
    Firmware(const Firmware& f);
    virtual ~Firmware();
    Firmware& operator=(const Firmware& f);

    virtual bool loadFile(std::string filename);
    virtual bool loadFromMemory(uint32_t *data, uint32_t addr, uint32_t len);

    virtual bool isValid() {return m_valid;};

    virtual std::string getSourceString() {return m_source;};

    /**
     * @brief compare two firmwares
     * compares only the address, length and data content, not the other fields
     * @param x firmware to be compared to 'this'
     * @return true if equal
     */
    bool operator==(const Firmware& x);

    /**
     * @brief get base address of the firmware image
     * @return base address of the firmware image
     */
    virtual uint32_t getAddress() {return m_flash_offset_address;};
    /**
     * @brief get length of the firmware image (in quadlets)
     * @return length of the firmware image (in quadlets)
     */
    virtual uint32_t getLength() {return m_length_quads;};

    /**
     * @brief get length of data to be written to the device (in quadlets)
     * @return length (in quadlets)
     */
    virtual uint32_t getWriteDataLen();
    /**
     * @brief prepares data to be written to the device
     * @param buff buffer to copy data into. should be at least getWriteDataLen() quadlets.
     * @return true if successful
     */
    virtual bool getWriteData(uint32_t *buff);

    virtual void show();
    virtual void setVerboseLevel(int l)
        {setDebugLevel(l);};
    void dumpData();

protected:
    // filename
    std::string         m_source;

    // header data
    enum eDatType       m_Type;
    uint32_t            m_flash_offset_address;
    uint32_t            m_length_quads;
    uint32_t            m_CRC32;
    uint32_t            m_checksum;
    uint32_t            m_version;
    bool                m_append_crc; // true to append
    uint32_t            m_footprint_quads;

    std::string         m_magic;
    uint32_t            m_header[ECHO_FIRMWARE_HEADER_LENGTH_QUADLETS];
    uint32_t            *m_data;

    bool m_valid;

private:
    DECLARE_DEBUG_MODULE;
};

class FirmwareUtil
{

public:
    FirmwareUtil(FireWorks::Device& parent);
    virtual ~FirmwareUtil();

    virtual void show();
    virtual void setVerboseLevel(int l)
        {setDebugLevel(l);};
    
    /**
     * @brief reads firmware block from device
     * @param start start address
     * @param length number of quadlets to read
     * @return Firmware structure containing the read data
     */
    Firmware getFirmwareFromDevice(uint32_t start, uint32_t length);


    /**
     * @brief writes a firmware to the device
     * @param f firmware to write
     * @return true if successful
     */
    bool writeFirmwareToDevice(Firmware f);

    /**
     * @brief erases the flash memory starting at addr
     * @param address 
     * @param nb_quads 
     * @return true if successful, false otherwise
     */
    bool eraseBlocks(unsigned int address, unsigned int nb_quads);

    /**
     * @brief checks whether a firmware is valid for this device
     * @param f firmware to check
     * @return true if valid, false if not
     */
    bool isValidForDevice(Firmware f);

protected:
    FireWorks::Device&          m_Parent;

private:
    struct dat_list
    {
        uint32_t    vendorid;
        uint32_t    boxtype;
        uint32_t    minversion;
        int         count;
        const char  **filenames;
    };

    struct dat_list m_datlists[ECHO_FIRMWARE_NUM_BOXTYPES];

private:
    DECLARE_DEBUG_MODULE;
};

} // namespace FireWorks

#endif
