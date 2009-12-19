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

DECLARE_GLOBAL_DEBUG_MODULE;

namespace Util {

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
#if USE_ABSOLUTE_NANOSLEEP
    struct timespec ts;
    ts.tv_sec = wake_at_usec / (1000000LL);
    ts.tv_nsec = (wake_at_usec % (1000000LL)) * 1000LL;
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                       "clock_nanosleep until %"PRId64" sec, %"PRId64" nanosec\n",
                       (int64_t)ts.tv_sec, (int64_t)ts.tv_nsec);
    int err = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
    if(err) {
        // maybe signal occurred, but we're going to ignore that
    }
    debugOutputExtreme(DEBUG_LEVEL_VERBOSE,
                "back with err=%d\n",
                err);
#else
    // only sleep if needed
    ffado_microsecs_t now = getCurrentTime();
    if(wake_at_usec >= now) {
        ffado_microsecs_t to_sleep = wake_at_usec - now;
        SleepUsecRelative(to_sleep);
    }
#endif
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
    return getCurrentTimeAsUsecs();
}

ffado_microsecs_t
SystemTimeSource::getCurrentTimeAsUsecs()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ffado_microsecs_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}

} // end of namespace Util
