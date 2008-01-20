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

/* cycles.h
 * Based upon cycles.h from the jackdmp package.
 * Original Copyright:
 *
 *   Copyright (C) 2001 Paul Davis
 *   Code derived from various headers from the Linux kernel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __FFADO_CYCLES_H__
#define __FFADO_CYCLES_H__

/*
 * Standard way to access the cycle timer on i586+ CPUs.
 * Currently only used on SMP.
 *
 * If you really have a SMP machine with i486 chips or older,
 * compile for that, and this will just always return zero.
 * That's ok, it just means that the nicer scheduling heuristics
 * won't work for you.
 *
 * We only use the low 32 bits, and we'd simply better make sure
 * that we reschedule before that wraps. Scheduling at least every
 * four billion cycles just basically sounds like a good idea,
 * regardless of how fast the machine is.
 */

#ifdef __linux__

#ifdef __PPC__

/* PowerPC */

#define CPU_FTR_601            0x00000100

typedef unsigned long cycles_t;

/* For the "cycle" counter we use the timebase lower half. */

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles(void)
{
    cycles_t ret = 0;

    __asm__ __volatile__(
        "98:    mftb %0\n"
        "99:\n"
        ".section __ftr_fixup,\"a\"\n"
        "    .long %1\n"
        "    .long 0\n"
        "    .long 98b\n"
        "    .long 99b\n"
        ".previous"
    : "=r" (ret) : "i" (CPU_FTR_601));
    return ret;
}

#endif

#ifdef __i386__

typedef unsigned long long cycles_t;

extern cycles_t cacheflush_time;

#define rdtscll(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))

static inline cycles_t get_cycles (void)
{
    unsigned long long ret;

    rdtscll(ret);
    return ret;
}

#endif

#endif

#endif // __FFADO_CYCLES_H__
