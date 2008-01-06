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
EfcOverAVCCmd::serialize( Util::Cmd::IOSSerialize& se )
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

    if(!result) {
        debugWarning("Serialization failed\n");
    }

    return result;
}

bool
EfcOverAVCCmd::deserialize( Util::Cmd::IISDeserialize& de )
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
    
    if(!result) {
        debugWarning("Deserialization failed\n");
    }
    
    return result;
}

} // namespace FireWorks
