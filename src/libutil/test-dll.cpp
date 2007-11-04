/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "DelayLockedLoop.h"
#include <stdio.h>

using namespace Util;

int main() {
    int i=0;
    int i2=0;

    #define MAX_TEST_ORDER 2

    #define NB_VALUES 12
    #define NB_LOOPS 50000

    // this test is for a second order loop,


    float omega=6.28*0.001;
    float coeffs[MAX_TEST_ORDER];
    coeffs[0]=1.41*omega;
    coeffs[1]=omega*omega;

    DelayLockedLoop d1(1, coeffs);

    DelayLockedLoop d2(2, coeffs);

    // this sequence represents the average deviation of the sample period
    float deviation[NB_VALUES]={-0.001, 0.0, 0.001, 0.001, -0.001, 0.001, -0.001, 0.001, -0.001, 0.00, 0.001, -0.001};
    float average=0.0;

    // these are the actual period times
    float ideal_values[NB_LOOPS];
    float actual_values[NB_LOOPS];
    float actual_values2[NB_LOOPS];

    // we define a nominal sample time:
    float ts_nominal=1.0/48000.0;
    float period=100.0;

    // we calculate the deviated sample times
    for (i=0;i<NB_LOOPS;i++) {
        unsigned int idx=i % NB_VALUES;

        // this constructs time-between-samples type of data
        // for testing the first order loop
        actual_values[i]=period*ts_nominal*(1+deviation[idx]);

        // this is constructing an incrementing time line
        ideal_values[i]=0;
        if (i==0) ideal_values[i] = ts_nominal * period;
        else ideal_values[i] = ideal_values[i-1] + (ts_nominal * period);

        actual_values2[i] = ideal_values[i] + (ts_nominal*deviation[idx]);

        // calculate the average deviation to check which
        // direction the deviation sequence takes.
        average+=deviation[idx]*ts_nominal;
    }
    average /= NB_LOOPS;

    d1.setIntegrator(0,period*ts_nominal);

    d2.setIntegrator(0,ideal_values[0]);
    d2.setIntegrator(1,ideal_values[0]);

    for(i=0;i<50;i++) {
        d1.put(actual_values[i]);
        printf("%06d: IN = %8.4fms - OUT = %8.4fms, error output=%e\n", i, actual_values[i]*1000, d1.get()*1000, d1.getError());
    }

    printf("--------\n");
    for(i2=0;i2<50;i2++) {
        d2.put(actual_values2[i2]);
        printf("%06d: IN = %8.4fms - OUT = %8.4fms, error output=%e\n", i, actual_values2[i2]*1000, d2.get()*1000,  d2.getError());
    }

    printf("========= CONVERGENCE =========\n");
    for(;i<NB_LOOPS;i++) {
        d1.put(actual_values[i]);
    }
    printf("%06d: OUT = %8.4fms, E=%e, ideal=%8.4fms, diff=%f%%\n", i,
        d1.get()*1000,
        d1.getError(),
        ts_nominal * period*1000,
        (d1.get()-ts_nominal * period)/(ts_nominal * period)*100);

    printf("--------\n");
    for(;i2<NB_LOOPS;i2++) {
        d2.put(actual_values2[i2]);
    }
    printf("%06d: OUT = %8.4fms, E=%e, ideal=%8.4fms, diff=%f%%\n", i,
        d2.get()*1000,
        d2.getError(),
        ideal_values[NB_LOOPS-1]*1000,
        (d2.get()-ideal_values[NB_LOOPS-1])/(ideal_values[NB_LOOPS-1])*100);


    printf("Average deviation: %f, period time = %fms\n",average, ts_nominal * period*1000);


}
