/* debugmodule.h
 * Copyright (C) 2004 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef DEBUGMODULE_H
#define DEBUGMODULE_H

#include <stdio.h>
#include "ieee1394service.h"

#define DEBUG_LEVEL_INFO		(1<<0)
#define DEBUG_LEVEL_DEVICE     		(1<<1)
#define DEBUG_LEVEL_SUBUNIT    		(1<<2)
#define DEBUG_LEVEL_DESCRIPTOR     	(1<<3)
#define DEBUG_LEVEL_INFOBLOCK     	(1<<4)

#define DEBUG_LEVEL_TRANSFERS     	(1<<5)

#define DEBUG_LEVEL_ALL  (DEBUG_LEVEL_INFO | DEBUG_LEVEL_DEVICE | DEBUG_LEVEL_SUBUNIT | DEBUG_LEVEL_DESCRIPTOR | DEBUG_LEVEL_INFOBLOCK | DEBUG_LEVEL_TRANSFERS )

#define DEBUG_LEVEL_MODERATE (DEBUG_LEVEL_INFO | DEBUG_LEVEL_DEVICE | DEBUG_LEVEL_SUBUNIT)

#define DEBUG_LEVEL DEBUG_LEVEL_MODERATE 

#define DEBUG

#ifdef DEBUG
        #define debugError(format, args...) fprintf( stderr, "%s %s %d: " format,  __FILE__, __FUNCTION__, __LINE__, ##args )
//	#define debugPrint(Level, format, args...) if(DEBUG_LEVEL & Level) { int idebug=Level; while(idebug) {printf(" "); idebug=idebug>>1;} printf( format, ##args ); }
        #define debugPrint(Level, format, args...) if(DEBUG_LEVEL & Level) printf("DEBUG %s %s %d :"  format, __FILE__, __FUNCTION__, __LINE__, ##args );

#else
	#define debugError(format, args...) 
	#define debugPrint(Level, format, args...) 
#endif

unsigned char toAscii(unsigned char c);
void quadlet2char(quadlet_t quadlet,unsigned char* buff);
void hexDump(unsigned char *data_start, unsigned int length);

#endif
