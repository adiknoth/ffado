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
      m_wrap_at(0xFFFFFFFFFFFFFFFFLLU),
      m_Client(c), m_framecounter(0),
      m_tick_offset(0),
      m_buffer_tail_timestamp(0),
      m_buffer_next_tail_timestamp(0),
      m_dll_e2(0.0), m_dll_b(0.877), m_dll_c(0.384),
      m_nominal_rate(0.0), m_update_period(0)
{
    pthread_mutex_init(&m_framecounter_lock, NULL);

}

TimestampedBuffer::~TimestampedBuffer() {
    freebob_ringbuffer_free(m_event_buffer);
    free(m_cluster_buffer);
}

/**
 * \brief Set the nominal rate in frames/timeunit
 *
 * Sets the nominal rate in frames per time unit. This rate is used
 * to initialize the DLL that will extract the effective rate based
 * upon the timestamps it gets fed.
 *
 * @param r rate
 * @return true if successful
 */
bool TimestampedBuffer::setNominalRate(float r) {
    m_nominal_rate=r;
    debugOutput(DEBUG_LEVEL_VERBOSE," nominal rate=%e set to %e\n",
                                    m_nominal_rate, r);    
    return true;
}

/**
 * \brief Set the nominal update period (in frames)
 *
 * Sets the nominal update period. This period is the number of frames
 * between two timestamp updates (hence buffer writes)
 *
 * @param n period in frames
 * @return true if successful
 */
bool TimestampedBuffer::setUpdatePeriod(unsigned int n) {
    m_update_period=n;
    return true;
}

/**
 * \brief set the value at which timestamps should wrap around
 * @param w value to wrap at
 * @return true if successful
 */
bool TimestampedBuffer::setWrapValue(uint64_t w) {
    m_wrap_at=w;
    return true;
}

/**
 * \brief return the effective rate 
 *
 * Returns the effective rate calculated by the DLL.
 *
 * @return rate (in frames/timeunit)
 */
float TimestampedBuffer::getRate() {
    return ((float) m_update_period)/m_dll_e2;
}

/**
 * \brief Sets the size of the events
 * @param s event size in bytes
 * @return true if successful
 */
bool TimestampedBuffer::setEventSize(unsigned int s) {
    m_event_size=s;
    
    m_bytes_per_frame=m_event_size*m_events_per_frame;
    m_bytes_per_buffer=m_bytes_per_frame*m_buffer_size;
    
    return true;
}

/**
 * \brief Sets the number of events per frame
 * @param n number of events per frame
 * @return true if successful
 */
bool TimestampedBuffer::setEventsPerFrame(unsigned int n) {
    m_events_per_frame=n;
    
    m_bytes_per_frame=m_event_size*m_events_per_frame;
    m_bytes_per_buffer=m_bytes_per_frame*m_buffer_size;
    
    return true;
}
/**
 * \brief Sets the buffer size in frames
 * @param n number frames
 * @return true if successful
 */
bool TimestampedBuffer::setBufferSize(unsigned int n) {
    m_buffer_size=n;

    m_bytes_per_frame=m_event_size*m_events_per_frame;
    m_bytes_per_buffer=m_bytes_per_frame*m_buffer_size;

    return true;
}

/**
 * Sets the buffer offset in ticks. 
 *
 * A positive value means that the buffer is 'delayed' for nticks ticks.
 *
 * @note These offsets are only used when reading timestamps. Any function
 *       that returns a timestamp will incorporate this offset. 
 * @param nframes the number of ticks (positive = delay buffer)
 * @return true if successful
 */
bool TimestampedBuffer::setTickOffset(int nticks) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"Setting ticks offset to %d\n",nticks);
    m_tick_offset=nticks;
    return true;
}

/**
 * \brief Returns the current fill of the buffer
 *
 * This returns the buffer fill of the internal ringbuffer. This
 * can only be used as an indication because it's state is not 
 * guaranteed to be consistent at all times due to threading issues.
 *
 * In order to get the number of frames in the buffer, use the 
 * getFrameCounter, getBufferHeadTimestamp, getBufferTailTimestamp
 * functions
 *
 * @return the internal buffer fill in frames
 */
unsigned int TimestampedBuffer::getBufferFill() {
    return freebob_ringbuffer_read_space(m_event_buffer)/(m_bytes_per_frame);
}

/**
 * \brief Initializes the TimestampedBuffer
 *
 * Initializes the TimestampedBuffer, should be called before anything else
 * is done.
 *
 * @return true if successful
 */
bool TimestampedBuffer::init() {
    return true;
}

/**
 * \brief Resets the TimestampedBuffer
 *
 * Resets the TimestampedBuffer, clearing the buffers and counters.
 * (not true yet: Also resets the DLL to the nominal values.)
 * 
 * \note when this is called, you should make sure that the buffer
 *       tail timestamp gets set before continuing
 *
 * @return true if successful
 */
bool TimestampedBuffer::reset() {
    freebob_ringbuffer_reset(m_event_buffer);
    
    resetFrameCounter();
    
    return true;
}

/**
 * \brief Perpares the TimestampedBuffer
 * 
 * Prepare the TimestampedBuffer. This allocates all internal buffers and 
 * initializes all data structures.
 * 
 * This should be called after parameters such as buffer size, event size etc.. are set,
 * and before any read/write operations are performed.
 * 
 * @return true if successful
 */
bool TimestampedBuffer::prepare() {
    debugOutput(DEBUG_LEVEL_VERBOSE,"Preparing buffer (%p)\n",this);
    debugOutput(DEBUG_LEVEL_VERBOSE," Size=%u events, events/frame=%u, event size=%ubytes\n",
                                        m_buffer_size,m_events_per_frame,m_event_size);
                                        
    debugOutput(DEBUG_LEVEL_VERBOSE," update period %u\n",
                                    m_update_period);
    debugOutput(DEBUG_LEVEL_VERBOSE," nominal rate=%f\n",
                                    m_nominal_rate);
                                    
    debugOutput(DEBUG_LEVEL_VERBOSE," wrapping at %llu\n",m_wrap_at);

    assert(m_buffer_size);
    assert(m_events_per_frame);
    assert(m_event_size);

    assert(m_nominal_rate != 0.0L);
    assert(m_update_period != 0);
    
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
    
    // init the DLL
    m_dll_e2=m_nominal_rate * (float)m_update_period;
    
    m_dll_b=((float)(0.877));
    m_dll_c=((float)(0.384));

    return true;
}

/**
 * @brief Write frames to the buffer
 * 
 * Copies \ref nframes of frames from the buffer pointed to by \ref data to the
 * internal ringbuffer. The time of the last frame in the buffer is set to \ref ts.
 * 
 * @param nframes number of frames to copy
 * @param data pointer to the frame buffer
 * @param ts timestamp of the last frame copied
 * @return true if successful
 */
bool TimestampedBuffer::writeFrames(unsigned int nframes, char *data, uint64_t ts) {

    unsigned int write_size=nframes*m_event_size*m_events_per_frame;

    // add the data payload to the ringbuffer
    if (freebob_ringbuffer_write(m_event_buffer,data,write_size) < write_size) 
    {
        debugWarning("writeFrames buffer overrun\n");
        return false;
    }
    
    incrementFrameCounter(nframes,ts);
    
    return true;

}
/**
 * @brief Read frames from the buffer
 * 
 * Copies \ref nframes of frames from the internal buffer to the data buffer pointed 
 * to by \ref data.
 * 
 * @param nframes number of frames to copy
 * @param data pointer to the frame buffer
 * @return true if successful
 */
bool TimestampedBuffer::readFrames(unsigned int nframes, char *data) {

    unsigned int read_size=nframes*m_event_size*m_events_per_frame;

    // get the data payload to the ringbuffer
    if ((freebob_ringbuffer_read(m_event_buffer,data,read_size)) < read_size) 
    {
        debugWarning("readFrames buffer underrun\n");
        return false;
    }
    
    decrementFrameCounter(nframes);
    
    return true;

}

/**
 * @brief Performs block processing write of frames
 * 
 * This function allows for zero-copy writing into the ringbuffer.
 * It calls the client's processWriteBlock function to write frames
 * into the internal buffer's data area, in a thread safe fashion.
 * 
 * It also updates the timestamp.
 * 
 * @param nbframes number of frames to process
 * @param ts timestamp of the last frame written to the buffer
 * @return true if successful
 */
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
    
    incrementFrameCounter(nbframes,ts);
    
    return true;
    
}

/**
 * @brief Performs block processing read of frames
 * 
 * This function allows for zero-copy reading from the ringbuffer.
 * It calls the client's processReadBlock function to read frames
 * directly from the internal buffer's data area, in a thread safe 
 * fashion.
 * 
 * @param nbframes number of frames to process
 * @return true if successful
 */
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
            debugError("Event buffer underrun in buffer %p\n",this);
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
                debugError("Frame buffer overrun in buffer %p\n",this);
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
                debugError("Frame buffer overrun in buffer %p\n",this);
                return false;
            }

            freebob_ringbuffer_read_advance(m_event_buffer, bytesread);
            bytes2read -= bytesread;
        }

        // the bytes2read should always be cluster aligned
        assert(bytes2read%cluster_size==0);
    }

    decrementFrameCounter(nbframes);
    
    return true;
}

/**
 * @brief Sets the buffer tail timestamp.
 * 
 * Set the buffer tail timestamp to \ref new_timestamp. This will recalculate
 * the internal state such that the buffer's timeframe starts at 
 * \ref new_timestamp.
 * 
 * This is thread safe.
 *
 * @note considers offsets
 * 
 * @param new_timestamp 
 */
void TimestampedBuffer::setBufferTailTimestamp(uint64_t new_timestamp) {

    // add the offsets
    int64_t diff=m_buffer_next_tail_timestamp - m_buffer_tail_timestamp;
    if (diff < 0) diff += m_wrap_at;
    
    float rate;
    
    if (diff) {
        rate=(float)diff / (float)m_update_period;
    } else {
        rate=m_nominal_rate;
    }
        
    int64_t ts=new_timestamp;
    ts += m_tick_offset;
    
    if (ts >= (int64_t)m_wrap_at) {
        ts -= m_wrap_at;
    } else if (ts < 0) {
        ts += m_wrap_at;
    }
    
#ifdef DEBUG
    if (new_timestamp >= m_wrap_at) {
        debugWarning("timestamp not wrapped: %llu\n",new_timestamp);
    }    
    if ((ts >= (int64_t)m_wrap_at) || (ts < 0 )) {
        debugWarning("ts not wrapped correctly: %lld\n",ts);
    }
#endif
    
    pthread_mutex_lock(&m_framecounter_lock);
    
    m_buffer_tail_timestamp = ts;
    
    m_dll_e2=m_update_period * m_nominal_rate;
    m_buffer_next_tail_timestamp = (uint64_t)((float)m_buffer_tail_timestamp + m_dll_e2);
    
    pthread_mutex_unlock(&m_framecounter_lock);    
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Set buffer tail timestamp for (%p) to %11llu => %11lld, NTS=%llu, DLL2=%f, RATE=%f\n",
                this, new_timestamp, ts, m_buffer_next_tail_timestamp, m_dll_e2, rate);

}

/**
 * \brief return the timestamp of the first frame in the buffer
 * 
 * This function returns the timestamp of the very first sample in
 * the StreamProcessor's buffer. It also returns the framecounter value 
 * for which this timestamp is valid.
 *
 * @param ts address to store the timestamp in
 * @param fc address to store the associated framecounter in
 */
void TimestampedBuffer::getBufferHeadTimestamp(uint64_t *ts, uint64_t *fc) {
    // NOTE: this is still ok with threads, because we use *fc to compute
    //       the timestamp
    *fc = m_framecounter;
    *ts=getTimestampFromTail(*fc);
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
    *fc = (uint64_t)m_framecounter;
    *ts = m_buffer_tail_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * @brief Get timestamp for a specific position from the buffer tail
 *
 * Returns the timestamp for a position that is nframes earlier than the 
 * buffer tail
 *
 * @param nframes number of frames
 * @return timestamp value
 */
uint64_t TimestampedBuffer::getTimestampFromTail(int nframes)
{
    // ts(x) = m_buffer_tail_timestamp -
    //         (m_buffer_next_tail_timestamp - m_buffer_tail_timestamp)/(samples_between_updates)*(x)
    
    int64_t diff=m_buffer_next_tail_timestamp - m_buffer_tail_timestamp;
    if (diff < 0) diff += m_wrap_at;
    
    float rate=(float)diff / (float)m_update_period;
    
    int64_t timestamp;
    
    pthread_mutex_lock(&m_framecounter_lock);

    timestamp=(int64_t)m_buffer_tail_timestamp - (int64_t)((nframes) * rate);

    pthread_mutex_unlock(&m_framecounter_lock);
    
    if(timestamp >= (int64_t)m_wrap_at) {
        timestamp -= m_wrap_at;
    } else if(timestamp < 0) {
        timestamp += m_wrap_at;
    }
    
    return (uint64_t)timestamp;
}

/**
 * @brief Get timestamp for a specific position from the buffer head
 *
 * Returns the timestamp for a position that is nframes later than the 
 * buffer head
 *
 * @param nframes number of frames
 * @return timestamp value
 */
uint64_t TimestampedBuffer::getTimestampFromHead(int nframes)
{
    return getTimestampFromTail(m_framecounter-nframes);
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

/**
 * Decrements the frame counter in a thread safe way.
 * 
 * @param nbframes number of frames to decrement
 */
void TimestampedBuffer::decrementFrameCounter(int nbframes) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter -= nbframes;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Increments the frame counter in a thread safe way.
 * Also updates the timestamp.
 *
 * @note the offsets defined by setTicksOffset and setFrameOffset
 *       are added here.
 *
 * @param nbframes the number of frames to add
 * @param new_timestamp the new timestamp
 */
void TimestampedBuffer::incrementFrameCounter(int nbframes, uint64_t new_timestamp) {
    
    // add the offsets
    int64_t diff=m_buffer_next_tail_timestamp - m_buffer_tail_timestamp;
    if (diff < 0) diff += m_wrap_at;
    
    float rate=(float)diff / (float)m_update_period;
        
    int64_t ts=new_timestamp;
    ts += (int64_t)m_tick_offset;
    
    if (ts >= (int64_t)m_wrap_at) {
        ts -= m_wrap_at;
    } else if (ts < 0) {
        ts += m_wrap_at;
    }
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Setting buffer tail timestamp for (%p) to %11llu => %11lld\n",
                this, new_timestamp, ts);
    
#ifdef DEBUG
    if (new_timestamp >= m_wrap_at) {
        debugWarning("timestamp not wrapped: %llu\n",new_timestamp);
    }    
    if ((ts >= (int64_t)m_wrap_at) || (ts < 0 )) {
        debugWarning("ts not wrapped correctly: %lld\n",ts);
    }
#endif

    // update the DLL
    diff = ts-(int64_t)m_buffer_next_tail_timestamp;
    
#ifdef DEBUG
    if ((diff > 1000) || (diff < -1000)) {
        debugWarning("(%p) difference rather large: %lld, %011lld, %011lld\n", 
            this, diff, ts, m_buffer_next_tail_timestamp);
    }
#endif

    // idea to implement it for nbframes values that differ from m_update_period:
    // diff = diff * nbframes/m_update_period
    // m_buffer_next_tail_timestamp = m_buffer_tail_timestamp + diff
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "(%p): diff=%lld ",
                this, diff);
                    
    // the maximal difference we can allow (64secs)
    const int64_t max=m_wrap_at/2;
    
    if(diff > max) {
        diff -= m_wrap_at;
    } else if (diff < -max) {
        diff += m_wrap_at;
    }
    
    float err=diff;
    
    debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "diff2=%lld err=%f\n",
                    diff, err);
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "B: FC=%10u, TS=%011llu, NTS=%011llu\n",
                    m_framecounter, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
    
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter += nbframes;
    
    m_buffer_tail_timestamp=m_buffer_next_tail_timestamp;
    m_buffer_next_tail_timestamp += (int64_t)(m_dll_b * err + m_dll_e2);
    
    m_dll_e2 += m_dll_c*err;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "U: FC=%10u, TS=%011llu, NTS=%011llu\n",
                    m_framecounter, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
    
    if (m_buffer_next_tail_timestamp >= m_wrap_at) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "Unwrapping next tail timestamp: %011llu",
                m_buffer_next_tail_timestamp);
                
        m_buffer_next_tail_timestamp -= m_wrap_at;
        
        debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, " => %011llu\n",
                m_buffer_next_tail_timestamp);

    }
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "A: TS=%011llu, NTS=%011llu, DLLe2=%f, RATE=%f\n",
                m_buffer_tail_timestamp, m_buffer_next_tail_timestamp, m_dll_e2, rate);
    
    pthread_mutex_unlock(&m_framecounter_lock);
    
    if(m_buffer_tail_timestamp>=m_wrap_at) {
        debugError("Wrapping failed for m_buffer_tail_timestamp! %011llu\n",m_buffer_tail_timestamp);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " IN=%011lld, TS=%011llu, NTS=%011llu\n",
                    ts, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
        
    }
    if(m_buffer_next_tail_timestamp>=m_wrap_at) {
        debugError("Wrapping failed for m_buffer_next_tail_timestamp! %011llu\n",m_buffer_next_tail_timestamp);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " IN=%011lld, TS=%011llu, NTS=%011llu\n",
                    ts, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
    }
    
    // this DLL allows the calculation of any sample timestamp relative to the buffer tail,
    // to the next period and beyond (through extrapolation)
    //
    // ts(x) = m_buffer_tail_timestamp +
    //         (m_buffer_next_tail_timestamp - m_buffer_tail_timestamp)/(samples_between_updates)*x
    
}

/**
 * @brief Print status info.
 */
void TimestampedBuffer::dumpInfo() {
    
    uint64_t ts_head, fc;
    getBufferHeadTimestamp(&ts_head,&fc);
    
    int64_t diff=(int64_t)ts_head - (int64_t)m_buffer_tail_timestamp;

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  TimestampedBuffer (%p) info:\n",this);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Frame counter         : %d\n", m_framecounter);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer head timestamp : %011llu\n",ts_head);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Buffer tail timestamp : %011llu\n",m_buffer_tail_timestamp);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Head - Tail           : %011lld\n",diff);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  rate                  : %f (%f)\n",m_dll_e2,m_dll_e2/m_update_period);
}

} // end of namespace FreebobUtil
