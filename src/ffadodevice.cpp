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

IMPL_DEBUG_MODULE( FFADODevice, FFADODevice, DEBUG_LEVEL_VERBOSE );

FFADODevice::FFADODevice( std::auto_ptr< ConfigRom >( configRom ),
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
FFADODevice::getConfigRom() const
{
    return *m_pConfigRom;
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
    if (retval) {
        setOscBase(idstr.str());
    }
    return retval;
}

void
FFADODevice::setVerboseLevel(int l)
{
    m_verboseLevel=l;
    setDebugLevel(l);
//     m_pConfigRom->setVerboseLevel(l);
    m_p1394Service->setVerboseLevel(l);
}

bool
FFADODevice::enableStreaming() {
    return true;
}

bool
FFADODevice::disableStreaming() {
    return true;
}


int
convertESamplingFrequency(ESamplingFrequency freq)
{
    int value = 0;

    switch ( freq ) {
    case eSF_22050Hz:
        value = 22050;
        break;
    case eSF_24000Hz:
        value = 24000;
        break;
    case eSF_32000Hz:
        value = 32000;
        break;
    case eSF_44100Hz:
        value = 44100;
        break;
    case eSF_48000Hz:
        value = 48000;
        break;
    case eSF_88200Hz:
        value = 88200;
        break;
    case eSF_96000Hz:
        value = 96000;
        break;
    case eSF_176400Hz:
        value = 176400;
        break;
    case eSF_192000Hz:
        value = 192000;
        break;
    case eSF_AnyLow:
        value = 48000;
        break;
    case eSF_AnyMid:
        value = 96000;
        break;
    case eSF_AnyHigh:
        value = 192000;
        break;
    default:
        value = 0;
    }


    return value;
}

ESamplingFrequency
parseSampleRate( int sampleRate )
{
    ESamplingFrequency efreq;
    switch ( sampleRate ) {
    case 22050:
        efreq = eSF_22050Hz;
        break;
    case 24000:
        efreq = eSF_24000Hz;
        break;
    case 32000:
        efreq = eSF_32000Hz;
        break;
    case 44100:
        efreq = eSF_44100Hz;
        break;
    case 48000:
        efreq = eSF_48000Hz;
        break;
    case 88200:
        efreq = eSF_88200Hz;
        break;
    case 96000:
        efreq = eSF_96000Hz;
        break;
    case 176400:
        efreq = eSF_176400Hz;
        break;
    case 192000:
        efreq = eSF_192000Hz;
        break;
    default:
        efreq = eSF_DontCare;
    }

    return efreq;
}

std::ostream& operator<<( std::ostream& stream, ESamplingFrequency samplingFrequency )
{
    char* str;
    switch ( samplingFrequency ) {
    case eSF_22050Hz:
        str = "22050";
        break;
    case eSF_24000Hz:
        str = "24000";
        break;
    case eSF_32000Hz:
        str = "32000";
        break;
    case eSF_44100Hz:
        str = "44100";
        break;
    case eSF_48000Hz:
        str = "48000";
        break;
    case eSF_88200Hz:
        str = "88200";
        break;
    case eSF_96000Hz:
        str = "96000";
        break;
    case eSF_176400Hz:
        str = "176400";
        break;
    case eSF_192000Hz:
        str = "192000";
        break;
    case eSF_DontCare:
    default:
        str = "unknown";
    }
    return stream << str;
};
