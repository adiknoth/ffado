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

#ifndef FOCUSRITEVENDORDEPENDENT_H
#define FOCUSRITEVENDORDEPENDENT_H

#include "libavc/general/avc_generic.h"
#include "libavc/util/avc_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

#include <libavc1394/avc1394.h>

namespace BeBoB {

class FocusriteVendorDependentCmd: public AVC::VendorDependentCmd
{
public:
    FocusriteVendorDependentCmd(Ieee1394Service& ieee1394service);
    virtual ~FocusriteVendorDependentCmd();

    virtual bool serialize( AVC::IOSSerialize& se );
    virtual bool deserialize( AVC::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "FocusriteVendorDependentCmd"; }
    
    byte_t m_arg1;
    byte_t m_arg2;
    
    quadlet_t m_id;
    quadlet_t m_value;
    
};

}

#endif // FOCUSRITEVENDORDEPENDENT_H
