/* debugmodule.h
 * Copyright (C) 2004,05 by Daniel Wagner, Pieter Palmers
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
#include <libraw1394/raw1394.h>

// debug print control flags
#define DEBUG_LEVEL_INFO            (1<<0)
#define DEBUG_LEVEL_DEVICE          (1<<1)
#define DEBUG_LEVEL_SUBUNIT         (1<<2)
#define DEBUG_LEVEL_DESCRIPTOR      (1<<3)
#define DEBUG_LEVEL_INFOBLOCK       (1<<4)
#define DEBUG_LEVEL_TRANSFERS       (1<<5)

#define DEBUG_LEVEL_SCHEDULER       (1<<6)
#define DEBUG_LEVEL_IPC	            (1<<7)

// convenience defines
#define DEBUG_LEVEL_ALL      (  DEBUG_LEVEL_INFO \
			      | DEBUG_LEVEL_DEVICE \
			      | DEBUG_LEVEL_SUBUNIT \
			      | DEBUG_LEVEL_DESCRIPTOR \
			      | DEBUG_LEVEL_INFOBLOCK \
			      | DEBUG_LEVEL_TRANSFERS \
			      | DEBUG_LEVEL_SCHEDULER \
			      | DEBUG_LEVEL_IPC )
#define DEBUG_LEVEL_LOW      (  DEBUG_LEVEL_INFO \
                              | DEBUG_LEVEL_DEVICE )
#define DEBUG_LEVEL_MODERATE (  DEBUG_LEVEL_INFO \
			      | DEBUG_LEVEL_DEVICE \
			      | DEBUG_LEVEL_SUBUNIT )
#define DEBUG_LEVEL_HIGH     (  DEBUG_LEVEL_MODERATE \
                              | DEBUG_LEVEL_DESCRIPTOR \
                              | DEBUG_LEVEL_INFOBLOCK )

unsigned char toAscii( unsigned char c );
void quadlet2char( quadlet_t quadlet, unsigned char* buff );
void hexDump( unsigned char *data_start, unsigned int length );
void hexDumpQuadlets( quadlet_t *data_start, unsigned int length );

class DebugBase {
 public:
    DebugBase();
    virtual ~DebugBase();

    void setLevel( int level )
	{ m_level = level; }

    virtual void error( const char* file, 
			const char* function, 
			unsigned int line, 
			const char* format, ... ) const = 0;
    virtual void print( int level, 
			const char* file, 
			const char* function, 
			unsigned int line, 
			const char* format, ... ) const = 0;
    virtual void printShort( int level, 
			     const char* format, ... ) const = 0;

 protected:
    int m_level;
};

class DebugStandard : public DebugBase {
 public:
    DebugStandard();
    virtual ~DebugStandard();

    virtual void error( const char* file, 
			const char* function, 
			unsigned int line, 
			const char* format, ... ) const;
    virtual void print( int level, 
			const char* file, 
			const char* function, 
			unsigned int line, 
			const char* format, ... ) const;
    virtual void printShort( int level, 
			     const char* format, ... ) const;
};

class DebugAnsiColor : public DebugBase {
 public:
    DebugAnsiColor();
    virtual ~DebugAnsiColor();

    virtual void error( const char* file, 
			const char* function, 
			unsigned int line, 
			const char* format, ... ) const;
    virtual void print( int level, 
			const char* file, 
			const char* function, 
			unsigned int line, 
			const char* format, ... ) const;
    virtual void printShort( int level, 
			     const char* format, ... ) const;
    
 protected:
    const char* getLevelColor( int level ) const;
};

class DebugHtml : public DebugBase {
 public:
    DebugHtml();
    virtual ~DebugHtml();

    void error( const char* file, 
		const char* function, 
		unsigned int line, 
		const char* format, ... ) const;
    void print( int level, 
		const char* file, 
		const char* function, 
		unsigned int line, 
		const char* format, ... ) const;
    void printShort( int level, 
		     const char* format, ... ) const;

 protected:
    const char* getLevelColor( int level ) const;
};

#ifdef DEBUG

#define setDebugLevel(x) m_debug.setLevel( x )
#define debugError(format, args...) m_debug.error( __FILE__, __FUNCTION__, __LINE__, format, ##args )
#define debugPrint(level, format, args...) m_debug.print( level, __FILE__, __FUNCTION__, __LINE__, format, ##args )
#define debugPrintShort(level, format, args...) m_debug.printShort( level, format, ##args )

#define setGlobalDebugLevel(x) gGlobalDebugModule.setLevel( x )
#define debugGlobalError(format, args...) gGlobalDebugModule.error( __FILE__, __FUNCTION__, __LINE__, format, ##args )
#define debugGlobalPrint(level, format, args...) gGlobalDebugModule.print( level, __FILE__, __FUNCTION__, __LINE__, format, ##args )
#define debugGlobalPrintShort(level, format, args...) gGlobalDebugModule.printShort( level, format, ##args )

// XXX To change the debug module the header has to be edited
// which sucks big time
//#define DECLARE_DEBUG_MODULE DebugAnsiColor m_debug
#define DECLARE_DEBUG_MODULE
extern DebugAnsiColor m_debug;

// For static functions
#define DECLARE_GLOBAL_DEBUG_MODULE DebugAnsiColor gGlobalDebugModule
#define USE_GLOBAL_DEBUG_MODULE extern DebugAnsiColor gGlobalDebugModule

#else /* !DEBUG */

#define setDebugLevel(x)
#define debugError(format, args...)
#define debugPrint(level, format, args...)
#define debugPrint(level, format, args...)
#define debugPrintShort(level, format, args...)

#define DECLARE_DEBUG_MODULE

#endif /* DEBUG */

#endif
