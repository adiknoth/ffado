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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>

#include <signal.h>
#include "src/debugmodule/debugmodule.h"

#include <netinet/in.h>

#include "src/threads.h"

#include "src/libieee1394/cycletimer.h"
#include "src/libieee1394/configrom.h"
#include "src/libieee1394/ieee1394service.h"
#include "src/libieee1394/ARMHandler.h"

#include <libraw1394/raw1394.h>

DECLARE_GLOBAL_DEBUG_MODULE;

int run=1;
static void sighandler (int sig)
{
    run = 0;
}

class MyFunctor : public Functor
{
public:
    MyFunctor() {}
    virtual ~MyFunctor() {}

    void operator() () {
        printf("hello from the functor (%p)\n", this);
    };
};

int main(int argc, char *argv[])
{
    setDebugLevel(DEBUG_LEVEL_VERBOSE);
    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    const int PORT_TO_USE = 0;

    printf("FFADO Ieee1394Service test application\n");

    Ieee1394Service *m_service=NULL;

    m_service=new Ieee1394Service();
    m_service->setVerboseLevel(DEBUG_LEVEL_VERBOSE );
    m_service->initialize(PORT_TO_USE);
    m_service->setThreadParameters(true, 20);

    MyFunctor *test_busreset=new MyFunctor();

    printf(" adding (%p) as busreset handler\n", test_busreset);

    m_service->addBusResetHandler(test_busreset);

    nodeaddr_t addr =  m_service->findFreeARMBlock(0x0000FFFFE0000000ULL, 4, 4 );

    ARMHandler *test_arm=new ARMHandler(addr,
                         4,
                         RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                         RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                         0);

    printf(" adding (%p) as arm handler\n", test_arm);

    if (!m_service->registerARMHandler(test_arm)) {
        printf("  failed\n");
    }

    addr =  m_service->findFreeARMBlock(0x0000FFFFE0000000ULL, 4, 4 );

    ARMHandler *test_arm2=new ARMHandler(addr,
                         4,
                         RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                         RAW1394_ARM_READ | RAW1394_ARM_WRITE | RAW1394_ARM_LOCK,
                         0);

    printf(" adding (%p) as arm handler\n", test_arm2);

    if (!m_service->registerARMHandler(test_arm2)) {
        printf("  failed\n");
    }

    raw1394handle_t m_handle = raw1394_new_handle_on_port( PORT_TO_USE );
    if ( !m_handle ) {
        if ( !errno ) {
            debugFatal("libraw1394 not compatible\n");
        } else {
            debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s\n",
                strerror(errno) );
            debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
        }
        exit(-1);
    }

    while(run) {
        fflush(stderr);
        fflush(stdout);
        usleep(900*1000);

        uint32_t cycle_timer;
        uint64_t local_time;
        // read the CTR 'raw' from a handle
        // and read it from the 1394 service, which uses a DLL
        int err = raw1394_read_cycle_timer(m_handle, &cycle_timer, &local_time);
        uint64_t ctr = CYCLE_TIMER_TO_TICKS( cycle_timer );
        uint64_t ctr_dll = m_service->getCycleTimerTicks();

        if(err) {
            debugError("CTR read error\n");
        }
        debugOutput ( DEBUG_LEVEL_VERBOSE,
                    "Cycle timer: %011llu (%03us %04ucy %04uticks)\n",
                    ctr, (unsigned int)TICKS_TO_SECS( ctr ),
                    (unsigned int)TICKS_TO_CYCLES( ctr ), (unsigned int)TICKS_TO_OFFSET( ctr ) );
        debugOutput ( DEBUG_LEVEL_VERBOSE,
                    "   from DLL: %011llu (%03us %04ucy %04uticks)\n",
                    ctr_dll, (unsigned int)TICKS_TO_SECS( ctr_dll ),
                    (unsigned int)TICKS_TO_CYCLES( ctr_dll ), (unsigned int)TICKS_TO_OFFSET( ctr_dll ) );
        int64_t diff = diffTicks(ctr, ctr_dll);
        uint64_t abs_diff;
        if (diff < 0) {
            abs_diff = -diff;
        } else {
            abs_diff = diff;
        }
        debugOutput ( DEBUG_LEVEL_VERBOSE,
                    "      diff: %s%011llu (%03us %04ucy %04uticks)\n",
                    ((int64_t)abs_diff==diff?" ":"-"), abs_diff, (unsigned int)TICKS_TO_SECS( abs_diff ),
                    (unsigned int)TICKS_TO_CYCLES( abs_diff ), (unsigned int)TICKS_TO_OFFSET( abs_diff ) );
        if (abs_diff > 1000) {
            debugWarning("Alert, large diff: %lld\n", diff);
        }
        debugOutput ( DEBUG_LEVEL_VERBOSE,
                    " wait...\n");
    }

    delete m_service;
    delete test_busreset;
    delete test_arm;
    delete test_arm2;

    printf("Bye...\n");

    return EXIT_SUCCESS;
}
