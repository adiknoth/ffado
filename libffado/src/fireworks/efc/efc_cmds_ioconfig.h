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

#ifndef FIREWORKS_EFC_CMD_IOCONFIG_H
#define FIREWORKS_EFC_CMD_IOCONFIG_H

#include "efc_cmd.h"

namespace FireWorks {

class EfcGenericIOConfigCmd : public EfcCmd
{
public:
    EfcGenericIOConfigCmd(enum eIOConfigRegister r);
    virtual ~EfcGenericIOConfigCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual void showEfcCmd();
    
    bool setType( enum eCmdType type );
    enum eCmdType getType() {return m_type;};
    bool setRegister( enum eIOConfigRegister r );
    enum eIOConfigRegister getRegister() {return m_reg;};
    
    virtual const char* getCmdName() const
        { return "EfcGenericIOConfigCmd"; }

    uint32_t    m_value;

private:
    enum eCmdType           m_type;
    enum eIOConfigRegister  m_reg;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMD_IOCONFIG_H
