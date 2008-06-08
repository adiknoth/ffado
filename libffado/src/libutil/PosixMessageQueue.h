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

#ifndef __UTIL_POSIX_MESSAGE_QUEUE__
#define __UTIL_POSIX_MESSAGE_QUEUE__

#include "debugmodule/debugmodule.h"

#define _XOPEN_SOURCE 600
#include <time.h>
#include <mqueue.h>
#include <string>

namespace Util
{

class Functor;
class Mutex;

/**
 * @brief A POSIX Message Queue Wrapper.
 */

class PosixMessageQueue
{
public:
    class Message {
        public:
            Message() {};
            virtual ~Message() {};

            virtual unsigned getPriority() = 0;

            virtual unsigned int getLength() = 0;
            virtual bool serialize(char *buff) = 0;
            virtual bool deserialize(const char *buff, unsigned int length, unsigned prio) = 0;
    };

public:
    enum eDirection {
        eD_None,
        eD_ReadOnly,
        eD_WriteOnly,
        eD_ReadWrite
    };

    enum eResult {
        eR_OK,
        eR_Again,
        eR_Error,
        eR_Timeout,
    };

    // blocking mode
    enum eBlocking {
        eB_Blocking,
        eB_NonBlocking,
    };

public:
    PosixMessageQueue(std::string name);
    virtual ~PosixMessageQueue();

    virtual bool Open(enum eDirection, enum eBlocking blocking = eB_Blocking);
    virtual bool Create(enum eDirection, enum eBlocking blocking = eB_Blocking);
    virtual bool Close();

    virtual enum eResult Send(Message &m);
    virtual enum eResult Receive(Message &m);

    virtual enum eResult Clear();

    virtual int countMessages();
    virtual bool canSend();
    virtual bool canReceive();

    virtual bool setNotificationHandler(Util::Functor *f);
    virtual bool unsetNotificationHandler();
    virtual bool enableNotification();
    virtual bool disableNotification();

    virtual bool Wait();

    virtual unsigned int getMaxMessageLength()
        {return m_attr.mq_msgsize;};

    virtual void show();
    virtual void setVerboseLevel(int l);

private:
    bool doOpen(enum eDirection t, int, enum eBlocking);
    static void notifyCallbackStatic(sigval_t t) {
        PosixMessageQueue *obj;
        obj = static_cast<PosixMessageQueue *>(t.sival_ptr);
        obj->notifyCallback();
    }
    void notifyCallback();

protected:
    DECLARE_DEBUG_MODULE;

private:
    std::string         m_name;
    enum eBlocking      m_blocking;
    enum eDirection     m_direction;
    bool                m_owner;
    struct timespec     m_timeout;

    mqd_t               m_handle;
    struct mq_attr      m_attr;
    char *              m_tmp_buffer;
    Util::Functor *     m_notifyHandler;
    Util::Mutex&        m_notifyHandlerLock;
};

} // end of namespace

#endif // __UTIL_POSIX_MESSAGE_QUEUE__
