/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#include "avc_vendor_dependent_cmd.h"
#include "../util/avc_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
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
VendorDependentCmd::serialize( IOSSerialize& se )
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
VendorDependentCmd::deserialize( IISDeserialize& de )
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
