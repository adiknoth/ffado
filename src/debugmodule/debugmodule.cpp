/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "debugmodule.h"

#include <stdarg.h>
#include <netinet/in.h>

#include <iostream>

#include <time.h>

#define DO_MESSAGE_BUFFER_PRINT

#ifndef DO_MESSAGE_BUFFER_PRINT
	#warning Printing debug info without ringbuffer, not RT-safe!
#endif

using namespace std;

struct ColorEntry  {
    const char* preSequence;
    const char* postSequence;
};

ColorEntry colorTable[] = {
    { "\033[31mFatal",   "\033[0m" },
    { "\033[31mError",   "\033[0m" },
    { "\033[31mWarning", "\033[0m" },
    { "Debug",           ""        },
};


DebugModule::DebugModule( std::string name,  debug_level_t level )
    : m_name( name )
    , m_level( level )
{
    if ( !DebugModuleManager::instance()->registerModule( *this ) ) {
        cerr << "Could not register DebugModule (" << name
             << ") at DebugModuleManager"
             << endl;
    }
}

DebugModule::~DebugModule()
{
//     if ( m_level >= eDL_VeryVerbose ) {
//         cout << "Unregistering "
//              << this->getName()
//              << " at DebugModuleManager"
//              << endl;
//     }
    if ( !DebugModuleManager::instance()->unregisterModule( *this ) ) {
        cerr << "Could not unregister DebugModule at DebugModuleManager"
             << endl;
    }

}

void
DebugModule::printShort( debug_level_t level,
                         const char* format,
                         ... ) const
{
    if ( level > m_level ) {
        return;
    }

    va_list arg;

    va_start( arg, format );

    DebugModuleManager::instance()->va_print( format, arg );

    va_end( arg );
}

void
DebugModule::print( debug_level_t level,
                    const char*   file,
                    const char*   function,
                    unsigned int  line,
                    const char*   format,
                    ... ) const
{
    if ( level > m_level ) {
        return;
    }

    va_list arg;
    va_start( arg, format );

    // remove the path info from the filename
    const char *f = file;
    const char *fname = file;
    while((f=strstr(f, "/"))) {
        f++; // move away from delimiter
        fname=f;
    }
    
    // add a timing timestamp
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t ts_usec=(uint32_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
    
    DebugModuleManager::instance()->print( "%010lu: %s (%s)[%4d] %s: ", 
                 ts_usec, getPreSequence( level ),
                 fname,  line,  function );
    DebugModuleManager::instance()->va_print( format, arg );
    DebugModuleManager::instance()->print( "%s", getPostSequence( level ) );
    va_end( arg );
}

const char*
DebugModule::getPreSequence( debug_level_t level ) const
{
    if ( ( level <= eDL_Normal ) && ( level >= eDL_Fatal ) ) {
        return colorTable[level].preSequence;
    }
    return colorTable[eDL_Normal].preSequence;
}

const char*
DebugModule::getPostSequence( debug_level_t level ) const
{
    if ( ( level <= eDL_Normal ) && ( level >= eDL_Fatal ) ) {
        return colorTable[level].postSequence;
    }
    return colorTable[eDL_Normal].postSequence;
}

//--------------------------------------

DebugModuleManager* DebugModuleManager::m_instance = 0;

DebugModuleManager::DebugModuleManager()
    : mb_initialized(0)
    , mb_inbuffer(0)
    , mb_outbuffer(0)
    , mb_overruns(0)

{

}

DebugModuleManager::~DebugModuleManager()
{
    // cleanin up leftover modules
    for ( DebugModuleVectorIterator it = m_debugModules.begin();
          it != m_debugModules.end();
          ++it )
    {
        fprintf(stderr,"Cleaning up leftover debug module: %s\n",(*it)->getName().c_str());
        m_debugModules.erase( it );
        delete *it;
    }

    if (!mb_initialized)
        return;

    pthread_mutex_lock(&mb_write_lock);
    mb_initialized = 0;
    pthread_cond_signal(&mb_ready_cond);
    pthread_mutex_unlock(&mb_write_lock);

    pthread_join(mb_writer_thread, NULL);
    mb_flush();

    if (mb_overruns)
        fprintf(stderr, "WARNING: %d message buffer overruns!\n",
            mb_overruns);
    else
        fprintf(stderr, "no message buffer overruns\n");

    pthread_mutex_destroy(&mb_write_lock);
    pthread_cond_destroy(&mb_ready_cond);

}

bool
DebugModuleManager::init()
{
    if (mb_initialized)
        return true;

        // if ( m_level >= eDL_VeryVerbose )
        //         cout << "DebugModuleManager init..." << endl;

    pthread_mutex_init(&mb_write_lock, NULL);
    pthread_mutex_init(&mb_flush_lock, NULL);
    pthread_cond_init(&mb_ready_cond, NULL);

     mb_overruns = 0;
     mb_initialized = 1;

    if (pthread_create(&mb_writer_thread, NULL, &mb_thread_func, (void *)this) != 0)
         mb_initialized = 0;

    return true;
}

DebugModuleManager*
DebugModuleManager::instance()
{
    if ( !m_instance ) {
        m_instance = new DebugModuleManager;
        if ( !m_instance ) {
            cerr << "DebugModuleManager::instance Failed to create "
                 << "DebugModuleManager" << endl;
        }
        if ( !m_instance->init() ) {
            cerr << "DebugModuleManager::instance Failed to init "
                 << "DebugModuleManager" << endl;
        }
    }
    return m_instance;
}

bool
DebugModuleManager::registerModule( DebugModule& debugModule )
{
    bool already_present=false;

    for ( DebugModuleVectorIterator it = m_debugModules.begin();
          it != m_debugModules.end();
          ++it )
    {
        if ( *it == &debugModule ) {
            already_present=true;
            return true;
        }
    }

    if (already_present) {
        cerr << "DebugModuleManager::registerModule: Module already registered: "
            << "DebugModule (" << debugModule.getName() << ")" << endl;
    } else {
        m_debugModules.push_back( &debugModule );
    }
    return true;
}

bool
DebugModuleManager::unregisterModule( DebugModule& debugModule )
{

    for ( DebugModuleVectorIterator it = m_debugModules.begin();
          it != m_debugModules.end();
          ++it )
    {
        if ( *it == &debugModule ) {
            m_debugModules.erase( it );
            return true;
        }
    }

    cerr << "DebugModuleManager::unregisterModule: Could not unregister "
         << "DebugModule (" << debugModule.getName() << ")" << endl;
    return false;
}

bool
DebugModuleManager::setMgrDebugLevel( std::string name, debug_level_t level )
{
    for ( DebugModuleVectorIterator it = m_debugModules.begin();
          it != m_debugModules.end();
          ++it )
    {
        if ( (*it)->getName() == name ) {
            return (*it)->setLevel( level );
        }
    }

    cerr << "setDebugLevel: Did not find DebugModule ("
         << name << ")" << endl;
    return false;
}

void
DebugModuleManager::flush()
{
//     mb_flush();
}

void
DebugModuleManager::mb_flush()
{
    /* called WITHOUT the mb_write_lock */
    
    /* the flush lock is to allow a flush from multiple threads 
     * this allows a code section that outputs a lot of debug messages
     * and that can be blocked to flush the buffer itself such that it 
     * does not overflow.
     */
    DebugModuleManager *m=DebugModuleManager::instance();
    pthread_mutex_lock(&m->mb_flush_lock);
    while (mb_outbuffer != mb_inbuffer) {
        fputs(mb_buffers[mb_outbuffer], stderr);
        mb_outbuffer = MB_NEXT(mb_outbuffer);
    }
    pthread_mutex_unlock(&m->mb_flush_lock);
}

void *
DebugModuleManager::mb_thread_func(void *arg)
{

    DebugModuleManager *m=static_cast<DebugModuleManager *>(arg);

    /* The mutex is only to eliminate collisions between multiple
     * writer threads and protect the condition variable. */
     pthread_mutex_lock(&m->mb_write_lock);

    while (m->mb_initialized) {
         pthread_cond_wait(&m->mb_ready_cond, &m->mb_write_lock);

         /* releasing the mutex reduces contention */
         pthread_mutex_unlock(&m->mb_write_lock);
         m->mb_flush();
         pthread_mutex_lock(&m->mb_write_lock);
    }

     pthread_mutex_unlock(&m->mb_write_lock);

    return NULL;
}

void
DebugModuleManager::print(const char *fmt, ...)
{
    char msg[MB_BUFFERSIZE];
    va_list ap;
    unsigned int ntries=5;
    struct timespec wait = {0,50000};
    
    /* format the message first, to reduce lock contention */
    va_start(ap, fmt);
    if (vsnprintf(msg, MB_BUFFERSIZE, fmt, ap) >= MB_BUFFERSIZE) {
        print("WARNING: message truncated!\n");
    }
    va_end(ap);

    if (!mb_initialized) {
        /* Unable to print message with realtime safety.
         * Complain and print it anyway. */
        fprintf(stderr, "ERROR: messagebuffer not initialized: %s",
            msg);
        return;
    }
    
#ifdef DO_MESSAGE_BUFFER_PRINT
    while (ntries) { // try a few times
        if (pthread_mutex_trylock(&mb_write_lock) == 0) {
            strncpy(mb_buffers[mb_inbuffer], msg, MB_BUFFERSIZE);
            mb_inbuffer = MB_NEXT(mb_inbuffer);
            pthread_cond_signal(&mb_ready_cond);
            pthread_mutex_unlock(&mb_write_lock);
            break;
        } else {
            nanosleep(&wait, NULL);
            ntries--;
        }
    }
    if (ntries==0) {  /* lock collision */
	//         atomic_add(&mb_overruns, 1);
        // FIXME: atomicity
        mb_overruns++; // skip the atomicness for now
    }
#else
    fprintf(stderr,msg);
#endif
}

void
DebugModuleManager::va_print (const char *fmt, va_list ap)
{
    char msg[MB_BUFFERSIZE];
    unsigned int ntries=5;
    struct timespec wait = {0,50000};

    /* format the message first, to reduce lock contention */
    if (vsnprintf(msg, MB_BUFFERSIZE, fmt, ap) >= MB_BUFFERSIZE) {
        print("WARNING: message truncated!\n");
    }
    
    if (!mb_initialized) {
        /* Unable to print message with realtime safety.
         * Complain and print it anyway. */
        fprintf(stderr, "ERROR: messagebuffer not initialized: %s",
            msg);
        return;
    }
    
#ifdef DO_MESSAGE_BUFFER_PRINT
    while (ntries) { // try a few times
        if (pthread_mutex_trylock(&mb_write_lock) == 0) {
            strncpy(mb_buffers[mb_inbuffer], msg, MB_BUFFERSIZE);
            mb_inbuffer = MB_NEXT(mb_inbuffer);
            pthread_cond_signal(&mb_ready_cond);
            pthread_mutex_unlock(&mb_write_lock);
            break;
        } else {
            nanosleep(&wait, NULL);
            ntries--;
        }
    }
    
    if (ntries==0) {  /* lock collision */
//         atomic_add(&mb_overruns, 1);
        // FIXME: atomicity
        mb_overruns++; // skip the atomicness for now
    }
#else
    fprintf(stderr,msg);
#endif
}

//----------------------------------------

unsigned char
toAscii( unsigned char c )
{
    if ( ( c > 31 ) && ( c < 126) ) {
        return c;
    } else {
        return '.';
    }
}

/* converts a quadlet to a uchar * buffer
 * not implemented optimally, but clear
 */
void
quadlet2char( quadlet_t quadlet, unsigned char* buff )
{
    *(buff)   = (quadlet>>24)&0xFF;
    *(buff+1) = (quadlet>>16)&0xFF;
    *(buff+2) = (quadlet>> 8)&0xFF;
    *(buff+3) = (quadlet)    &0xFF;
}

void
hexDump( unsigned char *data_start, unsigned int length )
{
    unsigned int i=0;
    unsigned int byte_pos;
    unsigned int bytes_left;

    if ( length <= 0 ) {
        return;
    }
    if ( length >= 7 ) {
        for ( i = 0; i < (length-7); i += 8 ) {
            printf( "%04X: %02X %02X %02X %02X %02X %02X %02X %02X "
                    "- [%c%c%c%c%c%c%c%c]\n",

                    i,

                    *(data_start+i+0),
                    *(data_start+i+1),
                    *(data_start+i+2),
                    *(data_start+i+3),
                    *(data_start+i+4),
                    *(data_start+i+5),
                    *(data_start+i+6),
                    *(data_start+i+7),

                    toAscii( *(data_start+i+0) ),
                    toAscii( *(data_start+i+1) ),
                    toAscii( *(data_start+i+2) ),
                    toAscii( *(data_start+i+3) ),
                    toAscii( *(data_start+i+4) ),
                    toAscii( *(data_start+i+5) ),
                    toAscii( *(data_start+i+6) ),
                    toAscii( *(data_start+i+7) )
                );
        }
    }
    byte_pos = i;
    bytes_left = length - byte_pos;

    printf( "%04X:" ,i );
    for ( i = byte_pos; i < length; i += 1 ) {
        printf( " %02X", *(data_start+i) );
    }
    for ( i=0; i < 8-bytes_left; i+=1 ) {
        printf( "   " );
    }

    printf( " - [" );
    for ( i = byte_pos; i < length; i += 1) {
        printf( "%c", toAscii(*(data_start+i)));
    }
    for ( i = 0; i < 8-bytes_left; i += 1) {
        printf( " " );
    }

    printf( "]" );
    printf( "\n" );
}

void
hexDumpQuadlets( quadlet_t *data, unsigned int length )
{
    unsigned int i=0;

    if ( length <= 0 ) {
        return;
    }
    for (i = 0; i< length; i += 1) {
        printf( "%02d %04X: %08X (%08X)"
                "\n", i, i*4, data[i],ntohl(data[i]));
    }
}


