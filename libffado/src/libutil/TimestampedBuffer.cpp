/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "config.h"

#include "libutil/Atomic.h"
#include "libieee1394/cycletimer.h"

#include "TimestampedBuffer.h"
#include "assert.h"
#include "errno.h"

#include <cstdlib>
#include <cstring>

#define DLL_PI        (3.141592653589793238)
#define DLL_SQRT2     (1.414213562373095049)
#define DLL_2PI       (2.0 * DLL_PI)

// these are the defaults
#define DLL_OMEGA     (DLL_2PI * 0.01)
#define DLL_COEFF_B   (DLL_SQRT2 * DLL_OMEGA)
#define DLL_COEFF_C   (DLL_OMEGA * DLL_OMEGA)

#define FRAMES_PER_PROCESS_BLOCK 8
/*
#define ENTER_CRITICAL_SECTION { \
    if (pthread_mutex_trylock(&m_framecounter_lock) == EBUSY) { \
        debugWarning(" (%p) lock clash\n", this); \
        pthread_mutex_lock(&m_framecounter_lock); \
    } \
    }
*/
#define ENTER_CRITICAL_SECTION { \
    pthread_mutex_lock(&m_framecounter_lock); \
    }
#define EXIT_CRITICAL_SECTION { \
    pthread_mutex_unlock(&m_framecounter_lock); \
    }


namespace Util {

IMPL_DEBUG_MODULE( TimestampedBuffer, TimestampedBuffer, DEBUG_LEVEL_VERBOSE );

TimestampedBuffer::TimestampedBuffer(TimestampedBufferClient *c)
    : m_event_buffer(NULL), m_process_buffer(NULL), m_cluster_size( 0 ),
      m_process_block_size( 0 ),
      m_event_size(0), m_events_per_frame(0), m_buffer_size(0),
      m_bytes_per_frame(0), m_bytes_per_buffer(0),
      m_enabled( false ), m_transparent ( true ),
      m_wrap_at(0xFFFFFFFFFFFFFFFFLLU),
      m_Client(c), m_framecounter(0),
      m_buffer_tail_timestamp(TIMESTAMP_MAX + 1.0),
      m_buffer_next_tail_timestamp(TIMESTAMP_MAX + 1.0),
      m_dll_e2(0.0), m_dll_b(DLL_COEFF_B), m_dll_c(DLL_COEFF_C),
      m_nominal_rate(0.0), m_current_rate(0.0), m_update_period(0),
      // half a cycle is what we consider 'normal'
      m_max_abs_diff(3072/2)
{
    pthread_mutex_init(&m_framecounter_lock, NULL);
}

TimestampedBuffer::~TimestampedBuffer() {
    pthread_mutex_destroy(&m_framecounter_lock);

    if(m_event_buffer) ffado_ringbuffer_free(m_event_buffer);
    if(m_process_buffer) free(m_process_buffer);
}

/**
 * \brief Set the bandwidth of the DLL
 *
 * Sets the bandwith of the DLL in absolute frequency
 *
 * @param bw bandwidth in absolute frequency
 * @return true if successful
 */
bool TimestampedBuffer::setBandwidth(double bw) {
    double curr_bw = getBandwidth();
    debugOutput(DEBUG_LEVEL_VERBOSE," bandwidth %e => %e\n",
                                    curr_bw, bw);
    double tupdate = m_nominal_rate * (float)m_update_period;
    double bw_rel = bw * tupdate;
    if(bw_rel >= 0.5) {
        debugError("Requested bandwidth out of range: %f > %f\n", bw, 0.5 / tupdate);
        return false;
    }
    ENTER_CRITICAL_SECTION;
    m_dll_b = bw_rel * (DLL_SQRT2 * DLL_2PI);
    m_dll_c = bw_rel * bw_rel * DLL_2PI * DLL_2PI;
    EXIT_CRITICAL_SECTION;
    return true;
}

/**
 * \brief Returns the current bandwidth of the DLL
 *
 * Returns the current bandwith of the DLL in absolute frequency
 *
 * @return bandwidth in absolute frequency
 */
double TimestampedBuffer::getBandwidth() {
    double tupdate = m_nominal_rate * (float)m_update_period;
    double curr_bw = m_dll_b / (DLL_SQRT2 * DLL_2PI * tupdate);
    return curr_bw;
}


/**
 * \brief Set the nominal rate in timeunits/frame
 *
 * Sets the nominal rate in time units per frame. This rate is used
 * to initialize the DLL that will extract the effective rate based
 * upon the timestamps it gets fed.
 *
 * @param r rate
 * @return true if successful
 */
bool TimestampedBuffer::setNominalRate(float r) {
    debugOutput(DEBUG_LEVEL_VERBOSE," nominal rate %e => %e\n",
                                    m_nominal_rate, r);
    m_nominal_rate=r;
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
 * \brief Get the nominal update period (in frames)
 *
 * Gets the nominal update period. This period is the number of frames
 * between two timestamp updates (hence buffer writes)
 *
 * @return period in frames
 */
unsigned int TimestampedBuffer::getUpdatePeriod() {
    return m_update_period;
}

/**
 * \brief set the value at which timestamps should wrap around
 * @param w value to wrap at
 * @return true if successful
 */
bool TimestampedBuffer::setWrapValue(ffado_timestamp_t w) {
    m_wrap_at=w;
    return true;
}
#include <math.h>

/**
 * \brief return the effective rate
 *
 * Returns the effective rate calculated by the DLL.
 *
 * @return rate (in timeunits/frame)
 */
float TimestampedBuffer::getRate() {
    return m_current_rate;
}

/**
 * \brief presets the effective rate
 *
 * Presets the DLL such that the effective rate is as given
 * @param rate rate (in timeunits/frame)
 */
void TimestampedBuffer::setRate(float rate) {
    // we take the current tail timestamp and update the head timestamp
    // to ensure the rate is ok

    ENTER_CRITICAL_SECTION;

    m_current_rate = rate;
    m_dll_e2 = m_update_period * m_current_rate;
    m_buffer_next_tail_timestamp = (ffado_timestamp_t)((double)m_buffer_tail_timestamp + m_dll_e2);

    EXIT_CRITICAL_SECTION;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "for (%p) "
                       "NTS="TIMESTAMP_FORMAT_SPEC", DLL2=%f, RATE=%f\n",
                       this, m_buffer_next_tail_timestamp, m_dll_e2, getRate());
}

/**
 * \brief calculate the effective rate
 *
 * Returns the effective rate calculated by the DLL.
 * @note should be called with the lock held
 * @return rate (in timeunits/frame)
 */
float TimestampedBuffer::calculateRate() {
    ffado_timestamp_t diff;

    diff=m_buffer_next_tail_timestamp - m_buffer_tail_timestamp;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "getRate: %f/%f=%f\n",
                       (float)(diff),
                       (float)m_update_period,
                       ((float)(diff))/((float) m_update_period));

    // the maximal difference we can allow (64secs)
    const ffado_timestamp_t max=m_wrap_at/((ffado_timestamp_t)2);

    if(diff > max) {
        diff -= m_wrap_at;
    } else if (diff < -max) {
        diff += m_wrap_at;
    }

    float rate=((float)diff)/((float) m_update_period);
    if (rate<0.0) debugError("rate < 0! (%f)\n",rate);
    if (fabsf(m_nominal_rate - rate)>(m_nominal_rate*0.1)) {
        debugWarning("(%p) rate (%10.5f) more that 10%% off nominal "
                     "(rate=%10.5f, diff="TIMESTAMP_FORMAT_SPEC", update_period=%d)\n",
                     this, rate,m_nominal_rate,diff, m_update_period);

        return m_nominal_rate;
    } else {
        return rate;
    }
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
 * \brief Returns the current fill of the buffer
 *
 * This returns the buffer fill of the internal ringbuffer. This
 * can only be used as an indication because it's state is not
 * guaranteed to be consistent at all times due to threading issues.
 *
 * In order to get the number of frames in the buffer, use the
 * getBufferHeadTimestamp, getBufferTailTimestamp
 * functions
 *
 * @return the internal buffer fill in frames
 */
unsigned int TimestampedBuffer::getBufferFill() {
    //return ffado_ringbuffer_read_space(m_event_buffer)/(m_bytes_per_frame);
    return m_framecounter;
}

/**
 * \brief Returns the current write space in the buffer
 *
 * This returns the buffer free space of the internal ringbuffer. This
 * can only be used as an indication because it's state is not
 * guaranteed to be consistent at all times due to threading issues.
 *
 * @return the internal buffer fill in frames
 */
unsigned int TimestampedBuffer::getBufferSpace() {
    //return ffado_ringbuffer_write_space(m_event_buffer)/(m_bytes_per_frame);
    assert(m_buffer_size-m_framecounter >= 0);
    return m_buffer_size-m_framecounter;
}

/**
 * \brief Resets the TimestampedBuffer
 *
 * Resets the TimestampedBuffer, clearing the buffers and counters.
 * [DEL Also resets the DLL to the nominal values. DEL]
 *
 * \note when this is called, you should make sure that the buffer
 *       tail timestamp gets set before continuing
 *
 * @return true if successful
 */
bool TimestampedBuffer::clearBuffer() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Clearing buffer\n");
    ffado_ringbuffer_reset(m_event_buffer);
    resetFrameCounter();

    m_current_rate = m_nominal_rate;
    m_dll_e2 = m_current_rate * (float)m_update_period;

    return true;
}

/**
 * \brief Prepares the TimestampedBuffer
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

    debugOutput(DEBUG_LEVEL_VERBOSE," wrapping at "TIMESTAMP_FORMAT_SPEC"\n",m_wrap_at);

    assert(m_buffer_size);
    assert(m_events_per_frame);
    assert(m_event_size);

    assert(m_nominal_rate != 0.0L);
    assert(m_update_period != 0);

    m_current_rate = m_nominal_rate;

    if( !resizeBuffer(m_buffer_size) ) {
        debugError("Failed to allocate the event buffer\n");
        return false;
    }

    // allocate the temporary cluster buffer
    // NOTE: has to be a multiple of 8 in order to
    //       correctly decode midi bytes (since that 
    //       enforces packet alignment)
    m_cluster_size = m_events_per_frame * m_event_size;
    m_process_block_size = m_cluster_size * FRAMES_PER_PROCESS_BLOCK;
    if( !(m_process_buffer=(char *)calloc(m_process_block_size, 1))) {
            debugFatal("Could not allocate temporary cluster buffer\n");
        ffado_ringbuffer_free(m_event_buffer);
        return false;
    }

    // init the DLL
    m_dll_e2 = m_nominal_rate * (float)m_update_period;

    // init the timestamps to a bogus value, as there is not
    // really something sane to say about them
    m_buffer_tail_timestamp = TIMESTAMP_MAX + 1.0;
    m_buffer_next_tail_timestamp = TIMESTAMP_MAX + 1.0;

    return true;
}

/**
 * Resizes the timestamped buffer
 * @return true if successful, false if not
 */
bool
TimestampedBuffer::resizeBuffer(unsigned int new_size)
{
    assert(new_size);
    assert(m_events_per_frame);
    assert(m_event_size);

    // if present, free the previous buffer
    if(m_event_buffer) {
        ffado_ringbuffer_free(m_event_buffer);
    }
    // allocate a new one
    if( !(m_event_buffer = ffado_ringbuffer_create(
            (m_events_per_frame * new_size) * m_event_size))) {
        debugFatal("Could not allocate memory event ringbuffer\n");

        return false;
    }
    resetFrameCounter();

    m_current_rate = m_nominal_rate;
    m_dll_e2 = m_current_rate * (float)m_update_period;

    m_buffer_size = new_size;

    return true;
}

/**
 * @brief Insert a dummy frame to the head buffer
 *
 * Writes one frame of dummy data to the head of the buffer.
 * This is to assist the phase sync of several buffers.
 * 
 * Note: currently the dummy data is added to the tail of the
 *       buffer, but without updating the timestamp.
 *
 * @return true if successful
 */
bool TimestampedBuffer::writeDummyFrame() {

    unsigned int write_size=m_event_size*m_events_per_frame;
    
    char dummy[write_size]; // one frame of garbage
    memset(dummy,0,write_size);

    // add the data payload to the ringbuffer
    if (ffado_ringbuffer_write(m_event_buffer,dummy,write_size) < write_size)
    {
//         debugWarning("writeFrames buffer overrun\n");
        return false;
    }

//     incrementFrameCounter(nframes,ts);
    
    // increment without updating the DLL
    ENTER_CRITICAL_SECTION;
    m_framecounter++;
    EXIT_CRITICAL_SECTION;
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
bool TimestampedBuffer::writeFrames(unsigned int nframes, char *data, ffado_timestamp_t ts) {

    unsigned int write_size=nframes*m_event_size*m_events_per_frame;

    if (m_transparent) {
        // while disabled, we don't update the DLL, nor do we write frames
        // we just set the correct timestamp for the frames
        if (m_buffer_tail_timestamp < TIMESTAMP_MAX && m_buffer_next_tail_timestamp < TIMESTAMP_MAX) {
            incrementFrameCounter(nframes, ts);
            decrementFrameCounter(nframes);
        }
        setBufferTailTimestamp(ts);
    } else {
        // add the data payload to the ringbuffer
        size_t written = ffado_ringbuffer_write(m_event_buffer, data, write_size);
        if (written < write_size)
        {
            debugWarning("ringbuffer full, %u, %zd\n", write_size, written);
            return false;
        }
        incrementFrameCounter(nframes, ts);
    }
    return true;
}

/**
 * @brief Preload frames into the buffer
 *
 * Preload \ref nframes of frames from the buffer pointed to by \ref data to the
 * internal ringbuffer. Does not care about transparency. Keeps the buffer head or tail
 * timestamp constant.
 *
 * @note not thread safe
 *
 * @param nframes number of frames to copy
 * @param data pointer to the frame buffer
 * @param keep_head_ts if true, keep the head timestamp constant. If false, keep the
 *                     tail timestamp constant.
 * @return true if successful
 */
bool TimestampedBuffer::preloadFrames(unsigned int nframes, char *data, bool keep_head_ts) {
    unsigned int write_size = nframes * m_event_size * m_events_per_frame;
    // add the data payload to the ringbuffer
    size_t written = ffado_ringbuffer_write(m_event_buffer, data, write_size);
    if (written < write_size)
    {
        debugWarning("ringbuffer full, request: %u, actual: %zd\n", write_size, written);
        return false;
    }

    // make sure the head timestamp remains identical
    signed int fc;
    ffado_timestamp_t ts;

    if (keep_head_ts) {
        getBufferHeadTimestamp(&ts, &fc);
    } else {
        getBufferTailTimestamp(&ts, &fc);
    }
    // update frame counter
    m_framecounter += nframes;
    if (keep_head_ts) {
        setBufferHeadTimestamp(ts);
    } else {
        setBufferTailTimestamp(ts);
    }
    return true;
}

/**
 * @brief Drop frames from the head of the buffer
 *
 * drops \ref nframes of frames from the head of internal buffer
 *
 * @param nframes number of frames to drop
 * @return true if successful
 */
bool
TimestampedBuffer::dropFrames(unsigned int nframes) {
    unsigned int read_size = nframes * m_event_size * m_events_per_frame;
    ffado_ringbuffer_read_advance(m_event_buffer, read_size);
    decrementFrameCounter(nframes);
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

    if (m_transparent) {
        return true; // FIXME: the data still doesn't make sense!
    } else {
        // get the data payload to the ringbuffer
        if ((ffado_ringbuffer_read(m_event_buffer,data,read_size)) < read_size)
        {
            debugWarning("readFrames buffer underrun\n");
            return false;
        }
        decrementFrameCounter(nframes);
    }
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
bool TimestampedBuffer::blockProcessWriteFrames(unsigned int nbframes, ffado_timestamp_t ts) {
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "(%p) Writing %u frames for ts "TIMESTAMP_FORMAT_SPEC"\n",
                       this, nbframes, ts);
    int xrun;
    unsigned int offset = 0;

    ffado_ringbuffer_data_t vec[2];
    // we received one period of frames
    // this is period_size*dimension of events
    unsigned int events2write = nbframes * m_events_per_frame;
    unsigned int bytes2write = events2write * m_event_size;

    /* write events2write bytes to the ringbuffer
    *  first see if it can be done in one read.
    *  if so, ok.
    *  otherwise write up to a multiple of clusters directly to the buffer
    *  then do the buffer wrap around using ringbuffer_write
    *  then write the remaining data directly to the buffer in a third pass
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */

    while(bytes2write > 0) {
        int byteswritten = 0;

        unsigned int frameswritten = (nbframes * m_cluster_size - bytes2write) / m_cluster_size;
        offset = frameswritten;

        ffado_ringbuffer_get_write_vector(m_event_buffer, vec);

        if(vec[0].len + vec[1].len < m_process_block_size) { // this indicates a full event buffer
            debugError("Event buffer overrun in buffer %p, fill: %zd, bytes2write: %u \n",
                       this, ffado_ringbuffer_read_space(m_event_buffer), bytes2write);
            debugShowBackLog();
            return false;
        }

        /* if we don't take care we will get stuck in an infinite loop
        * because we align to a cluster boundary later
        * the remaining nb of bytes in one write operation can be
        * smaller than one cluster
        * this can happen because the ringbuffer size is always a power of 2
        */
        if(vec[0].len < m_process_block_size) {

            // encode to the temporary buffer
            // note that we always process 8 frames at once, in order to ensure that
            // we don't have to care about the DBC field
            xrun = m_Client->processWriteBlock(m_process_buffer, FRAMES_PER_PROCESS_BLOCK, offset);

            if(xrun < 0) {
                // xrun detected
                debugError("Frame buffer underrun in buffer %p\n",this);
                return false;
            }

            // use the ringbuffer function to write one cluster
            // the write function handles the wrap around.
            ffado_ringbuffer_write(m_event_buffer,
                                   m_process_buffer,
                                   m_process_block_size);

            // we advanced one cluster_size
            bytes2write -= m_process_block_size;

        } else { //

            if(bytes2write > vec[0].len) {
                // align to a cluster boundary
                byteswritten = vec[0].len - (vec[0].len % m_process_block_size);
            } else {
                byteswritten = bytes2write;
            }

            xrun = m_Client->processWriteBlock(vec[0].buf,
                                               byteswritten / m_cluster_size,
                                               offset);

            if(xrun < 0 ) {
                // xrun detected
                debugError("Frame buffer underrun in buffer %p\n",this);
                return false; // FIXME: return false ?
            }

            ffado_ringbuffer_write_advance(m_event_buffer, byteswritten);
            bytes2write -= byteswritten;
        }

        // the bytes2write should always be process block aligned
        assert(bytes2write % m_process_block_size == 0);

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

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "(%p) Reading %u frames\n",
                       this, nbframes);

    int xrun;
    unsigned int offset = 0;

    ffado_ringbuffer_data_t vec[2];
    // we received one period of frames on each connection
    // this is period_size*dimension of events

    unsigned int events2read = nbframes * m_events_per_frame;
    unsigned int bytes2read = events2read * m_event_size;
    /* read events2read bytes from the ringbuffer
    *  first see if it can be done in one read.
    *  if so, ok.
    *  otherwise read up to a multiple of clusters directly from the buffer
    *  then do the buffer wrap around using ringbuffer_read
    *  then read the remaining data directly from the buffer in a third pass
    *  Make sure that we cannot end up on a non-cluster aligned position!
    */

    while(bytes2read > 0) {
        unsigned int framesread = (nbframes * m_cluster_size - bytes2read) / m_cluster_size;
        offset = framesread;

        int bytesread = 0;

        ffado_ringbuffer_get_read_vector(m_event_buffer, vec);

        if(vec[0].len + vec[1].len < m_process_block_size) { // this indicates an empty event buffer
            debugError("Event buffer underrun in buffer %p\n",this);
            return false;
        }

        /* if we don't take care we will get stuck in an infinite loop
        * because we align to a cluster boundary later
        * the remaining nb of bytes in one read operation can be smaller than one cluster
        * this can happen because the ringbuffer size is always a power of 2
                */
        if(vec[0].len < m_process_block_size) {
            // use the ringbuffer function to read one cluster
            // the read function handles wrap around
            ffado_ringbuffer_read(m_event_buffer, m_process_buffer, m_process_block_size);

            assert(m_Client);
            // note that we always process 8 frames at once, in order to ensure that
            // we don't have to care about the DBC field
            xrun = m_Client->processReadBlock(m_process_buffer, FRAMES_PER_PROCESS_BLOCK, offset);

            if(xrun < 0) {
                // xrun detected
                debugError("Frame buffer overrun in buffer %p\n",this);
                    return false;
            }

            // we advanced one cluster_size
            bytes2read -= m_process_block_size;

        } else { //

            if(bytes2read > vec[0].len) {
                // align to a cluster boundary
                bytesread = vec[0].len - (vec[0].len % m_process_block_size);
            } else {
                bytesread = bytes2read;
            }

            assert(m_Client);
            xrun = m_Client->processReadBlock(vec[0].buf, bytesread/m_cluster_size, offset);

            if(xrun < 0) {
                // xrun detected
                debugError("Frame buffer overrun in buffer %p\n",this);
                return false;
            }

            ffado_ringbuffer_read_advance(m_event_buffer, bytesread);
            bytes2read -= bytesread;
        }

        // the bytes2read should always be cluster aligned
        assert(bytes2read % m_process_block_size == 0);
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
void TimestampedBuffer::setBufferTailTimestamp(ffado_timestamp_t new_timestamp) {

    // add the offsets
    ffado_timestamp_t ts = new_timestamp;

    if (ts >= m_wrap_at) {
        ts -= m_wrap_at;
    } else if (ts < 0) {
        ts += m_wrap_at;
    }

#ifdef DEBUG
    if (new_timestamp >= m_wrap_at) {
        debugWarning("timestamp not wrapped: "TIMESTAMP_FORMAT_SPEC"\n",new_timestamp);
    }
    if ((ts >= m_wrap_at) || (ts < 0 )) {
        debugWarning("ts not wrapped correctly: "TIMESTAMP_FORMAT_SPEC"\n",ts);
    }
#endif

    ENTER_CRITICAL_SECTION;

    m_buffer_tail_timestamp = ts;

    m_dll_e2 = m_update_period * (double)m_current_rate;
    m_buffer_next_tail_timestamp = (ffado_timestamp_t)((double)m_buffer_tail_timestamp + m_dll_e2);

    EXIT_CRITICAL_SECTION;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "for (%p) to "TIMESTAMP_FORMAT_SPEC" => "TIMESTAMP_FORMAT_SPEC", "
                       "NTS="TIMESTAMP_FORMAT_SPEC", DLL2=%f, RATE=%f\n",
                       this, new_timestamp, ts, m_buffer_next_tail_timestamp, m_dll_e2, getRate());
}

/**
 * @brief Sets the buffer head timestamp.
 *
 * Set the buffer tail timestamp such that the buffer head timestamp becomes
 * \ref new_timestamp. This does not consider offsets, because it's use is to
 * make sure the following is true after setBufferHeadTimestamp(x):
 *   x == getBufferHeadTimestamp()
 *
 * This is thread safe.
 *
 * @param new_timestamp
 */
void TimestampedBuffer::setBufferHeadTimestamp(ffado_timestamp_t new_timestamp) {

#ifdef DEBUG
    if (new_timestamp >= m_wrap_at) {
        debugWarning("timestamp not wrapped: "TIMESTAMP_FORMAT_SPEC"\n", new_timestamp);
    }
#endif

    ffado_timestamp_t ts = new_timestamp;

    ENTER_CRITICAL_SECTION;

    // add the time
    ts += (ffado_timestamp_t)(m_current_rate * (float)(m_framecounter));

    if (ts >= m_wrap_at) {
        ts -= m_wrap_at;
    } else if (ts < 0) {
        ts += m_wrap_at;
    }

    m_buffer_tail_timestamp = ts;

    m_dll_e2 = m_update_period * (double)m_current_rate;
    m_buffer_next_tail_timestamp = (ffado_timestamp_t)((double)m_buffer_tail_timestamp + m_dll_e2);

    EXIT_CRITICAL_SECTION;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "for (%p) to "TIMESTAMP_FORMAT_SPEC" => "TIMESTAMP_FORMAT_SPEC", "
                       "NTS="TIMESTAMP_FORMAT_SPEC", DLL2=%f, RATE=%f\n",
                       this, new_timestamp, ts, m_buffer_next_tail_timestamp, m_dll_e2, getRate());
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
void TimestampedBuffer::getBufferHeadTimestamp(ffado_timestamp_t *ts, signed int *fc) {
    ENTER_CRITICAL_SECTION;
    *fc = m_framecounter;
    *ts = getTimestampFromTail(*fc);
    EXIT_CRITICAL_SECTION;
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
void TimestampedBuffer::getBufferTailTimestamp(ffado_timestamp_t *ts, signed int *fc) {
    ENTER_CRITICAL_SECTION;
    *fc = m_framecounter;
    *ts = getTimestampFromTail(0);
    EXIT_CRITICAL_SECTION;
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
ffado_timestamp_t TimestampedBuffer::getTimestampFromTail(int nframes)
{
    // ts(x) = m_buffer_tail_timestamp -
    //         (m_buffer_next_tail_timestamp - m_buffer_tail_timestamp)/(samples_between_updates)*(x)
    ffado_timestamp_t timestamp;
    timestamp = m_buffer_tail_timestamp;

    timestamp -= (ffado_timestamp_t)((nframes) * m_current_rate);

    if(timestamp >= m_wrap_at) {
        timestamp -= m_wrap_at;
    } else if(timestamp < 0) {
        timestamp += m_wrap_at;
    }

    return timestamp;
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
ffado_timestamp_t TimestampedBuffer::getTimestampFromHead(int nframes)
{
    ffado_timestamp_t retval;
    ENTER_CRITICAL_SECTION;
    retval = getTimestampFromTail(m_framecounter - nframes);
    EXIT_CRITICAL_SECTION;
    return retval;
}

/**
 * Resets the frame counter, in a atomic way. This
 * is thread safe.
 */
void TimestampedBuffer::resetFrameCounter() {
    ENTER_CRITICAL_SECTION;
    m_framecounter = 0;
    EXIT_CRITICAL_SECTION;
}

/**
 * Decrements the frame counter in a thread safe way.
 *
 * @param nbframes number of frames to decrement
 */
void TimestampedBuffer::decrementFrameCounter(unsigned int nbframes) {
    ENTER_CRITICAL_SECTION;
    m_framecounter -= nbframes;
    EXIT_CRITICAL_SECTION;
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
void TimestampedBuffer::incrementFrameCounter(unsigned int nbframes, ffado_timestamp_t new_timestamp) {

    // require the timestamps to be in the correct range
    assert(new_timestamp < m_wrap_at);
    assert(new_timestamp >= 0);
    // if this is not true the timestamps have to be corrected
    // to account for the non-uniform update period
    assert(nbframes == m_update_period);

    // the difference between the given TS and the one predicted for this time instant
    // this is the error for the DLL
    ffado_timestamp_t diff = new_timestamp - m_buffer_next_tail_timestamp;

    // correct for when new_timestamp doesn't wrap at the same time as
    // m_buffer_next_tail_timestamp
    if (diff > m_wrap_at/2)
      diff = m_wrap_at - diff;
    else
    if (diff < -m_wrap_at/2)
      diff = m_wrap_at + diff;
#ifdef DEBUG

    // check whether the update is within the allowed bounds
    ffado_timestamp_t max_abs_diff = m_max_abs_diff;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       " nbframes: %d, m_update_period: %d \n",
                       nbframes, m_update_period);
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       " tail TS: "TIMESTAMP_FORMAT_SPEC", next tail TS: "TIMESTAMP_FORMAT_SPEC"\n", 
                       m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       " new TS: "TIMESTAMP_FORMAT_SPEC", diff: "TIMESTAMP_FORMAT_SPEC"\n", 
                       new_timestamp, diff);

    if (diff > max_abs_diff) {
        //debugShowBackLogLines(100);
        debugWarning("(%p) difference rather large (+): diff="TIMESTAMP_FORMAT_SPEC", max="TIMESTAMP_FORMAT_SPEC", "TIMESTAMP_FORMAT_SPEC", "TIMESTAMP_FORMAT_SPEC"\n",
            this, diff, max_abs_diff, new_timestamp, m_buffer_next_tail_timestamp);
    } else if (diff < -max_abs_diff) {
        //debugShowBackLogLines(100);
        debugWarning("(%p) difference rather large (-): diff="TIMESTAMP_FORMAT_SPEC", max="TIMESTAMP_FORMAT_SPEC", "TIMESTAMP_FORMAT_SPEC", "TIMESTAMP_FORMAT_SPEC"\n",
            this, diff, -max_abs_diff, new_timestamp, m_buffer_next_tail_timestamp);
    }

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "(%p): diff="TIMESTAMP_FORMAT_SPEC" ",
                       this, diff);
#endif

    double err = diff;
    debugOutputShortExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                            "diff2="TIMESTAMP_FORMAT_SPEC" err=%f\n",
                            diff, err);
    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "B: FC=%10u, TS="TIMESTAMP_FORMAT_SPEC", NTS="TIMESTAMP_FORMAT_SPEC"\n",
                       m_framecounter, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);

    ENTER_CRITICAL_SECTION;
    m_framecounter += nbframes;
    m_buffer_tail_timestamp = m_buffer_next_tail_timestamp;
    m_buffer_next_tail_timestamp = m_buffer_next_tail_timestamp + (ffado_timestamp_t)(m_dll_b * err + m_dll_e2);
    m_dll_e2 += m_dll_c*err;

    if (m_buffer_next_tail_timestamp >= m_wrap_at) {
        debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                           "Unwrapping next tail timestamp: "TIMESTAMP_FORMAT_SPEC"",
                           m_buffer_next_tail_timestamp);

        m_buffer_next_tail_timestamp -= m_wrap_at;

        debugOutputShortExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                                " => "TIMESTAMP_FORMAT_SPEC"\n",
                                m_buffer_next_tail_timestamp);

    }
    m_current_rate = calculateRate();
    EXIT_CRITICAL_SECTION;

    debugOutputExtreme(DEBUG_LEVEL_VERY_VERBOSE,
                       "A: TS="TIMESTAMP_FORMAT_SPEC", NTS="TIMESTAMP_FORMAT_SPEC", DLLe2=%f, RATE=%f\n",
                       m_buffer_tail_timestamp, m_buffer_next_tail_timestamp, m_dll_e2, m_current_rate);


    if(m_buffer_tail_timestamp>=m_wrap_at) {
        debugError("Wrapping failed for m_buffer_tail_timestamp! "TIMESTAMP_FORMAT_SPEC"\n",m_buffer_tail_timestamp);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " IN="TIMESTAMP_FORMAT_SPEC", TS="TIMESTAMP_FORMAT_SPEC", NTS="TIMESTAMP_FORMAT_SPEC"\n",
                    new_timestamp, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);

    }
    if(m_buffer_next_tail_timestamp>=m_wrap_at) {
        debugError("Wrapping failed for m_buffer_next_tail_timestamp! "TIMESTAMP_FORMAT_SPEC"\n",m_buffer_next_tail_timestamp);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " IN="TIMESTAMP_FORMAT_SPEC", TS="TIMESTAMP_FORMAT_SPEC", NTS="TIMESTAMP_FORMAT_SPEC"\n",
                    new_timestamp, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
    }
    
    if(m_buffer_tail_timestamp==m_buffer_next_tail_timestamp) {
        debugError("Current and next timestamps are equal: "TIMESTAMP_FORMAT_SPEC" "TIMESTAMP_FORMAT_SPEC"\n",
                   m_buffer_tail_timestamp,m_buffer_next_tail_timestamp);
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

    ffado_timestamp_t ts_head;
    signed int fc;
    getBufferHeadTimestamp(&ts_head,&fc);

#ifdef DEBUG
    ffado_timestamp_t diff=(ffado_timestamp_t)ts_head - (ffado_timestamp_t)m_buffer_tail_timestamp;
#endif

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  TimestampedBuffer (%p): %04d frames, %04d events\n",
                                          this, m_framecounter, getBufferFill());
    debugOutputShort( DEBUG_LEVEL_NORMAL, "   Timestamps           : head: "TIMESTAMP_FORMAT_SPEC", Tail: "TIMESTAMP_FORMAT_SPEC", Next tail: "TIMESTAMP_FORMAT_SPEC"\n",
                                          ts_head, m_buffer_tail_timestamp, m_buffer_next_tail_timestamp);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "    Head - Tail         : "TIMESTAMP_FORMAT_SPEC" (%f frames)\n", diff, diff/m_dll_e2*m_update_period);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "   DLL Rate             : %f (%f)\n", m_dll_e2, m_dll_e2/m_update_period);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "   DLL Bandwidth        : %10e 1/ticks (%f Hz)\n", getBandwidth(), getBandwidth() * TICKS_PER_SECOND);
}

} // end of namespace Util
