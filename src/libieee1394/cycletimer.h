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

/* Definitions and utility macro's to handle the ISO cycle timer */

#ifndef __CYCLETIMER_H__
#define __CYCLETIMER_H__

#include "debugmodule/debugmodule.h"

#include <inttypes.h>

#define CSR_CYCLE_TIME            0x200
#define CSR_REGISTER_BASE  0xfffff0000000ULL

#define CYCLES_PER_SECOND   8000U
#define TICKS_PER_CYCLE     3072U
#define TICKS_PER_HALFCYCLE (3072U/2U)
#define TICKS_PER_SECOND    24576000UL
#define TICKS_PER_USEC     (24.576000)

#define USECS_PER_TICK     (1.0/TICKS_PER_USEC)
#define USECS_PER_CYCLE    (125U)

#define CYCLE_TIMER_GET_SECS(x)   ((((x) & 0xFE000000UL) >> 25))
#define CYCLE_TIMER_GET_CYCLES(x) ((((x) & 0x01FFF000UL) >> 12))
#define CYCLE_TIMER_GET_OFFSET(x)  ((((x) & 0x00000FFFUL)))
#define CYCLE_TIMER_TO_TICKS(x) ((CYCLE_TIMER_GET_SECS(x)   * TICKS_PER_SECOND) +\
                                   (CYCLE_TIMER_GET_CYCLES(x) * TICKS_PER_CYCLE ) +\
                                   (CYCLE_TIMER_GET_OFFSET(x)            ))

// non-efficient versions, to be avoided in critical code
#define TICKS_TO_SECS(x) ((x)/TICKS_PER_SECOND)
#define TICKS_TO_CYCLES(x) (((x)/TICKS_PER_CYCLE) % CYCLES_PER_SECOND)
#define TICKS_TO_OFFSET(x) (((x)%TICKS_PER_CYCLE))

#define TICKS_TO_CYCLE_TIMER(x) (  ((TICKS_TO_SECS(x) & 0x7F) << 25) \
                                 | ((TICKS_TO_CYCLES(x) & 0x1FFF) << 12) \
                                 | ((TICKS_TO_OFFSET(x) & 0xFFF)))

#define TICKS_TO_SYT(x)         (((TICKS_TO_CYCLES(x) & 0xF) << 12) \
                                 | ((TICKS_TO_OFFSET(x) & 0xFFF)))

#define CYCLE_TIMER_UNWRAP_TICKS(x) (((uint64_t)(x)) \
                                       + (127ULL * TICKS_PER_SECOND) \
                                       + (CYCLES_PER_SECOND * TICKS_PER_CYCLE) \
                                       + (TICKS_PER_CYCLE) \
                                      )
#define CYCLE_TIMER_WRAP_TICKS(x) ((x % TICKS_PER_SECOND))

#define INVALID_TIMESTAMP_TICKS     0xFFFFFFFFFFFFFFFFULL

DECLARE_GLOBAL_DEBUG_MODULE;

/**
 * @brief Wraps x to the maximum number of ticks
 *
 * The input value is wrapped to the maximum value of the cycle
 * timer, in ticks (128sec * 24576000 ticks/sec).
 *
 * @param x time to wrap
 * @return wrapped time
 */
static inline uint64_t wrapAtMaxTicks(uint64_t x) {
    if (x >= TICKS_PER_SECOND * 128L) {
        x -= TICKS_PER_SECOND * 128L;
    }

#ifdef DEBUG
        if (x >= TICKS_PER_SECOND * 128L) {
            debugWarning("insufficient wrapping: %llu\n",x);
        }
#endif

    return x;
}

/**
 * @brief Wraps x to the minimum number of ticks
 *
 * The input value is wrapped to the minimum value of the cycle
 * timer, in ticks (= 0).
 *
 * @param x time to wrap
 * @return wrapped time
 */
static inline int64_t wrapAtMinTicks(int64_t x) {
    if (x < 0) {
        x += TICKS_PER_SECOND * 128L;
    }

#ifdef DEBUG
        if (x < 0) {
            debugWarning("insufficient wrapping: %lld\n",x);

            debugWarning("correcting...\n");
            while (x < 0) {
                x += TICKS_PER_SECOND * 128L;

                if (x < 0) {
                    debugWarning(" insufficient wrapping: %lld\n",x);
                }
            }
        }

#endif

    return (int64_t)x;
}

/**
 * @brief Wraps both at minimum and maximum value for ticks
 *
 * The input value is wrapped to the maximum value of the cycle
 * timer, in ticks (128sec * 24576000 ticks/sec), and
 * to the minimum value of the cycle timer, in ticks (= 0).
 *
 * @param x value to wrap
 * @return wrapped value
 */
static inline int64_t wrapAtMinMaxTicks(int64_t x) {

    if (x < 0) {
        x += TICKS_PER_SECOND * 128L;
    } else if (x >= (int64_t)(TICKS_PER_SECOND * 128L)) {
        x -= TICKS_PER_SECOND * 128L;
    }

#ifdef DEBUG
        if (x >= (int64_t)(TICKS_PER_SECOND * 128L)) {
            debugWarning("insufficient wrapping (max): %llu\n",x);
        }
        if (x < 0) {
            debugWarning("insufficient wrapping (min): %lld\n",x);
        }
#endif
    return x;

}

/**
 * @brief Computes the sum of two cycle values
 *
 * This function computes a sum between cycles
 * such that it respects wrapping (at 8000 cycles).
 *
 * The passed arguments are assumed to be valid cycle numbers,
 * i.e. they should be wrapped at 8000 cycles
 *
 * See addTicks
 *
 * @param x First cycle value
 * @param y Second cycle value
 * @return the sum x+y, wrapped
 */
static inline unsigned int addCycles(unsigned int x, unsigned int y) {
    unsigned int sum = x + y;
#ifdef DEBUG
    if (x >= CYCLES_PER_SECOND || y >= CYCLES_PER_SECOND ) {
        debugWarning("At least one argument not wrapped correctly: x=%u, y=%u\n",x,y);
    }
#endif

    // since both x and y are < CYCLES_PER_SECOND this should be enough to unwrap
    if (sum > CYCLES_PER_SECOND) sum -= CYCLES_PER_SECOND;
    return sum;
}

/**
 * @brief Computes a difference between cycles
 *
 * This function computes a difference between cycles
 * such that it respects wrapping (at 8000 cycles).
 *
 * See diffTicks
 *
 * @param x First cycle value
 * @param y Second cycle value
 * @return the difference x-y, unwrapped
 */
static inline int diffCycles(unsigned int x, unsigned int y) {
    int diff = (int)x - (int)y;

    // the maximal difference we allow (64secs)
    const int max=CYCLES_PER_SECOND/2;

    if(diff > max) {
        diff -= CYCLES_PER_SECOND;
    } else if (diff < -max) {
        diff += CYCLES_PER_SECOND;
    }

    return diff;
}

/**
 * @brief Computes a difference between timestamps
 *
 * This function computes a difference between timestamps
 * such that it respects wrapping.
 *
 * If x wraps around, but y doesn't, the result of x-y is
 * negative and very large. However the real difference is
 * not large. It can be calculated by unwrapping x and then
 * calculating x-y.
 *
 * @param x First timestamp
 * @param y Second timestamp
 * @return the difference x-y, unwrapped
 */
static inline int64_t diffTicks(int64_t x, int64_t y) {
    int64_t diff=(int64_t)x - (int64_t)y;

    // the maximal difference we allow (64secs)
    const int64_t wrapvalue=((int64_t)TICKS_PER_SECOND)*128LL;
    const int64_t max=wrapvalue/2LL;

    if(diff > max) {
        // this means that y has wrapped, but
        // x has not. we should unwrap y
        // by adding TICKS_PER_SECOND*128L, meaning that we should substract
        // this value from diff
        diff -= wrapvalue;
    } else if (diff < -max) {
        // this means that x has wrapped, but
        // y has not. we should unwrap x
        // by adding TICKS_PER_SECOND*128L, meaning that we should add
        // this value to diff
        diff += wrapvalue;
    }

#ifdef DEBUG
    if(diff > max || diff < -max) {
        debugWarning("difference does not make any sense\n");
        debugWarning("diff=%lld max=%lld\n", diff, max);
        
    }
#endif

    return (int64_t)diff;

}

/**
 * @brief Computes a sum of timestamps
 *
 * This function computes a sum of timestamps in ticks,
 * wrapping the result if necessary.
 *
 * @param x First timestamp
 * @param y Second timestamp
 * @return the sum x+y, wrapped
 */
static inline uint64_t addTicks(uint64_t x, uint64_t y) {
    uint64_t sum=x+y;

    return wrapAtMaxTicks(sum);
}

/**
 * @brief Computes a substraction of timestamps
 *
 * This function computes a substraction of timestamps in ticks,
 * wrapping the result if necessary.
 *
 * @param x First timestamp
 * @param y Second timestamp
 * @return the difference x-y, wrapped
 */
static inline uint64_t substractTicks(uint64_t x, uint64_t y) {
    int64_t subs=x-y;

    return wrapAtMinTicks(subs);
}

/**
 * @brief Converts a received SYT timestamp to a full timestamp in ticks.
 *
 *
 * @param syt_timestamp The SYT timestamp as present in the packet
 * @param rcv_cycle The cycle this timestamp was received on
 * @param ctr_now The current value of the cycle timer ('now')
 * @return
 */
static inline uint64_t sytRecvToFullTicks(uint64_t syt_timestamp, unsigned int rcv_cycle, uint64_t ctr_now) {
    uint64_t timestamp;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "SYT=%04llX CY=%u CTR=%08llX\n",
                       syt_timestamp, rcv_cycle, ctr_now);

    // reconstruct the full cycle
    uint64_t cc_cycles=CYCLE_TIMER_GET_CYCLES(ctr_now);
    uint64_t cc_seconds=CYCLE_TIMER_GET_SECS(ctr_now);

    // check for bogus ctr
    // the cycle timer should be ahead of the receive timer
    int diff_cycles = diffCycles(cc_cycles, rcv_cycle);
    if (diff_cycles<0) {
        debugWarning("current cycle timer not ahead of receive cycle: rcv: %u / cc: %llu (%d)\n",
                        rcv_cycle, cc_cycles, diff_cycles);
    }

    // the cycletimer has wrapped since this packet was received
    // we want cc_seconds to reflect the 'seconds' at the point this
    // was received
    if (rcv_cycle>cc_cycles && (diff_cycles>=0)) {
        if (cc_seconds) {
            cc_seconds--;
        } else {
            // seconds has wrapped around, so we'd better not substract 1
            // the good value is 127
            cc_seconds=127;
        }
    }

    // reconstruct the top part of the timestamp using the current cycle number
    uint64_t rcv_cycle_masked=rcv_cycle & 0xF;
    uint64_t syt_cycle=CYCLE_TIMER_GET_CYCLES(syt_timestamp);

    // if this is true, wraparound has occurred, undo this wraparound
    if(syt_cycle<rcv_cycle_masked) syt_cycle += 0x10;

    // this is the difference in cycles wrt the cycle the
    // timestamp was received
    uint64_t delta_cycles=syt_cycle-rcv_cycle_masked;

    // reconstruct the cycle part of the timestamp
    uint64_t new_cycles=rcv_cycle + delta_cycles;

    // if the cycles cause a wraparound of the cycle timer,
    // perform this wraparound
    // and convert the timestamp into ticks
    if(new_cycles<8000) {
        timestamp  = new_cycles * TICKS_PER_CYCLE;
    } else {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "Detected wraparound: %d + %d = %d\n",
                           rcv_cycle, delta_cycles, new_cycles);

        new_cycles-=8000; // wrap around
#ifdef DEBUG
        if (new_cycles >= 8000) {
            debugWarning("insufficient unwrapping\n");
        }
#endif
        timestamp  = new_cycles * TICKS_PER_CYCLE;
        // add one second due to wraparound
        timestamp += TICKS_PER_SECOND;
    }

    timestamp += CYCLE_TIMER_GET_OFFSET(syt_timestamp);

    timestamp = addTicks(timestamp, cc_seconds * TICKS_PER_SECOND);

    #ifdef DEBUG
        if(( TICKS_TO_CYCLE_TIMER(timestamp) & 0xFFFF) != syt_timestamp) {
            debugWarning("back-converted timestamp not equal to SYT\n");
            debugWarning("TS=%011llu TSC=%08lX SYT=%04X\n",
                  timestamp, TICKS_TO_CYCLE_TIMER(timestamp), syt_timestamp);
        }
    #endif

    return timestamp;
}

/**
 * @brief Converts a transmit SYT timestamp to a full timestamp in ticks.
 *
 * The difference between sytRecvToFullTicks and sytXmitToFullTicks is
 * the way SYT cycle wraparound is detected: in the receive version,
 * wraparound is present if rcv_cycle > current_cycle. In the xmit
 * version this is when current_cycle > xmt_cycle.
 *
 * @param syt_timestamp The SYT timestamp as present in the packet
 * @param xmt_cycle The cycle this timestamp was received on
 * @param ctr_now The current value of the cycle timer ('now')
 * @return
 */
static inline uint64_t sytXmitToFullTicks(uint64_t syt_timestamp, unsigned int xmt_cycle, uint64_t ctr_now) {
    uint64_t timestamp;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE, "SYT=%08llX CY=%04X CTR=%08llX\n",
                       syt_timestamp, xmt_cycle, ctr_now);

    // reconstruct the full cycle
    uint64_t cc_cycles=CYCLE_TIMER_GET_CYCLES(ctr_now);
    uint64_t cc_seconds=CYCLE_TIMER_GET_SECS(ctr_now);

    // check for bogus CTR
    int diff_cycles = diffCycles(xmt_cycle, cc_cycles);
    if (diff_cycles<0) {
        debugWarning("xmit cycle not ahead of current cycle: xmt: %u / cc: %llu (%d)\n",
                        xmt_cycle, cc_cycles, diff_cycles);
    }

    // the cycletimer has wrapped since this packet was received
    // we want cc_seconds to reflect the 'seconds' at the point this
    // is to be transmitted
    if (cc_cycles>xmt_cycle && (diff_cycles>=0)) {
        if (cc_seconds) {
            cc_seconds--;
        } else {
            // seconds has wrapped around, so we'd better not substract 1
            // the good value is 127
            cc_seconds=127;
        }
    }

    // reconstruct the top part of the timestamp using the current cycle number
    uint64_t xmt_cycle_masked=xmt_cycle & 0xF;
    uint64_t syt_cycle=CYCLE_TIMER_GET_CYCLES(syt_timestamp);

    // if this is true, wraparound has occurred, undo this wraparound
    if(syt_cycle<xmt_cycle_masked) syt_cycle += 0x10;

    // this is the difference in cycles wrt the cycle the
    // timestamp was received
    uint64_t delta_cycles=syt_cycle-xmt_cycle_masked;

    // reconstruct the cycle part of the timestamp
    uint64_t new_cycles=xmt_cycle + delta_cycles;

    // if the cycles cause a wraparound of the cycle timer,
    // perform this wraparound
    // and convert the timestamp into ticks
    if(new_cycles<8000) {
        timestamp  = new_cycles * TICKS_PER_CYCLE;
    } else {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "Detected wraparound: %d + %d = %d\n",
                           xmt_cycle, delta_cycles, new_cycles);

        new_cycles-=8000; // wrap around
#ifdef DEBUG
        if (new_cycles >= 8000) {
            debugWarning("insufficient unwrapping\n");
        }
#endif
        timestamp  = new_cycles * TICKS_PER_CYCLE;
        // add one second due to wraparound
        timestamp += TICKS_PER_SECOND;
    }

    timestamp += CYCLE_TIMER_GET_OFFSET(syt_timestamp);

    timestamp = addTicks(timestamp, cc_seconds * TICKS_PER_SECOND);

    #ifdef DEBUG
        if(( TICKS_TO_CYCLE_TIMER(timestamp) & 0xFFFF) != syt_timestamp) {
            debugWarning("back-converted timestamp not equal to SYT\n");
            debugWarning("TS=%011llu TSC=%08lX SYT=%04X\n",
                  timestamp, TICKS_TO_CYCLE_TIMER(timestamp), syt_timestamp);
        }
    #endif

    return timestamp;
}

#endif // __CYCLETIMER_H__
