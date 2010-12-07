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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include "src/debugmodule/debugmodule.h"

#include "libutil/ByteSwap.h"

#include "src/libutil/PosixThread.h"

#include "src/libstreaming/util/IsoHandler.h"
#include "src/libstreaming/util/StreamProcessorManager.h"
#include "src/libstreaming/util/IsoHandlerManager.h"
#include "src/libstreaming/amdtp/AmdtpStreamProcessor.h"
#include "src/libstreaming/amdtp/AmdtpPort.h"

using namespace Streaming;

int run;

static void sighandler (int sig)
{
    run = 0;
}

int main(int argc, char *argv[])
{

    int retval=0;
    int i=0;
    run=1;

    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    printf("FFADO streaming test application\n");
    printf(" ISO handler tests\n");

    // also create a processor manager to manage the actual stream
    // processors
    StreamProcessorManager *procMan = new StreamProcessorManager(512,3);
    if(!procMan) {
        printf("Could not create StreamProcessorManager\n");
        return -1;
    }
    procMan->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

    // now we can allocate the stream processors themselves

//     StreamProcessor *spt = new AmdtpTransmitStreamProcessor(3,2,44100,10);
//     if(!spt) {
//         printf("Could not create transmit AmdtpTransmitStreamProcessor\n");
//         return -1;
//     }
//     spt->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

     AmdtpReceiveStreamProcessor *spr = new AmdtpReceiveStreamProcessor(2,44100,7);
//     ReceiveStreamProcessor *spr = new ReceiveStreamProcessor(0,2,44100);
    if(!spr) {
        printf("Could not create receive AmdtpStreamProcessor\n");
        return -1;
    }
    spr->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
    spr->setChannel(0);

     AmdtpReceiveStreamProcessor *spr2 = new AmdtpReceiveStreamProcessor(2,44100,11);
//     ReceiveStreamProcessor *spr = new ReceiveStreamProcessor(0,2,44100);
    if(!spr2) {
        printf("Could not create receive AmdtpStreamProcessor\n");
        return -1;
    }
    spr2->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
    spr2->setChannel(1);

//     printf("----------------------\n");
//     if (procMan->registerProcessor(spt)) {
//         printf("Could not register transmit stream processor with the Processor manager\n");
//         return -1;
//     }
//     printf("----------------------\n");

    // also register it with the processor manager, so that it is aware of
    // buffer sizes etc...
    if (procMan->registerProcessor(spr)) {
        printf("Could not register receive stream processor with the Processor manager\n");
        return -1;
    }
    printf("----------------------\n");

    if (procMan->registerProcessor(spr2)) {
        printf("Could not register receive stream processor with the Processor manager\n");
        return -1;
    }
    printf("----------------------\n");

    AmdtpAudioPort *p1=new AmdtpAudioPort(
                   std::string("Test port 1"),
                   AmdtpAudioPort::E_Capture,
                   1,
                   0,
                   AmdtpPortInfo::E_MBLA,
                   0
        );
    if (!p1) {
        printf("Could not create port 1\n");
        return -1;
    }

    p1->setBufferSize(512);

    printf("----------------------\n");

    if (spr2->addPort(p1)) {
        printf("Could not register port with receive stream processor\n");
        return -1;
    }

    Util::PosixThread *thread=new Util::PosixThread(procMan);


    procMan->prepare();

    // start the runner
    thread->Start();
    printf("----------------------\n");

    if(procMan->start()) {
        printf("Could not start handlers\n");
        return -1;
    }
    printf("----------------------\n");

    int periods=0;
    int periods_print=0;
    while(run) {
        periods++;
         if(periods>periods_print) {
            printf("\n");
            printf("============================================\n");
            procMan->dumpInfo();
            printf("--------------------------------------------\n");
/*            hexDumpQuadlets((quadlet_t*)(p1->getBufferAddress()),10);
            printf("--------------------------------------------\n");*/
            hexDumpQuadlets((quadlet_t*)(p1->getBufferAddress()),10);
            printf("============================================\n");
            printf("\n");
            periods_print+=100;
         }
        procMan->waitForPeriod();
        procMan->transfer();

    }

    thread->Stop();

    procMan->stop();

//     procMan->unregisterProcessor(spt);
    procMan->unregisterProcessor(spr);
    procMan->unregisterProcessor(spr2);

    delete thread;

    delete procMan;

//     delete spt;
    delete spr;
    delete spr2;

    printf("Bye...\n");


  return EXIT_SUCCESS;
}
