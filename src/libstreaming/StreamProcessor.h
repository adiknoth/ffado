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

#include "libutil/TimestampedBuffer.h"

namespace Streaming {

class StreamProcessorManager;

/*!
\brief Class providing a generic interface for Stream Processors

 A stream processor multiplexes or demultiplexes an ISO stream into a 
 collection of ports. This class should be subclassed, and the relevant
 functions should be overloaded.
 
*/
class StreamProcessor : public IsoStream, 
                        public PortManager, 
                        public Util::TimestampedBufferClient {

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
    
    virtual bool prepareForEnable(uint64_t time_to_enable_at);
    virtual bool prepareForDisable();
    
    bool enable(uint64_t time_to_enable_at); ///< enable the stream processing 
    bool disable(); ///< disable the stream processing 
    bool isEnabled() {return !m_is_disabled;};

    virtual bool putFrames(unsigned int nbframes, int64_t ts) = 0; ///< transfer the buffer contents from client
    virtual bool getFrames(unsigned int nbframes) = 0; ///< transfer the buffer contents to the client

    virtual bool reset(); ///< reset the streams & buffers (e.g. after xrun)

    virtual bool prepare(); ///< prepare the streams & buffers (e.g. prefill)

    virtual void dumpInfo();

    virtual bool init();

    virtual void setVerboseLevel(int l);

    virtual bool prepareForStop() {return true;};
    virtual bool prepareForStart() {return true;};


public:
    Util::TimestampedBuffer *m_data_buffer;

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
         * @brief Can this StreamProcessor handle a transfer of nframes frames?
         *
         * this function indicates if the streamprocessor can handle a transfer of 
         * nframes frames. It is used to detect underruns-to-be.
         *
         * @param nframes number of frames
         * @return true if the StreamProcessor can handle this amount of frames
         *         false if it can't
         */
        virtual bool canClientTransferFrames(unsigned int nframes) = 0;
        
        /**
         * \brief return the time until the next period boundary should be signaled (in microseconds)
         *
         * Return the time until the next period boundary signal. If this StreamProcessor 
         * is the current synchronization source, this function is called to 
         * determine when a buffer transfer can be made. When this value is
         * smaller than 0, a period boundary is assumed to be crossed, hence a
         * transfer can be made.
         *
         * \return the time in usecs
         */
        int64_t getTimeUntilNextPeriodSignalUsecs();
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
        uint64_t getTimeAtPeriodUsecs();

        /**
         * \brief return the time of the next period boundary (in internal units) 
         *
         * The same as getTimeUntilNextPeriodSignalUsecs() but in internal units.
         *
         * @return the time in internal units
         */
        virtual uint64_t getTimeAtPeriod() = 0;

        uint64_t getTimeNow();
        
        
        /**
         * Returns the sync delay. This is the time a syncsource
         * delays a period signal, e.g. to cope with buffering.
         * @return the sync delay
         */
        int getSyncDelay() {return m_sync_delay;};
        /**
         * sets the sync delay
         * @param d sync delay
         */
        void setSyncDelay(int d) {m_sync_delay=d;};
        
        /**
         * Returns the minimal sync delay a SP needs
         * @return minimal sync delay
         */
        virtual int getMinimalSyncDelay() = 0;

        bool setSyncSource(StreamProcessor *s);
        float getTicksPerFrame() {return m_ticks_per_frame;};

        int getLastCycle() {return m_last_cycle;};

        int getBufferFill();
        
    protected:
        StreamProcessor *m_SyncSource;

        float m_ticks_per_frame;

        int m_last_cycle;
        int m_sync_delay;

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
        virtual bool putFrames(unsigned int nbframes, int64_t ts) {return false;};
	
        virtual enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped) = 0;
 	virtual void setVerboseLevel(int l);

    uint64_t getTimeAtPeriod();
    bool canClientTransferFrames(unsigned int nframes);

protected:
    bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset) {return true;};

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
        virtual bool getFrames(unsigned int nbframes) {return false;};

	virtual enum raw1394_iso_disposition 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) = 0;
 	virtual void setVerboseLevel(int l);

    uint64_t getTimeAtPeriod();
    bool canClientTransferFrames(unsigned int nframes);

protected:
    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset) {return true;};

    DECLARE_DEBUG_MODULE;


};

}

#endif /* __FREEBOB_STREAMPROCESSOR__ */


