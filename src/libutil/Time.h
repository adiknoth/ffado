/*
Time.h
FreeBob Streaming API
FreeBob = Firewire (pro-)audio for linux

http://freebob.sf.net

Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
	
Based upon JackTime.h from the jackdmp package.
Original Copyright:

Copyright (C) 2001-2003 Paul Davis
Copyright (C) 2004-2006 Grame

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software 
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef __Time__
#define __Time__

#include <stdio.h>
#include <inttypes.h>

// #define GETCYCLE_TIME

/**
 * Type used to represent the value of free running
 * monotonic clock with units of microseconds.
 */

typedef uint64_t freebob_microsecs_t;


#ifdef __cplusplus
extern "C"
{
#endif


#include <unistd.h>

    static inline void FreebobSleep(long usec) {
        usleep(usec);
    }

#ifdef GETCYCLE_TIME
	#include "cycles.h"
    extern freebob_microsecs_t __freebob_cpu_mhz;
    extern freebob_microsecs_t GetMhz();
    extern void InitTime();
    static inline freebob_microsecs_t GetMicroSeconds (void) {
        return get_cycles() / __freebob_cpu_mhz;
    }
#else
	#include <time.h>
    extern void InitTime();
    static inline freebob_microsecs_t GetMicroSeconds (void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (freebob_microsecs_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    }
#endif


#ifdef __cplusplus
}
#endif


#endif



