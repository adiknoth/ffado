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

#include "yamaha_cmd.h"

#include "libutil/ByteSwap.h"
#include <iostream>

using namespace std;
using namespace AVC;

namespace BeBoB {
namespace Yamaha {

YamahaVendorDependentCmd::YamahaVendorDependentCmd(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_subfunction ( 0x00 )
{
    /* company id is different between commands */
    m_companyId = 0x000000;
}

bool
YamahaVendorDependentCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_subfunction,"YamahaVendorDependentCmd subfunction");

    return result;
}

bool
YamahaVendorDependentCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read(&m_subfunction);


    return result;
}

//---------

YamahaSyncStateCmd::YamahaSyncStateCmd(Ieee1394Service& ieee1394service)
    : YamahaVendorDependentCmd( ieee1394service )
{
	m_companyId = 0x010203;
    m_subfunction = 0x21;
    m_syncstate = 0x00;
}

bool
YamahaSyncStateCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= YamahaVendorDependentCmd::serialize( se );
    result &= se.write(m_syncstate,"YamahaSyncStateCmd m_syncstate");
    return result;
}

bool
YamahaSyncStateCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= YamahaVendorDependentCmd::deserialize( de );
    result &= de.read(&m_syncstate);
    return result;
}

//---------

YamahaDigInDetectCmd::YamahaDigInDetectCmd(Ieee1394Service& ieee1394service)
    : YamahaVendorDependentCmd( ieee1394service )
{
	m_companyId = 0x0007f5;
    m_subfunction = 0x00;
    m_digin = 0x00;
}

bool
YamahaDigInDetectCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    uint8_t buf[] = {0x00, 0x01};

    result &= YamahaVendorDependentCmd::serialize( se );
    result &= se.write(buf[0], "YamahaDigInDetectCmd Unknown 1");
    result &= se.write(buf[1], "YamahaDigInDetectCmd Unknown 2");
    result &= se.write(m_digin, "YamahaDigInDetectCmd m_digin");

    return result;
}

bool
YamahaDigInDetectCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    uint8_t tmp;
    bool result = true;

    result &= YamahaVendorDependentCmd::deserialize( de );
    result &= de.read(&tmp);
    result &= de.read(&tmp);
    result &= de.read(&m_digin);

    return result;
}

}
}
