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

#include "debugmodule/debugmodule.h"

DECLARE_GLOBAL_DEBUG_MODULE;

#include "libutil/ByteSwap.h"
#include "libutil/SystemTimeSource.h"
#include <inttypes.h>

#include <emmintrin.h>
/*
void test() {
    vSInt16	*in, *out; //must be 16 byte aligned

    for( x = 0; x < array_bytes / sizeof( vSInt16); x++ )
    {
        vSInt16 v = in[x]; //load 16 bytes
        v = _mm_or_si128( _mm_slli_epi16( v, 8 ), _mm_srli_epi16( v, 8 ) ); //swap it
        out[x] = v; //store it out
    }
}*/

#define NB_QUADLETS (4096 * 4096)
#define NB_TESTS 10
int
main(int argc, char **argv) {
    quadlet_t *buffer_1;
    quadlet_t *buffer_ref;
    int i=0;
    
    Util::SystemTimeSource time;
    ffado_microsecs_t start;
    ffado_microsecs_t elapsed;
    
    setDebugLevel(DEBUG_LEVEL_NORMAL);
    
    buffer_1 = new quadlet_t[NB_QUADLETS];
    buffer_ref = new quadlet_t[NB_QUADLETS];
    
    debugOutput(DEBUG_LEVEL_NORMAL, "Generating test data...\n");
    for (i=0; i<NB_QUADLETS; i++) {
        byte_t tmp = i & 0xFF;
        buffer_1[i]   = tmp << 24;
        tmp = (i + 1) & 0xFF;
        buffer_1[i]   |= tmp << 16;
        tmp = (i + 2) & 0xFF;
        buffer_1[i]   |= tmp << 8;
        tmp = (i + 3) & 0xFF;
        buffer_1[i]   |= tmp;
    }
    
    // do reference conversion
    
    for (i=0; i<NB_QUADLETS; i++) {
        buffer_ref[i] = htonl(buffer_1[i]);
    }
    
    debugOutput(DEBUG_LEVEL_NORMAL, "Performing byte-swap...\n");
    
    int test=0;
    for (test=0; test<NB_TESTS; test++) {
        for (i=0; i<NB_QUADLETS; i++) {
            byte_t tmp = i & 0xFF;
            buffer_1[i]   = tmp << 24;
            tmp = (i + 1) & 0xFF;
            buffer_1[i]   |= tmp << 16;
            tmp = (i + 2) & 0xFF;
            buffer_1[i]   |= tmp << 8;
            tmp = (i + 3) & 0xFF;
            buffer_1[i]   |= tmp;
        }

        start = time.getCurrentTimeAsUsecs();
        byteSwapToBus(buffer_1, NB_QUADLETS);
        elapsed = time.getCurrentTimeAsUsecs() - start;
        debugOutput(DEBUG_LEVEL_NORMAL, " took %lluusec...\n", elapsed);
        
    }

    // check
    debugOutput(DEBUG_LEVEL_NORMAL, "Checking results...\n");
    bool all_ok=true;
    for (i=0; i<NB_QUADLETS; i++) {
        if (buffer_1[i] != buffer_ref[i]) {
            debugOutput(DEBUG_LEVEL_NORMAL, " bad result: %08X should be %08X\n",
                        buffer_1[i], buffer_ref[i]);
            all_ok=false;
        } else {
            //debugOutput(DEBUG_LEVEL_VERBOSE, "good result: %08X should be %08X\n",
            //            buffer_1[i], buffer_ref[i]);
        }
    }

    delete[] buffer_1;
    delete[] buffer_ref;
    
    return 0;
}
