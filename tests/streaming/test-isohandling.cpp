/***************************************************************************
  Copyright (C) 2005 by Pieter Palmers   *
                                                                        *
  This program is free software; you can redistribute it and/or modify  *
  it under the terms of the GNU General Public License as published by  *
  the Free Software Foundation; either version 2 of the License, or     *
  (at your option) any later version.                                   *
                                                                        *
  This program is distributed in the hope that it will be useful,       *
  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
  GNU General Public License for more details.                          *
                                                                        *
  You should have received a copy of the GNU General Public License     *
  along with this program; if not, write to the                         *
  Free Software Foundation, Inc.,                                       *
  59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include "debugmodule/debugmodule.h"

#include <netinet/in.h>

#include "IsoHandler.h"
#include "IsoStream.h"
#include "StreamProcessorManager.h"
#include "AmdtpStreamProcessor.h"
#include "IsoHandlerManager.h"
#include "StreamRunner.h"
#include "FreebobPosixThread.h"
#include "AmdtpPort.h"

using namespace FreebobStreaming;


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

	printf("Freebob streaming test application\n");
	printf(" ISO handler tests\n");

	// the first thing we need is a ISO handler manager
	IsoHandlerManager *isomanager = new IsoHandlerManager();
	if(!isomanager) {
		printf("Could not create IsoHandlerManager\n");
		return -1;
	}

	isomanager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
	printf("----------------------\n");

	// also create a processor manager to manage the actual stream
	// processors	
	StreamProcessorManager *procMan = new StreamProcessorManager(512,3);
	if(!procMan) {
		printf("Could not create StreamProcessorManager\n");
		return -1;
	}
	procMan->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

	// now we can allocate the stream processors themselves

// 	StreamProcessor *spt = new AmdtpTransmitStreamProcessor(3,2,44100,10);
// 	if(!spt) {
// 		printf("Could not create transmit AmdtpTransmitStreamProcessor\n");
// 		return -1;
// 	}
// 	spt->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

 	AmdtpReceiveStreamProcessor *spr = new AmdtpReceiveStreamProcessor(0,2,44100,11);
// 	ReceiveStreamProcessor *spr = new ReceiveStreamProcessor(0,2,44100);
	if(!spr) {
		printf("Could not create receive AmdtpStreamProcessor\n");
		return -1;
	}
	spr->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

 	AmdtpReceiveStreamProcessor *spr2 = new AmdtpReceiveStreamProcessor(1,2,44100,7);
// 	ReceiveStreamProcessor *spr = new ReceiveStreamProcessor(0,2,44100);
	if(!spr2) {
		printf("Could not create receive AmdtpStreamProcessor\n");
		return -1;
	}
	spr2->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

// 	if (isomanager->registerStream(spt)) {
// 		printf("Could not register transmit stream processor with the ISO manager\n");
// 		return -1;
// 	}
// 	printf("----------------------\n");


	// now we have an xmit stream, 
	// register it with the manager that assigns an iso handler

	if (isomanager->registerStream(spr)) {
		printf("Could not register receive stream processor with the ISO manager\n");
		return -1;
	}
	printf("----------------------\n");

	if (isomanager->registerStream(spr2)) {
		printf("Could not register receive stream processor with the ISO manager\n");
		return -1;
	}
	printf("----------------------\n");

// 	printf("----------------------\n");
// 	if (procMan->registerProcessor(spt)) {
// 		printf("Could not register transmit stream processor with the Processor manager\n");
// 		return -1;
// 	}
// 	printf("----------------------\n");

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
		           AmdtpAudioPort::E_Int24,
		           AmdtpAudioPort::E_PeriodBuffered, 
		           512,
		           1, 
		           0, 
		           AmdtpPortInfo::E_MBLA, 
		           0
		);
	if (!p1) {
		printf("Could not create port 1\n");
		return -1;
	}

	printf("----------------------\n");

	if (spr2->addPort(p1)) {
		printf("Could not register port with receive stream processor\n");
		return -1;
	}
	
	// now create the runner that does the actual streaming
	StreamRunner *runner = new StreamRunner(isomanager,procMan);
	if(!runner) {
		printf("Could not create StreamRunner\n");
		return -1;
	}

	FreebobPosixThread *thread=new FreebobPosixThread(runner);

	// start the runner
	thread->Start();
	printf("----------------------\n");

	if(isomanager->startHandlers()) {
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
			isomanager->dumpInfo();
			printf("--------------------------------------------\n");
			procMan->dumpInfo();
			printf("--------------------------------------------\n");
			hexDumpQuadlets((quadlet_t*)(p1->getBufferAddress()),10);
			printf("============================================\n");
			printf("\n");
			periods_print+=100;
 		}
		procMan->waitForPeriod();
		procMan->transfer();
		
	}

	thread->Stop();

	isomanager->stopHandlers();

	spr->deletePort(p1);

// 	isomanager->unregisterStream(spt);
	isomanager->unregisterStream(spr);
	isomanager->unregisterStream(spr2);

// 	procMan->unregisterProcessor(spt);
	procMan->unregisterProcessor(spr);
	procMan->unregisterProcessor(spr2);

	delete thread;
	delete runner;

	delete p1;

	delete procMan;
	delete isomanager;

// 	delete spt;
	delete spr;
	delete spr2;

	printf("Bye...\n");


  return EXIT_SUCCESS;
}
