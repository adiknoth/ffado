/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* 
 * Debug and statistics helper
 *
 */ 

#ifndef __FREEBOB_DEBUG_H__
#define __FREEBOB_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libfreebob/freebob_streaming.h"

#include <libgen.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "messagebuffer.h"

unsigned long getCurrentUTime();

static unsigned long debugGetCurrentUTime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec*1000000+now.tv_usec;
}

// debug print control flags
#define DEBUG_LEVEL_BUFFERS           	(1<<0)
#define DEBUG_LEVEL_HANDLERS			(1<<1)
#define DEBUG_LEVEL_HANDLERS_LOWLEVEL	(1<<2)
#define DEBUG_LEVEL_XRUN_RECOVERY     	(1<<4)
#define DEBUG_LEVEL_WAIT     			(1<<5)

#define DEBUG_LEVEL_RUN_CYCLE         	(1<<8)

#define DEBUG_LEVEL_PACKETCOUNTER		(1<<16)
#define DEBUG_LEVEL_STARTUP				(1<<17)
#define DEBUG_LEVEL_THREADS				(1<<18)
#define DEBUG_LEVEL_STREAMS				(1<<19)

#ifdef DEBUG
//#if 0
	#define printMessage(format, args...) freebob_messagebuffer_add( "FREEBOB MSG: %s:%d (%s): " format, basename(__FILE__), __LINE__, __FUNCTION__, ##args )
	#define printError(format, args...) freebob_messagebuffer_add( "FREEBOB ERR: %s:%d (%s): " format,  basename(__FILE__), __LINE__, __FUNCTION__, ##args )
	
//	#define printEnter() freebob_messagebuffer_add( "FREEBOB ENTERS: %s (%s)\n", __FUNCTION__,  __FILE__)
//	#define printExit() freebob_messagebuffer_add( "FREEBOB EXITS: %s (%s)\n", __FUNCTION__,  __FILE__)
	#define printEnter() 
	#define printExit() 
	
	#define debugError(format, args...) freebob_messagebuffer_add( stderr, "FREEBOB ERR: %s:%d (%s): " format,  basename(__FILE__), __LINE__, __FUNCTION__, ##args )
	#define debugPrint(Level, format, args...) if(DEBUG_LEVEL & (Level))  freebob_messagebuffer_add("DEBUG %s:%d (%s): "  format, basename(__FILE__), __LINE__, __FUNCTION__, ##args );
	
	#define debugPrintShort(Level, format, args...) if(DEBUG_LEVEL & (Level))  freebob_messagebuffer_add( format,##args );
	#define debugPrintWithTimeStamp(Level, format, args...) if(DEBUG_LEVEL & (Level)) freebob_messagebuffer_add( "%16lu: "format, debugGetCurrentUTime(),##args );
	#define SEGFAULT int *test=NULL;	*test=1;

	// default debug level
	// #define DEBUG_LEVEL 0
	// #define DEBUG_LEVEL (DEBUG_LEVEL_BUFFERS | DEBUG_LEVEL_RUN_CYCLE | (DEBUG_LEVEL_XRUN_RECOVERY)| DEBUG_LEVEL_STARTUP | DEBUG_LEVEL_WAIT | DEBUG_LEVEL_PACKETCOUNTER)
	//#define DEBUG_LEVEL (DEBUG_LEVEL_BUFFERS | DEBUG_LEVEL_RUN_CYCLE | (DEBUG_LEVEL_XRUN_RECOVERY)| DEBUG_LEVEL_STARTUP )
	//#define DEBUG_LEVEL (DEBUG_LEVEL_RUN_CYCLE | (DEBUG_LEVEL_XRUN_RECOVERY)| DEBUG_LEVEL_STARTUP | DEBUG_LEVEL_PACKETCOUNTER| DEBUG_LEVEL_WAIT)
	#define DEBUG_LEVEL (  	\
			  	0 * DEBUG_LEVEL_BUFFERS | \
				0 * DEBUG_LEVEL_HANDLERS | \
			       	0 * DEBUG_LEVEL_HANDLERS_LOWLEVEL | \
				1 * DEBUG_LEVEL_XRUN_RECOVERY | \
				0 * DEBUG_LEVEL_WAIT | \
				0 * DEBUG_LEVEL_RUN_CYCLE | \
				1 * DEBUG_LEVEL_PACKETCOUNTER | \
				1 * DEBUG_LEVEL_STARTUP | \
				0 * DEBUG_LEVEL_THREADS | \
				0 * DEBUG_LEVEL_STREAMS \
			    )

	//#define DEBUG_LEVEL (DEBUG_LEVEL_XRUN_RECOVERY | DEBUG_LEVEL_STARTUP | DEBUG_LEVEL_PACKETCOUNTER)


#else
	#define DEBUG_LEVEL
	
	#define printMessage(format, args...) freebob_messagebuffer_add( "FREEBOB MSG: " format, ##args )
	#define printError(format, args...)   freebob_messagebuffer_add( "FREEBOB ERR: " format, ##args )
	
	#define printEnter() 
	#define printExit() 
	
	#define debugError(format, args...) 
	#define debugPrint(Level, format, args...) 
	#define debugPrintShort(Level, format, args...)	
	#define debugPrintWithTimeStamp(Level, format, args...)
#endif

#ifdef __cplusplus
}
#endif

#endif
