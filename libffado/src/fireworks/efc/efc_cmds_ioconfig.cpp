/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace FireWorks {

EfcGenericIOConfigCmd::EfcGenericIOConfigCmd(enum eIOConfigRegister r)
    : EfcCmd()
    , m_value ( 0 )
    , m_reg ( r )
{
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

        result &= se.write(htonl(m_value), "Value" );
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
            case eCR_DigitalMode:
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
            case eCR_DigitalMode:
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
            case eCR_DigitalMode:
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
            case eCR_DigitalMode:
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
    debugOutput(DEBUG_LEVEL_NORMAL, " Value       : %lu\n", m_value);
}

// --- The specific commands



} // namespace FireWorks
