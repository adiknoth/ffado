/*
	Modifications (C) 2005 Pieter Palmers
	
	Copied from the jackd sources
	function names changed in order to avoid naming problems when using this in
	a jackd backend.
	
    Copyright (C) 2004 Paul Davis

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

    $Id$
*/

#ifndef __freebob_thread_h__
#define __freebob_thread_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

typedef struct _freebob_thread_arg {
	freebob_device_t* device;
	void* (*work_function)(void*);
	int priority;
	int realtime;
	void* arg;
	pid_t cap_pid;
} freebob_thread_arg_t;

/** @file thread.h
 *
 * Library functions to standardize thread creation for JACK and its
 * clients.  These interfaces hide some system variations in the
 * handling of realtime scheduling and associated privileges.
 */

/**
 * Attempt to enable realtime scheduling for a thread.  On some
 * systems that may require special privileges.
 *
 * @param thread POSIX thread ID.
 * @param priority requested thread priority.
 *
 * @returns 0, if successful; EPERM, if the calling process lacks
 * required realtime privileges; otherwise some other error number.
 */
int freebob_acquire_real_time_scheduling (pthread_t thread, int priority);

/**
 * Create a thread for JACK or one of its clients.  The thread is
 * created executing @a start_routine with @a arg as its sole
 * argument.
 *
 * @param client the JACK client for whom the thread is being created. May be
 * NULL if the client is being created within the JACK server.
 * @param thread place to return POSIX thread ID.
 * @param priority thread priority, if realtime.
 * @param realtime true for the thread to use realtime scheduling.  On
 * some systems that may require special privileges.
 * @param start_routine function the thread calls when it starts.
 * @param arg parameter passed to the @a start_routine.
 *
 * @returns 0, if successful; otherwise some error number.
 */
int freebob_streaming_create_thread (freebob_device_t* dev,
			       pthread_t *thread,
			       int priority,
			       int realtime,	/* boolean */
			       void *(*start_routine)(void*),
			       void *arg);

/**
 * Drop realtime scheduling for a thread.
 *
 * @param thread POSIX thread ID.
 *
 * @returns 0, if successful; otherwise an error number.
 */
int freebob_drop_real_time_scheduling (pthread_t thread);


#ifdef __cplusplus
}
#endif

#endif /* __freebob_thread_h__ */
