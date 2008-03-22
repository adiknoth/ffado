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
}

PosixMutex::~PosixMutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void
PosixMutex::Lock()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) lock\n", this);
    pthread_mutex_lock(&m_mutex);
}

bool
PosixMutex::TryLock()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) trying to lock\n", this);
    return pthread_mutex_trylock(&m_mutex) == 0;
}

void
PosixMutex::Unlock()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) unlock\n", this);
    pthread_mutex_unlock(&m_mutex);
}

void
PosixMutex::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "(%p) mutex\n", this);
}


} // end of namespace
