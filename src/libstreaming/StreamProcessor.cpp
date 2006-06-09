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
	, m_manager(0)
	, m_period(0)
	, m_xruns(0)
	, m_framecounter(0)
	, m_framerate(framerate)
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
	
    m_PeriodStat.dumpInfo();
    m_PacketStat.dumpInfo();
    m_WakeupStat.dumpInfo();
	
	
}

bool StreamProcessor::init()
{
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

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

bool StreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
// TODO: implement

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

/**
 * Decrements the frame counter, in a atomic way. This
 * is thread safe.
 */
void StreamProcessor::decrementFrameCounter() {
	SUBSTRACT_ATOMIC((SInt32 *)&m_framecounter,m_period);
}
/**
 * Increments the frame counter, in a atomic way. This
 * is thread safe.
 */
void StreamProcessor::incrementFrameCounter(int nbframes) {
	ADD_ATOMIC((SInt32 *)&m_framecounter, nbframes);
}

/**
 * Resets the frame counter, in a atomic way. This
 * is thread safe.
 */
void StreamProcessor::resetFrameCounter() {
	ZERO_ATOMIC((SInt32 *)&m_framecounter);
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
