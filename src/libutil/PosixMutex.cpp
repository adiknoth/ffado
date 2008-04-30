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

// disable collision tracing for non-debug builds
#ifndef DEBUG
    #undef DEBUG_LOCK_COLLISION_TRACING
    #define DEBUG_LOCK_COLLISION_TRACING 0
#endif

// check whether backtracing is enabled
#if DEBUG_LOCK_COLLISION_TRACING
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
    #ifdef DEBUG
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    #else
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
    #endif
    pthread_mutex_init(&m_mutex, &attr);

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
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p) lock\n", this);
    #if DEBUG_LOCK_COLLISION_TRACING
    if(TryLock()) {
        // locking succeeded
        m_locked_by = debugBacktraceGet(1);
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "%p has lock\n",
                    m_locked_by);
        return;
    } else {
        void *lock_try_by = debugBacktraceGet(1);
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "Lock collision: %p wants lock, %p has lock\n",
                    lock_try_by);
        pthread_mutex_lock(&m_mutex);
    }
    #else
    pthread_mutex_lock(&m_mutex);
    #endif
}

bool
PosixMutex::TryLock()
{
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p) trying to lock\n", this);
    return pthread_mutex_trylock(&m_mutex) == 0;
}

void
PosixMutex::Unlock()
{
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p) unlock\n", this);
    pthread_mutex_unlock(&m_mutex);
}

void
PosixMutex::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "(%p) mutex\n", this);
}


} // end of namespace
