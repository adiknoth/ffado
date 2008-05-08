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

#include "focusrite_cmd.h"

#include "libutil/ByteSwap.h"
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
FocusriteVendorDependentCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_arg1,"FocusriteVendorDependentCmd arg1");
    result &= se.write(m_arg2,"FocusriteVendorDependentCmd arg2");
    // FIXME: this is not consistent, we should not have to care about CondSwapFromBus32 here
    result &= se.write(CondSwapToBus32(m_id),"FocusriteVendorDependentCmd ID");
    result &= se.write(CondSwapToBus32(m_value),"FocusriteVendorDependentCmd value");
    
    return result;
}

bool
FocusriteVendorDependentCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read(&m_arg1);
    result &= de.read(&m_arg2);
    result &= de.read(&m_id);
    m_id=CondSwapFromBus32(m_id);
    result &= de.read(&m_value);
    m_value=CondSwapFromBus32(m_value);

    return result;
}

}
}

