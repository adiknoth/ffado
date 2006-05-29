/* 
 *  Copied from the jackd source tree, adapted for freebob
 *  Modifications for FreeBoB by Pieter Palmers
 *
 */

/*
 * messagebuffer.c -- realtime-safe message handling for jackd.
 *
 *  This interface is included in libjack so backend drivers can use
 *  it, *not* for external client processes.  It implements the
 *  VERBOSE() and MESSAGE() macros in a realtime-safe manner.
 */

/*
 *  Copyright (C) 2004 Rui Nuno Capela, Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

#include "messagebuffer.h"
// #include "atomicity.h"

/* MB_NEXT() relies on the fact that MB_BUFFERS is a power of two */
#define MB_BUFFERS	128
#define MB_NEXT(index) ((index+1) & (MB_BUFFERS-1))
#define MB_BUFFERSIZE	512		/* message length limit */

static char fb_mb_buffers[MB_BUFFERS][MB_BUFFERSIZE];
static volatile unsigned int fb_mb_initialized = 0;
static volatile unsigned int fb_mb_inbuffer = 0;
static volatile unsigned int fb_mb_outbuffer = 0;
// static volatile _Atomic_word fb_mb_overruns = 0;
static volatile unsigned int fb_mb_overruns = 0;
static pthread_t fb_mb_writer_thread;
static pthread_mutex_t fb_mb_write_lock;
static pthread_cond_t fb_mb_ready_cond;

static void
fb_mb_flush()
{
	/* called WITHOUT the fb_mb_write_lock */
	while (fb_mb_outbuffer != fb_mb_inbuffer) {
		fputs(fb_mb_buffers[fb_mb_outbuffer], stderr);
		fb_mb_outbuffer = MB_NEXT(fb_mb_outbuffer);
	}
}

static void *
fb_mb_thread_func(void *arg)
{
	/* The mutex is only to eliminate collisions between multiple
	 * writer threads and protect the condition variable. */
// 	pthread_mutex_lock(&fb_mb_write_lock);

	while (fb_mb_initialized) {
// 		pthread_cond_wait(&fb_mb_ready_cond, &fb_mb_write_lock);
// 
// 		/* releasing the mutex reduces contention */
// 		pthread_mutex_unlock(&fb_mb_write_lock);
// 		fb_mb_flush();
// 		pthread_mutex_lock(&fb_mb_write_lock);
	}

// 	pthread_mutex_unlock(&fb_mb_write_lock);

	return NULL;
}

void 
freebob_messagebuffer_init ()
{
	if (fb_mb_initialized)
		return;
		
	fprintf(stderr, "Freebob message buffer init\n");

	pthread_mutex_init(&fb_mb_write_lock, NULL);
	pthread_cond_init(&fb_mb_ready_cond, NULL);

 	fb_mb_overruns = 0;
 	fb_mb_initialized = 1;

	if (pthread_create(&fb_mb_writer_thread, NULL, &fb_mb_thread_func, NULL) != 0)
 		fb_mb_initialized = 0;
}

void 
freebob_messagebuffer_exit ()
{
	if (!fb_mb_initialized)
		return;

	pthread_mutex_lock(&fb_mb_write_lock);
	fb_mb_initialized = 0;
	pthread_cond_signal(&fb_mb_ready_cond);
	pthread_mutex_unlock(&fb_mb_write_lock);

	pthread_join(fb_mb_writer_thread, NULL);
	fb_mb_flush();

	if (fb_mb_overruns)
		fprintf(stderr, "WARNING: %d message buffer overruns!\n",
			fb_mb_overruns);
	else
		fprintf(stderr, "no message buffer overruns\n");

	pthread_mutex_destroy(&fb_mb_write_lock);
	pthread_cond_destroy(&fb_mb_ready_cond);
}


void 
freebob_messagebuffer_add (const char *fmt, ...)
{
	char msg[MB_BUFFERSIZE];
	va_list ap;

	/* format the message first, to reduce lock contention */
	va_start(ap, fmt);
	vsnprintf(msg, MB_BUFFERSIZE, fmt, ap);
	va_end(ap);

	if (!fb_mb_initialized) {
		/* Unable to print message with realtime safety.
		 * Complain and print it anyway. */
		fprintf(stderr, "ERROR: messagebuffer not initialized: %s",
			msg);
		return;
	}

	if (pthread_mutex_trylock(&fb_mb_write_lock) == 0) {
		strncpy(fb_mb_buffers[fb_mb_inbuffer], msg, MB_BUFFERSIZE);
		fb_mb_inbuffer = MB_NEXT(fb_mb_inbuffer);
		pthread_cond_signal(&fb_mb_ready_cond);
		pthread_mutex_unlock(&fb_mb_write_lock);
	} else {			/* lock collision */
// 		atomic_add(&fb_mb_overruns, 1);
		// FIXME: atomicity
		fb_mb_overruns++; // skip the atomicness for now
	}
}

void 
freebob_messagebuffer_va_add (const char *fmt, va_list ap)
{
	char msg[MB_BUFFERSIZE];
	
	/* format the message first, to reduce lock contention */
	vsnprintf(msg, MB_BUFFERSIZE, fmt, ap);

	if (!fb_mb_initialized) {
		/* Unable to print message with realtime safety.
		 * Complain and print it anyway. */
		fprintf(stderr, "ERROR: messagebuffer not initialized: %s",
			msg);
		return;
	}

	if (pthread_mutex_trylock(&fb_mb_write_lock) == 0) {
		strncpy(fb_mb_buffers[fb_mb_inbuffer], msg, MB_BUFFERSIZE);
		fb_mb_inbuffer = MB_NEXT(fb_mb_inbuffer);
		pthread_cond_signal(&fb_mb_ready_cond);
		pthread_mutex_unlock(&fb_mb_write_lock);
	} else {			/* lock collision */
// 		atomic_add(&fb_mb_overruns, 1);
		// FIXME: atomicity
		fb_mb_overruns++; // skip the atomicness for now
	}
}
