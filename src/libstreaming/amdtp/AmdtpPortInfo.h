/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef __FFADO_AMDTPPORTINFO__
#define __FFADO_AMDTPPORTINFO__

#include "debugmodule/debugmodule.h"
#include <string>

namespace Streaming {
/*!
\brief Class containing the stream information for an AMDTP channel

 Contains the information that maps the port to an AMDTP stream position (i.e. channel)
 this allows the AMDTP stream demultiplexer to find the channel associated
 to this port.

*/
class AmdtpPortInfo {

public:
    /**
     * Sometimes a channel can have multiple formats, depending on the
     * device configuration (e.g. an SPDIF port could be plain audio in 24bit integer
     * or AC3 passthrough in IEC compliant frames.)
     *
     * This kind of enum allows to discriminate these formats when decoding
     * If all channels always have the same format, you won't be needing this
     */
    enum E_Formats {
        E_MBLA, ///< multibit linear audio
        E_Midi, ///< midi
        E_SPDIF,///< IEC.... format
    };

    AmdtpPortInfo( int position, int location, enum E_Formats format)
      : m_position(position), m_location(location), m_format(format)
    {};
    virtual ~AmdtpPortInfo() {};


    int getLocation()     {return m_location;};
    int getPosition()     {return m_position;};
    enum E_Formats getFormat()       {return m_format;};

protected:
    int m_position;
    int m_location;
    enum E_Formats m_format;

};

} // end of namespace Streaming

#endif /* __FFADO_AMDTPPORTINFO__ */
