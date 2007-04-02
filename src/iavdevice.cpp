/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#include "iavdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include <iostream>
#include <sstream>

IMPL_DEBUG_MODULE( IAvDevice, IAvDevice, DEBUG_LEVEL_VERBOSE );

IAvDevice::IAvDevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    : OscNode()
    , m_pConfigRom( configRom )
    , m_p1394Service( &ieee1394service )
    , m_verboseLevel( DEBUG_LEVEL_NORMAL )
    , m_nodeId ( nodeId )
{
    addOption(Util::OptionContainer::Option("id",std::string("dev?")));

    std::ostringstream nodestr;
    nodestr << "node" << nodeId;
    setOscBase(nodestr.str());
    ConfigRom& c = getConfigRom();

    addChildOscNode(&c);
}


ConfigRom&
IAvDevice::getConfigRom() const
{
    return *m_pConfigRom;
}

bool
IAvDevice::setId( unsigned int id)
{
    // FIXME: decent ID system nescessary
    std::ostringstream idstr;
    idstr << "dev" << id;
    debugOutput( DEBUG_LEVEL_VERBOSE, "Set id to %s...\n", idstr.str().c_str());

    return setOption("id",idstr.str());
}

void
IAvDevice::setVerboseLevel(int l)
{
    m_verboseLevel=l;
    setDebugLevel(l);
//     m_pConfigRom->setVerboseLevel(l);
    m_p1394Service->setVerboseLevel(l);
}

bool
IAvDevice::enableStreaming() {
    return true;
}

bool
IAvDevice::disableStreaming() {
    return true;
}
