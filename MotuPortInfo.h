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
#ifndef __FREEBOB_MOTUPORTINFO__
#define __FREEBOB_MOTUPORTINFO__

#include "../debugmodule/debugmodule.h"
#include <string>

namespace FreebobStreaming {
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
     /*
	enum E_Formats {
		E_MBLA, // Multibit linear audio, is acutally AMDTP. Change if needed
		E_Midi,
		E_SPDIF,
	};
    */
    
	/**
	 * Initialize Motu portinfo
	 * should not be called directly, is inherited by motu ports
	 *
	 * the position parameter is an example
	 * the name parameter is mandatory
	 *
	 * @param name 
	 * @param position example parameter
	 * @return 
	 */
	MotuPortInfo(std::string name, int position)
	  : m_name(name), m_position(position)
	{};
	virtual ~MotuPortInfo() {};


	std::string getName() {return m_name;};
	int getPosition()     {return m_position;};

protected:
    std::string m_name;

    int m_position;

};

} // end of namespace FreebobStreaming

#endif /* __FREEBOB_MOTUPORTINFO__ */


