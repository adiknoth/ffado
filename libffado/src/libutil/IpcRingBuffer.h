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

#ifndef __UTIL_IPC_RINGBUFFER__
#define __UTIL_IPC_RINGBUFFER__

#include "debugmodule/debugmodule.h"
#include <string>



#define FFADO_IPC_RINGBUFFER_MAGIC   0x57439812
#define FFADO_IPC_RINGBUFFER_VERSION 0

#define FFADO_IPC_MAX_MESSAGE_SIZE 16

#include "PosixMessageQueue.h"

#include <semaphore.h>

namespace Util {

/**
 * @brief A Ringbuffer for IPC use.
 */

class PosixMessageQueue;
class PosixSharedMemory;
class Mutex;
class Functor;

class IpcRingBuffer
{
// the message types
protected:
    enum eMessagePriorities {
        eMP_Realtime = 10,
    };

    // these are the views on the message contents
    struct DataWrittenMessage {
        unsigned int idx;
        unsigned int id;
    };

    class IpcMessage : public PosixMessageQueue::Message
    {
    public:
        enum eMessageType {
            eMT_Unknown     = 0,
            eMT_DataWritten = 1,
            eMT_DataAck     = 2,
        };

        IpcMessage()
        : Message()
        , m_priority( 0 )
        , m_data_len( 0 )
        {
            m_header.magic = FFADO_IPC_RINGBUFFER_MAGIC;
            m_header.version = FFADO_IPC_RINGBUFFER_VERSION;
            m_header.type = eMT_Unknown;
        };

        IpcMessage(enum eMessageType t)
        : Message()
        , m_priority( 0 )
        , m_data_len( 0 )
        {
            m_header.magic = FFADO_IPC_RINGBUFFER_MAGIC;
            m_header.version = FFADO_IPC_RINGBUFFER_VERSION;
            m_header.type = t;
        };

        virtual ~IpcMessage() {};

        virtual unsigned getPriority()
            {return m_priority;};
        virtual unsigned int getLength() 
            {return sizeof(m_header)
                    + sizeof(m_data);};

        virtual bool serialize(char *buff);
        virtual bool deserialize(const char *buff, unsigned int length, unsigned prio);

        void setType(enum eMessageType t) {m_header.type=t;};
        enum eMessageType getType() {return m_header.type;};
        char *getDataPtr() {return m_data;};
        void setDataSize(unsigned int i) 
                { assert(i<FFADO_IPC_MAX_MESSAGE_SIZE);
                  m_data_len = i; };
    private:
        unsigned            m_priority;
        struct header {
            unsigned int        magic;
            unsigned int        version;
            enum eMessageType   type;
        } m_header;
        unsigned int        m_data_len;
        char                m_data[FFADO_IPC_MAX_MESSAGE_SIZE];
    };

public:
    // the master is in control of the data structures
    // it represents one view of the ipc buffer
    // the slave represents the other side
    enum eBufferType {
        eBT_Master,
        eBT_Slave,
    };
    // the direction determines wheter we are going 
    // to read from, or write to the memory block
    enum eDirection {
        eD_Outward,
        eD_Inward,
    };
    // blocking mode
    enum eBlocking {
        eB_Blocking,
        eB_NonBlocking,
    };
    // possible responses of the operations
    enum eResult {
        eR_OK,
        eR_Again,
        eR_Error,
        eR_Timeout,
    };

public:
    IpcRingBuffer(std::string, enum eBufferType, enum eDirection,
                  enum eBlocking, unsigned int, unsigned int);
    ~IpcRingBuffer();

    bool init();

    enum IpcRingBuffer::eResult Write(char *block);
    enum IpcRingBuffer::eResult Read(char *block);
    // request a pointer to the next block to read
    // user has to call releaseBlockForRead before calling
    // next requestBlockForRead or Read
    enum IpcRingBuffer::eResult requestBlockForRead(void **block);
    // release the requested block.
    enum IpcRingBuffer::eResult releaseBlockForRead();

    // request a pointer to the next block to write
    // user has to call releaseBlockForWrite before calling
    // next requestBlockForWrite or Write
    enum IpcRingBuffer::eResult requestBlockForWrite(void **block);
    // release the requested block.
    enum IpcRingBuffer::eResult releaseBlockForWrite();

    enum IpcRingBuffer::eResult waitForRead();
    enum IpcRingBuffer::eResult waitForWrite();

    void show();
    void setVerboseLevel(int l);
    // only makes sense for outward buffers
    unsigned int getBufferFill();
private:
    void notificationHandler();

private:
    std::string         m_name;
    unsigned int        m_blocks;
    unsigned int        m_blocksize;
    enum eBufferType    m_type;
    enum eDirection     m_direction;
    enum eBlocking      m_blocking;
    bool                m_initialized;
    unsigned int        m_next_block;
    unsigned int        m_last_block_ack;
    unsigned int        m_idx;
    unsigned int        m_last_idx_ack;

    PosixMessageQueue&  m_ping_queue;     // for notifications
    PosixMessageQueue&  m_pong_queue;     // for responses
    PosixSharedMemory&  m_memblock;       // the actual data carrier
    Mutex&              m_access_lock;    // to make operations atomic
    Functor&            m_notify_functor; // to receive async notifications
    sem_t               m_activity;       // to signal that async activity occurred

    IpcMessage          m_LastDataMessageReceived;
    Mutex&              m_block_requested_for_read;
    IpcMessage          m_LastDataMessageSent;
    Mutex&              m_block_requested_for_write;

protected:
    DECLARE_DEBUG_MODULE;
};

} // Util

#endif // __UTIL_IPC_RINGBUFFER__
