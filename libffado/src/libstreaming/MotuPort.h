/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
#ifndef __FREEBOB_MOTUPORT__
#define __FREEBOB_MOTUPORT__

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

#endif /* __FREEBOB_MOTUPORT__ */

