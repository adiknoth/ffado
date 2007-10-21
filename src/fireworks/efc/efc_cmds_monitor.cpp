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

#include "efc_cmd.h"
#include "efc_cmds_monitor.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace FireWorks {

const char *eMonitorCommandToString(const enum eMonitorCommand command) {
    switch (command) {
        case eMoC_Gain:
            return "eMC_Gain";
        case eMoC_Solo:
            return "eMoC_Solo";
        case eMoC_Mute:
            return "eMoC_Mute";
        case eMoC_Pan:
            return "eMoC_Pan";
        default:
            return "invalid";
    }
}

EfcGenericMonitorCmd::EfcGenericMonitorCmd(enum eCmdType type, 
                                           enum eMonitorCommand command)
    : EfcCmd()
    , m_input ( -1 )
    , m_output ( -1 )
    , m_value ( 0 )
    , m_type ( type )
    , m_command ( command )
{
    m_category_id=EFC_CAT_MONITOR_MIX;
    if (type == eCT_Get) {
        switch (command) {
            case eMoC_Gain:
                m_command_id=EFC_CMD_MIXER_GET_GAIN;
                break;
            case eMoC_Solo:
                m_command_id=EFC_CMD_MIXER_GET_SOLO;
                break;
            case eMoC_Mute:
                m_command_id=EFC_CMD_MIXER_GET_MUTE;
                break;
            case eMoC_Pan:
                m_command_id=EFC_CMD_MIXER_GET_PAN;
                break;
            default:
                debugError("Invalid mixer get command: %d\n", command);
        }
    } else {
        switch (command) {
            case eMoC_Gain:
                m_command_id=EFC_CMD_MIXER_SET_GAIN;
                break;
            case eMoC_Solo:
                m_command_id=EFC_CMD_MIXER_SET_SOLO;
                break;
            case eMoC_Mute:
                m_command_id=EFC_CMD_MIXER_SET_MUTE;
                break;
            case eMoC_Pan:
                m_command_id=EFC_CMD_MIXER_SET_PAN;
                break;
            default:
                debugError("Invalid mixer set command: %d\n", command);
        }
    }
}

bool
EfcGenericMonitorCmd::serialize( Util::IOSSerialize& se )
{
    bool result=true;

    if (m_type == eCT_Get) {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS+2;

        result &= EfcCmd::serialize ( se );
        
        result &= se.write(htonl(m_input), "Input" );
        result &= se.write(htonl(m_output), "Output" );
        
    } else {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS+3;

        result &= EfcCmd::serialize ( se );
        
        result &= se.write(htonl(m_input), "Input" );
        result &= se.write(htonl(m_output), "Output" );
        result &= se.write(htonl(m_value), "Value" );
    }
    return result;
}

bool
EfcGenericMonitorCmd::deserialize( Util::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    if (m_type == eCT_Get) {
        EFC_DESERIALIZE_AND_SWAP(de, (quadlet_t *)&m_input, result);
        EFC_DESERIALIZE_AND_SWAP(de, (quadlet_t *)&m_output, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_value, result);
    }

    if(!result) {
        debugWarning("Deserialization failed\n");
    }

    return result;
}

void
EfcGenericMonitorCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC %s MONITOR %s:\n",
                                    (m_type==eCT_Get?"GET":"SET"),
                                    eMonitorCommandToString(m_command));
    debugOutput(DEBUG_LEVEL_NORMAL, " Input       : %ld\n", m_input);
    debugOutput(DEBUG_LEVEL_NORMAL, " Output      : %ld\n", m_output);
    debugOutput(DEBUG_LEVEL_NORMAL, " Value       : %lu\n", m_value);
}

// --- The specific commands



} // namespace FireWorks
