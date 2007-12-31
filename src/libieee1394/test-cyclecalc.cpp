/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#include "cycletimer.h"
#include "debugmodule/debugmodule.h"
#include <stdio.h>

DECLARE_GLOBAL_DEBUG_MODULE;

int main() {
    setDebugLevel(DEBUG_LEVEL_VERY_VERBOSE);
    printf("Cycle timer operation tests (incomplete)\n");
    
    /* TEST 1
     * check reconstruction of SYT RECEIVE timestamp
     *
     * The now_ctr has wrapped, while the cycle and syt have not
     *
     */

    uint32_t now_ctr = 0x140001DA;
    uint64_t now = CYCLE_TIMER_TO_TICKS(0x140001DA);
    unsigned int cycle = 7968;
    uint16_t syt = 0x583B;
    
    debugOutput(DEBUG_LEVEL_VERBOSE,"NOW_CTR          : %08X (%03us %04uc %04ut)\n",
                          now_ctr,
                          (unsigned int)CYCLE_TIMER_GET_SECS(now_ctr),
                          (unsigned int)CYCLE_TIMER_GET_CYCLES(now_ctr),
                          (unsigned int)CYCLE_TIMER_GET_OFFSET(now_ctr));

    debugOutput(DEBUG_LEVEL_VERBOSE,"NOW              : %011llu (%03us %04uc %04ut)\n",
                          now,
                          (unsigned int)TICKS_TO_SECS(now),
                          (unsigned int)TICKS_TO_CYCLES(now),
                          (unsigned int)TICKS_TO_OFFSET(now));
    debugOutput(DEBUG_LEVEL_VERBOSE,"SYT              : %08X (%03us %04uc %04ut)\n",
                          syt,
                          (unsigned int)CYCLE_TIMER_GET_SECS(syt),
                          (unsigned int)CYCLE_TIMER_GET_CYCLES(syt),
                          (unsigned int)CYCLE_TIMER_GET_OFFSET(syt));
    debugOutput(DEBUG_LEVEL_VERBOSE,"CYCLE            : %uc\n",
                          cycle);

    uint64_t calc_ts = sytRecvToFullTicks(syt, cycle, now_ctr);

    debugOutput(DEBUG_LEVEL_VERBOSE,"CALC_TS          : %011llu (%03us %04uc %04ut)\n",
                          calc_ts,
                          (unsigned int)TICKS_TO_SECS(calc_ts),
                          (unsigned int)TICKS_TO_CYCLES(calc_ts),
                          (unsigned int)TICKS_TO_OFFSET(calc_ts));

    
// BL: 1211722982: Debug (IsoHandler.cpp)[ 420] putPacket: received packet: length=168, channel=0, cycle=7968
// BL: 1211723031: Debug (cycletimer.h)[ 308] sytRecvToFullTicks: SYT=583B CY=7968 CTR=140001DA
// BL: 1211723037: Debug (StreamProcessor.cpp)[ 346] putPacket: RECV: CY=7968 TS=00245679163
// BL: 1211723043: Debug (AmdtpReceiveStreamProcessor.cpp)[ 135] processPacketData: STMP: 245679163ticks | syt_interval=8, tpf=557.254395
// BL: 1211723051: Debug (TimestampedBuffer.cpp)[1153] incrementFrameCounter:  nbframes: 8, m_update_period: 8
// BL: 1211723052: Debug (AmdtpTransmitStreamProcessor.cpp)[ 250] generatePacketHeader: Too early: CY=0254, TC=0257, CUT=0003, TST=00271126073 (0257), TSP=00271137849 (0261)
// BL: 1211723055: Debug (TimestampedBuffer.cpp)[1155] incrementFrameCounter:  tail TS:  270250705.174, next tail TS:  270255163.209
// BL: 1211723062: Debug (TimestampedBuffer.cpp)[1157] incrementFrameCounter:  new TS:  245679163.000, wrapped new TS:  245679163.000
//     

}
