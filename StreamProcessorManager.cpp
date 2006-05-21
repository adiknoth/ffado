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

#include "StreamProcessorManager.h"
#include "StreamProcessor.h"
#include <errno.h>

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_NORMAL );

StreamProcessorManager::StreamProcessorManager(unsigned int period, unsigned int nb_buffers)
	: m_nb_buffers(nb_buffers), m_period(period), m_xruns(0) {

}

StreamProcessorManager::~StreamProcessorManager() {

}

int StreamProcessorManager::registerProcessor(StreamProcessor *processor)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "Registering processor (%p) with manager\n",processor);
	assert(processor);

	if (processor->getType()==StreamProcessor::E_Receive) {
		m_ReceiveProcessors.push_back(processor);
		processor->setManager(this);
		return 0;
	}
	
	if (processor->getType()==StreamProcessor::E_Transmit) {
		m_TransmitProcessors.push_back(processor);
		processor->setManager(this);
		return 0;
	}

	return -1;
}

int StreamProcessorManager::unregisterProcessor(StreamProcessor *processor)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering processor (%p) with manager\n",processor);
	assert(processor);

	if (processor->getType()==StreamProcessor::E_Receive) {

		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {

			if ( *it == processor ) { 
					m_ReceiveProcessors.erase(it);
					processor->clearManager();
					return 0;
				}
		}
	}

	if (processor->getType()==StreamProcessor::E_Transmit) {
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {

			if ( *it == processor ) { 
					m_TransmitProcessors.erase(it);
					processor->clearManager();
					return 0;
				}
		}
	}

	return -1; //not found

}

bool StreamProcessorManager::Init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	if(sem_init(&m_period_semaphore, 0, 0)) {
		debugFatal( "Cannot init packet transfer semaphore\n");
		debugFatal( " Error: %s\n",strerror(errno));
		return false;
	} else {
		debugOutput( DEBUG_LEVEL_VERBOSE,"FREEBOB: successfull init of packet transfer semaphore\n");
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		if((*it)->init()) {
			debugFatal("Could not initialize receive processor\n");
			return false;
		}
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		if((*it)->init()) {
			debugFatal("Could not initialize receive processor\n");
			return false;
		}
	}

	return true;
}

bool StreamProcessorManager::Execute()
{
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

	bool period_ready=true;
	m_xrun_has_occured=false;

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		period_ready = period_ready && (*it)->isOnePeriodReady();
		m_xrun_has_occured = m_xrun_has_occured || (*it)->xrunOccurred();
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		period_ready = period_ready && (*it)->isOnePeriodReady();
		m_xrun_has_occured = m_xrun_has_occured || (*it)->xrunOccurred();
	}

	if(m_xrun_has_occured) {
		// do xrun signaling/handling
		m_xruns++;
		sem_post(&m_period_semaphore);
		return false;
	}

	if(period_ready) {
		// signal the waiting thread(s?) that a period is ready
		sem_post(&m_period_semaphore);
	}

	return true;

}

int StreamProcessorManager::waitForPeriod() {
	int ret;
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

	// Wait for packetizer thread to signal a period completion
	sem_wait(&m_period_semaphore);
	
	if(m_xrun_has_occured) {
		// notify the driver of the underrun
		ret = 0;
	} else {
		ret=m_period;
	}
	
	return ret;

}

void StreamProcessorManager::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting processors...\n");

	/* 
	 * Reset means:
	 * Bringing all buffers & connections into a know state
	 *   - Clear all capture buffers
	 *   - Put nb_periods*period_size of null frames into the playback buffers
	 *  => implemented by a reset() call, implementation dependant on the type
	 */
	
// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		(*it)->reset();
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->reset();
	}
}

int StreamProcessorManager::transfer() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");

	// a static cast could make sure that there is no performance
	// penalty for the virtual functions (to be checked)

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) { 
		(*it)->transfer();
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->transfer();
	}

	return 0;
}

void StreamProcessorManager::dumpInfo() {
	debugOutputShort( DEBUG_LEVEL_NORMAL, "Dumping StreamProcessorManager information...\n");

	debugOutputShort( DEBUG_LEVEL_NORMAL, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		(*it)->dumpInfo();
	}

	debugOutputShort( DEBUG_LEVEL_NORMAL, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->dumpInfo();
	}

}

void StreamProcessorManager::setVerboseLevel(int l) {
	setDebugLevel(l);

	debugOutputShort( DEBUG_LEVEL_NORMAL, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		(*it)->setVerboseLevel(l);
	}

	debugOutputShort( DEBUG_LEVEL_NORMAL, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->setVerboseLevel(l);
	}
}


} // end of namespace
