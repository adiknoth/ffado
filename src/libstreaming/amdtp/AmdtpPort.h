/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#ifndef __FFADO_AMDTPPORT__
#define __FFADO_AMDTPPORT__

/**
 * This file implements the AMDTP ports as used in the BeBoB's
 */

#include "debugmodule/debugmodule.h"
#include "../generic/Port.h"
#include "AmdtpPortInfo.h"

namespace Streaming {

/*!
\brief The Base Class for an AMDTP Audio Port

 The AMDTP/AM824/IEC61883-6 port that represents audio.

*/
class AmdtpAudioPort
    : public AudioPort, public AmdtpPortInfo
{

public:

    AmdtpAudioPort(std::string name,
                       enum E_Direction direction,
                   int position,
                   int location,
                   enum E_Formats format)
    : AudioPort(name, direction),
      AmdtpPortInfo(position, location, format)
    {};

    virtual ~AmdtpAudioPort() {};

protected:

};

/*!
\brief The Base Class for an AMDTP Midi Port

 The AMDTP/AM824/IEC61883-6 port that represents midi.

*/
class AmdtpMidiPort
    : public MidiPort, public AmdtpPortInfo
{

public:

    AmdtpMidiPort(std::string name,
                       enum E_Direction direction,
                   int position,
                   int location,
                   enum E_Formats format)
        : MidiPort(name, direction),
          AmdtpPortInfo(position, location, format)
    {};


    virtual ~AmdtpMidiPort() {};

protected:

};

} // end of namespace Streaming

#endif /* __FFADO_AMDTPPORT__ */
