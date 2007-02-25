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

#ifndef __POSIXTHREAD__
#define __POSIXTHREAD__

#include "Thread.h"
#include <pthread.h>

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

        static void* ThreadHandler(void* arg);

    public:

        PosixThread(RunnableInterface* runnable, bool real_time, int priority, int cancellation)
                : Thread(runnable), fThread((pthread_t)NULL), fPriority(priority), fRealTime(real_time), fRunning(false), fCancellation(cancellation)
        {}
        PosixThread(RunnableInterface* runnable)
                : Thread(runnable), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(PTHREAD_CANCEL_DEFERRED)
        {}
        PosixThread(RunnableInterface* runnable, int cancellation)
                : Thread(runnable), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(cancellation)
        {}

        virtual ~PosixThread()
        {}

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


#endif
