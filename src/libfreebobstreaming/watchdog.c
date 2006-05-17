/* watchdog.c
  Copyright (C) 2006 Pieter Palmers
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  Thread creation function including workarounds for real-time scheduling
  behaviour on different glibc versions.

*/

#include "libfreebob/freebob_streaming.h"
#include "freebob_streaming_private.h"
#include "freebob_connections.h"
#include "freebob_debug.h"
#include "thread.h"
#include "watchdog.h"

#include <signal.h>
#include <unistd.h>

void *freebob_streaming_watchdog_thread (void *arg)
{
	freebob_device_t *dev = (freebob_device_t *) arg;

	dev->watchdog_check = 0;

	while (1) {
		sleep (2);
		if (dev->watchdog_check == 0) {

			printError("watchdog: timeout");

			/* kill our process group, try to get a dump */
			kill (-getpgrp(), SIGABRT);
			/*NOTREACHED*/
			exit (1);
		}
		dev->watchdog_check = 0;
	}
}

int freebob_streaming_start_watchdog (freebob_device_t *dev)
{
	int watchdog_priority = dev->packetizer.priority + 10;
	int max_priority = sched_get_priority_max (SCHED_FIFO);

	debugPrint(DEBUG_LEVEL_STARTUP, "Starting Watchdog...\n");
	
	if ((max_priority != -1) &&
			(max_priority < watchdog_priority))
		watchdog_priority = max_priority;
	
	if (freebob_streaming_create_thread (dev, &dev->watchdog_thread, watchdog_priority,
		TRUE, freebob_streaming_watchdog_thread, dev)) {
			printError ("cannot start watchdog thread");
			return -1;
		}

	return 0;
}

void freebob_streaming_stop_watchdog (freebob_device_t *dev)
{
	debugPrint(DEBUG_LEVEL_STARTUP, "Stopping Watchdog...\n");
	
	pthread_cancel (dev->watchdog_thread);
	pthread_join (dev->watchdog_thread, NULL);
}
