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

/*
 * Copied from the jackd/jackdmp sources
 * function names changed in order to avoid naming problems when using this in
 * a jackd backend.
 */

/* Original license:
 *
 *  Copyright (C) 2001 Paul Davis
 *  Copyright (C) 2004-2006 Grame
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __FFADO_POSIXTHREAD__
#define __FFADO_POSIXTHREAD__

#include "Thread.h"
#include <pthread.h>
#include "PosixMutex.h"

namespace Util
{

/*!
\brief The POSIX thread base class.
*/

class PosixThread : public Thread
{

    protected:

        pthread_t fThread;
        int fPriority;
        bool fRealTime;
        volatile bool fRunning;
        int fCancellation;

        pthread_mutex_t handler_active_lock;
        pthread_cond_t handler_active_cond;
        int handler_active;

        static void* ThreadHandler(void* arg);
        Util::Mutex &m_lock;
    public:

        PosixThread(RunnableInterface* runnable, bool real_time, int priority, int cancellation)
                : Thread(runnable), fThread((pthread_t)NULL), fPriority(priority), fRealTime(real_time), fRunning(false), fCancellation(cancellation)
                , handler_active(0)
                , m_lock(*(new Util::PosixMutex("THREAD")))
        { pthread_mutex_init(&handler_active_lock, NULL); 
          pthread_cond_init(&handler_active_cond, NULL);
        }
        PosixThread(RunnableInterface* runnable)
                : Thread(runnable), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(PTHREAD_CANCEL_DEFERRED)
                , handler_active(0)
                , m_lock(*(new Util::PosixMutex("THREAD")))
        { pthread_mutex_init(&handler_active_lock, NULL); 
          pthread_cond_init(&handler_active_cond, NULL);
        }
        PosixThread(RunnableInterface* runnable, int cancellation)
                : Thread(runnable), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(cancellation)
                , handler_active(0)
                , m_lock(*(new Util::PosixMutex("THREAD")))
        { pthread_mutex_init(&handler_active_lock, NULL); 
          pthread_cond_init(&handler_active_cond, NULL);
        }

        PosixThread(RunnableInterface* runnable, std::string id, bool real_time, int priority, int cancellation)
                : Thread(runnable, id), fThread((pthread_t)NULL), fPriority(priority), fRealTime(real_time), fRunning(false), fCancellation(cancellation)
                , handler_active(0)
                , m_lock(*(new Util::PosixMutex(id)))
        { pthread_mutex_init(&handler_active_lock, NULL); 
          pthread_cond_init(&handler_active_cond, NULL);
        }
        PosixThread(RunnableInterface* runnable, std::string id)
                : Thread(runnable, id), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(PTHREAD_CANCEL_DEFERRED)
                , handler_active(0)
                , m_lock(*(new Util::PosixMutex(id)))
        { pthread_mutex_init(&handler_active_lock, NULL); 
          pthread_cond_init(&handler_active_cond, NULL);
        }
        PosixThread(RunnableInterface* runnable, std::string id, int cancellation)
                : Thread(runnable, id), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(cancellation)
                , handler_active(0)
                , m_lock(*(new Util::PosixMutex(id)))
        { pthread_mutex_init(&handler_active_lock, NULL); 
          pthread_cond_init(&handler_active_cond, NULL);
        }

        virtual ~PosixThread()
        { delete &m_lock; 
          pthread_mutex_destroy(&handler_active_lock); 
          pthread_cond_destroy(&handler_active_cond);
        }

        virtual int Start();
        virtual int Kill();
        virtual int Stop();

        virtual int AcquireRealTime();
        virtual int AcquireRealTime(int priority);
        virtual int DropRealTime();

        pthread_t GetThreadID();

    protected:

};

} // end of namespace


#endif // __FFADO_POSIXTHREAD__
