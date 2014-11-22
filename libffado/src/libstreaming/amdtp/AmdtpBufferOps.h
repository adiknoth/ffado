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

#ifndef __FFADO_AMDTPBUFFEROPS__
#define __FFADO_AMDTPBUFFEROPS__

#include <assert.h>
// to check for SSE etc...


#include <stdio.h>

#define AMDTP_FLOAT_MULTIPLIER 2147483392.0

#ifdef __SSE2__
//#if 0
#include <emmintrin.h>

// There's no need to warn about this anymore - jwoithe.
// #warning SSE2 build

//static inline void
void
convertFromFloatAndLabelAsMBLA(quadlet_t *data, unsigned int nb_elements)
{
    // Work input until data reaches 16 byte alignment
    while ((((unsigned long)data) & 0xF) && nb_elements > 0) {
        float *in = (float *)data;
        float v = (*in) * AMDTP_FLOAT_MULTIPLIER;
        unsigned int tmp = ((int) v);
        tmp = ( tmp >> 8 ) | 0x40000000;
        *data = (quadlet_t)tmp;
        data++;
        nb_elements--;
    }
    assert((((unsigned long)data) & 0xF) == 0);

    // now do the SSE based conversion/labeling
    __m128i v_int;
    __m128i label = _mm_set_epi32 (0x40000000, 0x40000000, 0x40000000, 0x40000000);
    __m128 mult = _mm_set_ps(AMDTP_FLOAT_MULTIPLIER, AMDTP_FLOAT_MULTIPLIER, AMDTP_FLOAT_MULTIPLIER, AMDTP_FLOAT_MULTIPLIER);
    __m128 v_float;
    while(nb_elements >= 4) {
        float *in = (float *)data;
        // load the data into the vector unit
        v_float = _mm_load_ps(in);
        // multiply
        v_float = _mm_mul_ps(v_float, mult);
        // convert to signed integer
        v_int = _mm_cvttps_epi32( v_float );
        // shift right 8 bits
        v_int = _mm_srli_epi32( v_int, 8 );
        // label it
        v_int = _mm_or_si128( v_int, label );
        // store result
        _mm_store_si128 ((__m128i*)data, v_int);

        data += 4;
        nb_elements -= 4;
    }

    // and do the remaining ones
    while (nb_elements > 0) {
        float *in = (float *)data;
        float v = (*in) * AMDTP_FLOAT_MULTIPLIER;
        unsigned int tmp = ((int) v);
        tmp = ( tmp >> 8 ) | 0x40000000;
        *data = (quadlet_t)tmp;
        data++;
        nb_elements--;
    }
}

//static inline void
void
convertFromInt24AndLabelAsMBLA(quadlet_t *data, unsigned int nb_elements)
{
    // Work input until data reaches 16 byte alignment
    while ((((unsigned long)data) & 0xF) && nb_elements > 0) {
        uint32_t in = (uint32_t)(*data);
        *data = (quadlet_t)((in & 0x00FFFFFF) | 0x40000000);
        data++;
        nb_elements--;
    }
    assert((((unsigned long)data) & 0xF) == 0);

    // now do the SSE based labeling
    __m128i v;
    const __m128i mask  = _mm_set_epi32 (0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF);
    const __m128i label = _mm_set_epi32 (0x40000000, 0x40000000, 0x40000000, 0x40000000);
    while(nb_elements >= 4) {
        // load the data into the vector unit
        v = _mm_load_si128((__m128i*)data);
        // mask
        v = _mm_and_si128( v, mask );
        // label
        v = _mm_or_si128( v, label );
        // store result
        _mm_store_si128 ((__m128i*)data, v);

        data += 4;
        nb_elements -= 4;
    }

    // and do the remaining ones
    while (nb_elements > 0) {
        uint32_t in = (uint32_t)(*data);
        *data = (quadlet_t)((in & 0x00FFFFFF) | 0x40000000);
        data++;
        nb_elements--;
    }
}

#else

//static inline void
void
convertFromFloatAndLabelAsMBLA(quadlet_t *data, unsigned int nb_elements)
{
    unsigned int i=0;
    for(; i<nb_elements; i++) {
        // don't care for overflow
        float *in = (float *)data;
        float v = (*in) * AMDTP_FLOAT_MULTIPLIER;
        unsigned int tmp = ((int) v);
        tmp = ( tmp >> 8 ) | 0x40000000;
        *data = (quadlet_t)tmp;
        data++;
    }
}

//static inline void
void
convertFromInt24AndLabelAsMBLA(quadlet_t *data, unsigned int nb_elements)
{
    unsigned int i=0;
    for(; i<nb_elements; i++) {
        uint32_t in = (uint32_t)(*data);
        *data = (quadlet_t)((in & 0x00FFFFFF) | 0x40000000);
        data++;
    }
}

#endif // sse2

#endif /* __FFADO_AMDTPBUFFEROPS__ */
