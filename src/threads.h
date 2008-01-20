/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef THREADS_H
#define THREADS_H

#include <semaphore.h>

class Functor
{
public:
    Functor() {}
    virtual ~Functor() {}

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

#endif


