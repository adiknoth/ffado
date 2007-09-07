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

#include "efc_avc_cmd.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;
using namespace AVC;

namespace FireWorks {

EfcOverAVCCmd::EfcOverAVCCmd(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_dummy_1 ( 0 )
    , m_dummy_2 ( 0 )
    , m_cmd( NULL )
{
    m_companyId=0x0;
}

EfcOverAVCCmd::~EfcOverAVCCmd()
{
}

bool
EfcOverAVCCmd::serialize( Util::IOSSerialize& se )
{
    if (m_cmd==NULL) {
        debugError("no child EFC command");
        return false;
    }
    bool result=true;
    result &= VendorDependentCmd::serialize( se );
    
    result &= se.write(m_dummy_1, "Dummy byte 1");
    result &= se.write(m_dummy_2, "Dummy byte 1");

    result &= m_cmd->serialize( se );

    return result;
}

bool
EfcOverAVCCmd::deserialize( Util::IISDeserialize& de )
{
    if (m_cmd==NULL) {
        debugError("no child EFC command");
        return false;
    }
    bool result=true;
    result &= VendorDependentCmd::deserialize( de );
    
    result &= de.read(&m_dummy_1);
    result &= de.read(&m_dummy_2);

    result &= m_cmd->deserialize( de );
    
    return result;
}

} // namespace FireWorks
