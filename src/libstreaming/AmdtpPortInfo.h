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
#ifndef __FREEBOB_AMDTPPORTINFO__
#define __FREEBOB_AMDTPPORTINFO__

#include "../debugmodule/debugmodule.h"
#include <string>

namespace FreebobStreaming {
/*!
\brief Class containing the stream information for an AMDTP channel

 Contains the information that maps the port to an AMDTP stream position (i.e. channel)
 this allows the AMDTP stream demultiplexer to find the channel associated 
 to this port.
 
*/
class AmdtpPortInfo {

public:
	enum E_Formats {
		E_MBLA,
		E_Midi,
		E_SPDIF,
	};
	enum E_Types {

	};

	AmdtpPortInfo(std::string name, int position, int location, enum E_Formats format, int type)
	  : m_name(name), m_position(position), m_location(location), m_format(format), m_type(type)
	{};
	virtual ~AmdtpPortInfo() {};


	std::string getName() {return m_name;};
	int getLocation()     {return m_location;};
	int getPosition()     {return m_position;};
	enum E_Formats getFormat()       {return m_format;};
	int getType()         {return m_type;};

protected:
    std::string m_name;

    int m_position;
    int m_location;
    enum E_Formats m_format;
    int m_type;

};

} // end of namespace FreebobStreaming

#endif /* __FREEBOB_AMDTPPORTINFO__ */


