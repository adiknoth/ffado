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

#ifndef __FreebobPosixThread__
#define __FreebobPosixThread__

#include "../debugmodule/debugmodule.h"

#include "FreebobThread.h"
#include <pthread.h>

namespace FreebobStreaming
{

/*!
\brief The POSIX thread base class.
*/

class FreebobPosixThread : public FreebobThread
{

    protected:

        pthread_t fThread;
        int fPriority;
        bool fRealTime;
		volatile bool fRunning;
        int fCancellation;

        static void* ThreadHandler(void* arg);

    public:

        FreebobPosixThread(FreebobRunnableInterface* runnable, bool real_time, int priority, int cancellation)
                : FreebobThread(runnable), fThread((pthread_t)NULL), fPriority(priority), fRealTime(real_time), fRunning(false), fCancellation(cancellation)
        {}
        FreebobPosixThread(FreebobRunnableInterface* runnable)
                : FreebobThread(runnable), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(PTHREAD_CANCEL_DEFERRED)
        {}
        FreebobPosixThread(FreebobRunnableInterface* runnable, int cancellation)
                : FreebobThread(runnable), fThread((pthread_t)NULL), fPriority(0), fRealTime(false), fRunning(false), fCancellation(cancellation)
        {}

        virtual ~FreebobPosixThread()
        {}

        virtual int Start();
        virtual int Kill();
		virtual int Stop();

        virtual int AcquireRealTime();
        virtual int AcquireRealTime(int priority);
        virtual int DropRealTime();

        pthread_t GetThreadID();

        void setVerboseLevel(int l) {setDebugLevel(l);};
	protected:

    DECLARE_DEBUG_MODULE;

};

} // end of namespace


#endif
