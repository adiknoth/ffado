/*
 * Copyright (C) 2013      by Takashi Sakamoto
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

#ifndef YAMAHA_VENDOR_DEPENDENT_H
#define YAMAHA_VENDOR_DEPENDENT_H

#include "libavc/general/avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

namespace BeBoB {
namespace Yamaha {

class YamahaVendorDependentCmd: public AVC::VendorDependentCmd
{
public:
    YamahaVendorDependentCmd(Ieee1394Service& ieee1394service);
    virtual ~YamahaVendorDependentCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "YamahaVendorDependentCmd"; }

protected:
    byte_t m_subfunction;
};

class YamahaSyncStateCmd: public YamahaVendorDependentCmd
{
public:
    YamahaSyncStateCmd(Ieee1394Service& ieee1394service);
    virtual ~YamahaSyncStateCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "YamahaSyncStateCmd"; }

    byte_t m_syncstate;
};

class YamahaDigInDetectCmd: public YamahaVendorDependentCmd
{
public:
	YamahaDigInDetectCmd(Ieee1394Service& ieee1394service);
    virtual ~YamahaDigInDetectCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "YamahaDigInDetectCmd"; }

    byte_t m_digin;
};

}
}

#endif // YAMAHA_VENDOR_DEPENDENT_H
