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

#include "../libutil/Atomic.h"

#include "StreamProcessor.h"
#include "StreamProcessorManager.h"
#include <assert.h>

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessor, StreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( ReceiveStreamProcessor, ReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( TransmitStreamProcessor, TransmitStreamProcessor, DEBUG_LEVEL_NORMAL );

StreamProcessor::StreamProcessor(enum IsoStream::EStreamType type, int port, int framerate) 
	: IsoStream(type, port)
	, m_nb_buffers(0)
	, m_period(0)
	, m_xruns(0)
	, m_framecounter(0)
	, m_framerate(framerate)
	, m_manager(NULL)
	, m_SyncSource(NULL)
	, m_ticks_per_frame(0)
	, m_running(false)
	, m_disabled(true)
{

}

StreamProcessor::~StreamProcessor() {

}

void StreamProcessor::dumpInfo()
{

    debugOutputShort( DEBUG_LEVEL_NORMAL, " StreamProcessor information\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Iso stream info:\n");
    
    IsoStream::dumpInfo();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Frame counter  : %d\n", m_framecounter);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Xruns          : %d\n", m_xruns);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Running        : %d\n", m_running);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Enabled        : %d\n", !m_disabled);
    
//     m_PeriodStat.dumpInfo();
//     m_PacketStat.dumpInfo();
//     m_WakeupStat.dumpInfo();

}

bool StreamProcessor::init()
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");
    
    pthread_mutex_init(&m_framecounter_lock, NULL);

    return IsoStream::init();
}

/**
 * Resets the frame counter, the xrun counter, the ports and the iso stream.
 * @return true if reset succeeded
 */
bool StreamProcessor::reset() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    resetFrameCounter();

    resetXrunCounter();

    // loop over the ports to reset them
    if (!PortManager::resetPorts()) {
        debugFatal("Could not reset ports\n");
        return false;
    }

    // reset the iso stream
    if (!IsoStream::reset()) {
        debugFatal("Could not reset isostream\n");
        return false;
    }
    return true;
	
}

bool StreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
// TODO: implement

	// init the ports
	
	if(!m_manager) {
		debugFatal("Not attached to a manager!\n");
		return -1;
	}

	m_nb_buffers=m_manager->getNbBuffers();
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting m_nb_buffers  : %d\n", m_nb_buffers);

	m_period=m_manager->getPeriodSize();
	debugOutput( DEBUG_LEVEL_VERBOSE, "Setting m_period      : %d\n", m_period);

	// loop over the ports to reset them
	PortManager::preparePorts();

	// reset the iso stream
	IsoStream::prepare();
	
	return true;

}

/**
 * @brief Notify the StreamProcessor that frames were written
 *
 * This notifies the StreamProcessor of the fact that frames were written to the internal
 * buffer. This is for framecounter & timestamp bookkeeping.
 *
 * @param nbframes the number of frames that are written to the internal buffers
 * @param ts the new timestamp of the 'tail' of the buffer, i.e. the last sample
 *           present in the buffer.
 * @return true if successful
 */
bool StreamProcessor::putFrames(unsigned int nbframes, int64_t ts) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Putting %d frames for %llu into frame buffer...\n", nbframes,ts);
        incrementFrameCounter(nbframes, ts);
	return true;
}

/**
 * @brief Notify the StreamProcessor that frames were read
 *
 * This notifies the StreamProcessor of the fact that frames were read from the internal
 * buffer. This is for framecounter & timestamp bookkeeping.
 *
 * @param nbframes the number of frames that are read from the internal buffers
 * @param ts the new timestamp of the 'head' of the buffer, i.e. the first sample
 *           present in the buffer.
 * @return true if successful
 */
bool StreamProcessor::getFrames(unsigned int nbframes, int64_t ts) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Getting %d frames from frame buffer...\n", nbframes);
        decrementFrameCounter(nbframes, ts);
	return true;
}

bool StreamProcessor::isRunning() {
	return m_running;
}

void StreamProcessor::enable()  {
	if(!m_running) {
		debugWarning("The StreamProcessor is not running yet, enable() might not be a good idea.\n");
	}
	m_disabled=false;
}

bool StreamProcessor::setSyncSource(StreamProcessor *s) {
    m_SyncSource=s;
    return true;
}

/**
 * Decrements the frame counter, in a atomic way. This
 * also sets the buffer tail timestamp
 * is thread safe.
 */
void StreamProcessor::decrementFrameCounter(int nbframes, uint64_t new_timestamp) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter -= nbframes;
    m_buffer_head_timestamp = new_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Increments the frame counter, in a atomic way.
 * also sets the buffer head timestamp
 * This is thread safe.
 */
void StreamProcessor::incrementFrameCounter(int nbframes, uint64_t new_timestamp) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter += nbframes;
    m_buffer_tail_timestamp = new_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
    
}

/**
 * Sets the frame counter, in a atomic way. 
 * also sets the buffer head timestamp
 * This is thread safe.
 */
void StreamProcessor::setFrameCounter(int new_framecounter, uint64_t new_timestamp) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter = new_framecounter;
    m_buffer_tail_timestamp = new_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Sets the buffer tail timestamp (in usecs)
 * This is thread safe.
 */
void StreamProcessor::setBufferTailTimestamp(uint64_t new_timestamp) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_buffer_tail_timestamp = new_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Sets the buffer head timestamp (in usecs)
 * This is thread safe.
 */
void StreamProcessor::setBufferHeadTimestamp(uint64_t new_timestamp) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_buffer_head_timestamp = new_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Sets both the buffer head and tail timestamps (in usecs)
 * (avoids multiple mutex lock/unlock's)
 * This is thread safe.
 */
void StreamProcessor::setBufferTimestamps(uint64_t new_head, uint64_t new_tail) {
    pthread_mutex_lock(&m_framecounter_lock);
    m_buffer_head_timestamp = new_head;
    m_buffer_tail_timestamp = new_tail;
    pthread_mutex_unlock(&m_framecounter_lock);
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
void StreamProcessor::getBufferHeadTimestamp(uint64_t *ts, uint64_t *fc) {
    pthread_mutex_lock(&m_framecounter_lock);
    *fc = m_framecounter;
    *ts = m_buffer_head_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
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
void StreamProcessor::getBufferTailTimestamp(uint64_t *ts, uint64_t *fc) {
    pthread_mutex_lock(&m_framecounter_lock);
    *fc = m_framecounter;
    *ts = m_buffer_tail_timestamp;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Resets the frame counter, in a atomic way. This
 * is thread safe.
 */
void StreamProcessor::resetFrameCounter() {
    pthread_mutex_lock(&m_framecounter_lock);
    m_framecounter = 0;
    pthread_mutex_unlock(&m_framecounter_lock);
}

/**
 * Resets the xrun counter, in a atomic way. This
 * is thread safe.
 */
void StreamProcessor::resetXrunCounter() {
	ZERO_ATOMIC((SInt32 *)&m_xruns);
}

void StreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	IsoStream::setVerboseLevel(l);
	PortManager::setVerboseLevel(l);

}

ReceiveStreamProcessor::ReceiveStreamProcessor(int port, int framerate) 
	: StreamProcessor(IsoStream::EST_Receive, port, framerate) {

}

ReceiveStreamProcessor::~ReceiveStreamProcessor() {

}

void ReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	StreamProcessor::setVerboseLevel(l);

}


TransmitStreamProcessor::TransmitStreamProcessor( int port, int framerate) 
	: StreamProcessor(IsoStream::EST_Transmit, port, framerate) {

}

TransmitStreamProcessor::~TransmitStreamProcessor() {

}

void TransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	StreamProcessor::setVerboseLevel(l);

}

}
