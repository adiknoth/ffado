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

#ifndef __FFADO_MOTUPORT__
#define __FFADO_MOTUPORT__

/**
 * This file implements the ports used in Motu devices
 */

#include "../debugmodule/debugmodule.h"
#include "Port.h"
#include "MotuPortInfo.h"

namespace Streaming {

/*!
\brief The Base Class for Motu Audio Port


*/
class MotuAudioPort
    : public AudioPort, public MotuPortInfo
{

public:

    MotuAudioPort(std::string name,
                       enum E_Direction direction,
                   int position,
                   int size)
    : AudioPort(name, direction),
      MotuPortInfo(name, position, size) // TODO: add more port information parameters here if nescessary
    {};

    virtual ~MotuAudioPort() {};

protected:

};

/*!
\brief The Base Class for an Motu Midi Port


*/
class MotuMidiPort
    : public MidiPort, public MotuPortInfo
{

public:

    MotuMidiPort(std::string name,
                       enum E_Direction direction,
                   int position)
        : MidiPort(name, direction),
          MotuPortInfo(name, position, 0)  // TODO: add more port information parameters here if nescessary
    {};


    virtual ~MotuMidiPort() {};

protected:

};

/*!
\brief The Base Class for an Motu Control Port


*/
class MotuControlPort
    : public ControlPort, public MotuPortInfo
{

public:

    MotuControlPort(std::string name,
                       enum E_Direction direction,
                   int position)
        : ControlPort(name, direction),
          MotuPortInfo(name, position, 2) // TODO: add more port information parameters here if nescessary
    {};


    virtual ~MotuControlPort() {};

protected:

};

} // end of namespace Streaming

#endif /* __FFADO_MOTUPORT__ */

