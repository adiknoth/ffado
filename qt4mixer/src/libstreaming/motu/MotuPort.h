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

#ifndef __FFADO_MOTUPORT__
#define __FFADO_MOTUPORT__

/**
 * This file implements the ports used in Motu devices
 */

#include "MotuPortInfo.h"
#include "../generic/Port.h"

#include "debugmodule/debugmodule.h"

namespace Streaming {

/*!
\brief The Base Class for Motu Audio Port


*/
class MotuAudioPort
    : public AudioPort, public MotuPortInfo
{

public:

    MotuAudioPort(PortManager &m,
                  std::string name,
                  enum E_Direction direction,
                  int position,
                  int size)
    : AudioPort(m, name, direction),
      MotuPortInfo( position, size) // TODO: add more port information parameters here if nescessary
    {};

    virtual ~MotuAudioPort() {};
};

/*!
\brief The Base Class for an Motu Midi Port


*/
class MotuMidiPort
    : public MidiPort, public MotuPortInfo
{

public:

    MotuMidiPort(PortManager &m,
                 std::string name,
                 enum E_Direction direction,
                 int position)
        : MidiPort(m, name, direction),
          MotuPortInfo(position, 0)  // TODO: add more port information parameters here if nescessary
    {};

    virtual ~MotuMidiPort() {};
};

/*!
\brief The Base Class for an Motu Control Port


*/
class MotuControlPort
    : public ControlPort, public MotuPortInfo
{

public:

    MotuControlPort(PortManager &m,
                    std::string name,
                    enum E_Direction direction,
                    int position)
        : ControlPort(m, name, direction),
          MotuPortInfo(position, 2) // TODO: add more port information parameters here if nescessary
    {};

    virtual ~MotuControlPort() {};
};

} // end of namespace Streaming

#endif /* __FFADO_MOTUPORT__ */

