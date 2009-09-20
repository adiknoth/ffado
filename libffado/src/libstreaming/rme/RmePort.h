/*
 * Copyright (C) 2005-2008 by Jonathan Woithe
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#ifndef __FFADO_RMEPORT__
#define __FFADO_RMEPORT__

/**
 * This file implements the ports used in Rme devices
 */

#include "RmePortInfo.h"
#include "../generic/Port.h"

#include "debugmodule/debugmodule.h"

namespace Streaming {

/*!
\brief The Base Class for Rme Audio Port


*/
class RmeAudioPort
    : public AudioPort, public RmePortInfo
{

public:

    RmeAudioPort(PortManager &m,
                 std::string name,
                 enum E_Direction direction,
                 int position,
                 int size)
    : AudioPort(m, name, direction),
      RmePortInfo( position, size) // TODO: add more port information parameters here if nescessary
    {};

    virtual ~RmeAudioPort() {};
};

/*!
\brief The Base Class for an RME Midi Port


*/
class RmeMidiPort
    : public MidiPort, public RmePortInfo
{

public:

    RmeMidiPort(PortManager &m,
                std::string name,
                enum E_Direction direction,
                int position)
        : MidiPort(m, name, direction),
          RmePortInfo(position, 0)  // TODO: add more port information parameters here if nescessary
    {};

    virtual ~RmeMidiPort() {};
};

} // end of namespace Streaming

#endif /* __FFADO_RMEPORT__ */

