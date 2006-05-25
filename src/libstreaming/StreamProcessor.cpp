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

#include "StreamProcessor.h"
#include "StreamProcessorManager.h"
#include <assert.h>

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessor, StreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( ReceiveStreamProcessor, ReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( TransmitStreamProcessor, TransmitStreamProcessor, DEBUG_LEVEL_NORMAL );

StreamProcessor::StreamProcessor(enum IsoStream::EStreamType type, int channel, int port, int framerate) 
	: IsoStream(type, channel, port)
	, m_manager(0)
	, m_nb_buffers(0)
	, m_period(0)
	, m_xruns(0)
	, m_framecounter(0)
	, m_framerate(framerate)
{

}

StreamProcessor::~StreamProcessor() {

}

int StreamProcessor::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugWarning("BUG: received packet in StreamProcessor base class!\n");

	return -1;
}

int StreamProcessor::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped, unsigned int max_length) {
	
	debugWarning("BUG: packet requested from StreamProcessor base class!\n");

	return -1;
}

void StreamProcessor::dumpInfo()
{

	debugOutputShort( DEBUG_LEVEL_NORMAL, " StreamProcessor information\n");
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Iso stream info:\n");
	
	((IsoStream*)this)->dumpInfo();
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Frame counter  : %d\n", m_framecounter);
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Xruns          : %d\n", m_xruns);
	
};

int StreamProcessor::init()
{
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

	return IsoStream::init();
}

void StreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

	m_framecounter=0;

	// loop over the ports to reset them
	PortManager::reset();

	// reset the iso stream
	IsoStream::reset();

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
	debugOutputShort( DEBUG_LEVEL_VERBOSE, "Setting m_nb_buffers  : %d\n", m_nb_buffers);

	m_period=m_manager->getPeriodSize();
	debugOutputShort( DEBUG_LEVEL_VERBOSE, "Setting m_period      : %d\n", m_period);

	// loop over the ports to reset them
	PortManager::prepare();

	// reset the iso stream
	IsoStream::prepare();
	
	return true;

}

int StreamProcessor::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
// TODO: implement

	return 0;
}

void StreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	IsoStream::setVerboseLevel(l);
	PortManager::setVerboseLevel(l);

}


ReceiveStreamProcessor::ReceiveStreamProcessor(int channel, int port, int framerate) 
	: StreamProcessor(IsoStream::EST_Receive, channel, port, framerate) {

}

ReceiveStreamProcessor::~ReceiveStreamProcessor() {

}

int ReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );

	return 0;
}

void ReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	StreamProcessor::setVerboseLevel(l);

}


TransmitStreamProcessor::TransmitStreamProcessor(int channel, int port, int framerate) 
	: StreamProcessor(IsoStream::EST_Transmit, channel, port, framerate) {

}

TransmitStreamProcessor::~TransmitStreamProcessor() {

}

int TransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) {
	*length=0;
	*tag = 1;
	*sy = 0;

	return 0;
}

void TransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	StreamProcessor::setVerboseLevel(l);

}

}
