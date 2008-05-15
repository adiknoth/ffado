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

#include "PosixMessageQueue.h"

#include "Functors.h"
#include "PosixMutex.h"

#include <errno.h>
#include <string.h>

#define MQ_INVALID_ID ((mqd_t) -1)
// one second
#define POSIX_MESSAGEQUEUE_DEFAULT_TIMEOUT_SEC   10
#define POSIX_MESSAGEQUEUE_DEFAULT_TIMEOUT_NSEC  0

#define POSIX_MESSAGEQUEUE_MAX_MESSAGE_SIZE      1024
// note 10 is the default hard limit
#define POSIX_MESSAGEQUEUE_MAX_NB_MESSAGES       10

namespace Util
{

IMPL_DEBUG_MODULE( PosixMessageQueue, PosixMessageQueue, DEBUG_LEVEL_NORMAL );

PosixMessageQueue::PosixMessageQueue(std::string name)
: m_name( "/" + name )
, m_blocking( eB_Blocking )
, m_direction( eD_None )
, m_owner( false )
, m_handle( MQ_INVALID_ID )
, m_tmp_buffer( NULL )
, m_notifyHandler( NULL )
, m_notifyHandlerLock( *(new PosixMutex()) )
{
    m_timeout.tv_sec = POSIX_MESSAGEQUEUE_DEFAULT_TIMEOUT_SEC;
    m_timeout.tv_nsec = POSIX_MESSAGEQUEUE_DEFAULT_TIMEOUT_NSEC;

    memset(&m_attr, 0, sizeof(m_attr));
    m_attr.mq_maxmsg = POSIX_MESSAGEQUEUE_MAX_NB_MESSAGES;
    m_attr.mq_msgsize = POSIX_MESSAGEQUEUE_MAX_MESSAGE_SIZE;
    m_tmp_buffer = new char[m_attr.mq_msgsize];
}

PosixMessageQueue::~PosixMessageQueue()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) PosixMessageQueue destroy\n",
                this, m_name.c_str());
    Close();
    if(m_owner) {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "(%p, %s) unlink\n",
                    this, m_name.c_str());

        if(mq_unlink(m_name.c_str()) == MQ_INVALID_ID) {
            debugError("(%p, %s) could not unlink message queue: %s\n",
                    this, m_name.c_str(), strerror(errno));
        }
    }
    delete[] m_tmp_buffer;
}

bool
PosixMessageQueue::doOpen(enum eDirection t, int flags, enum eBlocking b)
{

    if(m_handle != MQ_INVALID_ID) {
        debugError("(%p, %s) already open\n",
                   this, m_name.c_str());
        return false;
    }

    switch(t) {
        case eD_ReadOnly: flags |= O_RDONLY; break;
        case eD_WriteOnly: flags |= O_WRONLY; break;
        case eD_ReadWrite: flags |= O_RDWR; break;
        default:
            debugError("bad direction\n");
            return false;
    }

    if(b == eB_NonBlocking) {
        flags |= O_NONBLOCK;
    }

    if(flags & O_CREAT) {
        // only user has permissions
        m_handle = mq_open(m_name.c_str(), flags, S_IRWXU, &m_attr);
    } else {

        m_handle = mq_open(m_name.c_str(), flags);
    }
    if(m_handle == MQ_INVALID_ID) {
        debugError("(%p, %s) could not open: %s\n", 
                   this, m_name.c_str(), strerror(errno));
        return false;
    }
    if(flags & O_CREAT) {
        m_owner = true;
    }
    if(mq_getattr(m_handle, &m_attr) == MQ_INVALID_ID) {
        debugError("(%p, %s) could get attr: %s\n", 
                   this, m_name.c_str(), strerror(errno));
        return false;
    }
    m_blocking = b;
    return true;
}

bool
PosixMessageQueue::Open(enum eDirection t, enum eBlocking b)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) open\n",
                this, m_name.c_str());

    if(m_handle != MQ_INVALID_ID) {
        debugError("(%p, %s) already open\n",
                   this, m_name.c_str());
        return false;
    }
    return doOpen(t, 0, b);
}

bool
PosixMessageQueue::Create(enum eDirection t, enum eBlocking b)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) create\n",
                this, m_name.c_str());

    if(m_handle != MQ_INVALID_ID) {
        debugError("(%p, %s) already open\n",
                   this, m_name.c_str());
        return false;
    }
    return doOpen(t, O_CREAT | O_EXCL, b);
}

bool
PosixMessageQueue::Close()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) close\n",
                this, m_name.c_str());

    if(m_handle == MQ_INVALID_ID) {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "(%p, %s) not open\n",
                    this, m_name.c_str());
        return true;
    }
    if(mq_close(m_handle)) {
        debugError("(%p, %s) could not close: %s\n", 
                   this, m_name.c_str(), strerror(errno));
        return false;
    }
    m_handle = MQ_INVALID_ID;
    return true;
}

enum PosixMessageQueue::eResult
PosixMessageQueue::Send(PosixMessageQueue::Message &m)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) send\n",
                this, m_name.c_str());
    if(m_direction == eD_ReadOnly) {
        debugError("Cannot write to read-only queue\n");
        return eR_Error;
    }

    int len = m.getLength();
    if (len > m_attr.mq_msgsize) {
        debugError("Message too long\n");
        return eR_Error;
    }

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += m_timeout.tv_sec;
    timeout.tv_nsec += m_timeout.tv_nsec;
    if(timeout.tv_nsec >= 1000000000LL) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000LL;
    }

    if(!m.serialize(m_tmp_buffer)) {
        debugError("Could not serialize\n");
        return eR_Error;
    }

    if(mq_timedsend(m_handle, m_tmp_buffer, len, m.getPriority(), &timeout) == MQ_INVALID_ID) {
        switch(errno) {
            case EAGAIN:
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) full\n",
                            this, m_name.c_str());
                return eR_Again;
            case ETIMEDOUT:
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) read timed out\n",
                            this, m_name.c_str());
                return eR_Timeout;
            default:
                debugError("(%p, %s) could not send: %s\n", 
                           this, m_name.c_str(), strerror(errno));
                return eR_Error;
        }
    }
    return eR_OK;
}

enum PosixMessageQueue::eResult
PosixMessageQueue::Receive(PosixMessageQueue::Message &m)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) receive\n",
                this, m_name.c_str());
    if(m_direction == eD_WriteOnly) {
        debugError("Cannot read from write-only queue\n");
        return eR_Error;
    }

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += m_timeout.tv_sec;
    timeout.tv_nsec += m_timeout.tv_nsec;
    if(timeout.tv_nsec >= 1000000000LL) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000LL;
    }

    signed int len;
    unsigned prio;
    if((len = mq_timedreceive(m_handle, m_tmp_buffer, m_attr.mq_msgsize, &prio, &timeout)) < 0) {
        switch(errno) {
            case EAGAIN:
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) empty\n",
                            this, m_name.c_str());
                return eR_Again;
            case ETIMEDOUT:
                debugOutput(DEBUG_LEVEL_VERBOSE,
                            "(%p, %s) read timed out\n",
                            this, m_name.c_str());
                return eR_Timeout;
            default:
                debugError("(%p, %s) could not receive: %s\n", 
                           this, m_name.c_str(), strerror(errno));
                return eR_Error;
        }
    }

    if(!m.deserialize(m_tmp_buffer, len, prio)) {
        debugError("Could not parse message\n");
        return eR_Error;
    }
    return eR_OK;
}


int
PosixMessageQueue::countMessages()
{
    if(m_handle == MQ_INVALID_ID) {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "(%p, %s) invalid handle\n",
                    this, m_name.c_str());
        return -1;
    }
    struct mq_attr attr;
    if(mq_getattr(m_handle, &attr) == MQ_INVALID_ID) {
        debugError("(%p, %s) could get attr: %s\n", 
                   this, m_name.c_str(), strerror(errno));
        return -1;
    }
    return attr.mq_curmsgs;
}

bool
PosixMessageQueue::canSend()
{
    return countMessages() < m_attr.mq_maxmsg;
}

bool
PosixMessageQueue::canReceive()
{
    return countMessages() > 0;
}

bool
PosixMessageQueue::setNotificationHandler(Util::Functor *f)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) setting handler to %p\n",
                this, m_name.c_str(), f);

    // ensure we don't change the notifier while
    // it's used
    MutexLockHelper lock(m_notifyHandlerLock);
    if(m_notifyHandler == NULL) {
        m_notifyHandler = f;
        return true;
    } else {
        debugError("handler already present\n");
        return false;
    }
}

bool
PosixMessageQueue::unsetNotificationHandler()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) unsetting handler\n",
                this, m_name.c_str());

    // ensure we don't change the notifier while
    // it's used
    MutexLockHelper lock(m_notifyHandlerLock);
    if(m_notifyHandler != NULL) {
        m_notifyHandler = NULL;
        return true;
    } else {
        debugWarning("no handler present\n");
        return true; // not considered an error
    }
}

void
PosixMessageQueue::notifyCallback()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) Notified\n",
                this, m_name.c_str());
    // make sure the handler is not changed
    MutexLockHelper lock(m_notifyHandlerLock);
    if(m_notifyHandler) {
        (*m_notifyHandler)();
    }
}

bool
PosixMessageQueue::enableNotification()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) set\n",
                this, m_name.c_str());

    sigevent evp;
    memset (&evp, 0, sizeof(evp));
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_value.sival_ptr = (void *)this;
    evp.sigev_notify_function = &notifyCallbackStatic;

    if(mq_notify(m_handle, &evp) == MQ_INVALID_ID) {
        debugError("(%p, %s) could set notifier: %s\n", 
                   this, m_name.c_str(), strerror(errno));
        return false;
    }
    return true;
}

bool
PosixMessageQueue::disableNotification()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) unset\n",
                this, m_name.c_str());

    if(mq_notify(m_handle, NULL) == MQ_INVALID_ID) {
        debugError("(%p, %s) could unset notifier: %s\n", 
                   this, m_name.c_str(), strerror(errno));
        return false;
    }
    return true;
}

void
PosixMessageQueue::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "(%p) MessageQueue %s\n", this, m_name.c_str());
}

void
PosixMessageQueue::setVerboseLevel(int i)
{
    setDebugLevel(i);
    m_notifyHandlerLock.setVerboseLevel(i);
}

} // end of namespace
