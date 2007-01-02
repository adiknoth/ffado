/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006,2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
 
/* Definitions and utility macro's to handle the ISO cyclecounter */

#ifndef __CYCLECOUNTER_H__
#define __CYCLECOUNTER_H__

#define CSR_CYCLE_TIME            0x200
#define CSR_REGISTER_BASE  0xfffff0000000ULL

#define CYCLES_PER_SECOND   8000
#define TICKS_PER_CYCLE     3072
#define TICKS_PER_SECOND   (CYCLES_PER_SECOND * TICKS_PER_CYCLE)
#define TICKS_PER_USEC     (TICKS_PER_SECOND/1000000.0)

#define USECS_PER_TICK     (1.0/TICKS_PER_USEC)

#define CYCLE_COUNTER_GET_SECS(x)   ((((x) & 0xFE000000) >> 25))
#define CYCLE_COUNTER_GET_CYCLES(x) ((((x) & 0x01FFF000) >> 12))
#define CYCLE_COUNTER_GET_OFFSET(x)  ((((x) & 0x00000FFF)))
#define CYCLE_COUNTER_TO_TICKS(x) ((CYCLE_COUNTER_GET_SECS(x)   * TICKS_PER_SECOND) +\
                                   (CYCLE_COUNTER_GET_CYCLES(x) * TICKS_PER_CYCLE ) +\
                                   (CYCLE_COUNTER_GET_OFFSET(x)            ))
                                   
// non-efficient versions, to be avoided in critical code
#define TICKS_TO_SECS(x) ((x)/TICKS_PER_SECOND)
#define TICKS_TO_CYCLES(x) (((x)/TICKS_PER_CYCLE) % CYCLES_PER_SECOND)
#define TICKS_TO_OFFSET(x) (((x)%TICKS_PER_CYCLE))

#define CYCLE_COUNTER_UNWRAP_TICKS(x) ((x) \
                                       + (127 * TICKS_PER_SECOND) \
                                       + (CYCLES_PER_SECOND * TICKS_PER_CYCLE) \
                                       + (TICKS_PER_CYCLE) \
                                      )

#endif // __CYCLECOUNTER_H__
