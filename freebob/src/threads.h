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
    MemberFunctor0( const CalleePtr& pCallee, MemFunPtr pMemFun )
        : m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
        {}
    virtual ~MemberFunctor0()
        {}

    virtual void operator() ()
        { ( ( *m_pCallee ).*m_pMemFun )(); }

private:
    CalleePtr  m_pCallee;
    MemFunPtr  m_pMemFun;
};

template< typename CalleePtr, typename MemFunPtr, typename Parm0 >
class MemberFunctor1
    : public Functor
{
public:
    MemberFunctor1( const CalleePtr& pCallee, MemFunPtr pMemFun, Parm0 parm0 )
        : m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
	, m_parm0( parm0 )
        {}
    virtual ~MemberFunctor1()
        {}

    virtual void operator() ()
        { ( ( *m_pCallee ).*m_pMemFun )( m_parm0 ); }

private:
    CalleePtr  m_pCallee;
    MemFunPtr  m_pMemFun;
    Parm0      m_parm0;
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

template< typename CalleePtr, typename Callee,  typename Ret >
inline void asyncCall( const CalleePtr& pCallee,
                           Ret( Callee::*pFunction )( ) )
{
    WorkerThread::instance()->addFunctor(new MemberFunctor0< CalleePtr, Ret ( Callee::* )( ) > ( pCallee,  pFunction ));
}

#endif
