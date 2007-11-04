/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * FFADO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
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
    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    printf("FFADO Ieee1394Service test application\n");

    Ieee1394Service *m_service=NULL;

    m_service=new Ieee1394Service();
    m_service->initialize(2);

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

    while(run) {
        fflush(stderr);
        fflush(stdout);
        sleep(1);
    }

    delete m_service;
    delete test_busreset;
    delete test_arm;
    delete test_arm2;

    printf("Bye...\n");

    return EXIT_SUCCESS;
}
