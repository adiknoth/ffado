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

#ifndef __MUTEX__
#define __MUTEX__

namespace Util
{

/**
 * @brief The mutex interface.
 */

class Mutex
{
public:
    Mutex() {};
    virtual ~Mutex() {};

    virtual void Lock() = 0;
    virtual bool TryLock() = 0;
    virtual void Unlock() = 0;

    virtual bool isLocked() = 0;

    virtual void show() = 0;
    virtual void setVerboseLevel(int) = 0;
};


/**
 * @brief A class to implement monitors
 * Locks a mutex when an instance is created,
 * unlocks it as soon as the instance is destroyed.
 * when this class is created on the stack at function
 * entry, this implements a monitor
 */
class MutexLockHelper
{
public:
    MutexLockHelper(Mutex &m)
    : m_mutex( m )
    , m_early_unlocked(false)
      {m.Lock();};
    virtual ~MutexLockHelper()
      {if(!m_early_unlocked) m_mutex.Unlock();};

    /**
     * Allows to unlock the mutex before the object is
     * destroyed
     */
    void earlyUnlock()
      {m_early_unlocked=true; m_mutex.Unlock();};
private:
    Mutex &m_mutex;
    bool   m_early_unlocked;
};

} // end of namespace

#endif
