/*
Modifications for Freebob (C) 2006, Pieter Palmers

Copied from the jackd/jackdmp sources
function names changed in order to avoid naming problems when using this in
a jackd backend.

Copyright (C) 2001 Paul Davis
Copyright (C) 2004-2006 Grame

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __FreebobThread__
#define __FreebobThread__

#include "FreebobAtomic.h"
#include <pthread.h>

namespace FreebobStreaming
{

/*!
\brief The base class for runnable objects, that have an <B> Init </B> and <B> Execute </B> method to be called in a thread.
*/

class FreebobRunnableInterface
{

    public:

        FreebobRunnableInterface()
        {}
        virtual ~FreebobRunnableInterface()
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

class FreebobThread
{

    protected:

        FreebobRunnableInterface* fRunnable;

    public:

        FreebobThread(FreebobRunnableInterface* runnable): fRunnable(runnable)
        {}
        virtual ~FreebobThread()
        {}

        virtual int Start() = 0;
        virtual int Kill() = 0;
		virtual int Stop() = 0;

        virtual int AcquireRealTime() = 0;
        virtual int AcquireRealTime(int priority) = 0;
        virtual int DropRealTime() = 0;

        virtual void SetParams(UInt64 period, UInt64 computation, UInt64 constraint) // Empty implementation, will only make sense on OSX...
        {}

        virtual pthread_t GetThreadID() = 0;
};

} // end of namespace

#endif
