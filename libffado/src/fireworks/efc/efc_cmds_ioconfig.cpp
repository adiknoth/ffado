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

#include "efc_cmd.h"
#include "efc_cmds_ioconfig.h"

#include "libutil/ByteSwap.h"
#include <iostream>
#include <cstring>

using namespace std;

namespace FireWorks {

EfcGenericIOConfigCmd::EfcGenericIOConfigCmd(enum eIOConfigRegister r)
    : EfcCmd()
    , m_value ( 0 )
    , m_reg ( r )
{
    m_category_id = EFC_CAT_IO_CONFIG;
    m_type=eCT_Get;
    setRegister(r);
}

bool
EfcGenericIOConfigCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    if (m_type == eCT_Get) {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS;

        result &= EfcCmd::serialize ( se );

    } else {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS+1;

        result &= EfcCmd::serialize ( se );

        result &= se.write(CondSwapToBus32(m_value), "Value" );
    }
    return result;
}

bool
EfcGenericIOConfigCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    if (m_type == eCT_Get) {
        EFC_DESERIALIZE_AND_SWAP(de, &m_value, result);
    }

    return result;
}

bool
EfcGenericIOConfigCmd::setType( enum eCmdType type )
{
    m_type=type;
    if (m_type == eCT_Get) {
        switch (m_reg) {
            case eCR_Mirror:
                m_command_id=EFC_CMD_IO_CONFIG_GET_MIRROR;
                break;
            case eCR_DigitalInterface:
                m_command_id=EFC_CMD_IO_CONFIG_GET_DIGITAL_MODE;
                break;
            case eCR_Phantom:
                m_command_id=EFC_CMD_IO_CONFIG_GET_PHANTOM;
                break;
            default:
                debugError("Invalid IOConfig get command: %d\n", m_reg);
                return false;
        }
    } else {
        switch (m_reg) {
            case eCR_Mirror:
                m_command_id=EFC_CMD_IO_CONFIG_SET_MIRROR;
                break;
            case eCR_DigitalInterface:
                m_command_id=EFC_CMD_IO_CONFIG_SET_DIGITAL_MODE;
                break;
            case eCR_Phantom:
                m_command_id=EFC_CMD_IO_CONFIG_SET_PHANTOM;
                break;
            default:
                debugError("Invalid IOConfig set command: %d\n", m_reg);
                return false;
        }
    }
    return true;
}
bool
EfcGenericIOConfigCmd::setRegister( enum eIOConfigRegister r )
{
    m_reg=r;
    if (m_type == eCT_Get) {
        switch (m_reg) {
            case eCR_Mirror:
                m_command_id=EFC_CMD_IO_CONFIG_GET_MIRROR;
                break;
            case eCR_DigitalInterface:
                m_command_id=EFC_CMD_IO_CONFIG_GET_DIGITAL_MODE;
                break;
            case eCR_Phantom:
                m_command_id=EFC_CMD_IO_CONFIG_GET_PHANTOM;
                break;
            default:
                debugError("Invalid IOConfig get command: %d\n", m_reg);
                return false;
        }
    } else {
        switch (m_reg) {
            case eCR_Mirror:
                m_command_id=EFC_CMD_IO_CONFIG_SET_MIRROR;
                break;
            case eCR_DigitalInterface:
                m_command_id=EFC_CMD_IO_CONFIG_SET_DIGITAL_MODE;
                break;
            case eCR_Phantom:
                m_command_id=EFC_CMD_IO_CONFIG_SET_PHANTOM;
                break;
            default:
                debugError("Invalid IOConfig set command: %d\n", m_reg);
                return false;
        }
    }
    return true;
}

void
EfcGenericIOConfigCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC IOCONFIG %s %s:\n",
                                    (m_type==eCT_Get?"GET":"SET"),
                                    eIOConfigRegisterToString(m_reg));
    debugOutput(DEBUG_LEVEL_NORMAL, " Value       : %u\n", m_value);
}

// --- The specific commands

EfcIsocMapIOConfigCmd::EfcIsocMapIOConfigCmd(void)
    : EfcCmd()
    , m_samplerate (0)
    , m_flags (0)
    , m_num_playmap_entries (0)
    , m_num_phys_out (0)
    , m_num_recmap_entries (0)
    , m_num_phys_in (0)
{
    m_category_id = EFC_CAT_IO_CONFIG;
    m_reg = eCR_IsocMap;
    m_type = eCT_Get;
    memset(m_playmap, 0, sizeof(m_playmap));
    memset(m_recmap, 0, sizeof(m_recmap));
}
bool
EfcIsocMapIOConfigCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    unsigned int i;

    assert((m_num_playmap_entries <= EFC_MAX_ISOC_MAP_ENTRIES)
        || (m_num_recmap_entries <= EFC_MAX_ISOC_MAP_ENTRIES));

    if (m_type == eCT_Get) {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS;

        result &= EfcCmd::serialize ( se );
    } else {
        // the length should be specified before
        // the header is serialized
        m_length = EFC_HEADER_LENGTH_QUADLETS + sizeof(IsoChannelMap);

        result &= EfcCmd::serialize ( se );

        result &= se.write(CondSwapToBus32(m_samplerate), "Samplerate" );
        result &= se.write(CondSwapToBus32(m_flags), "Flags" );

        result &= se.write(CondSwapToBus32(m_num_playmap_entries), "Num. of Entries for Play Map" );
        result &= se.write(CondSwapToBus32(m_num_phys_out), "Num. of Phys. Out" );
        for (i = 0; i < EFC_MAX_ISOC_MAP_ENTRIES; i++)
            result &= se.write(CondSwapToBus32(m_playmap[i]), "Play Map Entry" );

        result &= se.write(CondSwapToBus32(m_num_recmap_entries), "Num. of Entries for Rec Map" );
        result &= se.write(CondSwapToBus32(m_num_phys_in), "Num. of Phys. In" );
        for (i = 0; i < EFC_MAX_ISOC_MAP_ENTRIES; i++)
            result &= se.write(CondSwapToBus32(m_recmap[i]), "Rec Map Entry" );
    }

    return result;
}

bool
EfcIsocMapIOConfigCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    unsigned int i;

    result &= EfcCmd::deserialize ( de );

    if (m_type == eCT_Get) {
        EFC_DESERIALIZE_AND_SWAP(de, &m_samplerate, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_flags, result);

        EFC_DESERIALIZE_AND_SWAP(de, &m_num_playmap_entries, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_num_phys_out, result);
        for (i = 0; i < EFC_MAX_ISOC_MAP_ENTRIES; i++)
            EFC_DESERIALIZE_AND_SWAP(de, &m_playmap[i], result);

        EFC_DESERIALIZE_AND_SWAP(de, &m_num_recmap_entries, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_num_phys_in, result);
        for (i = 0; i < EFC_MAX_ISOC_MAP_ENTRIES; i++)
            EFC_DESERIALIZE_AND_SWAP(de, &m_recmap[i], result);
    }

    return result;
}

bool
EfcIsocMapIOConfigCmd::setType( enum eCmdType type )
{
    m_type=type;
    if (m_type == eCT_Get)
        m_command_id = EFC_CMD_IO_CONFIG_GET_ISOC_MAP;
    else
        m_command_id = EFC_CMD_IO_CONFIG_SET_ISOC_MAP;
    return true;
}

void
EfcIsocMapIOConfigCmd::showEfcCmd()
{
    unsigned int i;

    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC IOCONFIG %s %s:\n",
                                    (m_type==eCT_Get?"GET":"SET"),
                                    eIOConfigRegisterToString(m_reg));
    debugOutput(DEBUG_LEVEL_NORMAL, " Samplerate        : %u\n", m_samplerate);
    debugOutput(DEBUG_LEVEL_NORMAL, " Flags             : %u\n", m_flags);
    debugOutput(DEBUG_LEVEL_NORMAL, " Playback:");
    debugOutput(DEBUG_LEVEL_NORMAL, "  Num. of Entries  : %u\n", m_num_playmap_entries);
    debugOutput(DEBUG_LEVEL_NORMAL, "  Num. of Phys. Out: %u\n", m_num_phys_out);
    for (i = 0; i < EFC_MAX_ISOC_MAP_ENTRIES; i++)
        debugOutput(DEBUG_LEVEL_NORMAL, "  Entriy %02d        : %u\n", i, m_playmap[i]);
    debugOutput(DEBUG_LEVEL_NORMAL, " Record:");
    debugOutput(DEBUG_LEVEL_NORMAL, "  Num. of Entries  : %u\n", m_num_recmap_entries);
    debugOutput(DEBUG_LEVEL_NORMAL, "  Num. of Phys. In : %u\n", m_num_phys_in);
    for (i = 0; i < EFC_MAX_ISOC_MAP_ENTRIES; i++)
        debugOutput(DEBUG_LEVEL_NORMAL, "  Entriy %02d        : %u\n", i, m_recmap[i]);
}


} // namespace FireWorks
