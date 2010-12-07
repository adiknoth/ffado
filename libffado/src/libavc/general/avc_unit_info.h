/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef AVCUNITINFO_H
#define AVCUNITINFO_H

#include "avc_generic.h"

#define AVC1394_CMD_UNIT_INFO 0x30

namespace AVC {


class UnitInfoCmd: public AVCCommand
{
public:
    UnitInfoCmd( Ieee1394Service& ieee1349service );
    virtual ~UnitInfoCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "UnitInfoCmd"; }

    reserved_t  m_reserved;
    unit_type_t m_unit_type;
    unit_t      m_unit;

    company_id_t m_company_id;
};

}

#endif // AVCUNITINFO_H
