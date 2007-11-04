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
#include "efc_cmds_hardware.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace FireWorks {

IMPL_DEBUG_MODULE( EfcCmd, EfcCmd, DEBUG_LEVEL_NORMAL );

// static int to keep track of the sequence index
uint32_t EfcCmd::m_seqnum = 1;

// some generic string generation functions
const char *eMixerTargetToString(const enum eMixerTarget target) {
    switch (target) {
        case eMT_PhysicalOutputMix:
            return "PhysicalOutputMix";
        case eMT_PhysicalInputMix:
            return "PhysicalInputMix";
        case eMT_PlaybackMix:
            return "PlaybackMix";
        case eMT_RecordMix:
            return "RecordMix";
        default:
            return "invalid";
    }
}

const char *eMixerCommandToString(const enum eMixerCommand command) {
    switch (command) {
        case eMC_Gain:
            return "Gain";
        case eMC_Solo:
            return "Solo";
        case eMC_Mute:
            return "Mute";
        case eMC_Pan:
            return "Pan";
        case eMC_Nominal:
            return "Nominal";
        default:
            return "invalid";
    }
}

const char *eIOConfigRegisterToString(const enum eIOConfigRegister reg) {
    switch (reg) {
        case eCR_Mirror:
            return "Mirror";
        case eCR_DigitalMode:
            return "DigitalMode";
        case eCR_Phantom:
            return "Phantom";
        default:
            return "invalid";
    }
}

// the real deal

EfcCmd::EfcCmd(uint32_t cat, uint32_t cmd)
    : m_length ( 0 )
    , m_category_id ( cat )
    , m_command_id ( cmd )
{
    memset(&m_header,0,sizeof(m_header));
}

EfcCmd::EfcCmd()
    : m_length ( 0 )
    , m_category_id ( EFC_CAT_INVALID )
    , m_command_id ( EFC_CMD_INVALID )
{
    memset(&m_header,0,sizeof(m_header));
}

EfcCmd::~EfcCmd()
{
}

bool
EfcCmd::serialize( Util::IOSSerialize& se )
{
    bool result=true;
    
    result &= se.write(htonl(m_length), "EFC length");
    
    unsigned int i=0;
    
    // assign the category and the command
    m_header.category=m_category_id;
    m_header.command=m_command_id;
    
    // set the sequence number
    m_header.seqnum=m_seqnum++;
    
    // serialize the header
    quadlet_t *header_as_quadlets=(quadlet_t *)&m_header;
    result &= se.write(htonl(*(header_as_quadlets+i)), "EFC header version"); i++;
    result &= se.write(htonl(*(header_as_quadlets+i)), "EFC header seqnum"); i++;
    result &= se.write(htonl(*(header_as_quadlets+i)), "EFC header category"); i++;
    result &= se.write(htonl(*(header_as_quadlets+i)), "EFC header command"); i++;
    result &= se.write(htonl(*(header_as_quadlets+i)), "EFC header return value"); i++;
    
    return result;
}

bool
EfcCmd::deserialize( Util::IISDeserialize& de )
{
    bool result=true;
    
    result &= de.read(&m_length);
    m_length=ntohl(m_length);
    
    // read the EFC header
    quadlet_t *header_as_quadlets=(quadlet_t *)&m_header;
    for (unsigned int i=0; i<sizeof(m_header)/4; i++) {
        result &= de.read((header_as_quadlets+i));
        *(header_as_quadlets+i)=ntohl(*(header_as_quadlets+i));
    }

    // check the EFC version
    if(m_header.version > 1) {
        debugError("Unsupported EFC version: %d\n", m_header.version);
        return false;
    }

    // check whether the category and command of the response are valid
    if (m_header.category != m_category_id) {
        debugError("Invalid category response: %d != %d", m_header.category, m_category_id);
        return false;
    }
    if (m_header.command != m_command_id) {
        debugError("Invalid command response: %d != %d", m_header.command, m_command_id);
        return false;
    }
    return result;
}

void 
EfcCmd::showEfcCmd()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Length: %d\n", m_length);
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Header:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Version         : 0x%08X\n", m_header.version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Sequence number : 0x%08X\n", m_header.seqnum);
    debugOutput(DEBUG_LEVEL_NORMAL, " Category        : 0x%08X\n", m_header.category);
    debugOutput(DEBUG_LEVEL_NORMAL, " Command         : 0x%08X\n", m_header.command);
    debugOutput(DEBUG_LEVEL_NORMAL, " Return Value    : 0x%08X\n", m_header.retval);
}

} // namespace FireWorks
