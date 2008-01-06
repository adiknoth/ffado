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

#ifndef FIREWORKS_EFC_CMD_FLASH_H
#define FIREWORKS_EFC_CMD_FLASH_H

#include "efc_cmd.h"
// #define EFC_CMD_FLASH_ERASE             0
// #define EFC_CMD_FLASH_READ              1
// #define EFC_CMD_FLASH_WRITE             2
// #define EFC_CMD_FLASH_GET_STATUS        3
// #define EFC_CMD_FLASH_GET_SESSION_BASE  4
// #define EFC_CMD_FLASH_LOCK              5

#define EFC_FLASH_SIZE_BYTES            256
#define EFC_FLASH_SIZE_QUADS            (EFC_FLASH_SIZE_BYTES / 4)

namespace FireWorks {

class EfcFlashEraseCmd : public EfcCmd
{
public:
    EfcFlashEraseCmd();
    virtual ~EfcFlashEraseCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcFlashEraseCmd"; }

    virtual void showEfcCmd();

    uint32_t    m_address;
};

class EfcFlashReadCmd : public EfcCmd
{
public:
    EfcFlashReadCmd();
    virtual ~EfcFlashReadCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcFlashReadCmd"; }

    virtual void showEfcCmd();

    uint32_t    m_address;
    uint32_t    m_nb_quadlets;
    uint32_t    m_data[EFC_FLASH_SIZE_QUADS];
};

class EfcFlashWriteCmd : public EfcCmd
{
public:
    EfcFlashWriteCmd();
    virtual ~EfcFlashWriteCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcFlashWriteCmd"; }

    virtual void showEfcCmd();

    uint32_t    m_address;
    uint32_t    m_nb_quadlets;
    uint32_t    m_data[EFC_FLASH_SIZE_QUADS];
};

class EfcFlashLockCmd : public EfcCmd
{
public:
    EfcFlashLockCmd();
    virtual ~EfcFlashLockCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcFlashLockCmd"; }

    virtual void showEfcCmd();

    bool     m_lock;
};

class EfcFlashGetStatusCmd : public EfcCmd
{
public:
    EfcFlashGetStatusCmd();
    virtual ~EfcFlashGetStatusCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcFlashGetStatusCmd"; }

    virtual void showEfcCmd();

    bool     m_ready;
};

class EfcFlashGetSessionBaseCmd : public EfcCmd
{
public:
    EfcFlashGetSessionBaseCmd();
    virtual ~EfcFlashGetSessionBaseCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcFlashGetSessionBaseCmd"; }

    virtual void showEfcCmd();

    uint32_t    m_address;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_FLASH_H
