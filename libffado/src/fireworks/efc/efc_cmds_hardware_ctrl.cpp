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
#include "efc_cmds_hardware_ctrl.h"

#include "libutil/ByteSwap.h"
#include <iostream>
#include <unistd.h>

using namespace std;

namespace FireWorks {

EfcGetClockCmd::EfcGetClockCmd()
    : EfcCmd(EFC_CAT_HARDWARE_CONTROL, EFC_CMD_HWCTRL_GET_CLOCK)
    , m_clock ( EFC_CMD_HW_CLOCK_UNSPECIFIED )
    , m_samplerate ( EFC_CMD_HW_CLOCK_UNSPECIFIED )
    , m_index ( 0 )
{
}

bool
EfcGetClockCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;

    result &= EfcCmd::serialize ( se );

    return result;
}

bool
EfcGetClockCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    EFC_DESERIALIZE_AND_SWAP(de, &m_clock, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_samplerate, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_index, result);

    return result;
}

void
EfcGetClockCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Get Clock:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Clock       : %u\n", m_clock);
    debugOutput(DEBUG_LEVEL_NORMAL, " Samplerate  : %u\n", m_samplerate);
    debugOutput(DEBUG_LEVEL_NORMAL, " Index       : %u\n", m_index);
}

// ----
EfcSetClockCmd::EfcSetClockCmd()
    : EfcCmd(EFC_CAT_HARDWARE_CONTROL, EFC_CMD_HWCTRL_SET_CLOCK)
    , m_clock ( EFC_CMD_HW_CLOCK_UNSPECIFIED )
    , m_samplerate ( EFC_CMD_HW_CLOCK_UNSPECIFIED )
    , m_index ( 0 )
{
}

bool
EfcSetClockCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS+3;

    result &= EfcCmd::serialize ( se );

    result &= se.write(CondSwapToBus32(m_clock), "Clock" );
    result &= se.write(CondSwapToBus32(m_samplerate), "Samplerate" );
    result &= se.write(CondSwapToBus32(m_index), "Index" );

    return result;
}

bool
EfcSetClockCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    EFC_DESERIALIZE_AND_SWAP(de, &m_clock, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_samplerate, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_index, result);

    /*
     * With firmware version 5.8, after setting, approximately 100ms needs to
     * get it effective. If getting command is executed during this period,
     * previous value is returned. This is a simple hack for this issue.
     */
    usleep(150000);

    return result;
}

void
EfcSetClockCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Set Clock:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Clock       : %u\n", m_clock);
    debugOutput(DEBUG_LEVEL_NORMAL, " Samplerate  : %u\n", m_samplerate);
    debugOutput(DEBUG_LEVEL_NORMAL, " Index       : %u\n", m_index);
}

// ----
EfcPhyReconnectCmd::EfcPhyReconnectCmd()
    : EfcCmd(EFC_CAT_HARDWARE_CONTROL, EFC_CMD_HWCTRL_RECONNECT_PHY)
{
}

bool
EfcPhyReconnectCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;

    result &= EfcCmd::serialize ( se );

    return result;
}

bool
EfcPhyReconnectCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    return result;
}

void
EfcPhyReconnectCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Phy Reconnect\n");
}

// -- get flags 
EfcGetFlagsCmd::EfcGetFlagsCmd()
    : EfcCmd(EFC_CAT_HARDWARE_CONTROL, EFC_CMD_HWCTRL_GET_FLAGS)
    , m_flags ( 0 )
{
}

bool
EfcGetFlagsCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;

    result &= EfcCmd::serialize ( se );

    return result;
}

bool
EfcGetFlagsCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    EFC_DESERIALIZE_AND_SWAP(de, &m_flags, result);

    return result;
}

void
EfcGetFlagsCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Get Flags:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Flags       : %08X\n", m_flags);
}

// ----
EfcChangeFlagsCmd::EfcChangeFlagsCmd()
    : EfcCmd(EFC_CAT_HARDWARE_CONTROL, EFC_CMD_HWCTRL_CHANGE_FLAGS)
    , m_setmask ( 0 )
    , m_clearmask ( 0 )
{
}

bool
EfcChangeFlagsCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS+2;

    result &= EfcCmd::serialize ( se );

    result &= se.write(CondSwapToBus32(m_setmask), "SetMask" );
    result &= se.write(CondSwapToBus32(m_clearmask), "ClearMask" );

    return result;
}

bool
EfcChangeFlagsCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= EfcCmd::deserialize ( de );
    return result;
}

void
EfcChangeFlagsCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Change flags:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Set mask     : %08X\n", m_setmask);
    debugOutput(DEBUG_LEVEL_NORMAL, " Clear mask   : %08X\n", m_clearmask);
}


// ----
EfcIdentifyCmd::EfcIdentifyCmd()
    : EfcCmd(EFC_CAT_HARDWARE_CONTROL, EFC_CMD_HWCTRL_IDENTIFY)
{
}

bool
EfcIdentifyCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;

    result &= EfcCmd::serialize ( se );

    return result;
}

bool
EfcIdentifyCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    return result;
}

void
EfcIdentifyCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Identify\n");
}

} // namespace FireWorks
