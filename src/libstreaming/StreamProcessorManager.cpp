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
#include "Port.h"
#include <errno.h>
#include <assert.h>


namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_NORMAL );

StreamProcessorManager::StreamProcessorManager(unsigned int period, unsigned int nb_buffers)
	: m_nb_buffers(nb_buffers), m_period(period), m_xruns(0), m_isoManager(0) {

}

StreamProcessorManager::~StreamProcessorManager() {
	if (m_isoManager) delete m_isoManager;
	
}

/**
 * Registers \ref processor with this manager.
 *
 * also registers it with the isohandlermanager
 *
 * be sure to call isohandlermanager->init() first!
 * and be sure that the processors are also ->init()'ed
 *
 * @param processor 
 * @return true if successfull
 */
bool StreamProcessorManager::registerProcessor(StreamProcessor *processor)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "Registering processor (%p)\n",processor);
	assert(processor);
	assert(m_isoManager);

	if (processor->getType()==StreamProcessor::E_Receive) {
		processor->setVerboseLevel(getDebugLevel()); // inherit debug level
		
		m_ReceiveProcessors.push_back(processor);
		
		processor->setManager(this);
				
		return true;
	}
	
	if (processor->getType()==StreamProcessor::E_Transmit) {
		processor->setVerboseLevel(getDebugLevel()); // inherit debug level
		
		m_TransmitProcessors.push_back(processor);
		
		processor->setManager(this);
		
		return true;
	}

	debugFatal("Unsupported processor type!\n");
	
	return false;
}

bool StreamProcessorManager::unregisterProcessor(StreamProcessor *processor)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering processor (%p)\n",processor);
	assert(processor);

	if (processor->getType()==StreamProcessor::E_Receive) {

		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {

			if ( *it == processor ) { 
					m_ReceiveProcessors.erase(it);
					
					processor->clearManager();
					
					if(!m_isoManager->unregisterStream(processor)) {
						debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister receive stream processor from the Iso manager\n");
						
						return false;
						
					}
					
					return true;
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
					
					if(!m_isoManager->unregisterStream(processor)) {
						debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister transmit stream processor from the Iso manager\n");
						
						return false;
						
					}
					
					return true;
				}
		}
	}
	
	debugFatal("Processor (%p) not found!\n",processor);

	return false; //not found

}

bool StreamProcessorManager::init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	
	m_isoManager=new IsoHandlerManager();
	
	if(!m_isoManager) {
		debugFatal("Could not create IsoHandlerManager\n");
		return false;
	}
	
	if(!m_isoManager->Init()) {
		debugFatal("Could not init IsoHandlerManager\n");
		return false;
	}

	
	if(sem_init(&m_period_semaphore, 0, 0)) {
		debugFatal( "Cannot init packet transfer semaphore\n");
		debugFatal( " Error: %s\n",strerror(errno));
		return false;
	} else {
		debugOutput( DEBUG_LEVEL_VERBOSE,"Successfull init of packet transfer semaphore\n");
	}

	return true;
}

bool StreamProcessorManager::Init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	return true;
}

bool StreamProcessorManager::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
			if(!(*it)->setPortBuffersize(m_period)) {
				debugFatal( " could not set buffer size (%p)...\n",(*it));
				return false;
				
			}
			
			if(!(*it)->prepare()) {
				debugFatal(  " could not prepare (%p)...\n",(*it));
				return false;
				
			}
			
			if (!m_isoManager->registerStream(*it)) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register receive stream processor (%p) with the Iso manager\n",*it);
				return false;
			}
			
			
		}

	debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
			if(!(*it)->setPortBuffersize(m_period)) {
				debugFatal( " could not set buffer size (%p)...\n",(*it));
				return false;
			
			}
			
			if(!(*it)->prepare()) {
				debugFatal( " could not prepare (%p)...\n",(*it));
				return false;
			
			}
			
			if (!m_isoManager->registerStream(*it)) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register transmit stream processor (%p) with the Iso manager\n",*it);
				return false;
			}
			
		}

	if (!m_isoManager->prepare()) {
		debugFatal("Could not prepare isoManager\n");
		return false;
	}

	return true;
}

bool StreamProcessorManager::Execute()
{
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");
	if(!m_isoManager->Execute()) {
		debugFatal("Could not execute isoManager\n");
		return false;
	}

	bool period_ready=true;
	m_xrun_has_occured=false;

 	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " RCV PROC: ");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		period_ready = period_ready && (*it)->isOnePeriodReady();
		m_xrun_has_occured = m_xrun_has_occured || (*it)->xrunOccurred();
	 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "(%d/%d/%d) ", period_ready, m_xrun_has_occured,(*it)->m_framecounter);
	}
	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "\n");

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " XMIT PROC: ");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		period_ready = period_ready && (*it)->isOnePeriodReady();
		m_xrun_has_occured = m_xrun_has_occured || (*it)->xrunOccurred();
	 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "(%d/%d/%d) ", period_ready, m_xrun_has_occured,(*it)->m_framecounter);
	}
	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "\n");

	if(m_xrun_has_occured) {
		// do xrun signaling/handling
		m_xruns++;
		sem_post(&m_period_semaphore);
		return false;
	}

	if(period_ready) {
		// signal the waiting thread(s?) that a period is ready
		sem_post(&m_period_semaphore);
	 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "Period done...\n");

		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			(*it)->decrementFrameCounter();
		}
	
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			(*it)->decrementFrameCounter();
		}
	}

	return true;

}

bool StreamProcessorManager::start() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting Processors...\n");
	assert(m_isoManager);
	
	debugOutput( DEBUG_LEVEL_VERBOSE, " Iso handlers...\n");
	if (!m_isoManager->startHandlers()) {
		debugFatal("Could not start handlers...\n");
		return false;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, " waiting for all StreamProcessors to start running...\n");
	// we have to wait untill all streamprocessors indicate that they are running
	// i.e. that there is actually some data stream flowing
	int wait_cycles=2000; // two seconds
	bool notRunning=true;
	while (notRunning && wait_cycles) {
		wait_cycles--;
		notRunning=false;
		
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			if(!(*it)->isRunning()) notRunning=true;
		}
	
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			if(!(*it)->isRunning()) notRunning=true;
		}
		usleep(1000);
	}
	
	if(!wait_cycles) { // timout has occurred
		debugFatal("One or more streams are not starting up (timeout):\n");
		           
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			if(!(*it)->isRunning()) {
				debugFatal(" receive stream %p not running\n",*it);
			} else {	
				debugFatal(" receive stream %p running\n",*it);
			}
		}
	
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			if(!(*it)->isRunning()) {
				debugFatal(" transmit stream %p not running\n",*it);
			} else {	
				debugFatal(" transmit stream %p running\n",*it);
			}
		}
		return false;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, " StreamProcessors running...\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Resetting frame counters...\n");
	
	// now we reset the frame counters
	// FIXME: check how we are going to do sync
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
		(*it)->resetFrameCounter();
	}
	
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
		(*it)->resetFrameCounter();
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, " Enabling StreamProcessors...\n");
	// and we enable the streamprocessors
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {		
		(*it)->enable();
	}
	
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->enable();
	}
	
	
	// dump the iso stream information when in verbose mode
	if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
		m_isoManager->dumpInfo();
	}
	
	
}

bool StreamProcessorManager::stop() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	assert(m_isoManager);
	return m_isoManager->stopHandlers();
}

bool StreamProcessorManager::waitForPeriod() {
	int ret;
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

	// Wait for packetizer thread to signal a period completion
	sem_wait(&m_period_semaphore);
	
	if(m_xrun_has_occured) return false;
	
	return true;

}

bool StreamProcessorManager::reset() {

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
		if(!(*it)->reset()) {
			debugFatal("could not reset stream processor (%p)",*it);
			return false;
		}
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		if(!(*it)->reset()) {
			debugFatal("could not reset stream processor (%p)",*it);
			return false;
		}
	}
	return true;
}

bool StreamProcessorManager::transfer() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Transferring period...\n");

	// a static cast could make sure that there is no performance
	// penalty for the virtual functions (to be checked)

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) { 
		if(!(*it)->transfer()) {
			debugFatal("could not transfer() stream processor (%p)",*it);
			return false;
		}
	}

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		if(!(*it)->transfer()) {
			debugFatal("could not transfer() stream processor (%p)",*it);
			return false;
		}
	}

	return true;
}

bool StreamProcessorManager::transfer(enum StreamProcessor::EProcessorType t) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");

	// a static cast could make sure that there is no performance
	// penalty for the virtual functions (to be checked)

// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Receive processors...\n");
	if (t==StreamProcessor::E_Receive) {
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) { 
			if(!(*it)->transfer()) {
				debugFatal("could not transfer() stream processor (%p)",*it);
				return false;
			}
		}
	} else {
// 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, " Transmit processors...\n");
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			if(!(*it)->transfer()) {
				debugFatal("could not transfer() stream processor (%p)",*it);
				return false;
			}
		}
	}

	return true;
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

	debugOutputShort( DEBUG_LEVEL_NORMAL, "Iso handler info:\n");
	m_isoManager->dumpInfo();

}

void StreamProcessorManager::setVerboseLevel(int l) {
	setDebugLevel(l);
	
	if (m_isoManager) m_isoManager->setVerboseLevel(l);

	debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		(*it)->setVerboseLevel(l);
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->setVerboseLevel(l);
	}
}


int StreamProcessorManager::getPortCount(enum Port::E_PortType type, enum Port::E_Direction direction) {
	int count=0;

	if (direction == Port::E_Capture) {
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			count += (*it)->getPortCount(type);
		}
	} else {
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			count += (*it)->getPortCount(type);
		}
	}
	return count;
}

int StreamProcessorManager::getPortCount(enum Port::E_Direction direction) {
	int count=0;

	if (direction == Port::E_Capture) {
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			count += (*it)->getPortCount();
		}
	} else {
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			count += (*it)->getPortCount();
		}
	}
	return count;
}

// TODO: implement a port map here, instead of the loop

Port* StreamProcessorManager::getPortByIndex(int idx, enum Port::E_Direction direction) {
	int count=0;
	int prevcount=0;

	if (direction == Port::E_Capture) {
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			count += (*it)->getPortCount();
			if (count > idx) {
				return (*it)->getPortAtIdx(idx-prevcount);
			}
			prevcount=count;
		}
	} else {
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			count += (*it)->getPortCount();
			if (count > idx) {
				return (*it)->getPortAtIdx(idx-prevcount);
			}
			prevcount=count;
		}
	}
	return NULL;
}


} // end of namespace
