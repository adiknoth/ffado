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
#include "efc_cmds_mixer.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace FireWorks {

const char *eMixerTargetToString(const enum eMixerTarget target) {
    switch (target) {
        case eMT_PhysicalOutputMix:
            return "eMT_PhysicalOutputMix";
        case eMT_PhysicalInputMix:
            return "eMT_PhysicalInputMix";
        case eMT_PlaybackMix:
            return "eMT_PlaybackMix";
        case eMT_RecordMix:
            return "eMT_RecordMix";
        default:
            return "invalid";
    }
}

const char *eMixerCommandToString(const enum eMixerCommand command) {
    switch (command) {
        case eMC_Gain:
            return "eMC_Gain";
        case eMC_Solo:
            return "eMC_Solo";
        case eMC_Mute:
            return "eMC_Mute";
        case eMC_Pan:
            return "eMC_Pan";
        case eMC_Nominal:
            return "eMC_Nominal";
        default:
            return "invalid";
    }
}

EfcGenericMixerCmd::EfcGenericMixerCmd(enum eCmdType type, 
                                       enum eMixerTarget target, 
                                       enum eMixerCommand command)
    : EfcCmd()
    , m_channel ( -1 )
    , m_value ( 0 )
    , m_type ( type )
    , m_target ( target )
    , m_command ( command )
{
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
    }

    if (type == eCT_Get) {
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
        }
    }
}

bool
EfcGenericMixerCmd::serialize( Util::IOSSerialize& se )
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
    return result;
}

bool
EfcGenericMixerCmd::deserialize( Util::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    EFC_DESERIALIZE_AND_SWAP(de, (quadlet_t *)&m_channel, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_value, result);

    return result;
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
