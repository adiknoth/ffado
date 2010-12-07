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

/*
 * Copied from the jackd/jackdmp sources
 * function names changed in order to avoid naming problems when using this in
 * a jackd backend.
 */

/* Original license:
 *
 *  Copyright (C) 2004-2006 Grame
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __FFADOATOMIC__
#define __FFADOATOMIC__

#include <stdint.h>


static inline char CAS(volatile uint32_t value, uint32_t newvalue, volatile int32_t* addr)
{
    return __sync_bool_compare_and_swap (addr, value, newvalue);
}

static inline long INC_ATOMIC(volatile int32_t* val)
{
    int32_t actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual + 1, val));
    return actual;
}

static inline long DEC_ATOMIC(volatile int32_t* val)
{
    int32_t actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual - 1, val));
    return actual;
}

static inline long ADD_ATOMIC(volatile int32_t* val, int32_t addval)
{
    int32_t actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual + addval, val));
    return actual;
}

static inline long SUBSTRACT_ATOMIC(volatile int32_t* val, int32_t addval)
{
    int32_t actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual - addval, val));
    return actual;
}

static inline long ZERO_ATOMIC(volatile int32_t* val)
{
    int32_t actual;
    do {
        actual = *val;
    } while (!CAS(actual, 0, val));
    return actual;
}

#endif // __FFADO_ATOMIC__

