/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006,2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
 
#include "libutil/Atomic.h"
#include "libstreaming/cycletimer.h"

#include "TimestampedBuffer.h"
#include "assert.h"

namespace FreebobUtil {

IMPL_DEBUG_MODULE( TimestampedBuffer, TimestampedBuffer, DEBUG_LEVEL_VERBOSE );

TimestampedBuffer::TimestampedBuffer(TimestampedBufferClient *c)
    : m_event_buffer(NULL), m_cluster_buffer(NULL),
      m_event_size(0), m_events_per_frame(0), m_buffer_size(0), 
      m_bytes_per_frame(0), m_bytes_per_buffer(0),
      m_Client(c), m_framecounter(0), m_buffer_tail_timestamp(0), 
      m_dll_e2(0.0), m_dll_b(0.877), m_dll_c(0.384),
      m_nominal_rate(0.0), m_update_period(0)
{

}

TimestampedBuffer::~TimestampedBuffer() {
    freebob_ringbuffer_free(m_event_buffer);
    free(m_cluster_buffer);
}

bool TimestampedBuffer::setEventSize(unsigned int s) {
    m_event_size=s;
    
    m_bytes_per_frame=m_event_size*m_events_per_frame;
    m_bytes_per_buffer=m_bytes_per_frame*m_buffer_size;
    
    return true;
}

bool TimestampedBuffer::setEventsPerFrame(unsigned int s) {
    m_events_per_frame=s;
    
    m_bytes_per_frame=m_event_size*m_events_per_frame;
    m_bytes_per_buffer=m_bytes_per_frame*m_buffer_size;
    
    return true;
}

bool TimestampedBuffer::setBufferSize(unsigned int s) {
    m_buffer_size=s;
    
    m_bytes_per_frame=m_event_size*m_events_per_frame;
    m_bytes_per_buffer=m_bytes_per_frame*m_buffer_size;
    
    return true;
}

unsigned int TimestampedBuffer::getBufferFill() {
    return freebob_ringbuffer_read_space(m_event_buffer)/(m_bytes_per_frame);
}

bool TimestampedBuffer::init() {
    
    pthread_mutex_init(&m_framecounter_lock, NULL);
    
    return true;
}

bool TimestampedBuffer::reset() {
    freebob_ringbuffer_reset(m_event_buffer);
    
    resetFrameCounter();
    
    return true;
}

void TimestampedBuffer::dumpInfo() {
    
    uint64_t ts_head, fc;
    getBufferHeadTimestamp(&ts_head,&fc);
    
    int64_t diff=(int64_t)ts_head - (int64_t)m_buffer_tail_timestamp;

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  TimestampedBuffer (%p) info:\n",this);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Frame counter         : %d\n", m_framecounter);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer head timestamp : %011llu\n",ts_head);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer tail timestamp : %011llu\n",m_buffer_tail_timestamp);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Head - Tail           : %011lld\n",diff);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  rate                  : %lf (%f)\n",m_dll_e2,m_dll_e2/m_update_period);
}

bool TimestampedBuffer::prepare() {
    debugOutput(DEBUG_LEVEL_VERBOSE,"Preparing buffer (%p)\n",this);
    debugOutput(DEBUG_LEVEL_VERBOSE," Size=%u events, events/frame=%u, event size=%ubytes\n",
                                        m_buffer_size,m_events_per_frame,m_event_size);

    assert(m_buffer_size);
    assert(m_events_per_frame);
    assert(m_event_size);

    if( !(m_event_buffer=freebob_ringbuffer_create(
            (m_events_per_frame * m_buffer_size) * m_event_size))) {
            
        debugFatal("Could not allocate memory event ringbuffer\n");
        return false;
    }
    
    // allocate the temporary cluster buffer
    if( !(m_cluster_buffer=(char *)calloc(m_events_per_frame,m_event_size))) {
            debugFatal("Could not allocate temporary cluster buffer\n");
        freebob_ringbuffer_free(m_event_buffer);
        return false;
    }

    assert(m_nominal_rate != 0.0);
    assert(m_update_period != 0);
    
    // init the DLL
    m_dll_e2=m_nominal_rate * (double)m_update_period;
    
    m_dll_b=((double)(0.877));
    m_dll_c=((double)(0.384));

    return true;
}

bool TimestampedBuffer::writeFrames(unsigned int nevents, char *data) {

    unsigned int write_size=nevents*m_event_size*m_events_per_frame;

    // add the data payload to the ringbuffer
    if (freebob_ringbuffer_write(m_event_buffer,data,write_size) < write_size) 
    {
        debugWarning("writeFrames buffer overrun\n");
        return false;
    }
    return true;

}

bool TimestampedBuffer::readFrames(unsigned int nevents, char *data) {

    unsigned int read_size=nevents*m_event_size*m_events_per_frame;

    // get the data payload to the ringbuffer
    if ((freebob_ringbuffer_read(m_event_buffer,data,read_size)) < read_size) 
    {
        debugWarning("readFrames buffer underrun\n");
        return false;
    }
    return true;

}

bool TimestampedBuffer::blockProcessWriteFrames(unsigned int nbframes, int64_t ts) {

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
    int xrun;
    unsigned int offset=0;
    
    freebob_ringbuffer_data_t vec[2];
    // we received one period of frames
    // this is period_size*dimension of events
    unsigned int events2write=nbframes*m_events_per_frame;
    unsigned int bytes2write=events2write*m_event_size;

    /* write events2write bytes to the ringbuffer 
    *  first see if it can be done in one read.
    *  if so, ok. 
    *  otherwise write up to a multiple of clusters directly to the buffer
    *  then do the buffer wrap around using ringbuffer_write
    *  then write the remaining data directly to the buffer in a third pass 
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */
    unsigned int cluster_size=m_events_per_frame*m_event_size;

    while(bytes2write>0) {
        int byteswritten=0;
        
        unsigned int frameswritten=(nbframes*cluster_size-bytes2write)/cluster_size;
        offset=frameswritten;
        
        freebob_ringbuffer_get_write_vector(m_event_buffer, vec);
            
        if(vec[0].len==0) { // this indicates a full event buffer
            debugError("Event buffer overrun in buffer %p\n",this);
            break;
        }
            
        /* if we don't take care we will get stuck in an infinite loop
        * because we align to a cluster boundary later
        * the remaining nb of bytes in one write operation can be 
        * smaller than one cluster
        * this can happen because the ringbuffer size is always a power of 2
        */
        if(vec[0].len<cluster_size) {
            
            // encode to the temporary buffer
            xrun = m_Client->processWriteBlock(m_cluster_buffer, 1, offset);
            
            if(xrun<0) {
                // xrun detected
                debugError("Frame buffer underrun in buffer %p\n",this);
                return false;
            }
                
            // use the ringbuffer function to write one cluster 
            // the write function handles the wrap around.
            freebob_ringbuffer_write(m_event_buffer,
                         m_cluster_buffer,
                         cluster_size);
                
            // we advanced one cluster_size
            bytes2write-=cluster_size;
                
        } else { // 
            
            if(bytes2write>vec[0].len) {
                // align to a cluster boundary
                byteswritten=vec[0].len-(vec[0].len%cluster_size);
            } else {
                byteswritten=bytes2write;
            }
                
            xrun = m_Client->processWriteBlock(vec[0].buf,
                         byteswritten/cluster_size,
                         offset);
            
            if(xrun<0) {
                    // xrun detected
                debugError("Frame buffer underrun in buffer %p\n",this);
                return false; // FIXME: return false ?
            }

            freebob_ringbuffer_write_advance(m_event_buffer, byteswritten);
            bytes2write -= byteswritten;
        }

        // the bytes2write should always be cluster aligned
        assert(bytes2write%cluster_size==0);

    }
    
    return true;
    
}

bool TimestampedBuffer::blockProcessReadFrames(unsigned int nbframes) {

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Reading %u from buffer (%p)...\n", nbframes, this);
    
    int xrun;
    unsigned int offset=0;
    
    freebob_ringbuffer_data_t vec[2];
    // we received one period of frames on each connection
    // this is period_size*dimension of events

    unsigned int events2read=nbframes*m_events_per_frame;
    unsigned int bytes2read=events2read*m_event_size;
    /* read events2read bytes from the ringbuffer 
    *  first see if it can be done in one read. 
    *  if so, ok. 
    *  otherwise read up to a multiple of clusters directly from the buffer
    *  then do the buffer wrap around using ringbuffer_read
    *  then read the remaining data directly from the buffer in a third pass 
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */
    unsigned int cluster_size=m_events_per_frame*m_event_size;
    
    while(bytes2read>0) {
        unsigned int framesread=(nbframes*cluster_size-bytes2read)/cluster_size;
        offset=framesread;

        int bytesread=0;

        freebob_ringbuffer_get_read_vector(m_event_buffer, vec);

        if(vec[0].len==0) { // this indicates an empty event buffer
            debugError("RCV: Event buffer underrun in processor %p\n",this);
            return false;
        }

        /* if we don't take care we will get stuck in an infinite loop
        * because we align to a cluster boundary later
        * the remaining nb of bytes in one read operation can be smaller than one cluster
        * this can happen because the ringbuffer size is always a power of 2
                */
        if(vec[0].len<cluster_size) {
            // use the ringbuffer function to read one cluster 
            // the read function handles wrap around
            freebob_ringbuffer_read(m_event_buffer,m_cluster_buffer,cluster_size);

            assert(m_Client);
            xrun = m_Client->processReadBlock(m_cluster_buffer, 1, offset);

            if(xrun<0) {
                // xrun detected
                debugError("RCV: Frame buffer overrun in processor %p\n",this);
                    return false;
            }

            // we advanced one cluster_size
            bytes2read-=cluster_size;

        } else { // 

            if(bytes2read>vec[0].len) {
                // align to a cluster boundary
                bytesread=vec[0].len-(vec[0].len%cluster_size);
            } else {
                bytesread=bytes2read;
            }
            
            assert(m_Client);
            xrun = m_Client->processReadBlock(vec[0].buf, bytesread/cluster_size, offset);

            if(xrun<0) {
                // xrun detected
                debugError("RCV: Frame buffer overrun in processor %p\n",this);
                return false;
            }

            freebob_ringbuffer_read_advance(m_event_buffer, bytesread);
            bytes2read -= bytesread;
        }

        // the bytes2read should always be cluster aligned
        assert(bytes2read%cluster_size==0);
    }

    return true;
}

/**
 * Decrements the frame counter, in a atomic way.
 * is thread safe.
 */
void TimestampedBuffer::decrementFrameCounter(int nbframes) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter -= nbframes;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Increments the frame counter, in a atomic way.
 * also sets the buffer tail timestamp
 * This is thread safe.
 */
void TimestampedBuffer::incrementFrameCounter(int nbframes, uint64_t new_timestamp) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Setting buffer tail timestamp for (%p) to %11llu\n",
                this, new_timestamp);
    
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter += nbframes;
    
    // update the DLL
    int64_t diff = (int64_t)new_timestamp-(int64_t)m_buffer_next_tail_timestamp;
    
    // idea to implement it for nbframes values that differ from m_update_period:
    // diff = diff * nbframes/m_update_period
    // m_buffer_next_tail_timestamp = m_buffer_tail_timestamp + diff
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p): diff=%lld ",
                this, diff);
                    
    // the maximal difference we can allow (64secs)
    const int64_t max=TICKS_PER_SECOND*64L;
    
    if(diff > max) {
        diff -= TICKS_PER_SECOND*128L;
    } else if (diff < -max) {
        diff += TICKS_PER_SECOND*128L;
    }

    double err=diff;
    
    debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "diff2=%lld err=%f\n",
                diff, err);
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "FC=%10u, TS=%011llu\n",m_framecounter, m_buffer_tail_timestamp);
    
    m_buffer_tail_timestamp=m_buffer_next_tail_timestamp;
    m_buffer_next_tail_timestamp += (uint64_t)(m_dll_b * err + m_dll_e2);
    
    if (m_buffer_next_tail_timestamp > TICKS_PER_SECOND*128L) {
        m_buffer_next_tail_timestamp -= TICKS_PER_SECOND*128L;
    }
    
    m_dll_e2 += m_dll_c*err;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "TS=%011llu, NTS=%011llu, DLLe2=%f\n",
                m_buffer_tail_timestamp, m_buffer_next_tail_timestamp, m_dll_e2);
    
    pthread_mutex_unlock(&m_framecounter_lock);
    
    // this DLL allows the calculation of any sample timestamp relative to the buffer tail,
    // to the next period and beyond (through extrapolation)
    //
    // ts(x) = m_buffer_tail_timestamp +
    //         (m_buffer_next_tail_timestamp - m_buffer_tail_timestamp)/(samples_between_updates)*x
    
}

/**
 * Sets the buffer tail timestamp (in usecs)
 * This is thread safe.
 */
void TimestampedBuffer::setBufferTailTimestamp(uint64_t new_timestamp) {
    
    pthread_mutex_lock(&m_framecounter_lock);
    
    m_buffer_tail_timestamp = new_timestamp;
    
    m_dll_e2=m_update_period * m_nominal_rate;
    m_buffer_next_tail_timestamp = (uint64_t)((double)m_buffer_tail_timestamp + m_dll_e2);
    
    pthread_mutex_unlock(&m_framecounter_lock);    
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Set buffer tail timestamp for (%p) to %11llu, NTS=%llu, DLL2=%f\n",
                this, new_timestamp, m_buffer_next_tail_timestamp, m_dll_e2);

}

/**
 * \brief return the timestamp of the first frame in the buffer
 * 
 * This function returns the timestamp of the very first sample in
 * the StreamProcessor's buffer. This is useful for slave StreamProcessors 
 * to find out what the base for their timestamp generation should
 * be. It also returns the framecounter value for which this timestamp
 * is valid.
 *
 * The system is built in such a way that we assume that the processing
 * of the buffers doesn't take any time. Assume we have a buffer transfer at 
 * time T1, meaning that the last sample of this buffer occurs at T1. As 
 * processing does not take time, we don't have to add anything to T1. When
 * transferring the processed buffer to the xmit processor, the timestamp
 * of the last sample is still T1.
 *
 * When starting the streams, we don't have any information on this last
 * timestamp. We prefill the buffer at the xmit side, and we should find
 * out what the timestamp for the last sample in the buffer is. If we sync
 * on a receive SP, we know that the last prefilled sample corresponds with
 * the first sample received - 1 sample duration. This is the same as if the last
 * transfer from iso to client would have emptied the receive buffer.
 *
 *
 * @param ts address to store the timestamp in
 * @param fc address to store the associated framecounter in
 */
void TimestampedBuffer::getBufferHeadTimestamp(uint64_t *ts, uint64_t *fc) {
    double rate=(double)m_buffer_next_tail_timestamp - (double)m_buffer_tail_timestamp;
    rate /= (double)m_update_period;
    
    pthread_mutex_lock(&m_framecounter_lock);
    *fc = m_framecounter;

    // ts(x) = m_buffer_tail_timestamp +
    //         (m_buffer_next_tail_timestamp - m_buffer_tail_timestamp)/(samples_between_updates)*x
    
    *ts=m_buffer_tail_timestamp + (uint64_t)(m_framecounter * rate);

    pthread_mutex_unlock(&m_framecounter_lock);
    if(*ts > TICKS_PER_SECOND*128L) {
        *ts -= TICKS_PER_SECOND*128L;
    }
}
        
/**
 * \brief return the timestamp of the last frame in the buffer
 * 
 * This function returns the timestamp of the last frame in
 * the StreamProcessor's buffer. It also returns the framecounter 
 * value for which this timestamp is valid.
 *
 * @param ts address to store the timestamp in
 * @param fc address to store the associated framecounter in
 */
void TimestampedBuffer::getBufferTailTimestamp(uint64_t *ts, uint64_t *fc) {
    pthread_mutex_lock(&m_framecounter_lock);
    *fc = m_framecounter;
    *ts = m_buffer_tail_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Resets the frame counter, in a atomic way. This
 * is thread safe.
 */
void TimestampedBuffer::resetFrameCounter() {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter = 0;
    pthread_mutex_unlock(&m_framecounter_lock);
}


} // end of namespace FreebobUtil
