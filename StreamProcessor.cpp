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

#include "libutil/Atomic.h"

#include "StreamProcessor.h"
#include "StreamProcessorManager.h"
#include "cycletimer.h"

#include <assert.h>

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessor, StreamProcessor, DEBUG_LEVEL_VERBOSE );
IMPL_DEBUG_MODULE( ReceiveStreamProcessor, ReceiveStreamProcessor, DEBUG_LEVEL_VERBOSE );
IMPL_DEBUG_MODULE( TransmitStreamProcessor, TransmitStreamProcessor, DEBUG_LEVEL_VERBOSE );

StreamProcessor::StreamProcessor(enum IsoStream::EStreamType type, int port, int framerate) 
	: IsoStream(type, port)
	, m_nb_buffers(0)
	, m_period(0)
	, m_xruns(0)
	, m_framerate(framerate)
	, m_manager(NULL)
	, m_running(false)
	, m_disabled(true)
	, m_is_disabled(true)
	, m_cycle_to_enable_at(0)
	, m_SyncSource(NULL)
	, m_ticks_per_frame(0)
	, m_last_cycle(0)
	, m_sync_delay(0)
{
    // create the timestamped buffer and register ourselves as its client
    m_data_buffer=new FreebobUtil::TimestampedBuffer(this);

}

StreamProcessor::~StreamProcessor() {
    if (m_data_buffer) delete m_data_buffer;
}

void StreamProcessor::dumpInfo()
{
    debugOutputShort( DEBUG_LEVEL_NORMAL, " StreamProcessor information\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Iso stream info:\n");
    
    IsoStream::dumpInfo();
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  StreamProcessor info:\n");
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Now                   : %011u\n",m_handler->getCycleTimerTicks());
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Xruns                 : %d\n", m_xruns);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Running               : %d\n", m_running);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Enabled               : %s\n", m_disabled ? "No" : "Yes");
    debugOutputShort( DEBUG_LEVEL_NORMAL, "   enable status        : %s\n", m_is_disabled ? "No" : "Yes");
    
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Device framerate      : Sync: %f, Buffer %f\n", 
        24576000.0*m_SyncSource->m_data_buffer->getRate(),
        24576000.0*m_data_buffer->getRate()
        );
    
    m_data_buffer->dumpInfo();
    
//     m_PeriodStat.dumpInfo();
//     m_PacketStat.dumpInfo();
//     m_WakeupStat.dumpInfo();

}

bool StreamProcessor::init()
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");
    
    m_data_buffer->init();
    
    return IsoStream::init();
}

/**
 * Resets the frame counter, the xrun counter, the ports and the iso stream.
 * @return true if reset succeeded
 */
bool StreamProcessor::reset() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    // reset the event buffer, discard all content
    if (!m_data_buffer->reset()) {
        debugFatal("Could not reset data buffer\n");
        return false;
    }
    
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

bool StreamProcessor::prepareForEnable(uint64_t time_to_enable_at) {
    debugOutput(DEBUG_LEVEL_VERBOSE," StreamProcessor::prepareForEnable for (%p)\n",this);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011u\n",m_handler->getCycleTimerTicks());
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Enable at             : %011u\n",time_to_enable_at);
    m_data_buffer->dumpInfo();
    return true;
}

bool StreamProcessor::prepareForDisable() {
    debugOutput(DEBUG_LEVEL_VERBOSE," StreamProcessor::prepareForDisable for (%p)\n",this);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  Now                   : %011u\n",m_handler->getCycleTimerTicks());
    m_data_buffer->dumpInfo();
    return true;
}

bool StreamProcessor::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	
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

int StreamProcessor::getBufferFill() {
//     return m_data_buffer->getFrameCounter();
    return m_data_buffer->getBufferFill();
}

uint64_t StreamProcessor::getTimeNow() {
    return m_handler->getCycleTimerTicks();
}


bool StreamProcessor::isRunning() {
	return m_running;
}

bool StreamProcessor::enable(uint64_t time_to_enable_at)  {
    // FIXME: time_to_enable_at will be in 'time' not cycles
    m_cycle_to_enable_at=time_to_enable_at;
    
    if(!m_running) {
            debugWarning("The StreamProcessor is not running yet, enable() might not be a good idea.\n");
    }

#ifdef DEBUG
    uint64_t now_cycles=TICKS_TO_CYCLES(m_handler->getCycleTimerTicks());
    const int64_t max=(int64_t)(TICKS_PER_SECOND/2);
    
    int64_t diff=(int64_t)m_cycle_to_enable_at-(int64_t)now_cycles;
    
    if (diff > max) {
        diff-=TICKS_PER_SECOND;
    } else if (diff < -max) {
        diff+=TICKS_PER_SECOND;
    }
    
    if (diff<0) {
        debugWarning("Request to enable streamprocessor %lld cycles ago (now=%llu, cy=%llu).\n",
            diff,now_cycles,time_to_enable_at);
    }
#endif

    m_disabled=false;
    
    return true;
}

bool StreamProcessor::disable()  {
    
    m_disabled=true;

    return true;

}

bool StreamProcessor::setSyncSource(StreamProcessor *s) {
    m_SyncSource=s;
    return true;
}

int64_t StreamProcessor::getTimeUntilNextPeriodUsecs() {
    uint64_t time_at_period=getTimeAtPeriod();
    
    uint64_t cycle_timer=m_handler->getCycleTimerTicks();
    
    // calculate the time until the next period
    int64_t until_next=substractTicks(time_at_period,cycle_timer);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> TAP=%11llu, CTR=%11llu, UTN=%11lld\n",
        time_at_period, cycle_timer, until_next
        );
    
    // now convert to usecs
    // don't use the mapping function because it only works
    // for absolute times, not the relative time we are
    // using here (which can also be negative).
    return (int64_t)(((float)until_next) / TICKS_PER_USEC);
}

uint64_t StreamProcessor::getTimeAtPeriodUsecs() {
    return (uint64_t)((float)getTimeAtPeriod() * TICKS_PER_USEC);
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

uint64_t ReceiveStreamProcessor::getTimeAtPeriod() {
    uint64_t next_period_boundary=m_data_buffer->getTimestampFromHead(m_period);
    
    #ifdef DEBUG
    uint64_t ts,fc;
    m_data_buffer->getBufferTailTimestamp(&ts,&fc);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> NPD=%11lld, LTS=%11llu, FC=%5u, TPF=%f\n",
        next_period_boundary, ts, fc, m_ticks_per_frame
        );
    #endif
    
    return next_period_boundary;
}

bool ReceiveStreamProcessor::canClientTransferFrames(unsigned int nbframes) {
    return m_data_buffer->getFrameCounter() >= (int) nbframes;
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

uint64_t TransmitStreamProcessor::getTimeAtPeriod() {
    uint64_t next_period_boundary=m_data_buffer->getTimestampFromTail((m_nb_buffers-1) * m_period);
    
    #ifdef DEBUG
    uint64_t ts,fc;
    m_data_buffer->getBufferTailTimestamp(&ts,&fc);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "=> NPD=%11lld, LTS=%11llu, FC=%5u, TPF=%f\n",
        next_period_boundary, ts, fc, m_ticks_per_frame
        );
    #endif
    
    return next_period_boundary;
}

bool TransmitStreamProcessor::canClientTransferFrames(unsigned int nbframes) {
    // there has to be enough space to put the frames in
    return m_data_buffer->getBufferSize() - m_data_buffer->getFrameCounter() > nbframes;
}


}
