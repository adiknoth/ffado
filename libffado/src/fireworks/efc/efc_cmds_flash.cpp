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
#include "efc_cmds_flash.h"

#include <byteswap.h>
#include <iostream>

using namespace std;

namespace FireWorks {

EfcFlashEraseCmd::EfcFlashEraseCmd()
    : EfcCmd(EFC_CAT_FLASH, EFC_CMD_FLASH_ERASE)
    , m_address ( 0xFFFFFFFF )
{
}

bool
EfcFlashEraseCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS + 1;

    result &= EfcCmd::serialize ( se );
    result &= se.write(bswap_32(m_address), "Address" );

    return result;
}

bool
EfcFlashEraseCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    return result;
}

void
EfcFlashEraseCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Flash Erase:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Address     : %lu\n", m_address);
}

// ----
EfcFlashReadCmd::EfcFlashReadCmd()
    : EfcCmd(EFC_CAT_FLASH, EFC_CMD_FLASH_READ)
    , m_address ( 0xFFFFFFFF )
    , m_nb_quadlets ( 0 )
{
}

bool
EfcFlashReadCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS+2;

    result &= EfcCmd::serialize ( se );

    result &= se.write(bswap_32(m_address), "Address" );
    result &= se.write(bswap_32(m_nb_quadlets), "Length (quadlets)" );

    return result;
}

bool
EfcFlashReadCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );

    EFC_DESERIALIZE_AND_SWAP(de, &m_address, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_quadlets, result);
    if (m_nb_quadlets > EFC_FLASH_SIZE_QUADS) {
        debugError("Too much quadlets returned: %u\n", m_nb_quadlets);
        return false;
    }
    for (unsigned int i=0; i < m_nb_quadlets; i++) {
        EFC_DESERIALIZE_AND_SWAP(de, &m_data[i], result);
    }
    return result;
}

void
EfcFlashReadCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Flash Read:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Address           : %lu\n", m_address);
    debugOutput(DEBUG_LEVEL_NORMAL, " Length (quadlets) : %lu\n", m_nb_quadlets);
    debugOutput(DEBUG_LEVEL_NORMAL, " Data              : \n");
    for (unsigned int i=0; i < m_nb_quadlets; i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "                     %08X \n", m_data[i]);
    }
}

// ----
EfcFlashWriteCmd::EfcFlashWriteCmd()
    : EfcCmd(EFC_CAT_FLASH, EFC_CMD_FLASH_WRITE)
    , m_address ( 0xFFFFFFFF )
    , m_nb_quadlets ( 0 )
{
}

bool
EfcFlashWriteCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    if (m_nb_quadlets > EFC_FLASH_SIZE_QUADS) {
        debugError("Too much quadlets to write: %u\n", m_nb_quadlets);
        return false;
    }

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS+2+m_nb_quadlets;

    result &= EfcCmd::serialize ( se );

    result &= se.write(bswap_32(m_address), "Address" );
    result &= se.write(bswap_32(m_nb_quadlets), "Length (quadlets)" );

    for (unsigned int i=0; i < m_nb_quadlets; i++) {
        result &= se.write(bswap_32(m_data[i]), "Data");
    }
    return result;
}

bool
EfcFlashWriteCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= EfcCmd::deserialize ( de );
    return result;
}

void
EfcFlashWriteCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Flash Write:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Address           : %lu\n", m_address);
    debugOutput(DEBUG_LEVEL_NORMAL, " Length (quadlets) : %lu\n", m_nb_quadlets);
    debugOutput(DEBUG_LEVEL_NORMAL, " Data              : \n");
    for (unsigned int i=0; i < m_nb_quadlets; i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "                     %08X \n", m_data[i]);
    }
}

// ------------------

EfcFlashLockCmd::EfcFlashLockCmd()
    : EfcCmd(EFC_CAT_FLASH, EFC_CMD_FLASH_LOCK)
    , m_lock ( false )
{
}

bool
EfcFlashLockCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;

    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS + 1;

    result &= EfcCmd::serialize ( se );
    result &= se.write(bswap_32(m_lock), "Locked" );

    return result;
}

bool
EfcFlashLockCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= EfcCmd::deserialize ( de );
    //EFC_DESERIALIZE_AND_SWAP(de, &m_lock, result);
    return result;
}

void
EfcFlashLockCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Flash Lock:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Locked     : %s\n", (m_lock?"Yes":"No"));
}

// ------------------

EfcFlashGetStatusCmd::EfcFlashGetStatusCmd()
    : EfcCmd(EFC_CAT_FLASH, EFC_CMD_FLASH_GET_STATUS)
    , m_ready ( false )
{
}

bool
EfcFlashGetStatusCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;
    result &= EfcCmd::serialize ( se );
    return result;
}

bool
EfcFlashGetStatusCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= EfcCmd::deserialize ( de );
    m_ready = !(m_header.retval == eERV_FlashBusy);
    return result;
}

void
EfcFlashGetStatusCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Flash Get Status:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Ready?     : %s\n", (m_ready?"Yes":"No"));
}

// ------------------

EfcFlashGetSessionBaseCmd::EfcFlashGetSessionBaseCmd()
    : EfcCmd(EFC_CAT_FLASH, EFC_CMD_FLASH_GET_SESSION_BASE)
    , m_address ( false )
{
}

bool
EfcFlashGetSessionBaseCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;
    result &= EfcCmd::serialize ( se );
    return result;
}

bool
EfcFlashGetSessionBaseCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= EfcCmd::deserialize ( de );
    EFC_DESERIALIZE_AND_SWAP(de, &m_address, result);
    return result;
}

void
EfcFlashGetSessionBaseCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC Flash Get Session Base:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Address           : %lu\n", m_address);
}

} // namespace FireWorks
