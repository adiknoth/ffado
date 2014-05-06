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

#include "config_debug.h"

#include "../fbtypes.h"
#include <assert.h>

#include <cstdio>
#include <vector>
#include <iostream>
#include <stdint.h>
#include <semaphore.h>

#define FFADO_ASSERT(x) { \
    if(!(x)) { \
        m_debugModule.print( DebugModule::eDL_Fatal,        \
                                __FILE__,                   \
                                __FUNCTION__,               \
                                __LINE__,                   \
                                "Assertion failed...\n");      \
        debugPrintBacktrace( 10 ); \
        DebugModuleManager::instance()->flush(); \
        assert(x); \
    }}

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

/* MB_NEXT() relies on the fact that MB_BUFFERS is a power of two */
#define MB_NEXT(index)      (((index)+1) & (DEBUG_MB_BUFFERS-1))
#define MB_BUFFERSIZE       DEBUG_MAX_MESSAGE_LENGTH

// no backtrace support when not debugging
#ifndef DEBUG
    #undef DEBUG_BACKTRACE_SUPPORT
    #define DEBUG_BACKTRACE_SUPPORT 0
#endif

// no backlog support when not debugging
#ifndef DEBUG
    #undef DEBUG_BACKLOG_SUPPORT
    #define DEBUG_BACKLOG_SUPPORT 0
#endif

// the backlog is a similar buffer as the message buffer
#define DEBUG_BACKLOG_MB_NEXT(index)  (((index)+1) & (DEBUG_BACKLOG_MB_BUFFERS-1))
#define DEBUG_BACKLOG_MIN_LEVEL       DEBUG_LEVEL_VERY_VERBOSE

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
// for debug messages
#ifdef DEBUG_MESSAGES
#define printMessage( format, args... )                             \
                m_debugModule.print( DebugModule::eDL_Message,      \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                    format,                         \
                                    ##args )
#else
#define printMessage( format, args... )                             \
                m_debugModule.printShort( DebugModule::eDL_Message,      \
                                    format,                         \
                                    ##args )
#endif
#define printMessageShort( format, args... )                        \
                m_debugModule.printShort( DebugModule::eDL_Message, \
                                     format,                        \
                                     ##args )

#define DECLARE_DEBUG_MODULE static DebugModule m_debugModule
#define DECLARE_DEBUG_MODULE_REFERENCE DebugModule &m_debugModule
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

#if DEBUG_BACKLOG_SUPPORT

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

#ifdef DEBUG_MESSAGES

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
    #define DEBUG_NORMAL( x ) x;

    #if DEBUG_EXTREME_ENABLE
        #define debugOutputExtreme( level, format, args... )           \
                    m_debugModule.print( level,                        \
                                        __FILE__,                     \
                                        __FUNCTION__,                 \
                                        __LINE__,                     \
                                        format,                       \
                                        ##args )
        #define debugOutputShortExtreme( level, format, args... )      \
                    m_debugModule.printShort( level,                   \
                                        format,                       \
                                        ##args )
        #define DEBUG_EXTREME( x ) x;
    #else
        #define debugOutputExtreme( level, format, args... )
        #define debugOutputShortExtreme( level, format, args... )
        #define DEBUG_EXTREME( x )
    #endif

#else

    #define debugOutput( level, format, args... )
    #define debugOutputShort( level, format, args... )
    #define DEBUG_NORMAL( x )

    #define debugOutputExtreme( level, format, args... )
    #define debugOutputShortExtreme( level, format, args... )
    #define DEBUG_EXTREME( x )

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

/*
 * Backtrace support
 */
#if DEBUG_BACKTRACE_SUPPORT
    #define debugPrintBacktrace( _SIZE_ )                       \
        DebugModuleManager::instance()->printBacktrace( _SIZE_ );
    #define debugBacktraceGet( _ID_ )                       \
        DebugModuleManager::instance()->getBacktracePtr( _ID_ );
    #define debugGetFunctionNameFromAddr( _ADDR_, _BUFF_, _MAX_SIZE_ )              \
        DebugModuleManager::instance()->getFunctionName( _ADDR_, _BUFF_, _MAX_SIZE_ );
#else
    #define debugPrintBacktrace( _SIZE_ )
    #define debugBacktraceGet( _ID_ )       NULL 
    #define debugGetFunctionNameFromAddr( _ADDR_, _BUFF_, _MAX_SIZE_ )
#endif

/*
 * helper functions
 */

unsigned char toAscii( unsigned char c );
void quadlet2char( fb_quadlet_t quadlet, unsigned char* buff );
void hexDump( unsigned char *data_start, unsigned int length );
void hexDumpQuadlets( quadlet_t *data_start, unsigned int length );

class DebugModuleManager;

class DebugModule {
public:
    friend class DebugModuleManager;

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
                     ... ) const
#ifdef __GNUC__
            __attribute__((format(printf, 3, 4)))
#endif
            ;

    void print( debug_level_t level,
                const char*   file,
                const char*   function,
                unsigned int  line,
                const char*   format,
                ... ) const
#ifdef __GNUC__
            __attribute__((format(printf, 6, 7)))
#endif
            ;

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
    DebugModuleManager* m_manager;
};


class DebugModuleManager {
public:
    friend class DebugModule;

    static DebugModuleManager* instance();
    ~DebugModuleManager();

    bool setMgrDebugLevel( std::string name, debug_level_t level );

    void flush();

#if DEBUG_BACKLOG_SUPPORT
    // the backlog is a ringbuffer of all the messages
    // that have been recorded using the debugPrint
    // statements, regardless of the debug level.
    // This is useful to obtain more debug info
    // when something goes wrong without having too 
    // much output in normal operation
    void showBackLog();
    void showBackLog(int nblines);
#endif

#if DEBUG_BACKTRACE_SUPPORT
    void printBacktrace(int len);
    void *getBacktracePtr(int id);
    void getFunctionName( void *, char *, int );
#endif

protected:
    bool registerModule( DebugModule& debugModule );
    bool unregisterModule( DebugModule& debugModule );

    bool init();

    void print(const char *msg);

#if DEBUG_BACKLOG_SUPPORT
    void backlog_print(const char *msg);
#endif

private:
    DebugModuleManager();

    typedef std::vector< DebugModule* > DebugModuleVector;
    typedef std::vector< DebugModule* >::iterator DebugModuleVectorIterator;

    unsigned int mb_initialized;

#if DEBUG_USE_MESSAGE_BUFFER
    char mb_buffers[DEBUG_MB_BUFFERS][MB_BUFFERSIZE];
    unsigned int mb_inbuffer;
    unsigned int mb_outbuffer;
    unsigned int mb_overruns;
    pthread_t mb_writer_thread;
    pthread_mutex_t mb_write_lock;
    pthread_mutex_t mb_flush_lock;
    sem_t        mb_writes;
#endif

#if DEBUG_BACKTRACE_SUPPORT
    pthread_mutex_t m_backtrace_lock;
    char m_backtrace_strbuffer[MB_BUFFERSIZE];
    void *m_backtrace_buffer[DEBUG_MAX_BACKTRACE_LENGTH];
    void *m_backtrace_buffer_seen[DEBUG_MAX_BACKTRACE_FUNCTIONS_SEEN];
    int m_backtrace_buffer_nb_seen;
#endif

    static void *mb_thread_func(void *arg);
    void mb_flush();

#if DEBUG_BACKLOG_SUPPORT
    // the backlog
    char bl_mb_buffers[DEBUG_BACKLOG_MB_BUFFERS][MB_BUFFERSIZE];
    unsigned int bl_mb_inbuffer;
    pthread_mutex_t bl_mb_write_lock;
#endif

    static DebugModuleManager* m_instance;
    DebugModuleVector          m_debugModules;
};

#endif
