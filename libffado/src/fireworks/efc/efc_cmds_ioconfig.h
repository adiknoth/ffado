/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef FIREWORKS_EFC_CMD_IOCONFIG_H
#define FIREWORKS_EFC_CMD_IOCONFIG_H

#include "efc_cmd.h"

namespace FireWorks {

#define EFC_MAX_ISOC_MAP_ENTRIES    32
typedef struct tag_efc_isoc_map
{
    uint32_t    samplerate;
    uint32_t    flags;

    uint32_t    num_playmap_entries;
    uint32_t    num_phys_out;
    uint32_t     playmap[ EFC_MAX_ISOC_MAP_ENTRIES ];

    uint32_t    num_recmap_entries;
    uint32_t    num_phys_in;
    uint32_t     recmap[ EFC_MAX_ISOC_MAP_ENTRIES ];
} IsoChannelMap;

class EfcGenericIOConfigCmd : public EfcCmd
{
public:
    EfcGenericIOConfigCmd(enum eIOConfigRegister r);
    virtual ~EfcGenericIOConfigCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual void showEfcCmd();
    
    bool setType( enum eCmdType type );
    enum eCmdType getType() {return m_type;};
    bool setRegister( enum eIOConfigRegister r );
    enum eIOConfigRegister getRegister() {return m_reg;};
    
    virtual const char* getCmdName() const
        { return "EfcGenericIOConfigCmd"; };

    uint32_t   m_value;

private:
    enum eCmdType           m_type;
    enum eIOConfigRegister  m_reg;
};

class EfcIsocMapIOConfigCmd : public EfcCmd
{
public:
    EfcIsocMapIOConfigCmd();
    virtual ~EfcIsocMapIOConfigCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual void showEfcCmd();

    bool setType( enum eCmdType type );
    enum eCmdType getType() {return m_type;};

    virtual const char* getCmdName() const
        { return "EfcIsocMapIOConfigCmd"; };

    uint32_t    m_samplerate;
    uint32_t    m_flags;

    uint32_t    m_num_playmap_entries;
    uint32_t    m_num_phys_out;
    uint32_t     m_playmap[ EFC_MAX_ISOC_MAP_ENTRIES ];

    uint32_t    m_num_recmap_entries;
    uint32_t    m_num_phys_in;
    uint32_t     m_recmap[ EFC_MAX_ISOC_MAP_ENTRIES ];

private:
    enum eCmdType           m_type;
    enum eIOConfigRegister  m_reg;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_IOCONFIG_H
