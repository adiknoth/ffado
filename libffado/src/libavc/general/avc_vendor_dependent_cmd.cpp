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

#include "avc_vendor_dependent_cmd.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/ByteSwap.h"
#include <iostream>

using namespace std;

#define AVC1394_CMD_VENDOR_DEPENDENT 0x00

namespace AVC {

VendorDependentCmd::VendorDependentCmd(Ieee1394Service& ieee1394service)
    : AVCCommand( ieee1394service, AVC1394_CMD_VENDOR_DEPENDENT )
{
}

VendorDependentCmd::~VendorDependentCmd()
{
}

bool
VendorDependentCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= AVCCommand::serialize( se );
    byte_t tmp;
    tmp=(m_companyId >> 16) & 0xFF;
    result &= se.write(tmp,"VendorDependentCmd companyid[2]");
    tmp=(m_companyId >> 8) & 0xFF;
    result &= se.write(tmp,"VendorDependentCmd companyid[1]");
    tmp=(m_companyId) & 0xFF;
    result &= se.write(tmp,"VendorDependentCmd companyid[0]");
    return result;
}

bool
VendorDependentCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= AVCCommand::deserialize( de );
    
    byte_t tmp[3];
    result &= de.read(&tmp[2]);
    result &= de.read(&tmp[1]);
    result &= de.read(&tmp[0]);
    
    m_companyId = (tmp[2] << 16) | (tmp[1]<<8) | tmp[0];
    
    return result;
}

}
