/*
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

#include "PosixMutex.h"
#include <errno.h>

// disable collision tracing for non-debug builds
#ifndef DEBUG
    #undef DEBUG_LOCK_COLLISION_TRACING
    #define DEBUG_LOCK_COLLISION_TRACING 0
#endif

// check whether backtracing is enabled
#if DEBUG_LOCK_COLLISION_TRACING
    #define DEBUG_LOCK_COLLISION_TRACING_INDEX 2
    #define DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN 64
    #if DEBUG_BACKTRACE_SUPPORT
    // ok
    #else
        #error cannot enable lock tracing without backtrace support
    #endif
#endif

namespace Util
{

IMPL_DEBUG_MODULE( PosixMutex, PosixMutex, DEBUG_LEVEL_NORMAL );

PosixMutex::PosixMutex()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    #ifdef DEBUG
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    #else
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
    #endif
    pthread_mutex_init(&m_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    #if DEBUG_LOCK_COLLISION_TRACING
    m_locked_by = NULL;
    #endif
}

PosixMutex::~PosixMutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void
PosixMutex::Lock()
{
    int err;
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "(%p) lock\n", this);
    #if DEBUG_LOCK_COLLISION_TRACING
    if(TryLock()) {
        // locking succeeded
        m_locked_by = debugBacktraceGet( DEBUG_LOCK_COLLISION_TRACING_INDEX );

        char name[ DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN ];
        name[0] = 0;
        debugGetFunctionNameFromAddr(m_locked_by, name, DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN);

        debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                    "(%p) %s obtained lock\n",
                    this, name);
        return;
    } else {
        void *lock_try_by = debugBacktraceGet( DEBUG_LOCK_COLLISION_TRACING_INDEX );

        char name1[ DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN ];
        name1[0] = 0;
        debugGetFunctionNameFromAddr(lock_try_by, name1, DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN);
        char name2[ DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN ];
        name2[0] = 0;
        debugGetFunctionNameFromAddr(m_locked_by, name2, DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN);

        debugWarning("(%p) lock collision: %s wants lock, %s has lock\n",
                    this, name1, name2);
        if((err = pthread_mutex_lock(&m_mutex))) {
            if (err == EDEADLK) {
                debugError("Resource deadlock detected\n");
                debugPrintBacktrace(10);
            } else {
                debugError("Error locking the mutex: %d\n", err);
            }
        } else {
            debugWarning("(%p) lock collision: %s got lock (from %s?)\n",
                        this, name1, name2);
        }
    }
    #else
    #ifdef DEBUG
    if((err = pthread_mutex_lock(&m_mutex))) {
        if (err == EDEADLK) {
            debugError("Resource deadlock detected\n");
            debugPrintBacktrace(10);
        } else {
            debugError("Error locking the mutex: %d\n", err);
        }
    } else {
        debugWarning("(%p) lock collision\n", this);
    }
    #else
    pthread_mutex_lock(&m_mutex);
    #endif
    #endif
}

bool
PosixMutex::TryLock()
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "(%p) trying to lock\n", this);
    return pthread_mutex_trylock(&m_mutex) == 0;
}

bool
PosixMutex::isLocked()
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "(%p) checking lock\n", this);
    int res=pthread_mutex_trylock(&m_mutex);
    if(res == 0) {
        pthread_mutex_unlock(&m_mutex);
        return false;
    } else {
        if(res != EBUSY) {
            debugError("Bogus error code: %d\n", res);
        }
        return true;
    }
}

void
PosixMutex::Unlock()
{
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE, "(%p) unlock\n", this);
    #if DEBUG_LOCK_COLLISION_TRACING
    // unlocking
    m_locked_by = NULL;
    void *unlocker = debugBacktraceGet( DEBUG_LOCK_COLLISION_TRACING_INDEX );
    char name[ DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN ];
    name[0] = 0;
    debugGetFunctionNameFromAddr(unlocker, name, DEBUG_LOCK_COLLISION_TRACING_NAME_MAXLEN);
    debugOutput(DEBUG_LEVEL_ULTRA_VERBOSE,
                "(%p) %s releases lock\n",
                this, name);
    #endif

    #ifdef DEBUG
    int err;
    if((err = pthread_mutex_unlock(&m_mutex))) {
        debugError("Error unlocking the mutex: %d\n", err);
    }
    #else
    pthread_mutex_unlock(&m_mutex);
    #endif
}

void
PosixMutex::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "(%p) mutex (%s)\n", this, (isLocked() ? "Locked" : "Unlocked"));
}


} // end of namespace
