/* threads.h
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

#ifndef THREADS_H
#define THREADS_H

#include <semaphore.h>

#include "workerthread.h"

class Functor
{
public:
    virtual void operator() () = 0;
};

////////////////////////////////////////////////////////////////////////

template< typename CalleePtr, typename MemFunPtr >
class MemberFunctor0
    : public Functor
{
public:
    MemberFunctor0( const CalleePtr& pCallee, 
		    MemFunPtr pMemFun, 
		    bool bDelete = true )
        : m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
	, m_pSem( 0 )
	, m_bDelete( bDelete )
        {}

    MemberFunctor0( const CalleePtr& pCallee, 
		    MemFunPtr pMemFun, 
		    sem_t* pSem,
		    bool bDelete = true )
        : m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
	, m_pSem( pSem )
	, m_bDelete( bDelete )
        {}

    virtual ~MemberFunctor0()
        {}

    virtual void operator() ()
        { 
	    ( ( *m_pCallee ).*m_pMemFun )(); 
	    if ( m_pSem ) {
		sem_post( m_pSem);
	    }
	    if (m_bDelete) {
		delete this;
	    }
	}

private:
    CalleePtr  m_pCallee;
    MemFunPtr  m_pMemFun;
    sem_t* m_pSem;
    bool       m_bDelete;
};

template< typename CalleePtr, typename MemFunPtr, typename Parm0 >
class MemberFunctor1
    : public Functor
{
public:
    MemberFunctor1( const CalleePtr& pCallee, 
		    MemFunPtr pMemFun, 
		    Parm0 parm0,
		    bool bDelete = true)
        : m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
	, m_parm0( parm0 )
	, m_pSem( 0 )
	, m_bDelete( bDelete )	
        {}

    MemberFunctor1( const CalleePtr& pCallee, 
		    MemFunPtr pMemFun, 
		    Parm0 parm0,
		    sem_t* pSem,
		    bool bDelete = true )
        : m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
	, m_parm0( parm0 )
	, m_pSem( 0 )
	, m_bDelete( bDelete )
        {}
    virtual ~MemberFunctor1()
        {}

    virtual void operator() ()
        { 
	    ( ( *m_pCallee ).*m_pMemFun )( m_parm0 ); 
	    if ( m_pSem ) {
		sem_post( m_pSem);
	    }
	    if (m_bDelete) {
		delete this;
	    }
	}

private:
    CalleePtr  m_pCallee;
    MemFunPtr  m_pMemFun;
    Parm0      m_parm0;
    sem_t* m_pSem;
    bool       m_bDelete;
};

////////////////////////////////////////////////////////////////////////

template< typename CalleePtr >
class DestroyFunctor
    : public Functor
{
public:
    DestroyFunctor( const CalleePtr& pCallee, 
		    bool bDelete = true )
        : m_pCallee( pCallee )
	, m_bDelete( bDelete )
        {}

    virtual ~DestroyFunctor()
        {}

    virtual void operator() ()
        { 
	    delete m_pCallee;
	    
	    if (m_bDelete) {
		delete this;
	    }
	}

private:
    CalleePtr  m_pCallee;
    bool       m_bDelete;
};

////////////////////////////////////////////////////////////////////////

// 0 params
template< typename CalleePtr, typename Callee,  typename Ret >
inline Functor* deferCall( const CalleePtr& pCallee,
                           Ret( Callee::*pFunction )( ) )
{
    return new MemberFunctor0< CalleePtr, Ret ( Callee::* )( ) >
        ( pCallee,  pFunction );
}

// 1 params
template< typename CalleePtr, typename Callee,  typename Ret, typename Parm0 >
inline Functor* deferCall( const CalleePtr& pCallee,
                           Ret( Callee::*pFunction )( ),
			   Parm0 parm0 )
{
    return new MemberFunctor1< CalleePtr, Ret ( Callee::* )( ), Parm0 >
        ( pCallee,  pFunction, parm0 );
}

////////////////////////////////////////////////////////////////////////

// 0 params
template< typename CalleePtr, typename Callee,  typename Ret >
inline void asyncCall( const CalleePtr& pCallee,
		       Ret( Callee::*pFunction )( ) )
{
    WorkerThread::instance()->addFunctor(new MemberFunctor0< CalleePtr, Ret ( Callee::* )( ) > ( pCallee,  pFunction ));
}

// 1 params
template< typename CalleePtr, typename Callee, typename Ret, typename Parm0 >
inline void asyncCall( const CalleePtr& pCallee,
		       Ret( Callee::*pFunction )( Parm0 ),
		       Parm0 parm0 )
{
    WorkerThread::instance()->addFunctor(new MemberFunctor1< CalleePtr, Ret ( Callee::* )( Parm0 ), Parm0 > ( pCallee,  pFunction, parm0 ));
}

////////////////////////////////////////////////////////////////////////

// 0 params
template< typename CalleePtr, typename Callee,  typename Ret >
inline void syncCall( const CalleePtr& pCallee,
		      Ret( Callee::*pFunction )( ) )
{
    sem_t sem;
    sem_init( &sem, 1, 0 );

    WorkerThread::instance()->addFunctor(new MemberFunctor0< CalleePtr, Ret ( Callee::* )( ) > ( pCallee,  pFunction, &sem ));

    sem_wait( &sem );
    sem_destroy( &sem );
}

////////////////////////////////////////////////////////////////////////

// 0 params
template< typename CalleePtr, typename Callee,  typename Ret >
inline void sleepCall( const CalleePtr& pCallee,
		       Ret( Callee::*pFunction )( ) )
{
    WorkerThread::instance()->addFunctor(new MemberFunctor0< CalleePtr, Ret ( Callee::* )( ) > ( pCallee,  pFunction ), true);
}

// 1 params
template< typename CalleePtr, typename Callee, typename Ret, typename Parm0 >
inline void sleepCall( const CalleePtr& pCallee,
		       Ret( Callee::*pFunction )( Parm0 ),
		       Parm0 parm0 )
{
    WorkerThread::instance()->addFunctor(new MemberFunctor1< CalleePtr, Ret ( Callee::* )( Parm0 ), Parm0 > ( pCallee,  pFunction, parm0 ), true);
}

////////////////////////////////////////////////////////////////////////

// 0 params
template< typename CalleePtr >
inline void destroyCall( const CalleePtr& pCallee )
{
    WorkerThread::instance()->addFunctor(new DestroyFunctor< CalleePtr> ( pCallee));
}


#endif


