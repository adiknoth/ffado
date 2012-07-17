/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2012 by Jonathan Woithe
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

#ifndef __FFADO_SYSTEMTIMESOURCE__
#define __FFADO_SYSTEMTIMESOURCE__

#include "../debugmodule/debugmodule.h"

#include <stdint.h>

typedef uint64_t ffado_microsecs_t;

// Ensure this is defined even for kernels/glib versions which don't include
// it.  This allows runtime-time testing for the feature.
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

namespace Util {

class SystemTimeSource
{
private: // don't allow objects to be created
    SystemTimeSource() {};
    virtual ~SystemTimeSource() {};

public:
    static bool setSource(clockid_t id);
    static clockid_t getSource(void);
    static int clockGettime(struct timespec *tp);

    static ffado_microsecs_t getCurrentTime();
    static ffado_microsecs_t getCurrentTimeAsUsecs();

    static void SleepUsecRelative(ffado_microsecs_t usecs);
    static void SleepUsecAbsolute(ffado_microsecs_t wake_time);
    
    /**
     * @brief sleeps for a random amount of usecs
     * @param max_usec max usecs
     * @return number of usecs slept
     */
    static ffado_microsecs_t SleepUsecRandom(ffado_microsecs_t max_usec);
};

} // end of namespace Util

#endif /* __FFADO_SYSTEMTIMESOURCE__ */


