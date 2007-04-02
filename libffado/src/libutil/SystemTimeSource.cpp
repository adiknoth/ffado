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

#include "SystemTimeSource.h"
#include "Time.h"

namespace Util {

IMPL_DEBUG_MODULE( SystemTimeSource, SystemTimeSource, DEBUG_LEVEL_NORMAL );

SystemTimeSource::SystemTimeSource() {
//     InitTime();
}

SystemTimeSource::~SystemTimeSource() {

}
    
freebob_microsecs_t SystemTimeSource::getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
    
//     return GetMicroSeconds();
}

freebob_microsecs_t SystemTimeSource::getCurrentTimeAsUsecs() {
    return getCurrentTime();
}

} // end of namespace Util
