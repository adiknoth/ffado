/*
Modifications for FFADO by Pieter Palmers

Copied from the jackd/jackdmp sources
function names changed in order to avoid naming problems when using this in
a jackd backend.

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

#ifndef __FFADOATOMIC__
#define __FFADOATOMIC__

typedef unsigned short UInt16;
typedef unsigned long UInt32;
typedef long SInt32;
typedef unsigned long long UInt64;

#if defined(__APPLE__)

#if defined(__ppc__)

static inline int CAS(register UInt32 value, register UInt32 newvalue, register volatile void* addr)
{
    register int result;
    asm volatile (
        "# CAS                    \n"
        "    lwarx    r0, 0, %1    \n"         // creates a reservation on addr
        "    cmpw    r0, %2        \n"            //  test value at addr
        "    bne-    1f          \n"
        "    sync                \n"         //  synchronize instructions
        "    stwcx.    %3, 0, %1    \n"         //  if the reservation is not altered
        //  stores the new value at addr
        "    bne-    1f          \n"
        "   li      %0, 1       \n"
        "    b        2f          \n"
        "1:                     \n"
        "   li      %0, 0       \n"
        "2:                     \n"
        : "=r" (result)
        : "r" (addr), "r" (value), "r" (newvalue)
        : "r0"
        );
    return result;
}

#endif

#if defined(__i386__) || defined(__x86_64__)

#ifdef __SMP__
#    define LOCK "lock ; "
#else
#    define LOCK ""
#endif

static inline char CAS(volatile UInt32 value, UInt32 newvalue, volatile void* addr)
{
    register char ret;
    __asm__ __volatile__ (
        "# CAS \n\t"
        LOCK "cmpxchg %2, (%1) \n\t"
        "sete %0               \n\t"
        : "=a" (ret)
        : "c" (addr), "d" (newvalue), "a" (value)
        );
    return ret;
}

#endif

#endif

#ifdef __linux__

#ifdef __PPC__

static inline int CAS(register UInt32 value, register UInt32 newvalue, register volatile void* addr)
{
    register int result;
    register UInt32 tmp;
    asm volatile (
        "# CAS                    \n"
        "    lwarx    %4, 0, %1    \n"         // creates a reservation on addr
        "    cmpw    %4, %2        \n"        //  test value at addr
        "    bne-    1f          \n"
        "    sync                \n"         //  synchronize instructions
        "    stwcx.    %3, 0, %1    \n"         //  if the reservation is not altered
        //  stores the new value at addr
        "    bne-    1f          \n"
        "   li      %0, 1       \n"
        "    b        2f          \n"
        "1:                     \n"
        "   li      %0, 0       \n"
        "2:                     \n"
        : "=r" (result)
        : "r" (addr), "r" (value), "r" (newvalue), "r" (tmp)
        );
    return result;
}

#endif

#if defined(__i386__) || defined(__x86_64__)

#ifdef __SMP__
#    define LOCK "lock ; "
#else
#    define LOCK ""
#endif

static inline char CAS(volatile UInt32 value, UInt32 newvalue, volatile void* addr)
{
    register char ret;
    __asm__ __volatile__ (
        "# CAS \n\t"
        LOCK "cmpxchg %2, (%1) \n\t"
        "sete %0               \n\t"
        : "=a" (ret)
        : "c" (addr), "d" (newvalue), "a" (value)
        );
    return ret;
}

#endif

#endif

static inline long INC_ATOMIC(volatile SInt32* val)
{
    SInt32 actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual + 1, val));
    return actual;
}

static inline long DEC_ATOMIC(volatile SInt32* val)
{
    SInt32 actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual - 1, val));
    return actual;
}

static inline long ADD_ATOMIC(volatile SInt32* val, SInt32 addval)
{
    SInt32 actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual + addval, val));
    return actual;
}

static inline long SUBSTRACT_ATOMIC(volatile SInt32* val, SInt32 addval)
{
    SInt32 actual;
    do {
        actual = *val;
    } while (!CAS(actual, actual - addval, val));
    return actual;
}

static inline long ZERO_ATOMIC(volatile SInt32* val)
{
    SInt32 actual;
    do {
        actual = *val;
    } while (!CAS(actual, 0, val));
    return actual;
}

#endif // __FFADO_ATOMIC__

