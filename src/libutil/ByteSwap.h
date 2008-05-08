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

#ifndef __FFADO_BYTESWAP__
#define __FFADO_BYTESWAP__

#include <byteswap.h>
#include <inttypes.h>
#include <endian.h>
#include <assert.h>

// to check for SSE etc...
#include "config.h"

#include <stdio.h>

#if __BYTE_ORDER == __BIG_ENDIAN

// no-op for big endian machines

static inline uint64_t
CondSwap64(uint64_t d)
{
    return d;
}

static inline uint32_t
CondSwap32(uint32_t d)
{
    return d;
}

static inline uint16_t
CondSwap16(uint16_t d)
{
    return d;
}

static inline void
byteSwapToBus(quadlet_t *data, unsigned int nb_elements)
{
    return;
}

static inline void
byteSwapFromBus(quadlet_t *data, unsigned int nb_elements)
{
    return;
}

#else

static inline uint64_t
CondSwap64(uint64_t d)
{
    return bswap_64(d);
}

static inline uint32_t
CondSwap32(uint32_t d)
{
    return bswap_32(d);
}

static inline uint16_t
CondSwap16(uint16_t d)
{
    return bswap_16(d);
}

#ifdef __SSE2__
#include <emmintrin.h>
#warning SSE2 build

//static inline void
void
byteSwapToBus(quadlet_t *data, unsigned int nb_elements)
{
    // Work input until data reaches 16 byte alignment
    while ((((unsigned long)data) & 0xF) && nb_elements > 0) {
        *data = CondSwap32(*data);
        data++;
        nb_elements--;
    }
    assert((((unsigned long)data) & 0xF) == 0);

    // now do the SSE based conversion
    // we have to go from [A B C D] to [D C B A]
    // where A, B, C, D are bytes
    //
    // the algorithm is:
    // 1) [A B C D] => [B A D C]
    // 2) [B A D C] => [D C B A]
    //
    // i.e. first do a 2x(2x8bit) swap
    // then a 2x16bit swap
    
    __m128i v;
    while(nb_elements >= 4) {
        // prefetch the data for the next round
         __builtin_prefetch(data+128, 0, 0);

        // load the data into the vector unit
        v = _mm_load_si128((__m128i*)data);
        // do first swap
        v = _mm_or_si128( _mm_slli_epi16( v, 8 ), _mm_srli_epi16( v, 8 ) ); //swap it
        // do second swap
        v = _mm_or_si128( _mm_slli_epi32( v, 16 ), _mm_srli_epi32( v, 16 ) ); //swap it
        // store result
        _mm_store_si128 ((__m128i*)data, v);
        
        data += 4;
        nb_elements -= 4;
    }

    // and do the remaining ones
    while (nb_elements > 0) {
        *data = CondSwap32(*data);
        data++;
        nb_elements--;
    }
}

//static inline void
void
byteSwapFromBus(quadlet_t *data, unsigned int nb_elements)
{
    // Work input until data reaches 16 byte alignment
    while ((((unsigned long)data) & 0xF) && nb_elements > 0) {
        *data = CondSwap32(*data);
        data++;
        nb_elements--;
    }
    assert((((unsigned long)data) & 0xF) == 0);

    // now do the SSE based conversion
    // we have to go from [A B C D] to [D C B A]
    // where A, B, C, D are bytes
    //
    // the algorithm is:
    // 1) [A B C D] => [B A D C]
    // 2) [B A D C] => [D C B A]
    //
    // i.e. first do a 2x(2x8bit) swap
    // then a 2x16bit swap
    
    __m128i v;
    while(nb_elements >= 4) {
        // load the data into the vector unit
        v = _mm_load_si128((__m128i*)data);
        // do first swap
        v = _mm_or_si128( _mm_slli_epi16( v, 8 ), _mm_srli_epi16( v, 8 ) ); //swap it
        // do second swap
        v = _mm_or_si128( _mm_slli_epi32( v, 16 ), _mm_srli_epi32( v, 16 ) ); //swap it
        // store result
        _mm_store_si128 ((__m128i*)data, v);
        
        data += 4;
        nb_elements -= 4;
    }

    // and do the remaining ones
    while (nb_elements > 0) {
        *data = CondSwap32(*data);
        data++;
        nb_elements--;
    }
}

#else

static inline void
byteSwapToBus(quadlet_t *data, unsigned int nb_elements)
{
    unsigned int i=0;
    for(; i<nb_elements; i++) {
        *data = CondSwap32(*data);
        data++;
    }
}

static inline void
byteSwapFromBus(quadlet_t *data, unsigned int nb_elements)
{
    unsigned int i=0;
    for(; i<nb_elements; i++) {
        *data = CondSwap32(*data);
        data++;
    }
}

#endif // sse2

#endif // byte order

#endif // h
