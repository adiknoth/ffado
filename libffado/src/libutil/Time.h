/*
    Copyright (C) 2005-2007 by Pieter Palmers

    This file is part of FFADO
    FFADO = Free Firewire (pro-)audio drivers for linux

    FFADO is based upon FreeBoB.

    Based upon JackTime.h from the jackdmp package.
    Original Copyright:

    Copyright (C) 2001-2003 Paul Davis
    Copyright (C) 2004-2006 Grame

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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

typedef uint64_t ffado_microsecs_t;


#ifdef __cplusplus
extern "C"
{
#endif


#include <unistd.h>

    static inline void FFADOSleep(long usec) {
        usleep(usec);
    }

#ifdef GETCYCLE_TIME
    #include "cycles.h"
    extern ffado_microsecs_t __ffado_cpu_mhz;
    extern ffado_microsecs_t GetMhz();
    extern void InitTime();
    static inline ffado_microsecs_t GetMicroSeconds (void) {
        return get_cycles() / __ffado_cpu_mhz;
    }
#else
    #include <time.h>
    extern void InitTime();
    static inline ffado_microsecs_t GetMicroSeconds (void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (ffado_microsecs_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    }
#endif


#ifdef __cplusplus
}
#endif


#endif



