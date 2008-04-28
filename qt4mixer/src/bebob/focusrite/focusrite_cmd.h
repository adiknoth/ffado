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

#ifndef FOCUSRITEVENDORDEPENDENT_H
#define FOCUSRITEVENDORDEPENDENT_H

#include "libavc/general/avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

#include <libavc1394/avc1394.h>

namespace BeBoB {
namespace Focusrite {

class FocusriteVendorDependentCmd: public AVC::VendorDependentCmd
{
public:
    FocusriteVendorDependentCmd(Ieee1394Service& ieee1394service);
    virtual ~FocusriteVendorDependentCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "FocusriteVendorDependentCmd"; }
    
    byte_t m_arg1;
    byte_t m_arg2;
    
    quadlet_t m_id;
    quadlet_t m_value;
    
};

}
}

#endif // FOCUSRITEVENDORDEPENDENT_H
