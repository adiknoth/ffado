/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#include "SystemTimeSource.h"
#include "Time.h"

// needed for clock_nanosleep
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <time.h>
#include <stdlib.h>

namespace Util {

IMPL_DEBUG_MODULE( SystemTimeSource, SystemTimeSource, DEBUG_LEVEL_NORMAL );

SystemTimeSource::SystemTimeSource()
{
}

SystemTimeSource::~SystemTimeSource()
{
}

void
SystemTimeSource::SleepUsecRelative(ffado_microsecs_t usecs)
{
    //usleep(usecs);
    struct timespec ts;
    ts.tv_sec = usecs / (1000000LL);
    ts.tv_nsec = (usecs % (1000000LL)) * 1000LL;
    clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
}

void
SystemTimeSource::SleepUsecAbsolute(ffado_microsecs_t wake_at_usec)
{
    struct timespec ts;
    ts.tv_sec = wake_at_usec / (1000000LL);
    ts.tv_nsec = (wake_at_usec % (1000000LL)) * 1000LL;
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
}

ffado_microsecs_t
SystemTimeSource::SleepUsecRandom(ffado_microsecs_t max_usec)
{
    long int rnd = random();
    long long int tmp = (rnd*max_usec);
    tmp /= RAND_MAX;
    ffado_microsecs_t usec = tmp;
    SleepUsecRelative(usec);
    return usec;
}

ffado_microsecs_t
SystemTimeSource::getCurrentTime()
{
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     return tv.tv_sec * 1000000ULL + tv.tv_usec;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ffado_microsecs_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}

ffado_microsecs_t
SystemTimeSource::getCurrentTimeAsUsecs()
{
    return getCurrentTime();
}

} // end of namespace Util
