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

#include "debugmodule.h"

#include <stdarg.h>
#include "libutil/ByteSwap.h"

#include <iostream>

#include <time.h>
#include <string.h>

#if DEBUG_BACKTRACE_SUPPORT
    #include <execinfo.h>
    #include <cxxabi.h>
    #define GNU_SOURCE
    #include <dlfcn.h>
#endif

#if DEBUG_USE_MESSAGE_BUFFER
#else
    #ifdef DEBUG
        #warning Printing debug info without ringbuffer, not RT-safe!
    #else
        #error Printing debug info without ringbuffer, not RT-safe (not allowed for non-debug builds)!
    #endif
#endif

using namespace std;

struct ColorEntry  {
    const char* preSequence;
    const char* postSequence;
};

ColorEntry colorTable[] = {
    { "",           ""        },
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

    if (m_manager && !m_manager->unregisterModule( *this ) ) {
        cerr << "Could not unregister DebugModule at DebugModuleManager"
             << endl;
    }

}

void
DebugModule::printShort( debug_level_t level,
                         const char* format,
                         ... ) const
{

    // bypass for performance
#if DEBUG_BACKLOG_SUPPORT
    if (level > BACKLOG_MIN_LEVEL 
        && level > m_level) {
        return;
    }
#else
    if ( level >m_level ) {
        return;
    }
#endif

    const char *warning = "WARNING: message truncated!\n";
    const int warning_size = 32;
    va_list arg;
    char msg[MB_BUFFERSIZE];

    // format the message such that it remains together
    int chars_written=0;
    int retval=0;

    va_start( arg, format );
    retval = vsnprintf(msg+chars_written, MB_BUFFERSIZE, format, arg);
    va_end( arg );
    if (retval >= 0) {  // ignore errors
        chars_written += retval;
    }

    // output a warning if the message was truncated
    if (chars_written == MB_BUFFERSIZE) {
        snprintf(msg+MB_BUFFERSIZE-warning_size, warning_size, "%s", warning);
    }

#if DEBUG_BACKLOG_SUPPORT
    // print to backlog if necessary
    if (level <= BACKLOG_MIN_LEVEL) {
        DebugModuleManager::instance()->backlog_print( msg );
    }
#endif

    // print to stderr if necessary
    if ( level <= m_level ) {
        DebugModuleManager::instance()->print( msg );
    }
}

void
DebugModule::print( debug_level_t level,
                    const char*   file,
                    const char*   function,
                    unsigned int  line,
                    const char*   format,
                    ... ) const
{
    // bypass for performance
#if DEBUG_BACKLOG_SUPPORT
    if (level > BACKLOG_MIN_LEVEL 
        && level > m_level) {
        return;
    }
#else
    if ( level >m_level ) {
        return;
    }
#endif

    const char *warning = "WARNING: message truncated!\n";
    const int warning_size = 32;

    va_list arg;
    char msg[MB_BUFFERSIZE];

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
    uint64_t ts_usec=(uint64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);

    // format the message such that it remains together
    int chars_written=0;
    int retval=0;
    retval = snprintf(msg, MB_BUFFERSIZE, "%011"PRIu64": %s (%s)[%4u] %s: ", 
                      ts_usec, getPreSequence( level ),
                      fname,  line,  function );
    if (retval >= 0) chars_written += retval; // ignore errors

    va_start( arg, format );
    retval = vsnprintf( msg + chars_written,
                        MB_BUFFERSIZE - chars_written,
                        format, arg);
    va_end( arg );
    if (retval >= 0) chars_written += retval; // ignore errors

    retval = snprintf( msg + chars_written,
                       MB_BUFFERSIZE - chars_written,
                       "%s", getPostSequence( level ) );
    if (retval >= 0) chars_written += retval; // ignore errors

    // output a warning if the message was truncated
    if (chars_written == MB_BUFFERSIZE) {
        snprintf(msg + MB_BUFFERSIZE - warning_size,
                 warning_size,
                 "%s", warning);
    }

#if DEBUG_BACKLOG_SUPPORT
    // print to backlog if necessary
    if (level <= BACKLOG_MIN_LEVEL) {
        DebugModuleManager::instance()->backlog_print( msg );
    }
#endif

    // print to stderr if necessary
    if ( level <= m_level ) {
        DebugModuleManager::instance()->print( msg );
    }
}

const char*
DebugModule::getPreSequence( debug_level_t level ) const
{
    if ( ( level <= eDL_Normal ) && ( level >= eDL_Message ) ) {
        return colorTable[level].preSequence;
    }
    return colorTable[eDL_Normal].preSequence;
}

const char*
DebugModule::getPostSequence( debug_level_t level ) const
{
    if ( ( level <= eDL_Normal ) && ( level >= eDL_Message ) ) {
        return colorTable[level].postSequence;
    }
    return colorTable[eDL_Normal].postSequence;
}

//--------------------------------------

DebugModuleManager* DebugModuleManager::m_instance = 0;

DebugModuleManager::DebugModuleManager()
    : mb_initialized(0)
#if DEBUG_USE_MESSAGE_BUFFER
    , mb_inbuffer(0)
    , mb_outbuffer(0)
    , mb_overruns(0)
#endif
#if DEBUG_BACKTRACE_SUPPORT
    , m_backtrace_buffer_nb_seen(0)
#endif
#if DEBUG_BACKLOG_SUPPORT
    , bl_mb_inbuffer(0)
#endif
{

}

DebugModuleManager::~DebugModuleManager()
{
    // cleaning up leftover modules
    while (!m_debugModules.empty())
    {
        DebugModule *mod = m_debugModules.back();
        unregisterModule(*mod);
    }

    if (!mb_initialized)
        return;

#if DEBUG_USE_MESSAGE_BUFFER
    pthread_mutex_lock(&mb_write_lock);
    mb_initialized = 0;
    sem_post(&mb_writes);
    pthread_mutex_unlock(&mb_write_lock);

    pthread_join(mb_writer_thread, NULL);
    mb_flush();
#endif

#if DEBUG_BACKTRACE_SUPPORT
    pthread_mutex_lock(&m_backtrace_lock);
    // print a list of the symbols seen in a backtrace
    fprintf(stderr, "Backtrace saw %d symbols:\n", m_backtrace_buffer_nb_seen);
    char **strings = backtrace_symbols(m_backtrace_buffer_seen, m_backtrace_buffer_nb_seen);
    if (strings == NULL) {
        perror("backtrace_symbols");
    } else {
        char* outbuf = NULL;
        size_t length;
        int status;
        Dl_info info;
        for (int j = 0; j < m_backtrace_buffer_nb_seen; j++) {
            if (dladdr(m_backtrace_buffer_seen[j], &info) != 0) {
                outbuf = __cxxabiv1::__cxa_demangle(info.dli_sname, outbuf, &length, &status);
                if(outbuf && status == 0) {
                    fprintf(stderr, " %p => %s\n", 
                            m_backtrace_buffer_seen[j], outbuf);
                    free(outbuf);
                    outbuf = NULL;
                } else {
                    fprintf(stderr, " %p => %s (demangle status: %d)\n", 
                            m_backtrace_buffer_seen[j], strings[j], status);
                }
            } else {
                fprintf(stderr, " %p => %s\n", 
                        m_backtrace_buffer_seen[j], strings[j]);
            }
        }
        free(strings);
    }
    pthread_mutex_unlock(&m_backtrace_lock);
#endif

#if DEBUG_USE_MESSAGE_BUFFER
    if (mb_overruns)
        fprintf(stderr, "WARNING: %d message buffer overruns!\n",
            mb_overruns);
    else
        fprintf(stderr, "no message buffer overruns\n");

    pthread_mutex_destroy(&mb_write_lock);
    sem_destroy(&mb_writes);
#endif

#if DEBUG_BACKTRACE_SUPPORT
    pthread_mutex_destroy(&m_backtrace_lock);
#endif

#if DEBUG_BACKLOG_SUPPORT
    pthread_mutex_destroy(&bl_mb_write_lock);
#endif

}

bool
DebugModuleManager::init()
{
    if (mb_initialized)
        return true;

        // if ( m_level >= eDL_VeryVerbose )
        //         cout << "DebugModuleManager init..." << endl;

#if DEBUG_BACKTRACE_SUPPORT
    pthread_mutex_init(&m_backtrace_lock, NULL);
#endif

#if DEBUG_BACKLOG_SUPPORT
    pthread_mutex_init(&bl_mb_write_lock, NULL);
#endif

#if DEBUG_USE_MESSAGE_BUFFER
    pthread_mutex_init(&mb_flush_lock, NULL);
    pthread_mutex_init(&mb_write_lock, NULL);
    sem_init(&mb_writes, 0, 0);

    mb_overruns = 0;

    int res;
    #if DEBUG_MESSAGE_BUFFER_REALTIME
    /* Get the client thread to run as an RT-FIFO
        scheduled thread of appropriate priority.
    */
    pthread_attr_t attributes;
    struct sched_param rt_param;
    pthread_attr_init(&attributes);
    if ((res = pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED))) {
        fprintf(stderr, "Cannot request explicit scheduling for RT thread  %d %s\n", res, strerror(res));
        return -1;
    }
    if ((res = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE))) {
        fprintf(stderr, "Cannot request joinable thread creation for RT thread  %d %s\n", res, strerror(res));
        return -1;
    }
    if ((res = pthread_attr_setscope(&attributes, PTHREAD_SCOPE_SYSTEM))) {
        fprintf(stderr, "Cannot set scheduling scope for RT thread %d %s\n", res, strerror(res));
        return -1;
    }

    if ((res = pthread_attr_setschedpolicy(&attributes, SCHED_FIFO))) {

    //if ((res = pthread_attr_setschedpolicy(&attributes, SCHED_RR))) {
        fprintf(stderr, "Cannot set FIFO scheduling class for RT thread  %d %s\n", res, strerror(res));
        return -1;
    }

    memset(&rt_param, 0, sizeof(rt_param));
    rt_param.sched_priority = DEBUG_MESSAGE_BUFFER_REALTIME_PRIO;

    if ((res = pthread_attr_setschedparam(&attributes, &rt_param))) {
        fprintf(stderr, "Cannot set scheduling priority for RT thread %d %s\n", res, strerror(res));
        return -1;
    }

    mb_initialized = 1; // set to 1 otherwise the thread might exit (race condition)
    if ((res = pthread_create(&mb_writer_thread, &attributes, mb_thread_func, (void *)this))) {
        fprintf(stderr, "Cannot create thread %d %s\n", res, strerror(res));
        mb_initialized = 0;
    }
    #else
    mb_initialized = 1; // set to 1 otherwise the thread might exit (race condition)
    if ((res = pthread_create(&mb_writer_thread, NULL, &mb_thread_func, (void *)this))) {
        fprintf(stderr, "Cannot create thread %d %s\n", res, strerror(res));
        mb_initialized = 0;
    }
    #endif
#endif

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
        if (debugModule.m_manager == NULL)
                debugModule.m_manager = this;
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
            if (debugModule.m_manager == this)
                debugModule.m_manager = NULL;
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
#if DEBUG_USE_MESSAGE_BUFFER
    mb_flush();
#else
    fflush(stderr);
#endif
}

#if DEBUG_USE_MESSAGE_BUFFER
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
    fflush(stderr);
    pthread_mutex_unlock(&m->mb_flush_lock);
}

void *
DebugModuleManager::mb_thread_func(void *arg)
{

    DebugModuleManager *m=static_cast<DebugModuleManager *>(arg);

    while (m->mb_initialized) {
        sem_wait(&m->mb_writes);
        m->mb_flush();
    }

    return NULL;
}
#endif

#if DEBUG_BACKLOG_SUPPORT
void
DebugModuleManager::showBackLog()
{
     DebugModuleManager *m=DebugModuleManager::instance();
    // locking the flush lock ensures that the backlog is
    // printed as one entity
    pthread_mutex_lock(&m->mb_flush_lock);
    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "* BEGIN OF BACKLOG PRINT\n");
    fprintf(stderr, "=====================================================\n");

    for (unsigned int i=0; i<BACKLOG_MB_BUFFERS;i++) {
        unsigned int idx=(i+bl_mb_inbuffer)%BACKLOG_MB_BUFFERS;
        fputs("BL: ", stderr);
        fputs(bl_mb_buffers[idx], stderr);
    }
    fprintf(stderr, "BL: \n");

    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "* END OF BACKLOG PRINT\n");
    fprintf(stderr, "=====================================================\n");
    pthread_mutex_unlock(&m->mb_flush_lock);
}

void
DebugModuleManager::showBackLog(int nblines)
{
     DebugModuleManager *m=DebugModuleManager::instance();
    // locking the flush lock ensures that the backlog is
    // printed as one entity
    pthread_mutex_lock(&m->mb_flush_lock);
    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "* BEGIN OF BACKLOG PRINT\n");
    fprintf(stderr, "=====================================================\n");

    int lines_to_skip = BACKLOG_MB_BUFFERS - nblines;
    if (lines_to_skip < 0) lines_to_skip = 0;
    for (unsigned int i=0; i<BACKLOG_MB_BUFFERS;i++) {
        if (lines_to_skip-- < 0) {
            unsigned int idx=(i+bl_mb_inbuffer)%BACKLOG_MB_BUFFERS;
            fputs("BL: ", stderr);
            fputs(bl_mb_buffers[idx], stderr);
        }
    }
    fprintf(stderr, "BL: \n");

    fprintf(stderr, "=====================================================\n");
    fprintf(stderr, "* END OF BACKLOG PRINT\n");
    fprintf(stderr, "=====================================================\n");
    pthread_mutex_unlock(&m->mb_flush_lock);
}

void
DebugModuleManager::backlog_print(const char *msg)
{
    unsigned int ntries;
    struct timespec wait = {0, DEBUG_MESSAGE_BUFFER_COLLISION_WAIT_NSEC};
    // the backlog
    ntries=DEBUG_MESSAGE_BUFFER_COLLISION_WAIT_NTRIES;
    while (ntries) { // try a few times
        if (pthread_mutex_trylock(&bl_mb_write_lock) == 0) {
            strncpy(bl_mb_buffers[bl_mb_inbuffer], msg, MB_BUFFERSIZE);
            bl_mb_inbuffer = BACKLOG_MB_NEXT(bl_mb_inbuffer);
            pthread_mutex_unlock(&bl_mb_write_lock);
            break;
        } else {
            nanosleep(&wait, NULL);
            ntries--;
        }
    }
    // just bail out should it have failed
}
#endif

void
DebugModuleManager::print(const char *msg)
{
#if DEBUG_USE_MESSAGE_BUFFER
    unsigned int ntries;
    struct timespec wait = {0,50000};

    if (!mb_initialized) {
        /* Unable to print message with realtime safety.
         * Complain and print it anyway. */
        fprintf(stderr, "ERROR: messagebuffer not initialized: %s",
            msg);
        return;
    }

    ntries=6;
    while (ntries) { // try a few times
        if (pthread_mutex_trylock(&mb_write_lock) == 0) {
            strncpy(mb_buffers[mb_inbuffer], msg, MB_BUFFERSIZE);
            mb_inbuffer = MB_NEXT(mb_inbuffer);
            sem_post(&mb_writes);
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

#if DEBUG_BACKTRACE_SUPPORT
void
DebugModuleManager::printBacktrace(int len)
{
    int nptrs;
    int chars_written=0;

    if(len > DEBUG_MAX_BACKTRACE_LENGTH) {
        len = DEBUG_MAX_BACKTRACE_LENGTH;
    }

    pthread_mutex_lock(&m_backtrace_lock);
    nptrs = backtrace(m_backtrace_buffer, len);
    chars_written += snprintf(m_backtrace_strbuffer, MB_BUFFERSIZE-chars_written, "BACKTRACE (%d/%d): ", nptrs, len);

    for (int j = 0; j < nptrs; j++) {
        char name[64];
        name[0]=0;
        getFunctionName(m_backtrace_buffer[j], name, 64);
        chars_written += snprintf(m_backtrace_strbuffer + chars_written, MB_BUFFERSIZE-chars_written, "%s\n", name);
    }
    chars_written += snprintf(m_backtrace_strbuffer + chars_written, MB_BUFFERSIZE-chars_written, "\n");

    // make sure the string is terminated properly
    m_backtrace_strbuffer[MB_BUFFERSIZE-2] = '\n';
    m_backtrace_strbuffer[MB_BUFFERSIZE-1] = 0;
    
    // save the pointers to the pointers-seen list such that we can
    // dump their info later on
    bool seen;
    for (int i=0; i<nptrs; i++) {
        seen = false;
        int j;
        for (j=0; j<m_backtrace_buffer_nb_seen && j < DEBUG_MAX_BACKTRACE_FUNCTIONS_SEEN; j++) {
            if(m_backtrace_buffer_seen[j] == m_backtrace_buffer[i]) {
                seen = true;
                break;
            }
        }
        if (!seen 
           && j < DEBUG_MAX_BACKTRACE_FUNCTIONS_SEEN) {
            m_backtrace_buffer_seen[j] = m_backtrace_buffer[i];
            m_backtrace_buffer_nb_seen++;
        }
    }

    print(m_backtrace_strbuffer);

    pthread_mutex_unlock(&m_backtrace_lock);
}

void *
DebugModuleManager::getBacktracePtr(int id)
{
    int nptrs;
    void *retval = NULL;

    if(id >= DEBUG_MAX_BACKTRACE_LENGTH) {
        return NULL;
    }

    pthread_mutex_lock(&m_backtrace_lock);
    nptrs = backtrace(m_backtrace_buffer, id+1);
    if(id >= nptrs) {
        id = nptrs-1;
    }
    retval = m_backtrace_buffer[id];
    // save the pointers to the pointers-seen list such that we can
    // dump their info later on
    bool seen = false;
    int j;
    for (j=0; j<m_backtrace_buffer_nb_seen && j < DEBUG_MAX_BACKTRACE_FUNCTIONS_SEEN; j++) {
        if(m_backtrace_buffer_seen[j] == m_backtrace_buffer[id]) {
            seen = true;
            break;
        }
    }
    if (!seen
       && j < DEBUG_MAX_BACKTRACE_FUNCTIONS_SEEN) {
        m_backtrace_buffer_seen[j] = m_backtrace_buffer[id];
        m_backtrace_buffer_nb_seen++;
    }
    pthread_mutex_unlock(&m_backtrace_lock);

    return retval;
}

void
DebugModuleManager::getFunctionName(void *ptr, char *buff, int len)
{
    char* outbuf = NULL;
    size_t length;
    int status;
    Dl_info info;
    if (dladdr(ptr, &info) != 0) {
        outbuf = abi::__cxa_demangle(info.dli_sname, outbuf, &length, &status);
        if(outbuf && status == 0) {
            if(len < (int)length) {
                strncpy(buff, outbuf, len);
            } else {
                strncpy(buff, outbuf, length);
            }
        } else {
            snprintf(buff, len, "%p (%s)", ptr, info.dli_sname);
        }
    } else {
        snprintf(buff, len, "%p (I-ERR)", ptr);
    }
    if (outbuf) free(outbuf);
}

#endif

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
        fprintf(stderr, "%02d %04X: %08X (%08X)"
                "\n", i, i*4, data[i],CondSwapFromBus32(data[i]));
    }
}


