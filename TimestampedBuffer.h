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
#ifndef __FREEBOB_TIMESTAMPEDBUFFER__
#define __FREEBOB_TIMESTAMPEDBUFFER__

#include "../debugmodule/debugmodule.h"
#include "libutil/ringbuffer.h"

namespace FreebobUtil {

class TimestampedBufferClient;

class TimestampedBuffer {

public:

    TimestampedBuffer(TimestampedBufferClient *);
    virtual ~TimestampedBuffer();
    
    bool writeFrames(unsigned int nbframes, char *data);
    bool readFrames(unsigned int nbframes, char *data);
    
    bool blockProcessWriteFrames(unsigned int nbframes, int64_t ts);
    bool blockProcessReadFrames(unsigned int nbframes);
    
    bool init();
    bool prepare();
    bool reset();
    
    bool setEventSize(unsigned int s);
    bool setEventsPerFrame(unsigned int s);
    bool setBufferSize(unsigned int s);
    
    unsigned int getBufferFill();
    
    // timestamp stuff
    int getFrameCounter() {return m_framecounter;};

    void decrementFrameCounter(int nbframes);
    void incrementFrameCounter(int nbframes, uint64_t new_timestamp);
    void resetFrameCounter();
    
    void getBufferHeadTimestamp(uint64_t *ts, uint64_t *fc);
    void getBufferTailTimestamp(uint64_t *ts, uint64_t *fc);
    
    void setBufferTailTimestamp(uint64_t new_timestamp);
   
    // dll stuff
    void setNominalRate(double r) {m_nominal_rate=r;};
    double getRate() {return m_dll_e2;};
    
    void setUpdatePeriod(unsigned int t) {m_update_period=t;};
    
    // misc stuff
    void dumpInfo();
    
    
protected:

    freebob_ringbuffer_t * m_event_buffer;
    char* m_cluster_buffer;
    
    unsigned int m_event_size; // the size of one event
    unsigned int m_events_per_frame; // the number of events in a frame
    unsigned int m_buffer_size; // the number of frames in the buffer
    unsigned int m_bytes_per_frame;
    unsigned int m_bytes_per_buffer;
    
    TimestampedBufferClient *m_Client;

    DECLARE_DEBUG_MODULE;
    
private:
    // the framecounter gives the number of frames in the buffer
    signed int m_framecounter;
    
    // the buffer tail timestamp gives the timestamp of the last frame
    // that was put into the buffer
    uint64_t   m_buffer_tail_timestamp;
    uint64_t   m_buffer_next_tail_timestamp;
    
    // the buffer head timestamp gives the timestamp of the first frame
    // that was put into the buffer
//     uint64_t   m_buffer_head_timestamp;
    // this mutex protects the access to the framecounter
    // and the buffer head timestamp.
    pthread_mutex_t m_framecounter_lock;

    // tracking DLL variables
    double m_dll_e2;
    double m_dll_b;
    double m_dll_c;
    
    double m_nominal_rate;
    unsigned int m_update_period;
};

class TimestampedBufferClient {
    public:
        TimestampedBufferClient() {};
        virtual ~TimestampedBufferClient() {};

        virtual bool processReadBlock(char *data, unsigned int nevents, unsigned int offset)=0;
        virtual bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset)=0;

};

} // end of namespace FreebobUtil

#endif /* __FREEBOB_TIMESTAMPEDBUFFER__ */


