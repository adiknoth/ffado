/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#ifndef FIREWORKS_FIRMWARE_H
#define FIREWORKS_FIRMWARE_H

#include "debugmodule/debugmodule.h"

#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"
#include "efc/efc_cmds_flash.h"

#include <string>

class ConfigRom;
class Ieee1394Service;

namespace FireWorks {

#define ECHO_FIRMWARE_MAGIC_VALUES              {1651,1,0,0,0}
#define ECHO_FIRMWARE_NB_MAGIC_VALUES           5

#define ECHO_FIRMWARE_HEADER_LENGTH_BYTES       256
#define ECHO_FIRMWARE_HEADER_LENGTH_QUADLETS    (ECHO_FIRMWARE_HEADER_LENGTH_BYTES / 4)

#define ECHO_FIRMWARE_FILE_MAX_LENGTH_BYTES     (384*1024 + ECHO_FIRMWARE_HEADER_LENGTH_BYTES)
#define ECHO_FIRMWARE_FILE_MAX_LENGTH_QUADLETS  (ECHO_FIRMWARE_HEADER_LENGTH_BYTES / 4)

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

public:
    Firmware();
    virtual ~Firmware();

    virtual bool loadFile(std::string filename);

    virtual void show();

protected:
    // filename
    std::string         m_filename;

    // header data
    enum eDatType       m_Type;
    uint32_t            m_flash_offset_address;
    uint32_t            m_length_quads;
    uint32_t            m_CRC32;
    uint32_t            m_checksum;
    uint32_t            m_version;
    bool                m_append_crc; // true to append
    uint32_t            m_footprint_quads;

private:
    DECLARE_DEBUG_MODULE;
};

class FirmwareUtil
{

public:
    FirmwareUtil(FireWorks::Device& parent,
                 FireWorks::Firmware& f);
    virtual ~FirmwareUtil();

    virtual void show();

protected:
    FireWorks::Device&          m_Parent;
    FireWorks::Firmware&        m_Firmware;
private:
    DECLARE_DEBUG_MODULE;
};

} // namespace FireWorks

#endif
