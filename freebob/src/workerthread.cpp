/* workerthread.cpp
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

#include "threads.h"
#include "workerthread.h"
#include "debugmodule.h"

WorkerThread* WorkerThread::m_pInstance = 0;

WorkerThread::WorkerThread()
{
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
	debugPrint( DEBUG_LEVEL_INFO,  "WorkerThread instance created.\n" );
	m_pInstance = new WorkerThread();
    }
    return m_pInstance;
}

void
WorkerThread::addFunctor( Functor* pFunctor )
{
    pthread_mutex_lock( &m_mutex );
    m_queue.push( pFunctor );
    pthread_mutex_unlock( &m_mutex );
    pthread_cond_signal( &m_cond );
}

void
WorkerThread::run()
{
    while ( true ) {
	pthread_mutex_lock( &m_mutex );
        if ( m_queue.empty() ) {
            debugPrint( DEBUG_LEVEL_INFO, "Waiting on condition variable.\n" );
            pthread_cond_wait( &m_cond,  &m_mutex );
            debugPrint( DEBUG_LEVEL_INFO, "Awoken from condition wait.\n" );
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
    debugPrint( DEBUG_LEVEL_INFO, "Starting WorkerThread main loop.\n" );
    WorkerThread* pWorkerThread = ( WorkerThread* ) arg;
    pWorkerThread->run();
    debugPrint( DEBUG_LEVEL_INFO, "WorkerThread main loop exited.\n" );
    return NULL;
}
