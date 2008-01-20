/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef DEBUGMODULE_H
#define DEBUGMODULE_H

#include "config.h"

#include "../fbtypes.h"
#include <assert.h>

#include <vector>
#include <iostream>

typedef short debug_level_t;

#define DEBUG_LEVEL_MESSAGE        0
#define DEBUG_LEVEL_FATAL          1
#define DEBUG_LEVEL_ERROR          2
#define DEBUG_LEVEL_WARNING        3
#define DEBUG_LEVEL_NORMAL         4
#define DEBUG_LEVEL_INFO           5
#define DEBUG_LEVEL_VERBOSE        6
#define DEBUG_LEVEL_VERY_VERBOSE   7
#define DEBUG_LEVEL_ULTRA_VERBOSE  8

#define DEBUG_MAX_MESSAGE_LENGTH 512

/* MB_NEXT() relies on the fact that MB_BUFFERS is a power of two */
#define MB_BUFFERS          (1<<16)

#define MB_NEXT(index)      (((index)+1) & (MB_BUFFERS-1))

#define MB_BUFFERSIZE       DEBUG_MAX_MESSAGE_LENGTH

#ifdef DEBUG
    #if DEBUG_IMPLEMENT_BACKLOG
        #define IMPLEMENT_BACKLOG
    #endif
#endif

#ifdef IMPLEMENT_BACKLOG
// the backlog is a similar buffer as the message buffer
#define BACKLOG_MB_BUFFERS      (256)
#define BACKLOG_MB_NEXT(index)  (((index)+1) & (BACKLOG_MB_BUFFERS-1))
#define BACKLOG_MIN_LEVEL       DEBUG_LEVEL_VERY_VERBOSE
#endif

#define debugFatal( format, args... )                               \
                m_debugModule.print( DebugModule::eDL_Fatal,        \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                     format,                        \
                                     ##args )
#define debugError( format, args... )                               \
                m_debugModule.print( DebugModule::eDL_Error,        \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                     format,                        \
                                     ##args )
#define debugWarning( format, args... )                             \
                m_debugModule.print( DebugModule::eDL_Warning,      \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                    format,                         \
                                    ##args )

#define debugFatalShort( format, args... )                          \
                m_debugModule.printShort( DebugModule::eDL_Fatal,   \
                                     format,                        \
                                     ##args )
#define debugErrorShort( format, args... )                          \
                m_debugModule.printShort( DebugModule::eDL_Error,   \
                                     format,                        \
                                     ##args )
#define debugWarningShort( format, args... )                        \
                m_debugModule.printShort( DebugModule::eDL_Warning, \
                                     format,                        \
                                     ##args )

// these are for messages that are also displayed when not compiled
// for debug
#define printMessage( format, args... )                             \
                m_debugModule.print( DebugModule::eDL_Message,      \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                    format,                         \
                                    ##args )
#define printMessageShort( format, args... )                        \
                m_debugModule.printShort( DebugModule::eDL_Message, \
                                     format,                        \
                                     ##args )

#define DECLARE_DEBUG_MODULE static DebugModule m_debugModule
#define IMPL_DEBUG_MODULE( ClassName, RegisterName, Level )        \
                DebugModule ClassName::m_debugModule =             \
                    DebugModule( #RegisterName, Level )

#define DECLARE_GLOBAL_DEBUG_MODULE extern DebugModule m_debugModule
#define IMPL_GLOBAL_DEBUG_MODULE( RegisterName, Level )            \
                DebugModule m_debugModule =                        \
            DebugModule( #RegisterName, Level )

#define setDebugLevel( Level ) {                                    \
                m_debugModule.setLevel( Level ); \
                }

/*                m_debugModule.print( eDL_Normal,                        \
                                     __FILE__,                     \
                                     __FUNCTION__,                 \
                                     __LINE__,                     \
                                     "Setting debug level to %d\n",  \
                                     Level ); \
                }*/

#define getDebugLevel(  )                                     \
                m_debugModule.getLevel( )

#define flushDebugOutput()      DebugModuleManager::instance()->flush()

#ifdef IMPLEMENT_BACKLOG

#define debugShowBackLog() \
    {                                                       \
        m_debugModule.print( DebugModule::eDL_Warning,      \
                             __FILE__,                      \
                             __FUNCTION__,                  \
                             __LINE__,                      \
                             "Backlog print requested\n");  \
        DebugModuleManager::instance()->showBackLog();      \
    }
#define debugShowBackLogLines(x) \
    {                                                       \
        m_debugModule.print( DebugModule::eDL_Warning,      \
                             __FILE__,                      \
                             __FUNCTION__,                  \
                             __LINE__,                      \
                             "Backlog print requested\n");  \
        DebugModuleManager::instance()->showBackLog(x);     \
    }

#else
#define debugShowBackLog()
#define debugShowBackLogLines(x)

#endif

#ifdef DEBUG

    #define debugOutput( level, format, args... )                  \
                m_debugModule.print( level,                        \
                                     __FILE__,                     \
                                     __FUNCTION__,                 \
                                     __LINE__,                     \
                                     format,                       \
                                     ##args )

    #define debugOutputShort( level, format, args... )             \
                m_debugModule.printShort( level,                   \
                                     format,                       \
                                     ##args )

#else

    #define debugOutput( level, format, args... )
    #define debugOutputShort( level, format, args... )

#endif

/* Enable preemption checking for Linux Realtime Preemption kernels.
 *
 * This checks if any RT-safe code section does anything to cause CPU
 * preemption.  Examples are sleep() or other system calls that block.
 * If a problem is detected, the kernel writes a syslog entry, and
 * sends SIGUSR2 to the client.
 */

// #define DO_PREEMPTION_CHECKING

#include <sys/time.h>

#ifdef DO_PREEMPTION_CHECKING
#define CHECK_PREEMPTION(onoff) \
    gettimeofday((struct timeval *)1, (struct timezone *)onoff)
#else
#define CHECK_PREEMPTION(onoff)
#endif

unsigned char toAscii( unsigned char c );
void quadlet2char( fb_quadlet_t quadlet, unsigned char* buff );
void hexDump( unsigned char *data_start, unsigned int length );
void hexDumpQuadlets( quadlet_t *data_start, unsigned int length );

class DebugModule {
public:
    enum {
        eDL_Message      = DEBUG_LEVEL_MESSAGE,
        eDL_Fatal        = DEBUG_LEVEL_FATAL,
        eDL_Error        = DEBUG_LEVEL_ERROR,
        eDL_Warning      = DEBUG_LEVEL_WARNING,
        eDL_Normal       = DEBUG_LEVEL_NORMAL,
        eDL_Info         = DEBUG_LEVEL_INFO,
        eDL_Verbose      = DEBUG_LEVEL_VERBOSE,
        eDL_VeryVerbose  = DEBUG_LEVEL_VERY_VERBOSE,
        eDL_UltraVerbose = DEBUG_LEVEL_ULTRA_VERBOSE,
    } EDebugLevel;

    DebugModule( std::string name, debug_level_t level );
    virtual ~DebugModule();

    void printShort( debug_level_t level,
                     const char* format,
                     ... ) const;

    void print( debug_level_t level,
                const char*   file,
                const char*   function,
                unsigned int  line,
                const char*   format,
                ... ) const;

    bool setLevel( debug_level_t level )
        { m_level = level; return true; }
    debug_level_t getLevel()
        { return m_level; }
    std::string getName()
        { return m_name; }

protected:
    const char* getPreSequence( debug_level_t level ) const;
    const char* getPostSequence( debug_level_t level ) const;

private:
    std::string   m_name;
    debug_level_t m_level;
};


class DebugModuleManager {
public:
    friend class DebugModule;

    static DebugModuleManager* instance();
    ~DebugModuleManager();

    bool setMgrDebugLevel( std::string name, debug_level_t level );

    void flush();

    // the backlog is a ringbuffer of all the messages
    // that have been recorded using the debugPrint
    // statements, regardless of the debug level.
    // This is useful to obtain more debug info
    // when something goes wrong without having too 
    // much output in normal operation
    void showBackLog();
    void showBackLog(int nblines);

protected:
    bool registerModule( DebugModule& debugModule );
    bool unregisterModule( DebugModule& debugModule );

    bool init();

    void print(const char *msg);
    void backlog_print(const char *msg);

private:
    DebugModuleManager();

    typedef std::vector< DebugModule* > DebugModuleVector;
    typedef std::vector< DebugModule* >::iterator DebugModuleVectorIterator;

    char mb_buffers[MB_BUFFERS][MB_BUFFERSIZE];
    unsigned int mb_initialized;
    unsigned int mb_inbuffer;
    unsigned int mb_outbuffer;
    unsigned int mb_overruns;
    pthread_t mb_writer_thread;
    pthread_mutex_t mb_write_lock;
    pthread_mutex_t mb_flush_lock;
    pthread_cond_t mb_ready_cond;

    static void *mb_thread_func(void *arg);
    void mb_flush();

#ifdef IMPLEMENT_BACKLOG
    // the backlog
    char bl_mb_buffers[BACKLOG_MB_BUFFERS][MB_BUFFERSIZE];
    unsigned int bl_mb_inbuffer;
    pthread_mutex_t bl_mb_write_lock;
#endif

    static DebugModuleManager* m_instance;
    DebugModuleVector          m_debugModules;
};

#endif
