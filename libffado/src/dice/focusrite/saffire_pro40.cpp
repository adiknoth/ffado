/*
 * Copyright (C) 2009 by Pieter Palmers
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

#include "saffire_pro40.h"

#include "focusrite_eap.h"

namespace Dice {
namespace Focusrite {

SaffirePro40::SaffirePro40( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ))
    : Dice::Device( d , configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Dice::Focusrite::SaffirePro40 (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

SaffirePro40::~SaffirePro40()
{
}

void
SaffirePro40::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Dice::Focusrite::SaffirePro40\n");
    Dice::Device::showDevice();
}

bool SaffirePro40::setNickName(std::string name) {
    return getEAP()->writeRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name.c_str(), name.size());
}
std::string SaffirePro40::getNickName() {
    char name[16];
    getEAP()->readRegBlock(Dice::EAP::eRT_Application, 0x44, (quadlet_t*)name, 16);
    return std::string(name);
}

}
}
// vim: et
