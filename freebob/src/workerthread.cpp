/* workerthread.cpp
 * Copyright (C) 2004,05 by Daniel Wagner
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

#include "threads.h"
#include "workerthread.h"

WorkerThread* WorkerThread::m_pInstance = 0;

WorkerThread::WorkerThread()
{
    setDebugLevel( DEBUG_LEVEL_SCHEDULER );

    pthread_create( &m_thread, NULL, workerThread, this );
    pthread_mutex_init( &m_mutex, NULL );
    pthread_cond_init( &m_cond,  NULL );
}

WorkerThread::~WorkerThread()
{
    while ( !m_queue.empty() ) {
        Functor* pFunctor = m_queue.front();
        m_queue.pop();
        delete pFunctor;
    }
    pthread_cancel (m_thread);
    pthread_join (m_thread, NULL);
    pthread_cond_destroy( &m_cond );
    pthread_mutex_destroy( &m_mutex );
}

WorkerThread*
WorkerThread::instance()
{
    // We assume here is no race condition
    // because the hole driver is single threaded...
    if ( !m_pInstance ) {
	m_pInstance = new WorkerThread();
    }
    return m_pInstance;
}

void
WorkerThread::addFunctor( Functor* pFunctor, bool sleeper )
{
    pthread_mutex_lock( &m_mutex );
    if ( !sleeper ) {
        m_queue.push( pFunctor );
    } else {
        m_sleepQueue.push( pFunctor );
    }
    pthread_mutex_unlock( &m_mutex );
    pthread_cond_signal( &m_cond );
}

bool
WorkerThread::wakeSleepers()
{
    debugPrint( DEBUG_LEVEL_SCHEDULER,
                "Wake sleeping functors (nr = %d).\n",
                m_sleepQueue.size() );
    while ( !m_sleepQueue.empty() ) {
        pthread_mutex_lock( &m_mutex );
        Functor* pFunctor = m_sleepQueue.front();
        m_sleepQueue.pop();
        m_queue.push( pFunctor );
        pthread_mutex_unlock( &m_mutex );
    }
    pthread_cond_signal( &m_cond );
    return true;
}

void
WorkerThread::run()
{
    while ( true ) {
	pthread_mutex_lock( &m_mutex );
        if ( m_queue.empty() ) {
            debugPrint( DEBUG_LEVEL_SCHEDULER,
                        "Waiting on condition variable.\n" );
            pthread_cond_wait( &m_cond,  &m_mutex );
            debugPrint( DEBUG_LEVEL_SCHEDULER,
                        "Awoken from condition wait.\n" );
        }
	pthread_mutex_unlock( &m_mutex );

	while ( !m_queue.empty() ) {
	    pthread_mutex_lock( &m_mutex );
	    Functor* pFunctor = m_queue.front();
	    m_queue.pop();
	    pthread_mutex_unlock( &m_mutex );

	    ( *pFunctor )();

	    pthread_testcancel();
	}
    }
}

void*
WorkerThread::workerThread( void* arg )
{
    WorkerThread* pWorkerThread = ( WorkerThread* ) arg;
    pWorkerThread->run();
    return NULL;
}
