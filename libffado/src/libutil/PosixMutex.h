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

#ifndef __POSIX_MUTEX__
#define __POSIX_MUTEX__



#include "Mutex.h"
#include <pthread.h>

#include "debugmodule/debugmodule.h"

namespace Util
{

/**
 * @brief The POSIX mutex implementation.
 */

class PosixMutex : public Mutex
{
public:
    PosixMutex();
    virtual ~PosixMutex();

    virtual void Lock();
    virtual bool TryLock();
    virtual void Unlock();

    virtual bool isLocked();

    virtual void show();
    virtual void setVerboseLevel(int l) {setDebugLevel(l);};

protected:
    DECLARE_DEBUG_MODULE;

private:
    pthread_mutex_t m_mutex;

    #if DEBUG_LOCK_COLLISION_TRACING
    void *m_locked_by;
    #endif
};

} // end of namespace

#endif
