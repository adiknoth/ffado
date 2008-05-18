/*
 * Copyright (C) 2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "edirol_fa101.h"

namespace BeBoB {
namespace Edirol {

EdirolFa101Device::EdirolFa101Device( DeviceManager& d,
                                      std::auto_ptr<ConfigRom>( configRom ))
    : BeBoB::AvDevice( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Edirol::EdirolFa101Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    if (AVC::AVCCommand::getSleepAfterAVCCommand() < 500) {
        AVC::AVCCommand::setSleepAfterAVCCommand( 500 );
    }
}

EdirolFa101Device::~EdirolFa101Device()
{
}

void
EdirolFa101Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a BeBoB::EdirolFa101::EdirolFa101Device\n");
    BeBoB::AvDevice::showDevice();
}

}
}
