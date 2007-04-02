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
#ifndef __FREEBOB_SYSTEMTIMESOURCE__
#define __FREEBOB_SYSTEMTIMESOURCE__

#include "../debugmodule/debugmodule.h"
#include "TimeSource.h"

namespace Util {

class SystemTimeSource 
    : public Util::TimeSource
{

public:

	SystemTimeSource();
	virtual ~SystemTimeSource();
        
    // implement the TimeSource interface
    freebob_microsecs_t getCurrentTime();
    freebob_microsecs_t getCurrentTimeAsUsecs();
    inline freebob_microsecs_t unWrapTime(freebob_microsecs_t t) {return t;};
    inline freebob_microsecs_t wrapTime(freebob_microsecs_t t) {return t;};

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace Util

#endif /* __FREEBOB_SYSTEMTIMESOURCE__ */


