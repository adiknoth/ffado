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

#ifndef FIREWORKS_EFC_AVC_H
#define FIREWORKS_EFC_AVC_H

#include "libavc/general/avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

#include <libavc1394/avc1394.h>

#include "efc_cmd.h"

namespace FireWorks {

class EfcOverAVCCmd: public AVC::VendorDependentCmd
{
public:
    EfcOverAVCCmd(Ieee1394Service& ieee1394service);
    virtual ~EfcOverAVCCmd();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcOverAVCCmd"; }

    uint8_t     m_dummy_1;
    uint8_t     m_dummy_2;
    
    EfcCmd*     m_cmd;
};

} // namespace FireWorks

#endif // FIREWORKS_EFC_AVC_H
