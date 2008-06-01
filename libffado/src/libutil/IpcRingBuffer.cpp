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

#include "IpcRingBuffer.h"
#include "PosixMessageQueue.h"
#include "PosixSharedMemory.h"
#include "Mutex.h"
#include "PosixMutex.h"
#include "Functors.h"

#include <cstring>

// FIXME: if we restrict the nb_blocks to a power of two, the overflows
//        can be implemented using masks

namespace Util {

IMPL_DEBUG_MODULE( IpcRingBuffer, IpcRingBuffer, DEBUG_LEVEL_VERBOSE );

IpcRingBuffer::IpcRingBuffer(std::string name,
                             enum eBufferType type,
                             enum eDirection dir,
                             enum eBlocking blocking,
                             unsigned int blocks, unsigned int block_size)
: m_name(name)
, m_blocks(blocks)
, m_blocksize(block_size)
, m_type( type )
, m_direction( dir )
, m_blocking( blocking )
, m_initialized( false )
, m_next_block( 1 )
, m_last_block_ack( 0 )
, m_idx( 1 )
, m_last_idx_ack( 0 )
, m_ping_queue( *(new PosixMessageQueue(name+":ping")) )
, m_pong_queue( *(new PosixMessageQueue(name+":pong")) )
, m_memblock( *(new PosixSharedMemory(name+":mem", blocks*block_size)) )
, m_access_lock( *(new PosixMutex()) )
, m_notify_functor( *(new MemberFunctor0< IpcRingBuffer*, void (IpcRingBuffer::*)() >
                     ( this, &IpcRingBuffer::notificationHandler, false )) )
, m_block_requested_for_read( *(new PosixMutex()) )
, m_block_requested_for_write( *(new PosixMutex()) )
{
    m_ping_queue.setVerboseLevel(getDebugLevel());
    m_pong_queue.setVerboseLevel(getDebugLevel());
    m_memblock.setVerboseLevel(getDebugLevel());
    m_access_lock.setVerboseLevel(getDebugLevel());
    sem_init(&m_activity, 0, 0);
}

IpcRingBuffer::~IpcRingBuffer()
{
    // make sure everyone is done with this
    // should not be necessary AFAIK
    m_access_lock.Lock();
    m_initialized=false;
    delete &m_memblock;
    delete &m_ping_queue;
    delete &m_pong_queue;
    m_access_lock.Unlock();

    delete &m_access_lock;
    delete &m_notify_functor;
    sem_destroy(&m_activity);
}

bool
IpcRingBuffer::init()
{
    if(m_initialized) {
        debugError("(%p, %s) Already initialized\n", 
                   this, m_name.c_str());
        return false;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) init %s\n", this, m_name.c_str());
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) direction %d, %d blocks of %d bytes\n",
                                     this, m_direction, m_blocks, m_blocksize);
    switch(m_type) {
        case eBT_Master:
            // a master creates and owns all of the shared memory structures
            // for outward connections we write the data, else we read
            if(!m_memblock.Create( PosixSharedMemory::eD_ReadWrite ))
            {
                debugError("(%p, %s) Could not create memblock\n",
                           this, m_name.c_str());
                return false;
            }

            m_memblock.LockInMemory(true);
            // for outward connections we do the pinging, else
            // we do the pong-ing. note that for writing, we open read-write
            // in order to be able to dequeue when the queue is full
            if(!m_ping_queue.Create( (m_direction == eD_Outward ?
                                       PosixMessageQueue::eD_ReadWrite :
                                       PosixMessageQueue::eD_ReadOnly),
                                     (m_blocking==eB_Blocking ?
                                       PosixMessageQueue::eB_Blocking :
                                       PosixMessageQueue::eB_NonBlocking)
                                   ))
            {
                debugError("(%p, %s) Could not create ping queue\n",
                           this, m_name.c_str());
                return false;
            }
            if(!m_pong_queue.Create( (m_direction == eD_Outward ?
                                       PosixMessageQueue::eD_ReadOnly :
                                       PosixMessageQueue::eD_ReadWrite),
                                     (m_blocking==eB_Blocking ?
                                       PosixMessageQueue::eB_Blocking :
                                       PosixMessageQueue::eB_NonBlocking)
                                   ))
            {
                debugError("(%p, %s) Could not create pong queue\n",
                           this, m_name.c_str());
                return false;
            }
            break;
        case eBT_Slave:
            // a slave only opens the shared memory structures
            // for outward connections we write the data, else we read
            if(!m_memblock.Open( (m_direction == eD_Outward
                                    ? PosixSharedMemory::eD_ReadWrite
                                    : PosixSharedMemory::eD_ReadOnly) ))
            {
                debugError("(%p, %s) Could not open memblock\n",
                           this, m_name.c_str());
                return false;
            }
            m_memblock.LockInMemory(true);
            // for outward connections we do the pinging, else
            // we do the pong-ing. note that for writing, we open read-write
            // in order to be able to dequeue when the queue is full
            if(!m_ping_queue.Open( (m_direction == eD_Outward ?
                                      PosixMessageQueue::eD_ReadWrite :
                                      PosixMessageQueue::eD_ReadOnly),
                                   (m_blocking==eB_Blocking ?
                                      PosixMessageQueue::eB_Blocking :
                                      PosixMessageQueue::eB_NonBlocking)
                                 ))
            {
                debugError("(%p, %s) Could not open ping queue\n",
                           this, m_name.c_str());
                return false;
            }
            if(!m_pong_queue.Open( (m_direction == eD_Outward ?
                                      PosixMessageQueue::eD_ReadOnly :
                                      PosixMessageQueue::eD_ReadWrite),
                                   (m_blocking==eB_Blocking ?
                                      PosixMessageQueue::eB_Blocking :
                                      PosixMessageQueue::eB_NonBlocking)
                                 ))
            {
                debugError("(%p, %s) Could not open pong queue\n",
                           this, m_name.c_str());
                return false;
            }
            break;
    }

    // if we are on the sending end of the buffer, we need a notifier
    // on the pong queue
    // the receiving end is driven by the messages in the ping queue
    if(m_direction == eD_Outward) {
        if(!m_pong_queue.setNotificationHandler(&m_notify_functor)) {
            debugError("Could not set Notification Handler\n");
            return false;
        }
        // enable the handler
        if(!m_pong_queue.enableNotification()) {
            debugError("Could not enable notification\n");
        }
        // now clear the queue to eliminate messages that might be there
        // from earlier runs
        m_pong_queue.Clear();
    } else {
        // if we are on the receiving end, clear any waiting messages in the ping
        // queue
        m_ping_queue.Clear();
    }
    
    m_initialized = true;
    return true;
}

void
IpcRingBuffer::notificationHandler()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) IpcRingBuffer %s\n", this, m_name.c_str());
    // prevent multiple access
    MutexLockHelper lock(m_access_lock);

    // The first thing we do is re-enable the handler
    // it is not going to be called since there are messages in the queue
    // first enabling the handler, then reading all received messages will
    // ensure that we either read or get notified of any message that arrives
    // while this handler is running
    if(!m_pong_queue.enableNotification()) {
        debugError("Could not re-enable notification\n");
    }

    // no need for a lock protecting the pong queue as long as we are the only ones reading it
    // while we have messages to read, read them
    while(m_pong_queue.canReceive()) {
        // message placeholder
        IpcMessage m_ack = IpcMessage(); // FIXME: stack allocation not strictly RT safe
        // read ping message (blocks)
        enum PosixMessageQueue::eResult msg_res;
        msg_res = m_pong_queue.Receive(m_ack);
        switch(msg_res) {
            case PosixMessageQueue::eR_OK:
                break;
            default: // we were just notified, anything except OK is an error
                debugError("Could not read from ping queue\n");
        }
    
        IpcMessage::eMessageType type = m_ack.getType();
        if(type == IpcMessage::eMT_DataAck) {
            // get a view on the data
            struct DataWrittenMessage* data = reinterpret_cast<struct DataWrittenMessage*>(m_ack.getDataPtr());
    
            debugOutput(DEBUG_LEVEL_VERBOSE, "Received ack idx %d at id %d\n", data->idx, data->id);
    
            // check counters
            unsigned int expected_block_ack = m_last_block_ack+1;
            if(expected_block_ack == m_blocks) expected_block_ack = 0;
            if(data->id != expected_block_ack) {
                debugWarning("unexpected block id: %d (expected %d)\n", data->id, expected_block_ack);
            }

            unsigned int expected_block_idx = m_last_idx_ack+1; //will auto-overflow
            if(data->idx != expected_block_idx) {
                debugWarning("unexpected block idx: %d (expected %d)\n", data->idx, expected_block_idx);
            }
    
            // prepare the next expected values
            // this is the only value used (and written in case of error) in the other thread
            m_last_block_ack = data->id;
            // this is not used
            m_last_idx_ack = data->idx;
            // signal activity
            if(m_blocking == eB_Blocking) {
                sem_post(&m_activity);
            }
        } else {
            debugError("Invalid message received (type %d)\n", type);
        }
    }
}

unsigned int
IpcRingBuffer::getBufferFill()
{
    // the next pointer is last_written+1
    // so the bufferfill = last_written - last_ack
    //                   = last_written+1 - last_ack - 1
    // last_ack is always <= last_written. if not,
    // last_written has wrapped
    // => wrap if: last_written < last_ack
    //          or last_written+1 < last_ack+1
    //          or m_next_block < last_ack+1
    // unwrap if this happens
    int bufferfill = m_next_block - m_last_block_ack - 1;
    if(m_next_block <= m_last_block_ack) {
        bufferfill += m_blocks;
    }
    assert(bufferfill>=0);
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) fill: %d\n", this, m_name.c_str(), bufferfill);
    return (unsigned int)bufferfill;
}


enum IpcRingBuffer::eResult
IpcRingBuffer::requestBlockForWrite(void **block)
{
    if(!m_block_requested_for_write.TryLock()) {
        debugError("Already a block requested for write\n");
        return eR_Error;
    }

    // check if we can write a message
    // we can send when: 
    //  - we are not overwriting
    //    AND -    we are in blocking mode and 
    //        - OR (we are in non-blocking mode and there is space)

    if(m_blocking == eB_Blocking) {
        if(getBufferFill() >= m_blocks) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) full\n", this, m_name.c_str());
            // make it wait
            sem_wait(&m_activity);
        }
    } else {
        // there are no free data blocks, or there is no message space
        if(getBufferFill() >= m_blocks || !m_ping_queue.canSend()) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) full\n", this, m_name.c_str());
            m_block_requested_for_write.Unlock();
            return eR_Again;
        }
    }

    // check for overflow
    if(m_next_block == m_last_block_ack) {
        debugWarning("Overwriting not yet read block %u\n", m_next_block);
        // we have to increment the block_read pointer
        // in order to keep consistency
        m_last_block_ack++;
        if(m_last_block_ack == m_blocks) {
            m_last_block_ack = 0;
        }
    }

    int offset = m_next_block * m_blocksize;
    *block = m_memblock.requestBlock(offset, m_blocksize);
    if(*block) {
        // keep the lock, to be released by releaseBlockForWrite
        return eR_OK;
    } else {
        m_block_requested_for_write.Unlock();
        return eR_Error;
    }
}

enum IpcRingBuffer::eResult
IpcRingBuffer::releaseBlockForWrite()
{
    if(!m_block_requested_for_write.isLocked()) {
        debugError("No block requested for write\n");
        return eR_Error;
    }

    IpcMessage &m = m_LastDataMessageSent;

    // prepare ping message
    m.setType(IpcMessage::eMT_DataWritten);
    m.setDataSize(sizeof(struct DataWrittenMessage));

    // get a view on the data
    struct DataWrittenMessage* data = reinterpret_cast<struct DataWrittenMessage*>(m.getDataPtr());
    // set the data contents
    data->id = m_next_block;
    data->idx = m_idx;

    debugOutput(DEBUG_LEVEL_VERBOSE, "Releasing block idx %d at id %d\n", data->idx, data->id);

    // send ping message
    enum PosixMessageQueue::eResult msg_res;
    msg_res = m_ping_queue.Send(m);
    switch(msg_res) {
        case PosixMessageQueue::eR_OK:
            break;
        case PosixMessageQueue::eR_Again:
            // this is a bug since we checked whether it was empty or not
            debugError("Bad response value\n");
            m_block_requested_for_write.Unlock();
            return eR_Error; 
        case PosixMessageQueue::eR_Timeout:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Timeout\n");
            m_block_requested_for_write.Unlock();
            return eR_Timeout; // blocking and no space on time
        default:
            debugError("Could not send to ping queue\n");
            m_block_requested_for_write.Unlock();
            return eR_Error;
    }

    // increment and wrap
    m_next_block++;
    if(m_next_block == m_blocks) {
        m_next_block = 0;
    }
    m_idx++;
    m_block_requested_for_write.Unlock();
    return eR_OK;
}

enum IpcRingBuffer::eResult
IpcRingBuffer::Write(char *block)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p,  %s) IpcRingBuffer\n", this, m_name.c_str());
    if(m_direction == eD_Inward) {
        debugError("Cannot write to inbound buffer\n");
        return eR_Error;
    }

    enum IpcRingBuffer::eResult msg_res;
    void *xmit_block;
    // request a block for reading
    msg_res = requestBlockForWrite(&xmit_block);
    if(msg_res == eR_OK) {
        // if we receive a eR_OK, we should always be able to write to the shared memory
        memcpy(xmit_block, block, m_blocksize);
        releaseBlockForWrite();
    }
    return msg_res;
}

enum IpcRingBuffer::eResult
IpcRingBuffer::requestBlockForRead(void **block)
{
    if(!m_block_requested_for_read.TryLock()) {
        debugError("Already a block requested for read\n");
        return eR_Error;
    }
    // message placeholder
    IpcMessage &m = m_LastDataMessageReceived;

    // read ping message (blocks)
    enum PosixMessageQueue::eResult msg_res;
    msg_res = m_ping_queue.Receive(m);
    switch(msg_res) {
        case PosixMessageQueue::eR_OK:
            break;
        case PosixMessageQueue::eR_Again:
            m_block_requested_for_read.Unlock();
            return eR_Again; // non-blocking and no message
        case PosixMessageQueue::eR_Timeout:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Timeout\n");
            m_block_requested_for_read.Unlock();
            return eR_Timeout; // blocking and no message on time
        default:
            debugError("Could not read from ping queue\n");
            m_block_requested_for_read.Unlock();
            return eR_Error;
    }

    IpcMessage::eMessageType type = m.getType();
    if(type == IpcMessage::eMT_DataWritten) {
        // get a view on the data
        struct DataWrittenMessage* data = reinterpret_cast<struct DataWrittenMessage*>(m.getDataPtr());
        debugOutput(DEBUG_LEVEL_VERBOSE, "Requested block idx %d at id %d\n", data->idx, data->id);

        // check counters
        if(data->id != m_next_block) {
            debugWarning("unexpected block id: %d (expected %d)\n", data->id, m_next_block);
        }
        if(data->idx != m_idx) {
            debugWarning("unexpected block idx: %d (expected %d)\n", data->idx, m_idx);
        }

        int offset = data->id * m_blocksize;
        *block = m_memblock.requestBlock(offset, m_blocksize);
        if(*block) {
            // keep the mutex locked, we expect the thread that grabbed the block to also return it
            return eR_OK;
        } else {
            m_block_requested_for_read.Unlock();
            return eR_Error;
        }
    } else {
        debugError("Invalid message received (type %d)\n", type);
        m_block_requested_for_read.Unlock();
        return eR_Error;
    }
}

enum IpcRingBuffer::eResult
IpcRingBuffer::releaseBlockForRead()
{
    if(!m_block_requested_for_read.isLocked()) {
        debugError("No block requested for read\n");
        return eR_Error;
    }

    IpcMessage &m = m_LastDataMessageReceived;

    // get a view on the data
    struct DataWrittenMessage* data = reinterpret_cast<struct DataWrittenMessage*>(m.getDataPtr());
    debugOutput(DEBUG_LEVEL_VERBOSE, "Releasing block idx %d at id %d\n", data->idx, data->id);

    // write a response to the pong queue
    // reuse the message
    m.setType(IpcMessage::eMT_DataAck);
    enum PosixMessageQueue::eResult msg_res;
    msg_res = m_pong_queue.Send(m);
    switch(msg_res) {
        case PosixMessageQueue::eR_OK:
            break;
        case PosixMessageQueue::eR_Again:
            m_block_requested_for_read.Unlock(); // FIXME: this is not very correct
            debugOutput(DEBUG_LEVEL_VERBOSE, "Again on ACK\n");
            return eR_Again; // non-blocking and no message
        case PosixMessageQueue::eR_Timeout:
            m_block_requested_for_read.Unlock();
            debugOutput(DEBUG_LEVEL_VERBOSE, "Timeout on ACK\n");
            return eR_Timeout; // blocking and no message on time
        default:
            debugError("Could not write to pong queue\n");
            m_block_requested_for_read.Unlock();
            return eR_Error;
    }

    // prepare the next expected values
    m_next_block = data->id + 1;
    if(m_next_block == m_blocks) {
        m_next_block = 0;
    }
    m_idx = data->idx + 1;

    m_block_requested_for_read.Unlock();
    return eR_OK;
}

enum IpcRingBuffer::eResult
IpcRingBuffer::Read(char *block)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) IpcRingBuffer %s\n", this, m_name.c_str());
    if(m_direction == eD_Outward) {
        debugError("Cannot read from outward buffer\n");
        return eR_Error;
    }

    enum IpcRingBuffer::eResult msg_res;
    void *rcv_block;
    // request a block for reading
    msg_res = requestBlockForRead(&rcv_block);
    if(msg_res == eR_OK) {
        // if we receive a eR_OK, we should always be able to read the shared memory
        memcpy(block, rcv_block, m_blocksize);
        releaseBlockForRead();
    }
    return msg_res;
}

void
IpcRingBuffer::show()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p) IpcRingBuffer %s\n", this, m_name.c_str());
}

void
IpcRingBuffer::setVerboseLevel(int i)
{
    setDebugLevel(i);
    debugOutput(DEBUG_LEVEL_VERBOSE, "(%p, %s) verbose: %d\n", this, m_name.c_str(), i);
    m_ping_queue.setVerboseLevel(i);
    m_pong_queue.setVerboseLevel(i);
    m_memblock.setVerboseLevel(i);
    m_access_lock.setVerboseLevel(i);
}

bool
IpcRingBuffer::IpcMessage::serialize(char *buff)
{
    memcpy(buff, &m_header, sizeof(m_header));
    buff += sizeof(m_header);
    memcpy(buff, m_data, m_data_len);
    return true;
}
bool
IpcRingBuffer::IpcMessage::deserialize(const char *buff, unsigned int length, unsigned prio)
{
    assert(length >= sizeof(m_header));
    memcpy(&m_header, buff, sizeof(m_header));

    if(m_header.magic != FFADO_IPC_RINGBUFFER_MAGIC) {
        return false; // invalid magic
    }
    if(m_header.version != FFADO_IPC_RINGBUFFER_VERSION) {
        return false; // invalid version
    }

    m_data_len = length - sizeof(m_header);
    buff += sizeof(m_header);

    assert(m_data_len <= FFADO_IPC_MAX_MESSAGE_SIZE);
    memcpy(m_data, buff, m_data_len);

    m_priority = prio;
    return true;
}


} // Util
