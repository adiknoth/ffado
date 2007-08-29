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

#include "ffadodevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include <iostream>
#include <sstream>

#include <assert.h>

IMPL_DEBUG_MODULE( FFADODevice, FFADODevice, DEBUG_LEVEL_NORMAL );

FFADODevice::FFADODevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId )
    : Control::Container()
    , m_pConfigRom( configRom )
    , m_p1394Service( &ieee1394service )
    , m_nodeId ( nodeId )
{
    addOption(Util::OptionContainer::Option("id",std::string("dev?")));

    std::ostringstream nodestr;
    nodestr << "node" << nodeId;
//     setOscBase(nodestr.str());
    ConfigRom& c = getConfigRom();

//     addChildOscNode(&c);
}

std::string
FFADODevice::getName()
{
    return getConfigRom().getGuidString();
}

bool FFADODevice::compareGUID( FFADODevice *a, FFADODevice *b ) {
    assert(a);
    assert(b);
    return ConfigRom::compareGUID(a->getConfigRom(), b->getConfigRom());
}

ConfigRom&
FFADODevice::getConfigRom() const
{
    return *m_pConfigRom;
}

bool
FFADODevice::loadFromCache()
{
    return false;
}

bool
FFADODevice::saveCache()
{
    return false;
}

bool
FFADODevice::setId( unsigned int id)
{
    bool retval;
    // FIXME: decent ID system nescessary
    std::ostringstream idstr;
    idstr << "dev" << id;
    debugOutput( DEBUG_LEVEL_VERBOSE, "Set id to %s...\n", idstr.str().c_str());

    retval=setOption("id",idstr.str());
/*    if (retval) {
        setOscBase(idstr.str());
    }*/
    return retval;
}

void
FFADODevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
    setDebugLevel(l);
}

void
FFADODevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Node...........: %d\n", getNodeId());
    debugOutput(DEBUG_LEVEL_NORMAL, "GUID...........: %s\n", getConfigRom().getGuidString().c_str());
    
    std::string id=std::string("dev? [none]");
    getOption("id", id);
     
    debugOutput(DEBUG_LEVEL_NORMAL, "Assigned ID....: %s\n", id.c_str());

    flushDebugOutput();
}


bool
FFADODevice::enableStreaming() {
    return true;
}

bool
FFADODevice::disableStreaming() {
    return true;
}
