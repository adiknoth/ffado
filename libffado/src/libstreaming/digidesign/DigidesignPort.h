/*
 * Copyright (C) 2005-2008, 2011 by Jonathan Woithe
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

#ifndef __FFADO_DIGIDESIGNPORT__
#define __FFADO_DIGIDESIGNPORT__

/**
 * This file implements the ports used in Digidesign devices
 */

#include "DigidesignPortInfo.h"
#include "../generic/Port.h"

#include "debugmodule/debugmodule.h"

namespace Streaming {

/*!
\brief The Base Class for Digidesignu Audio Port


*/
class DigidesignAudioPort
    : public AudioPort, public DigidesignPortInfo
{

public:

    DigidesignAudioPort(PortManager &m,
                  std::string name,
                  enum E_Direction direction,
                  int position,
                  int size)
    : AudioPort(m, name, direction),
      DigidesignPortInfo( position, size) // TODO: add more port information parameters here if nescessary
    {};

    virtual ~DigidesignAudioPort() {};
};

/*!
\brief The Base Class for a Digidesign Midi Port


*/
class DigidesignMidiPort
    : public MidiPort, public DigidesignPortInfo
{

public:

    DigidesignMidiPort(PortManager &m,
                 std::string name,
                 enum E_Direction direction,
                 int position)
        : MidiPort(m, name, direction),
          DigidesignPortInfo(position, 0)  // TODO: add more port information parameters here if nescessary
    {};

    virtual ~DigidesignMidiPort() {};
};

} // end of namespace Streaming

#endif /* __FFADO_DIGIDESIGNPORT__ */
