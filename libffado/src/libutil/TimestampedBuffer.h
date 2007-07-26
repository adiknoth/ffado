/* $Id$ */

/*
 *   FFADO Streaming API
 *   FFADO = Firewire (pro-)audio for linux
 *
 *   http://ffado.sf.net
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
#ifndef __FFADO_TIMESTAMPEDBUFFER__
#define __FFADO_TIMESTAMPEDBUFFER__

#include "../debugmodule/debugmodule.h"
#include "libutil/ringbuffer.h"

//typedef float ffado_timestamp_t;
//#define TIMESTAMP_FORMAT_SPEC "%14.3f"
typedef double ffado_timestamp_t;
#define TIMESTAMP_FORMAT_SPEC "%14.3f"
// typedef int64_t ffado_timestamp_t;
// #define TIMESTAMP_FORMAT_SPEC "%012lld"

namespace Util {


class TimestampedBufferClient;

/**
 * \brief Class implementing a frame buffer that is time-aware
 *
 * This class implements a buffer that is time-aware. Whenever new frames
 * are written to the buffer, the timestamp corresponding to the last frame
 * in the buffer is updated. This allows to calculate the timestamp of any
 * other frame in the buffer.
 *
 * The buffer is a frame buffer, having the following parameters defining
 * it's behaviour:
 * - buff_size: buffer size in frames (setBufferSize())
 * - events_per_frame: the number of events per frame (setEventsPerFrame())
 * - event_size: the storage size of the events (in bytes) (setEventSize())
 *
 * The total size of the buffer (in bytes) is at least
 * buff_size*events_per_frame*event_size.
 *
 * Timestamp tracking is done by requiring that a timestamp is specified every
 * time frames are added to the buffer. In combination with the buffer fill and
 * the frame rate (calculated internally), this allows to calculate the timestamp
 * of any frame in the buffer. In order to initialize the internal data structures,
 * the setNominalRate() and setUpdatePeriod() functions are provided.
 *
 * \note Currently the class only supports fixed size writes of size update_period.
 *       This can change in the future, implementation ideas are already in place.
 *
 * The TimestampedBuffer class is time unit agnostic. It can handle any time unit
 * as long as it fits in a 64 bit unsigned integer. The buffer supports wrapped
 * timestamps using (...).
 *
 * There are two methods of reading and writing to the buffer.
 *
 * The first method uses conventional readFrames() and writeFrames() functions.
 *
 * The second method makes use of the TimestampedBufferClient interface. When a
 * TimestampedBuffer is created, it is required that a TimestampedBufferClient is
 * registered. This client implements the processReadBlock and processWriteBlock
 * functions. These are block processing 'callbacks' that allow zero-copy processing
 * of the buffer contents. In order to initiate block processing, the
 * blockProcessWriteFrames and blockProcessReadFrames functions are provided by
 * TimestampedBuffer.
 *
 */
class TimestampedBuffer {

public:


    TimestampedBuffer(TimestampedBufferClient *);
    virtual ~TimestampedBuffer();
    
    bool writeDummyFrame();
    
    bool writeFrames(unsigned int nbframes, char *data, ffado_timestamp_t ts);
    bool readFrames(unsigned int nbframes, char *data);

    bool blockProcessWriteFrames(unsigned int nbframes, ffado_timestamp_t ts);
    bool blockProcessReadFrames(unsigned int nbframes);

    bool init();
    bool prepare();
    bool reset();

    bool setEventSize(unsigned int s);
    bool setEventsPerFrame(unsigned int s);
    bool setBufferSize(unsigned int s);
    unsigned int getBufferSize() {return m_buffer_size;};

    unsigned int getBytesPerFrame() {return m_bytes_per_frame;};

    bool setWrapValue(ffado_timestamp_t w);

    unsigned int getBufferFill();

    // timestamp stuff
    int getFrameCounter() {return m_framecounter;};

    void getBufferHeadTimestamp(ffado_timestamp_t *ts, signed int *fc);
    void getBufferTailTimestamp(ffado_timestamp_t *ts, signed int *fc);

    void setBufferTailTimestamp(ffado_timestamp_t new_timestamp);
    void setBufferHeadTimestamp(ffado_timestamp_t new_timestamp);

    ffado_timestamp_t getTimestampFromTail(int nframes);
    ffado_timestamp_t getTimestampFromHead(int nframes);

    // buffer offset stuff
    /// return the tick offset value
    ffado_timestamp_t getTickOffset() {return m_tick_offset;};

    bool setFrameOffset(int nframes);
    bool setTickOffset(ffado_timestamp_t);

    // dll stuff
    bool setNominalRate(float r);
    float getRate();

    bool setUpdatePeriod(unsigned int t);

    // misc stuff
    void dumpInfo();
    void setVerboseLevel(int l) {setDebugLevel(l);};

private:
    void decrementFrameCounter(int nbframes);
    void incrementFrameCounter(int nbframes, ffado_timestamp_t new_timestamp);
    void resetFrameCounter();

protected:

    ffado_ringbuffer_t * m_event_buffer;
    char* m_cluster_buffer;

    unsigned int m_event_size; // the size of one event
    unsigned int m_events_per_frame; // the number of events in a frame
    unsigned int m_buffer_size; // the number of frames in the buffer
    unsigned int m_bytes_per_frame;
    unsigned int m_bytes_per_buffer;

    ffado_timestamp_t m_wrap_at; // value to wrap at

    TimestampedBufferClient *m_Client;

    DECLARE_DEBUG_MODULE;

private:
    // the framecounter gives the number of frames in the buffer
    signed int m_framecounter;

    // the offset that define the timing of the buffer
    ffado_timestamp_t m_tick_offset;

    // the buffer tail timestamp gives the timestamp of the last frame
    // that was put into the buffer
    ffado_timestamp_t   m_buffer_tail_timestamp;
    ffado_timestamp_t   m_buffer_next_tail_timestamp;

    // this mutex protects the access to the framecounter
    // and the buffer head timestamp.
    pthread_mutex_t m_framecounter_lock;

    // tracking DLL variables
    float m_dll_e2;
    float m_dll_b;
    float m_dll_c;

    float m_nominal_rate;
    unsigned int m_update_period;
};

/**
 * \brief Interface to be implemented by TimestampedBuffer clients
 */
class TimestampedBufferClient {
    public:
        TimestampedBufferClient() {};
        virtual ~TimestampedBufferClient() {};

        virtual bool processReadBlock(char *data, unsigned int nevents, unsigned int offset)=0;
        virtual bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset)=0;

};

} // end of namespace Util

#endif /* __FFADO_TIMESTAMPEDBUFFER__ */


