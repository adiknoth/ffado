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

#include "focusrite_cmd.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;
using namespace AVC;

namespace BeBoB {
namespace Focusrite {

FocusriteVendorDependentCmd::FocusriteVendorDependentCmd(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_arg1 ( 0x03 )
    , m_arg2 ( 0x01 )
    , m_id ( 0x00000000 )
    , m_value ( 0x00000000 )
{
    m_companyId=0x00130e;
}

FocusriteVendorDependentCmd::~FocusriteVendorDependentCmd()
{
}

bool
FocusriteVendorDependentCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_arg1,"FocusriteVendorDependentCmd arg1");
    result &= se.write(m_arg2,"FocusriteVendorDependentCmd arg2");
    // FIXME: this is not consistent, we should not have to care about ntohl here
    result &= se.write(htonl(m_id),"FocusriteVendorDependentCmd ID");
    result &= se.write(htonl(m_value),"FocusriteVendorDependentCmd value");
    
    return result;
}

bool
FocusriteVendorDependentCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read(&m_arg1);
    result &= de.read(&m_arg2);
    result &= de.read(&m_id);
    m_id=ntohl(m_id);
    result &= de.read(&m_value);
    m_value=ntohl(m_value);

    return result;
}

}
}

