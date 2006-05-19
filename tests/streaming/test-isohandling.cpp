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

#include "IsoHandler.h"
#include "IsoStream.h"
#include "StreamProcessorManager.h"
#include "StreamProcessor.h"
#include "IsoHandlerManager.h"
#include "StreamRunner.h"
#include "FreebobPosixThread.h"

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

	IsoStream *xms = new IsoStream(IsoStream::EST_Transmit,2,2);
	if(!xms) {
		printf("Could not create transmit IsoStream\n");
		return -1;
	}
	xms->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

	IsoStream *rcs = new IsoStream(IsoStream::EST_Receive,0,2);
	if(!rcs) {
		printf("Could not create receive IsoStream\n");
		return -1;
	}
	rcs->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

	IsoStream *rcs2 = new IsoStream(IsoStream::EST_Receive,1,2);
	if(!rcs2) {
		printf("Could not create receive IsoStream\n");
		return -1;
	}
	rcs2->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

	// now we have an xmit stream, attached to an xmit handler
	// register it with the manager

	IsoHandlerManager *isomanager = new IsoHandlerManager();
	if(!isomanager) {
		printf("Could not create IsoHandlerManager\n");
		return -1;
	}

	isomanager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
	
	if (isomanager->registerStream(xms)) {
		printf("Could not register transmit handler with the manager\n");
		return -1;
	}
	if (isomanager->registerStream(rcs)) {
		printf("Could not register receive handler with the manager\n");
		return -1;
	}
	if (isomanager->registerStream(rcs2)) {
		printf("Could not register receive handler 2 with the manager\n");
		return -1;
	}
	// also create a processor as a dummy for the stream runner
	
	StreamProcessorManager *procMan = new StreamProcessorManager(512);
	if(!procMan) {
		printf("Could not create StreamProcessor\n");
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

	if(isomanager->startHandlers()) {
		printf("Could not start handlers\n");
		return -1;
	}

	while(run) {
		isomanager->dumpInfo();
		sleep(1);
	}

	thread->Stop();

	isomanager->stopHandlers();

	isomanager->unregisterStream(xms);
	isomanager->unregisterStream(rcs);
	isomanager->unregisterStream(rcs2);

	delete thread;
	delete runner;
	delete procMan;
	delete isomanager;

	delete rcs2;
	delete rcs;
	delete xms;

	printf("Bye...\n");


  return EXIT_SUCCESS;
}
