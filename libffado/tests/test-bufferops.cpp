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

#include "debugmodule/debugmodule.h"

DECLARE_GLOBAL_DEBUG_MODULE;

#include "libutil/ByteSwap.h"
#include "libstreaming/amdtp/AmdtpBufferOps.h"

#include "libutil/SystemTimeSource.h"
#include "libutil/Time.h"

#include <inttypes.h>

// 32M of test data
#define NB_QUADLETS (1024 * 1024 * 32)
#define NB_TESTS 10

bool
testByteSwap(int nb_quadlets, int nb_tests) {
    quadlet_t *buffer_1;
    quadlet_t *buffer_ref;
    int i=0;
    
    ffado_microsecs_t start;
    ffado_microsecs_t elapsed;
    
    setDebugLevel(DEBUG_LEVEL_NORMAL);
    
    buffer_1 = new quadlet_t[nb_quadlets];
    buffer_ref = new quadlet_t[nb_quadlets];
    
    printMessage( "Generating test data...\n");
    for (i=0; i<nb_quadlets; i++) {
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
    
    for (i=0; i<nb_quadlets; i++) {
        buffer_ref[i] = CondSwapToBus32(buffer_1[i]);
    }
    
    printMessage( "Performing byte-swap...\n");
    
    int test=0;
    for (test=0; test<nb_tests; test++) {
        for (i=0; i<nb_quadlets; i++) {
            byte_t tmp = i & 0xFF;
            buffer_1[i]   = tmp << 24;
            tmp = (i + 1) & 0xFF;
            buffer_1[i]   |= tmp << 16;
            tmp = (i + 2) & 0xFF;
            buffer_1[i]   |= tmp << 8;
            tmp = (i + 3) & 0xFF;
            buffer_1[i]   |= tmp;
        }

        start = Util::SystemTimeSource::getCurrentTimeAsUsecs();
        byteSwapToBus(buffer_1, nb_quadlets);
        elapsed = Util::SystemTimeSource::getCurrentTimeAsUsecs() - start;
        printMessage( " took %"PRI_FFADO_MICROSECS_T"usec...\n", elapsed);
        
    }

    // check
    printMessage( "Checking results...\n");
    bool all_ok=true;
    for (i=0; i<nb_quadlets; i++) {
        if (buffer_1[i] != buffer_ref[i]) {
            printMessage( " bad result: %08X should be %08X\n",
                        buffer_1[i], buffer_ref[i]);
            all_ok=false;
        } else {
            //debugOutput(DEBUG_LEVEL_VERBOSE, "good result: %08X should be %08X\n",
            //            buffer_1[i], buffer_ref[i]);
        }
    }

    delete[] buffer_1;
    delete[] buffer_ref;
    return all_ok;
}

bool
testInt24Label(int nb_quadlets, int nb_tests) {
    quadlet_t *buffer_1;
    quadlet_t *buffer_ref;
    int i=0;
    
    ffado_microsecs_t start;
    ffado_microsecs_t elapsed;
    
    setDebugLevel(DEBUG_LEVEL_MESSAGE);
    
    buffer_1 = new quadlet_t[nb_quadlets];
    buffer_ref = new quadlet_t[nb_quadlets];
    
    printMessage( "Generating test data...\n");
    for (i=0; i<nb_quadlets; i++) {
        byte_t tmp = i & 0xFF;
        buffer_1[i]   = tmp << 16;
        tmp = (i + 1) & 0xFF;
        buffer_1[i]   |= tmp << 8;
        tmp = (i + 2) & 0xFF;
        buffer_1[i]   |= tmp;
    }
    
    // do reference conversion
    for (i=0; i<nb_quadlets; i++) {
        buffer_ref[i] = buffer_1[i] | 0x40000000;
    }
    
    printMessage( "Performing AMDTP labeling...\n");
    
    int test=0;
    for (test=0; test<nb_tests; test++) {
        for (i=0; i<nb_quadlets; i++) {
            byte_t tmp = i & 0xFF;
            buffer_1[i]   = tmp << 16;
            tmp = (i + 1) & 0xFF;
            buffer_1[i]   |= tmp << 8;
            tmp = (i + 2) & 0xFF;
            buffer_1[i]   |= tmp;
        }

        start = Util::SystemTimeSource::getCurrentTimeAsUsecs();
        convertFromInt24AndLabelAsMBLA(buffer_1, nb_quadlets);
        elapsed = Util::SystemTimeSource::getCurrentTimeAsUsecs() - start;
        printMessage( " took %"PRI_FFADO_MICROSECS_T"usec...\n", elapsed);
    }

    // check
    printMessage( "Checking results...\n");
    bool all_ok=true;
    for (i=0; i<nb_quadlets; i++) {
        if (buffer_1[i] != buffer_ref[i]) {
            printMessage( " bad result: %08X should be %08X\n",
                        buffer_1[i], buffer_ref[i]);
            all_ok=false;
        } else {
            //debugOutput(DEBUG_LEVEL_VERBOSE, "good result: %08X should be %08X\n",
            //            buffer_1[i], buffer_ref[i]);
        }
    }

    delete[] buffer_1;
    delete[] buffer_ref;
    return all_ok;
}

bool
testFloatLabel(int nb_quadlets, int nb_tests) {
    quadlet_t *buffer_1;
    quadlet_t *buffer_ref;
    quadlet_t *buffer_in;
    float *buffer_float;
    int i=0;
    
    ffado_microsecs_t start;
    ffado_microsecs_t elapsed;
    
    setDebugLevel(DEBUG_LEVEL_MESSAGE);
    
    buffer_1 = new quadlet_t[nb_quadlets];
    buffer_in = new quadlet_t[nb_quadlets];
    buffer_ref = new quadlet_t[nb_quadlets];
    buffer_float = new float[nb_quadlets];
    
    printMessage( "Generating test data...\n");
    for (i=0; i<nb_quadlets; i++) {
        byte_t tmp = i & 0xFF;
        buffer_in[i]   = tmp << 16;
        tmp = (i + 1) & 0xFF;
        buffer_in[i]   |= tmp << 8;
        tmp = (i + 2) & 0xFF;
        buffer_in[i]   |= tmp;
        
        // convert to float and normalize
        buffer_float[i] = (float)(buffer_in[i]);
        buffer_float[i] /= (float)(0x007FFFFF); // range: 0..2
        buffer_float[i] -= 1.0; // range: 1..-1
        
        // copy to input buffer
        float *t = &(buffer_float[i]);
        quadlet_t *v = (quadlet_t *)t;
        buffer_1[i] = *v;
    }
    
    // do reference conversion
    for (i=0; i<nb_quadlets; i++) {
        float v = (buffer_float[i]) * AMDTP_FLOAT_MULTIPLIER;
        unsigned int tmp = ((int) v);
        tmp = ( tmp >> 8 ) | 0x40000000;
        buffer_ref[i] = tmp;
    }
    
    printMessage( "Performing AMDTP labeling...\n");
    
    int test=0;
    for (test=0; test<nb_tests; test++) {
    for (i=0; i<nb_quadlets; i++) {
        // copy float to input buffer
        float *t = &(buffer_float[i]);
        quadlet_t *v = (quadlet_t *)t;
        buffer_1[i] = *v;
    }

        start = Util::SystemTimeSource::getCurrentTimeAsUsecs();
        convertFromFloatAndLabelAsMBLA(buffer_1, nb_quadlets);
        elapsed = Util::SystemTimeSource::getCurrentTimeAsUsecs() - start;
        printMessage( " took %"PRI_FFADO_MICROSECS_T"usec...\n", elapsed);
    }

    // check
    printMessage( "Checking results...\n");
    bool all_ok=true;
    for (i=0; i<nb_quadlets; i++) {
        if (buffer_1[i] != buffer_ref[i]) {
            printMessage( " bad result: %08X should be %08X\n",
                        buffer_1[i], buffer_ref[i]);
            all_ok=false;
        } else {
            //debugOutput(DEBUG_LEVEL_VERBOSE, "good result: %08X should be %08X\n",
            //            buffer_1[i], buffer_ref[i]);
        }
    }

    delete[] buffer_1;
    delete[] buffer_ref;
    delete[] buffer_in;
    delete[] buffer_float;
    return all_ok;
}

int
main(int argc, char **argv) {

    testByteSwap(NB_QUADLETS, NB_TESTS);
    testInt24Label(NB_QUADLETS, NB_TESTS);
    testFloatLabel(NB_QUADLETS, NB_TESTS);
    
    return 0;
}
