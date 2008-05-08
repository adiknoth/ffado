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
#include "efc_cmds_mixer.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace FireWorks {

EfcGenericMixerCmd::EfcGenericMixerCmd(enum eMixerTarget target, 
                                       enum eMixerCommand command)
    : EfcCmd()
    , m_channel ( -1 )
    , m_value ( 0 )
{
    m_type=eCT_Get;
    m_target=target;
    m_command=command;
    setTarget(target);
    setCommand(command);
    setType(eCT_Get);
}

EfcGenericMixerCmd::EfcGenericMixerCmd(enum eMixerTarget target, 
                                       enum eMixerCommand command,
                                       int channel)
    : EfcCmd()
    , m_channel ( channel )
    , m_value ( 0 )
{
    m_type=eCT_Get;
    m_target=target;
    m_command=command;
    setTarget(target);
    setCommand(command);
    setType(eCT_Get);
}

bool
EfcGenericMixerCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    if (m_type == eCT_Get) {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS+1;

        result &= EfcCmd::serialize ( se );
        
        result &= se.write(htonl(m_channel), "Channel" );
        
    } else {
        // the length should be specified before
        // the header is serialized
        m_length=EFC_HEADER_LENGTH_QUADLETS+2;

        result &= EfcCmd::serialize ( se );
        
        result &= se.write(htonl(m_channel), "Channel" );
        result &= se.write(htonl(m_value), "Value" );
    }

    if(!result) {
        debugWarning("Serialization failed\n");
    }

    return result;
}

bool
EfcGenericMixerCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    if (m_type == eCT_Get) {
        EFC_DESERIALIZE_AND_SWAP(de, (quadlet_t *)&m_channel, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_value, result);
    }

    if(!result) {
        debugWarning("Deserialization failed\n");
    }

    return result;
}

bool
EfcGenericMixerCmd::setType( enum eCmdType type )
{
    m_type=type;
    if (type == eCT_Get) {
        switch (m_command) {
            case eMC_Gain:
                m_command_id=EFC_CMD_MIXER_GET_GAIN;
                break;
            case eMC_Solo:
                m_command_id=EFC_CMD_MIXER_GET_SOLO;
                break;
            case eMC_Mute:
                m_command_id=EFC_CMD_MIXER_GET_MUTE;
                break;
            case eMC_Pan:
                m_command_id=EFC_CMD_MIXER_GET_PAN;
                break;
            case eMC_Nominal:
                m_command_id=EFC_CMD_MIXER_GET_NOMINAL;
                break;
            default:
                debugError("Invalid mixer get command: %d\n", m_command);
                return false;
        }
    } else {
        switch (m_command) {
            case eMC_Gain:
                m_command_id=EFC_CMD_MIXER_SET_GAIN;
                break;
            case eMC_Solo:
                m_command_id=EFC_CMD_MIXER_SET_SOLO;
                break;
            case eMC_Mute:
                m_command_id=EFC_CMD_MIXER_SET_MUTE;
                break;
            case eMC_Pan:
                m_command_id=EFC_CMD_MIXER_SET_PAN;
                break;
            case eMC_Nominal:
                m_command_id=EFC_CMD_MIXER_SET_NOMINAL;
                break;
            default:
                debugError("Invalid mixer set command: %d\n", m_command);
                return false;
        }
    }
    return true;
}

bool
EfcGenericMixerCmd::setTarget( enum eMixerTarget target )
{
    m_target=target;
    switch (target) {
        case eMT_PhysicalOutputMix:
            m_category_id=EFC_CAT_PHYSICAL_OUTPUT_MIX;
            break;
        case eMT_PhysicalInputMix:
            m_category_id=EFC_CAT_PHYSICAL_INPUT_MIX;
            break;
        case eMT_PlaybackMix:
            m_category_id=EFC_CAT_PLAYBACK_MIX;
            break;
        case eMT_RecordMix:
            m_category_id=EFC_CAT_RECORD_MIX;
            break;
        default:
            debugError("Invalid mixer target: %d\n", target);
            return false;
    }
    return true;
}

bool
EfcGenericMixerCmd::setCommand( enum eMixerCommand command )
{
    m_command=command;
    if (m_type == eCT_Get) {
        switch (command) {
            case eMC_Gain:
                m_command_id=EFC_CMD_MIXER_GET_GAIN;
                break;
            case eMC_Solo:
                m_command_id=EFC_CMD_MIXER_GET_SOLO;
                break;
            case eMC_Mute:
                m_command_id=EFC_CMD_MIXER_GET_MUTE;
                break;
            case eMC_Pan:
                m_command_id=EFC_CMD_MIXER_GET_PAN;
                break;
            case eMC_Nominal:
                m_command_id=EFC_CMD_MIXER_GET_NOMINAL;
                break;
            default:
                debugError("Invalid mixer get command: %d\n", command);
                return false;
        }
    } else {
        switch (command) {
            case eMC_Gain:
                m_command_id=EFC_CMD_MIXER_SET_GAIN;
                break;
            case eMC_Solo:
                m_command_id=EFC_CMD_MIXER_SET_SOLO;
                break;
            case eMC_Mute:
                m_command_id=EFC_CMD_MIXER_SET_MUTE;
                break;
            case eMC_Pan:
                m_command_id=EFC_CMD_MIXER_SET_PAN;
                break;
            case eMC_Nominal:
                m_command_id=EFC_CMD_MIXER_SET_NOMINAL;
                break;
            default:
                debugError("Invalid mixer set command: %d\n", command);
                return false;
        }
    }
    return true;
}


void
EfcGenericMixerCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC %s %s %s:\n",
                                    (m_type==eCT_Get?"GET":"SET"),
                                    eMixerTargetToString(m_target),
                                    eMixerCommandToString(m_command));
    debugOutput(DEBUG_LEVEL_NORMAL, " Channel     : %ld\n", m_channel);
    debugOutput(DEBUG_LEVEL_NORMAL, " Value       : %lu\n", m_value);
}

// --- The specific commands



} // namespace FireWorks
