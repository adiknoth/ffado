/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#ifndef __Time__
#define __Time__

#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include <time.h>

/**
 * Type used to represent the value of free running
 * monotonic clock with units of microseconds.
 */
typedef uint64_t ffado_microsecs_t;

static inline void SleepRelativeUsec(unsigned long usec) {
    usleep(usec);
}
#endif



