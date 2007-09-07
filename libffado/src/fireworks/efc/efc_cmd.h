/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef FIREWORKS_EFC_CMD_H
#define FIREWORKS_EFC_CMD_H

#include "debugmodule/debugmodule.h"

#include "libutil/cmd_serialize.h"

#define EFC_CAT_INVALID                 0xFFFFFFFF
#define EFC_CMD_INVALID                 0xFFFFFFFF

// Categories
#define	EFC_CAT_HARDWARE_INFO           0
#define EFC_CAT_FLASH                   1
#define EFC_CAT_TRANSPORT               2
#define EFC_CAT_HARDWARE_CONTROL        3
#define EFC_CAT_PHYSICAL_OUTPUT_MIX     4
#define EFC_CAT_PHYSICAL_INPUT_MIX      5
#define EFC_CAT_PLAYBACK_MIX            6
#define EFC_CAT_RECORD_MIX              7
#define EFC_CAT_MONITOR_MIX             8
#define EFC_CAT_IO_CONFIG               9
#define EFC_CAT_COUNT                   10

// size of the header
#define EFC_HEADER_LENGTH_QUADLETS      ((sizeof(uint32_t) + sizeof(struct EfcCmd::efc_header))/4)

// util macro to do deserialization and byteswap
#define EFC_DESERIALIZE_AND_SWAP(__de__, __value__, __result__) \
    { __result__ &= __de__.read(__value__); \
      *(__value__)=ntohl(*(__value__)); } \


namespace FireWorks {

class EfcCmd
{
public:
    enum EfcReturnValue {
        eERV_Ok             = 0,
        eERV_Bad            = 1,
        eERV_BadCommand     = 2,
        eERV_CommErr        = 3,
        eERV_BadQuadCount   = 4,
        eERV_Unsupported    = 5,
        eERV_1394Timeout    = 6,
        eERV_DspTimeout     = 7,
        eERV_BadRate        = 8,
        eERV_BadClock       = 9,
        eERV_BadChannel     = 10,
        eERV_BadPan         = 11,
        eERV_FlashBusy      = 12,
        eERV_BadMirror      = 13,
        eERV_BadLed         = 14,
        eERV_BadParameter   = 15,
        eERV_Incomplete     = 0x80000000L,
        eERV_Unspecified    = 0xFFFFFFFFL,
    };

    typedef struct efc_header
    {
        uint32_t    version;
        uint32_t    seqnum;
        uint32_t    category;
        uint32_t    command;
        uint32_t    retval;
    };

protected: // this HAS to be overloaded
    EfcCmd(uint32_t cat, uint32_t cmd);

public:
    virtual ~EfcCmd();
    
    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const = 0;

    virtual void showEfcCmd();

    uint32_t            m_length; // in quadlets, including length field and header.
    struct efc_header   m_header;

private:
    uint32_t            m_category_id;
    uint32_t            m_command_id;

public:
    static uint32_t     m_seqnum;
protected:
    DECLARE_DEBUG_MODULE;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_H
