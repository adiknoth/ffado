/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
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
#ifndef __FREEBOB_STREAMPROCESSOR__
#define __FREEBOB_STREAMPROCESSOR__

#include "../debugmodule/debugmodule.h"

#include "IsoStream.h"
#include "PortManager.h"

#include <pthread.h>

#include "libutil/StreamStatistics.h"

namespace FreebobStreaming {

class StreamProcessorManager;

/*!
\brief Class providing a generic interface for Stream Processors

 A stream processor multiplexes or demultiplexes an ISO stream into a 
 collection of ports. This class should be subclassed, and the relevant
 functions should be overloaded.
 
*/
class StreamProcessor : public IsoStream, 
                        public PortManager {

    friend class StreamProcessorManager;

public:
    enum EProcessorType {
            E_Receive,
            E_Transmit
    };

    StreamProcessor(enum IsoStream::EStreamType type, int port, int framerate);
    virtual ~StreamProcessor();

    virtual enum raw1394_iso_disposition 
            putPacket(unsigned char *data, unsigned int length, 
                    unsigned char channel, unsigned char tag, unsigned char sy, 
                        unsigned int cycle, unsigned int dropped) = 0;
    virtual enum raw1394_iso_disposition 
            getPacket(unsigned char *data, unsigned int *length,
                    unsigned char *tag, unsigned char *sy,
                    int cycle, unsigned int dropped, unsigned int max_length) = 0;

    virtual enum EProcessorType getType() =0;

    bool xrunOccurred() { return (m_xruns>0);};
    
    // move to private?
    void resetXrunCounter();

    bool isRunning(); ///< returns true if there is some stream data processed
    bool enable(uint64_t time_to_enable_at); ///< enable the stream processing 
    bool disable(); ///< disable the stream processing 
    bool isEnabled() {return !m_is_disabled;};

    virtual bool putFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents from client
    virtual bool getFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents to the client

    virtual bool reset(); ///< reset the streams & buffers (e.g. after xrun)

    virtual bool prepare(); ///< prepare the streams & buffers (e.g. prefill)

    virtual void dumpInfo();

    virtual bool init();

    virtual void setVerboseLevel(int l);

    virtual bool prepareForStop() {return true;};
    virtual bool prepareForStart() {return true;};
    
    virtual bool prepareForEnable();
    virtual bool prepareForDisable();

protected:
	

    void setManager(StreamProcessorManager *manager) {m_manager=manager;};
    void clearManager() {m_manager=0;};

    unsigned int m_nb_buffers; ///< cached from manager->getNbBuffers(), the number of periods to buffer
    unsigned int m_period; ///< cached from manager->getPeriod(), the period size

    unsigned int m_xruns;

    unsigned int m_framerate;

    StreamProcessorManager *m_manager;
    
    bool m_running;
    bool m_disabled;
    bool m_is_disabled;
    unsigned int m_cycle_to_enable_at;
    
    StreamStatistics m_PacketStat;
    StreamStatistics m_PeriodStat;
    
    StreamStatistics m_WakeupStat;
    

    DECLARE_DEBUG_MODULE;
    
    // frame counter & sync stuff
    public:
        /**
         * @brief Can this StreamProcessor handle a nframes of frames?
         *
         * this function indicates if the streamprocessor can handle nframes
         * of frames. It is used to detect underruns-to-be.
         *
         * @param nframes number of frames 
         * @return true if the StreamProcessor can handle this amount of frames
         *         false if it can't
         */
        virtual bool canClientTransferFrames(unsigned int nframes) {return true;};
        
        int getFrameCounter() {return m_framecounter;};
    
        void decrementFrameCounter(int nbframes, uint64_t new_timestamp);
        void incrementFrameCounter(int nbframes, uint64_t new_timestamp);
        void resetFrameCounter();
        
        /**
         * \brief return the time until the next period boundary (in microseconds)
         *
         * Return the time until the next period boundary. If this StreamProcessor 
         * is the current synchronization source, this function is called to 
         * determine when a buffer transfer can be made. When this value is
         * smaller than 0, a period boundary is assumed to be crossed, hence a
         * transfer can be made.
         *
         * \return the time in usecs
         */
        virtual int64_t getTimeUntilNextPeriodUsecs() = 0;
        /**
         * \brief return the time of the next period boundary (in microseconds)
         *
         * Returns the time of the next period boundary, in microseconds. The
         * goal of this function is to determine the exact point of the period
         * boundary. This is assumed to be the point at which the buffer transfer should
         * take place, meaning that it can be used as a reference timestamp for transmitting
         * StreamProcessors
         *
         * \return the time in usecs
         */
        virtual uint64_t getTimeAtPeriodUsecs() = 0;
        
        /**
         * \brief return the time of the next period boundary (in internal units) 
         *
         * The same as getTimeUntilNextPeriodUsecs() but in internal units.
         *
         * @return the time in internal units
         */
        virtual uint64_t getTimeAtPeriod() = 0;
        
        uint64_t getTimeNow();
        
        void getBufferHeadTimestamp(uint64_t *ts, uint64_t *fc);
        void getBufferTailTimestamp(uint64_t *ts, uint64_t *fc);
        
        void setBufferTailTimestamp(uint64_t new_timestamp);
        void setBufferHeadTimestamp(uint64_t new_timestamp);
        void setBufferTimestamps(uint64_t new_head, uint64_t new_tail);
        
        bool setSyncSource(StreamProcessor *s);
        float getTicksPerFrame() {return m_ticks_per_frame;};
        
        unsigned int getLastCycle() {return m_last_cycle;};
    
    private:
        // the framecounter gives the number of frames in the buffer
        signed int m_framecounter;
        
        // the buffer tail timestamp gives the timestamp of the last frame
        // that was put into the buffer
        uint64_t   m_buffer_tail_timestamp;
        
        // the buffer head timestamp gives the timestamp of the first frame
        // that was put into the buffer
        uint64_t   m_buffer_head_timestamp;
        
    protected:
        StreamProcessor *m_SyncSource;
        
        float m_ticks_per_frame;
        
        unsigned int m_last_cycle;

    private:
        // this mutex protects the access to the framecounter
        // and the buffer head timestamp.
        pthread_mutex_t m_framecounter_lock;

};

/*!
\brief Class providing a generic interface for receive Stream Processors

*/
class ReceiveStreamProcessor : public StreamProcessor {

public:
	ReceiveStreamProcessor(int port, int framerate);

	virtual ~ReceiveStreamProcessor();


	virtual enum EProcessorType getType() {return E_Receive;};
	
	virtual enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) 
	              {return RAW1394_ISO_STOP;};
	              
	virtual enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) = 0;
 	virtual void setVerboseLevel(int l);

protected:

     DECLARE_DEBUG_MODULE;

};

/*!
\brief Class providing a generic interface for receive Stream Processors

*/
class TransmitStreamProcessor : public StreamProcessor {

public:
	TransmitStreamProcessor(int port, int framerate);

	virtual ~TransmitStreamProcessor();

	virtual enum EProcessorType getType() {return E_Transmit;};

	virtual enum raw1394_iso_disposition 
		putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) {return RAW1394_ISO_STOP;};
		          
	virtual enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) = 0;
 	virtual void setVerboseLevel(int l);

protected:

     DECLARE_DEBUG_MODULE;


};

}

#endif /* __FREEBOB_STREAMPROCESSOR__ */


