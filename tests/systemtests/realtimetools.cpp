/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "realtimetools.h"

#include "debugmodule/debugmodule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

// needed for clock_nanosleep
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <time.h>

DECLARE_GLOBAL_DEBUG_MODULE;

int set_realtime_priority(unsigned int prio)
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Setting thread prio to %u\n", prio);
    if (prio > 0) {
        struct sched_param schp;
        /*
        * set the process to realtime privs
        */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = prio;
        
        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0) {
            perror("sched_setscheduler");
            return -1;
        }
  } else {
        struct sched_param schp;
        /*
        * set the process to realtime privs
        */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = 0;
        
        if (sched_setscheduler(0, SCHED_OTHER, &schp) != 0) {
            perror("sched_setscheduler");
            return -1;
        }
  }
  return 0;
}

void
rt_sleep_relative_usecs(uint64_t usecs) {
    //usleep(usecs);
    struct timespec ts;
    ts.tv_sec = usecs / (1000000LL);
    ts.tv_nsec = (usecs % (1000000LL)) * 1000LL;
    clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
}

void
rt_sleep_absolute_usecs(uint64_t wake_at_usec) {
    struct timespec ts;
    ts.tv_sec = wake_at_usec / (1000000LL);
    ts.tv_nsec = (wake_at_usec % (1000000LL)) * 1000LL;
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
}

uint64_t rt_gettime_usecs() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}

