/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "__vendor___device.h"

namespace BeBoB {
namespace __Vendor__ {

VendorDevice::VendorDevice( Ieee1394Service& ieee1394Service,
                            std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( ieee1394Service, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::__Vendor__::VendorDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

VendorDevice::~VendorDevice()
{
}

void
VendorDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a BeBoB::__Vendor__::VendorDevice\n");
    BeBoB::AvDevice::showDevice();
}

} // __Vendor__
} // BeBoB