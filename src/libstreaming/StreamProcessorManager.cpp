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
	: m_nb_buffers(nb_buffers), m_period(period), m_xruns(0), m_isoManager(0), m_nbperiods(0) {

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

	// the tread that runs the packet iterators
	m_streamingThread=new FreebobUtil::PosixThread(this,
	   m_thread_realtime, m_thread_priority+5, 
	   PTHREAD_CANCEL_DEFERRED);
	   
	if(!m_streamingThread) {
		debugFatal("Could not create streaming thread\n");
		return false;
	}
	
	m_isoManager=new IsoHandlerManager();
	
	if(!m_isoManager) {
		debugFatal("Could not create IsoHandlerManager\n");
		return false;
	}
	
	m_isoManager->setVerboseLevel(getDebugLevel());
	
	// the tread that keeps the handler's cycle counters up to date
	// NOTE: is lower priority nescessary? it can block
// 	m_isoManagerThread=new FreebobUtil::PosixThread(m_isoManager, m_thread_realtime, m_thread_priority+6, PTHREAD_CANCEL_DEFERRED);

    // now that we are using a DLL, we don't need to run this at RT priority
    // it only serves to cope with drift
    // however, in order to make the DLL fast enough, we have to increase
    // its bandwidth, making it more sensitive to deviations. These deviations
    // are mostly determined by the time difference between reading the cycle
    // time register and the local cpu clock.
    
 	m_isoManagerThread=new FreebobUtil::PosixThread(
 	      m_isoManager, 
 	      m_thread_realtime, m_thread_priority+6,
 	      PTHREAD_CANCEL_DEFERRED);
 	      
	if(!m_isoManagerThread) {
		debugFatal("Could not create iso manager thread\n");
		return false;
	}

	return true;
}

bool StreamProcessorManager::Init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing runner...\n");
	
	// no xrun has occurred (yet)
	m_xrun_happened=false;

	if(sem_init(&m_period_semaphore, 0, 0)) {
		debugFatal( "Cannot init packet transfer semaphore\n");
		debugFatal( " Error: %s\n",strerror(errno));
		return false;
    }
 
	return true;
}

bool StreamProcessorManager::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
			if(!(*it)->prepare()) {
				debugFatal(  " could not prepare (%p)...\n",(*it));
				return false;
				
			}
		}

	debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
			if(!(*it)->prepare()) {
				debugFatal( " could not prepare (%p)...\n",(*it));
				return false;
			
			}
			
		}

    // if there are no stream processors registered, 
    // fail
    if (m_ReceiveProcessors.size() + m_TransmitProcessors.size() == 0) {
        debugFatal("No stream processors registered, can't do anything usefull\n");
        return false;
    }

	return true;
}

bool StreamProcessorManager::Execute()
{

	bool period_ready=true;
    bool xrun_has_occured=false;
	bool this_period_ready;

// 	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "------------- EXECUTE -----------\n");

	if(!m_isoManager->iterate()) {
		debugFatal("Could not iterate the isoManager\n");
		return false;
	}
	
 	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " RCV PROC: ");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		
		this_period_ready = (*it)->isOnePeriodReady();
		period_ready = period_ready && this_period_ready;
// 		if (this_period_ready) {
// 		    m_isoManager->disablePolling(*it);
// 		}
// 		
		xrun_has_occured = xrun_has_occured || (*it)->xrunOccurred();
	 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "(%d/%d/%d) ", period_ready, xrun_has_occured,(*it)->m_framecounter);
	}
	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "\n");

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " XMIT PROC: ");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		this_period_ready = (*it)->isOnePeriodReady();
		period_ready = period_ready && this_period_ready;
// 		if (this_period_ready) {
// 		    m_isoManager->disablePolling(*it);
// 		}
		xrun_has_occured = xrun_has_occured || (*it)->xrunOccurred();
	 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "(%d/%d/%d) ", period_ready, xrun_has_occured,(*it)->m_framecounter);
	}
	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "\n");

	if(xrun_has_occured) {
		// do xrun signaling/handling
		debugWarning("Streaming thread detected xrun\n");
		m_xruns++;
		m_xrun_happened=true;
		sem_post(&m_period_semaphore);
        
		return false; // stop thread
	}

	if(period_ready) {
		// signal the waiting thread(s?) that a period is ready
		sem_post(&m_period_semaphore);
	 	debugOutputShort( DEBUG_LEVEL_VERY_VERBOSE, "Period done...\n");

		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			(*it)->decrementFrameCounter();
//  			m_isoManager->enablePolling(*it);
			
		}
	
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			(*it)->decrementFrameCounter();
//  			m_isoManager->enablePolling(*it);
		}
		
		m_nbperiods++;
	}

	return true;

}

bool StreamProcessorManager::start() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting Processors...\n");
	assert(m_isoManager);
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Creating handlers for the StreamProcessors...\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
			if (!(*it)->preparedForStart()) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Receive stream processor (%p) failed to prepare for start\n", *it);
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
			if (!(*it)->preparedForStart()) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Transmit stream processor (%p) failed to prepare for start\n", *it);
				return false;
			}
			if (!m_isoManager->registerStream(*it)) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Could not register transmit stream processor (%p) with the Iso manager\n",*it);
				return false;
			}
			
		}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing IsoHandlerManager...\n");
	if (!m_isoManager->prepare()) {
		debugFatal("Could not prepare isoManager\n");
		return false;
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting IsoHandler...\n");
	if (!m_isoManager->startHandlers(0)) {
		debugFatal("Could not start handlers...\n");
		return false;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting streaming thread...\n");
	
	// start the runner thread
	m_streamingThread->Start();
	
	// start the runner thread
	m_isoManagerThread->Start();
		
	debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for all StreamProcessors to start running...\n");
	// we have to wait until all streamprocessors indicate that they are running
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

	debugOutput( DEBUG_LEVEL_VERBOSE, "StreamProcessors running...\n");
	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting frame counters...\n");
	
	// now we reset the frame counters
	// FIXME: check how we are going to do sync
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}

 		(*it)->reset();

		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
	}
	
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
 		(*it)->reset();
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Enabling StreamProcessors...\n");
	// and we enable the streamprocessors
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {		
		(*it)->enable();
		m_isoManager->enablePolling(*it);
	}

	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->enable();
		m_isoManager->enablePolling(*it);
	}
	
	// dump the iso stream information when in verbose mode
	if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
		m_isoManager->dumpInfo();
	}
	
	return true;
	
}

bool StreamProcessorManager::stop() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping...\n");
	assert(m_isoManager);
	assert(m_streamingThread);

	debugOutput( DEBUG_LEVEL_VERBOSE, "Waiting for all StreamProcessors to prepare to stop...\n");
	// Most stream processors can just stop without special treatment.  However, some
	// (like the MOTU) need to do a few things before it's safe to turn off the iso
	// handling.
	int wait_cycles=2000; // two seconds ought to be sufficient
	bool allReady = false;
	while (!allReady && wait_cycles) {
		wait_cycles--;
		allReady = true;
		
		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {
			if(!(*it)->preparedForStop()) allReady = false;
		}
	
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			if(!(*it)->preparedForStop()) allReady = false;
		}
		usleep(1000);
	}


	debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping threads...\n");
	
	m_streamingThread->Stop();
	m_isoManagerThread->Stop();
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping handlers...\n");
	if(!m_isoManager->stopHandlers()) {
	   debugFatal("Could not stop ISO handlers\n");
	   return false;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Unregistering processors from handlers...\n");
    // now unregister all streams from iso manager
	debugOutput( DEBUG_LEVEL_VERBOSE, " Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
			if (!m_isoManager->unregisterStream(*it)) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister receive stream processor (%p) from the Iso manager\n",*it);
				return false;
			}
			
		}

	debugOutput( DEBUG_LEVEL_VERBOSE, " Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
			if (!m_isoManager->unregisterStream(*it)) {
				debugOutput(DEBUG_LEVEL_VERBOSE,"Could not unregister transmit stream processor (%p) from the Iso manager\n",*it);
				return false;
			}
			
		}
	
	return true;
	
}

bool StreamProcessorManager::waitForPeriod() {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

	// Wait for packetizer thread to signal a period completion
	sem_wait(&m_period_semaphore);
	
	if(m_xrun_happened) {
	   debugWarning("Detected underrun\n");
	   dumpInfo();
	   return false;
	}
	
	return true;

}

bool StreamProcessorManager::handleXrun() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Handling Xrun ...\n");

	/* 
	 * Reset means:
	 * 1) Stopping the packetizer thread
	 * 2) Bringing all buffers & streamprocessors into a know state
	 *    - Clear all capture buffers
	 *    - Put nb_periods*period_size of null frames into the playback buffers
	 * 3) Restarting the packetizer thread
	 */
	debugOutput( DEBUG_LEVEL_VERBOSE, "Stopping processormanager...\n");
	if(!stop()) {
	   debugFatal("Could not stop.\n");
	   return false;
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting Processors...\n");
	
	// now we reset the frame counters
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
		(*it)->reset();
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
	}
	
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
		
		(*it)->reset();
		
		if(getDebugLevel()>=DEBUG_LEVEL_VERBOSE) {
			(*it)->dumpInfo();
		}
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting processormanager...\n");

	if(!start()) {
	   debugFatal("Could not start.\n");
	   return false;
	}


	debugOutput( DEBUG_LEVEL_VERBOSE, "Xrun handled...\n");
	
	
	return true;
}

bool StreamProcessorManager::transfer() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Transferring period...\n");

	// a static cast could make sure that there is no performance
	// penalty for the virtual functions (to be checked)

	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) { 
		if(!(*it)->transfer()) {
			debugFatal("could not transfer() stream processor (%p)",*it);
			return false;
		}
	}

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
	debugOutputShort( DEBUG_LEVEL_NORMAL, "----------------------------------------------------\n");
	debugOutputShort( DEBUG_LEVEL_NORMAL, "Dumping StreamProcessorManager information...\n");
	debugOutputShort( DEBUG_LEVEL_NORMAL, "Period count: %d\n", m_nbperiods);

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
	debugOutputShort( DEBUG_LEVEL_NORMAL, "----------------------------------------------------\n");

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

bool StreamProcessorManager::setThreadParameters(bool rt, int priority) {
    m_thread_realtime=rt;
    m_thread_priority=priority;
    return true;
}


} // end of namespace
