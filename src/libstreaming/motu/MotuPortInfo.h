/*
 * Copyright (C) 2005-2007 by Jonathan Woithe
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

#ifndef __FFADO_MOTUPORTINFO__
#define __FFADO_MOTUPORTINFO__

#include "debugmodule/debugmodule.h"
#include <string>

namespace Streaming {
/*!
\brief Class containing the stream information for a Motu channel

 Contains the information that enables the decoding routine to find
 this port's data in the ISO events

*/
class MotuPortInfo {

public:
    /**
     * Sometimes a channel can have multiple formats, depending on the
     * device configuration (e.g. an SPDIF port could be plain audio in 24bit integer
     * or AC3 passthrough in IEC compliant frames.)
     *
     * This kind of enum allows to discriminate these formats when decoding
     * If all channels always have the same format, you won't be needing this
     */
//    enum E_Formats {
//        E_MBLA,     // Multibit linear audio
//        E_Midi,     // MIDI
//    };

    /**
     * Initialize Motu portinfo
     * should not be called directly, is inherited by motu ports
     *
     * the position parameter is an example
     * the name parameter is mandatory
     *
     * @param position Start position of port's data in iso event
     * @param format Format of data in iso event
     * @param size Size in bits of port's data in iso event
     * @return
     */
    MotuPortInfo( int position, int size)
      : m_position(position), m_size(size)
    {};
    virtual ~MotuPortInfo() {};


    int getPosition()     {return m_position;};
    int getSize()         {return m_size;};

protected:

    int m_position;
    int m_size;

};

} // end of namespace Streaming

#endif /* __FFADO_MOTUPORTINFO__ */
