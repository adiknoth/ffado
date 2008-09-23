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

#ifndef __THREAD__
#define __THREAD__

#include "../debugmodule/debugmodule.h"

#include "Atomic.h"
#include <pthread.h>
#include <string>

namespace Util
{

/*!
\brief The base class for runnable objects, that have an <B> Init </B> and <B> Execute </B> method to be called in a thread.
*/

class RunnableInterface
{

    public:

        RunnableInterface()
        {}
        virtual ~RunnableInterface()
        {}

        virtual bool Init()          /*! Called once when the thread is started */
        {
            return true;
        }
        virtual bool Execute() = 0;  /*! Must be implemented by subclasses */
};

/*!
\brief The thread base class.
*/

class Thread
{

    protected:

        RunnableInterface* fRunnable;

    public:

        Thread(RunnableInterface* runnable)
        : fRunnable(runnable)
        , m_id( "UNKNOWN" )
        {}
        Thread(RunnableInterface* runnable, std::string id)
        : fRunnable(runnable)
        , m_id( id )
        {}
        virtual ~Thread()
        {}

        virtual int Start() = 0;
        virtual int Kill() = 0;
        virtual int Stop() = 0;

        virtual int AcquireRealTime() = 0;
        virtual int AcquireRealTime(int priority) = 0;
        virtual int DropRealTime() = 0;

        virtual void SetParams(uint64_t period, uint64_t computation, uint64_t constraint) // Empty implementation, will only make sense on OSX...
        {}

        virtual pthread_t GetThreadID() = 0;

        virtual void setVerboseLevel(int l)
            {
                setDebugLevel(l);
                debugOutput( DEBUG_LEVEL_VERBOSE, "(%s) Setting verbose level to %d...\n", m_id.c_str(), l );
            };
    protected:
        std::string m_id;
        DECLARE_DEBUG_MODULE;

};

} // end of namespace

#endif
