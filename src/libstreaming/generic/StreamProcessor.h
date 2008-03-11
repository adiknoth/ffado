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

#ifndef __FFADO_STREAMPROCESSOR__
#define __FFADO_STREAMPROCESSOR__

#include "ffadodevice.h"

#include "PortManager.h"

#include "libutil/StreamStatistics.h"
#include "libutil/TimestampedBuffer.h"
#include "libutil/OptionContainer.h"

#include "debugmodule/debugmodule.h"

#include <pthread.h>

class Ieee1394Service;
class IsoHandlerManager;

namespace Streaming {

    class StreamProcessorManager;
/*!
\brief Class providing a generic interface for Stream Processors

 A stream processor multiplexes or demultiplexes an ISO stream into a
 collection of ports. This class should be subclassed, and the relevant
 functions should be overloaded.

*/
class StreamProcessor : public PortManager,
                        public Util::TimestampedBufferClient,
                        public Util::OptionContainer
{
public:
    ///> the streamprocessor type
    enum eProcessorType {
        ePT_Receive,
        ePT_Transmit
    };
    ///> returns the type of the streamprocessor
    virtual enum eProcessorType getType() { return m_processor_type; };
private:
    // this can only be set by the constructor
    enum eProcessorType m_processor_type;
    // pretty printing
    const char *ePTToString(enum eProcessorType);
protected:
    ///> the state the streamprocessor is in
    enum eProcessorState {
        ePS_Invalid,
        ePS_Created,
        // ePS_WaitingToStop, FIXME: this will be needed for the MOTU's
        ePS_Stopped,
        ePS_WaitingForStream,
        ePS_DryRunning,
        ePS_WaitingForStreamEnable,
        ePS_Running,
        ePS_WaitingForStreamDisable,
    };

    ///> set the SP state to a specific value
    void setState(enum eProcessorState);
    ///> get the SP state
    enum eProcessorState getState() {return m_state;};
private:
    enum eProcessorState m_state;
    // state switching
    enum eProcessorState m_next_state;
    unsigned int m_cycle_to_switch_state;
    bool updateState();
    // pretty printing
    const char *ePSToString(enum eProcessorState);

    bool doStop();
    bool doWaitForRunningStream();
    bool doDryRunning();
    bool doWaitForStreamEnable();
    bool doRunning();
    bool doWaitForStreamDisable();

    bool scheduleStateTransition(enum eProcessorState state, uint64_t time_instant);
    bool waitForState(enum eProcessorState state, unsigned int timeout);

public: //--- state stuff
    bool isRunning()
            {return m_state == ePS_Running;};
    bool isDryRunning()
            {return m_state == ePS_DryRunning;};
    bool isStopped()
            {return m_state == ePS_Stopped;};
    bool isWaitingForStream()
            {return m_state == ePS_WaitingForStream;};

    // these schedule and wait for the state transition
    bool startDryRunning(int64_t time_to_start_at);
    bool startRunning(int64_t time_to_start_at);
    bool stopDryRunning(int64_t time_to_stop_at);
    bool stopRunning(int64_t time_to_stop_at);

    // these only schedule the transition
    bool scheduleStartDryRunning(int64_t time_to_start_at);
    bool scheduleStartRunning(int64_t time_to_start_at);
    bool scheduleStopDryRunning(int64_t time_to_stop_at);
    bool scheduleStopRunning(int64_t time_to_stop_at);

    // the main difference between init and prepare is that when prepare is called,
    // the SP is registered to a manager (FIXME: can't it be called by the manager?)
    bool init();
    bool prepare();

public: // constructor/destructor
    StreamProcessor(FFADODevice &parent, enum eProcessorType type);
    virtual ~StreamProcessor();
protected:
    FFADODevice&                m_Parent;
    Ieee1394Service&            m_1394service;
    IsoHandlerManager&          m_IsoHandlerManager;
    StreamProcessorManager&     m_StreamProcessorManager;
    unsigned int                m_local_node_id;

public: // the public receive/transmit functions
    // the transmit interface accepts frames and provides packets
    // implement these for a transmit SP
    // leave default for a receive SP

    // the receive interface accepts packets and provides frames
    // these are implemented by the parent SP
    enum raw1394_iso_disposition
        putPacket(unsigned char *data, unsigned int length,
                  unsigned char channel, unsigned char tag, unsigned char sy,
                  unsigned int cycle, unsigned int dropped, unsigned int skipped);

    enum raw1394_iso_disposition
    getPacket(unsigned char *data, unsigned int *length,
                unsigned char *tag, unsigned char *sy,
                int cycle, unsigned int dropped, unsigned int skipped, unsigned int max_length);

    bool getFrames(unsigned int nbframes, int64_t ts); ///< transfer the buffer contents to the client
    bool putFrames(unsigned int nbframes, int64_t ts); ///< transfer the client contents to the buffer

    //FIXME: document wait functions
    bool waitForProducePacket();
    bool waitForProducePeriod();
    bool waitForProduce(unsigned int nframes);

    bool waitForConsumePacket();
    bool waitForConsumePeriod();
    bool waitForConsume(unsigned int nframes);

    bool canProducePacket();
    bool canProducePeriod();
    bool canProduce(unsigned int nframes);

    bool canConsumePacket();
    bool canConsumePeriod();
    bool canConsume(unsigned int nframes);

public:
    /**
     * @brief drop nframes from the internal buffer as if they were transferred to the client side
     *
     * Gets nframes of frames from the buffer as done by getFrames(), but does not transfer them
     * to the client side. Instead they are discarded.
     *
     * @param nframes number of frames
     * @return true if the operation was successful
     */
    bool dropFrames(unsigned int nframes, int64_t ts);

    /**
     * @brief put silence frames into the internal buffer
     *
     * Puts nframes of frames into the buffer as done by putFrames(), but does not transfer them
     * from the client side. Instead, silent frames are used.
     *
     * @param nframes number of frames
     * @return true if the operation was successful
     */
    bool putSilenceFrames(unsigned int nbframes, int64_t ts);

    /**
     * @brief Shifts the stream with the specified number of frames
     *
     * Used to align several streams to each other. It comes down to
     * making sure the head timestamp corresponds to the timestamp of
     * one master stream
     *
     * @param nframes the number of frames to shift
     * @return true if successful
     */
    bool shiftStream(int nframes);

    /**
     * @brief tries to fill/sink the stream as far as possible
     */
    void flush();

protected: // the helper receive/transmit functions
    enum eChildReturnValue {
        eCRV_OK,
        eCRV_Invalid,
        eCRV_Packet,
        eCRV_EmptyPacket,
        eCRV_XRun,
        eCRV_Again,
        eCRV_Defer,
    };
    // to be implemented by the children
    // the following methods are to be implemented by receive SP subclasses
    virtual enum eChildReturnValue processPacketHeader(unsigned char *data, unsigned int length,
                                     unsigned char channel, unsigned char tag,
                                     unsigned char sy, unsigned int cycle,
                                     unsigned int dropped)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual enum eChildReturnValue processPacketData(unsigned char *data, unsigned int length,
                                   unsigned char channel, unsigned char tag,
                                   unsigned char sy, unsigned int cycle,
                                   unsigned int dropped)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual bool processReadBlock(char *data, unsigned int nevents, unsigned int offset)
        {debugWarning("call not allowed\n"); return false;};

    // the following methods are to be implemented by transmit SP subclasses
    virtual enum eChildReturnValue generatePacketHeader(unsigned char *data, unsigned int *length,
                                      unsigned char *tag, unsigned char *sy,
                                      int cycle, unsigned int dropped,
                                      unsigned int max_length)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual enum eChildReturnValue generatePacketData(unsigned char *data, unsigned int *length,
                                    unsigned char *tag, unsigned char *sy,
                                    int cycle, unsigned int dropped,
                                    unsigned int max_length)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual enum eChildReturnValue generateEmptyPacketHeader(unsigned char *data, unsigned int *length,
                                            unsigned char *tag, unsigned char *sy,
                                            int cycle, unsigned int dropped,
                                            unsigned int max_length)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual enum eChildReturnValue generateEmptyPacketData(unsigned char *data, unsigned int *length,
                                          unsigned char *tag, unsigned char *sy,
                                          int cycle, unsigned int dropped,
                                          unsigned int max_length)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual enum eChildReturnValue generateSilentPacketHeader(unsigned char *data, unsigned int *length,
                                            unsigned char *tag, unsigned char *sy,
                                            int cycle, unsigned int dropped,
                                            unsigned int max_length)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual enum eChildReturnValue generateSilentPacketData(unsigned char *data, unsigned int *length,
                                          unsigned char *tag, unsigned char *sy,
                                          int cycle, unsigned int dropped,
                                          unsigned int max_length)
        {debugWarning("call not allowed\n"); return eCRV_Invalid;};
    virtual bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset)
        {debugWarning("call not allowed\n"); return false;};
    virtual bool transmitSilenceBlock(char *data, unsigned int nevents, unsigned int offset)
        {debugWarning("call not allowed\n"); return false;};
protected: // some generic helpers
    int provideSilenceToPort(Port *p, unsigned int offset, unsigned int nevents);
    bool provideSilenceBlock(unsigned int nevents, unsigned int offset);

private:
    bool getFramesDry(unsigned int nbframes, int64_t ts);
    bool getFramesWet(unsigned int nbframes, int64_t ts);
    bool putFramesDry(unsigned int nbframes, int64_t ts);
    bool putFramesWet(unsigned int nbframes, int64_t ts);

    bool transferSilence(unsigned int size);

public:
    // move to private?
    bool xrunOccurred() { return m_in_xrun; };

// the ISO interface (can we get rid of this?)
public:
    int getChannel() {return m_channel;};
    bool setChannel(int c)
        {m_channel = c; return true;};

    virtual unsigned int getNbPacketsIsoXmitBuffer();
    virtual unsigned int getPacketsPerPeriod();
    virtual unsigned int getMaxPacketSize() = 0;
private:
    int m_channel;

protected: // FIXME: move to private
    uint64_t m_dropped; /// FIXME:debug
    uint64_t m_last_dropped; /// FIXME:debug
    int m_last_good_cycle; /// FIXME:debug
    uint64_t m_last_timestamp; /// last timestamp (in ticks)
    uint64_t m_last_timestamp2; /// last timestamp (in ticks)
    bool m_correct_last_timestamp;
    uint64_t m_last_timestamp_at_period_ticks; // FIXME: still used?

//--- data buffering and accounting
public:
    void getBufferHeadTimestamp ( ffado_timestamp_t *ts, signed int *fc )
        {m_data_buffer->getBufferHeadTimestamp(ts, fc);};
    void getBufferTailTimestamp ( ffado_timestamp_t *ts, signed int *fc )
        {m_data_buffer->getBufferTailTimestamp(ts, fc);};

    void setBufferTailTimestamp ( ffado_timestamp_t new_timestamp )
        {m_data_buffer->setBufferTailTimestamp(new_timestamp);};
    void setBufferHeadTimestamp ( ffado_timestamp_t new_timestamp )
        {m_data_buffer->setBufferHeadTimestamp(new_timestamp);};
protected:
    Util::TimestampedBuffer *m_data_buffer;
    // the scratch buffer is temporary buffer space that can be
    // used by any function. It's pre-allocated when the SP is created.
    // the purpose is to avoid allocation of memory (or heap/stack) in
    // an RT context
    byte_t*         m_scratch_buffer;
    size_t          m_scratch_buffer_size_bytes;

protected:
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
        bool canClientTransferFrames(unsigned int nframes);

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
         * The same as getTimeAtPeriodUsecs() but in internal units.
         *
         * @return the time in internal units
         */
        uint64_t getTimeAtPeriod();

        uint64_t getTimeNow(); // FIXME: should disappear


        /**
         * Returns the sync delay. This is the time a syncsource
         * delays a period signal, e.g. to cope with buffering.
         * @return the sync delay
         */
        unsigned int getSyncDelay() {return m_sync_delay;};
        /**
         * sets the sync delay
         * @param d sync delay
         */
        void setSyncDelay(unsigned int d);

        /**
         * @brief get the maximal frame latency
         *
         * The maximum frame latency is the maximum time that will elapse
         * between the frame being received by the 1394 stack, and the moment this
         * frame is presented to the StreamProcessor. 
         *
         * For transmit SP's this is the maximum time that a frame is requested by
         * the handler ahead of the time the frame is intended to be transmitted.
         *
         * This is useful to figure out how longer than the actual reception time
         * we have to wait before trying to read the frame from the SP.
         *
         * @return maximal frame latency
         */
        int getMaxFrameLatency();

        float getTicksPerFrame();

        int getLastCycle() {return m_last_cycle;};

        int getBufferFill();

        // Child implementation interface
        /**
        * @brief prepare the child SP
        * @return true if successful, false otherwise
        * @pre the m_manager pointer points to a valid manager
        * @post getEventsPerFrame() returns the correct value
        * @post getEventSize() returns the correct value
        * @post getUpdatePeriod() returns the correct value
        * @post processPacketHeader(...) can be called
        * @post processPacketData(...) can be called
        */
        virtual bool prepareChild() = 0;
        /**
         * @brief get the number of events contained in one frame
         * @return the number of events contained in one frame
         */
        virtual unsigned int getEventsPerFrame() = 0;

        /**
         * @brief get the size of one frame in bytes
         * @return the size of one frame in bytes
         */
        virtual unsigned int getEventSize() = 0;

        /**
         * @brief get the nominal number of frames in a packet
         *
         * This is the amount of frames that is nominally present
         * in one packet. It is recommended that in the receive handler
         * you write this amount of frames when a valid packet has
         * been received. (although this is not mandatory)
         *
         * @return the nominal number of frames in a packet
         */
        virtual unsigned int getNominalFramesPerPacket() = 0;

        /**
         * @brief get the nominal number of packets needed for a certain amount of frames
         * @return the nominal number of packet necessary
         */
        virtual unsigned int getNominalPacketsNeeded(unsigned int nframes);

        /**
         * @brief returns the actual frame rate as calculated by the SP's DLL
         * @return the actual frame rate as detected by the DLL
         */
        float getActualRate()
            {return m_data_buffer->getRate();};

    protected:
        float m_ticks_per_frame;
        int m_last_cycle;
        unsigned int m_sync_delay;
    private:
        bool m_in_xrun;
        pthread_mutex_t m_activity_cond_lock;
        pthread_cond_t  m_activity_cond;

public:
    // debug stuff
    virtual void dumpInfo();
    virtual void printBufferInfo();
    virtual void setVerboseLevel(int l);
    const char *getStateString()
        {return ePSToString(getState());};
    const char *getTypeString()
        {return ePTToString(getType());};
    StreamStatistics m_PacketStat;
    StreamStatistics m_PeriodStat;
    StreamStatistics m_WakeupStat;
    DECLARE_DEBUG_MODULE;
};

}

#endif /* __FFADO_STREAMPROCESSOR__ */


