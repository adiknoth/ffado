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

FFADODevice::FFADODevice( std::auto_ptr<ConfigRom>( configRom ))
    : Control::Container()
    , m_pConfigRom( configRom )
{
    addOption(Util::OptionContainer::Option("id",std::string("dev?")));

    std::ostringstream nodestr;
    nodestr << "node" << getConfigRom().getNodeId();

    if (!addElement(&getConfigRom())) {
        debugWarning("failed to add ConfigRom to Control::Container\n");
    }
}

FFADODevice::~FFADODevice()
{
    if (!deleteElement(&getConfigRom())) {
        debugWarning("failed to remove ConfigRom from Control::Container\n");
    }
}

FFADODevice *
FFADODevice::createDevice(std::auto_ptr<ConfigRom>( x ))
{
    // re-implement this!!
    assert(0);
    return NULL;
}

std::string
FFADODevice::getName()
{
    return getConfigRom().getGuidString();
}

int
FFADODevice::getNodeId()
{
    return getConfigRom().getNodeId();
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

Ieee1394Service&
FFADODevice::get1394Service()
{
    return getConfigRom().get1394Service();
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

enum FFADODevice::eSyncState
FFADODevice::getSyncState( ) {
    return eSS_Unknown;
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
    return retval;
}

void
FFADODevice::handleBusReset()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Handle bus reset...\n");
    
    // update the config rom node id
    sleep(1);
    getConfigRom().setVerboseLevel(getDebugLevel());
    getConfigRom().updatedNodeId();

}

void
FFADODevice::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
    setDebugLevel(l);
    getConfigRom().setVerboseLevel(l);
}

void
FFADODevice::showDevice()
{
    Ieee1394Service& s = getConfigRom().get1394Service();
    debugOutput(DEBUG_LEVEL_NORMAL, "Attached to port.......: %d (%s)\n",
                                    s.getPort(), s.getPortName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Node...................: %d\n", getNodeId());
    debugOutput(DEBUG_LEVEL_NORMAL, "GUID...................: %s\n",
                                    getConfigRom().getGuidString().c_str());

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

const char *
FFADODevice::ClockSourceTypeToString(enum eClockSourceType t)
{
    switch(t) {
        default:            return "Erratic type      ";
        case eCT_Invalid:   return "Invalid           ";
        case eCT_Internal:  return "Internal          ";
        case eCT_1394Bus:   return "1394 Bus          ";
        case eCT_SytMatch:  return "Compound Syt Match";
        case eCT_SytStream: return "Sync Syt Match    ";
        case eCT_WordClock: return "WordClock         ";
        case eCT_SPDIF:     return "SPDIF             ";
        case eCT_ADAT:      return "ADAT              ";
        case eCT_TDIF:      return "TDIF              ";
        case eCT_AES:       return "AES               ";
    }
}
