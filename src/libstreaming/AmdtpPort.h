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
#ifndef __FREEBOB_AMDTPPORT__
#define __FREEBOB_AMDTPPORT__

/**
 * This file implements the AMDTP ports as used in the BeBoB's
 */

#include "../debugmodule/debugmodule.h"
#include "Port.h"
#include "AmdtpPortInfo.h"

namespace FreebobStreaming {

class AmdtpAudioPort 
	: public AudioPort, public AmdtpPortInfo
{

public:

	AmdtpAudioPort(std::string name, 
		           enum E_DataType datatype,
		           enum E_BufferType buffertype, 
		           unsigned int buffsize,
		           int position, 
		           int location, 
		           enum E_Formats format, 
		           int type)
	: AudioPort(name, datatype,	buffertype, buffsize),
	  AmdtpPortInfo(name, position, location, format, type)
	{};

	AmdtpAudioPort(std::string name, 
		           enum E_DataType datatype,
		           enum E_BufferType buffertype, 
		           unsigned int buffsize,
	               void *externalbuffer,
		           int position, 
		           int location, 
		           enum E_Formats format, 
		           int type)
	: AudioPort(name, datatype,	buffertype, buffsize, externalbuffer),
	  AmdtpPortInfo(name, position, location, format, type)
	{};

	virtual ~AmdtpAudioPort() {};

protected:
    DECLARE_DEBUG_MODULE;

};

class AmdtpMidiPort 
	: public MidiPort, public AmdtpPortInfo
{

public:

	AmdtpMidiPort(std::string name, 
		           unsigned int buffsize,
		           int position, 
		           int location, 
		           enum E_Formats format, 
		           int type)
	: MidiPort(name, buffsize),
	  AmdtpPortInfo(name, position, location, format, type)
	{};

	virtual ~AmdtpMidiPort() {};

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace FreebobStreaming

#endif /* __FREEBOB_AMDTPPORT__ */


