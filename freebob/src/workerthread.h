/* workerthread.h
 * Copyright (C) 2004 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

#include <queue>

#include <pthread.h>

class Functor;

class WorkerThread {
 public:
    static WorkerThread* instance();

    void addFunctor( Functor* pFunctor );

 protected:
    static void* workerThread( void* arg );
    void run();

 private:
    WorkerThread();
    virtual ~WorkerThread();

    static WorkerThread* m_pInstance;

    pthread_t m_thread;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    std::queue< Functor* > m_queue;
};

#endif
