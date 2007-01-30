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
	: m_SyncSource(NULL), m_nb_buffers(nb_buffers), m_period(period), m_xruns(0), 
	m_isoManager(0), m_nbperiods(0) {

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

bool StreamProcessorManager::setSyncSource(StreamProcessor *s) {
    m_SyncSource=s;
    return true;
}

StreamProcessor *StreamProcessorManager::getSyncSource() {
    return m_SyncSource;
}

bool StreamProcessorManager::init()
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	// the tread that runs the StreamProcessor
	// checking the period boundaries
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
	
	// the tread that keeps the handler's cycle timers up to date
        // and performs the actual packet transfer
        // needs high priority
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
        debugFatal( "Cannot init period transfer semaphore\n");
        debugFatal( " Error: %s\n",strerror(errno));
        return false;
    }

    return true;
}

bool StreamProcessorManager::prepare() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
	
	// if no sync source is set, select one here
	if(m_SyncSource == NULL) {
	   debugWarning("Sync Source is not set. Defaulting to first StreamProcessor.\n");
	}
	
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
			if(m_SyncSource == NULL) {
				debugWarning(" => Sync Source is %p.\n", *it);
				m_SyncSource = *it;
			}
	}

	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
			if(m_SyncSource == NULL) {
				debugWarning(" => Sync Source is %p.\n", *it);
				m_SyncSource = *it;
			}
	}

        // now do the actual preparation
	debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare Receive processors...\n");
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
			if(!(*it)->setSyncSource(m_SyncSource)) {
				debugFatal(  " could not set sync source (%p)...\n",(*it));
				return false;
			}
			
			if(!(*it)->prepare()) {
				debugFatal(  " could not prepare (%p)...\n",(*it));
				return false;
			}
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Prepare Transmit processors...\n");
	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
			if(!(*it)->setSyncSource(m_SyncSource)) {
				debugFatal(  " could not set sync source (%p)...\n",(*it));
				return false;
			}		
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

// FIXME: this can be removed
bool StreamProcessorManager::Execute()
{
        // temp measure, polling
        usleep(1000);

        // FIXME: move this to an IsoHandlerManager sub-thread
        // and make this private again in IHM
        m_isoManager->updateCycleTimers();
        
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
			if (!(*it)->prepareForStart()) {
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
			if (!(*it)->prepareForStart()) {
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

	debugOutput( DEBUG_LEVEL_VERBOSE, "Disabling StreamProcessors...\n");
        if (!disableStreamProcessors()) {
		debugFatal("Could not disable StreamProcessors...\n");
		return false;
	}
		
	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting IsoHandlers...\n");
	if (!m_isoManager->startHandlers(0)) {
		debugFatal("Could not start handlers...\n");
		return false;
	}
	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Starting streaming threads...\n");

	// note: libraw1394 doesn't like it if you poll() and/or iterate() before 
	//       starting the streams.
	// start the runner thread
	// FIXME: maybe this should go into the isomanager itself.
	m_isoManagerThread->Start();
	
	// start the runner thread
	// FIXME: not used anymore (for updatecycletimers ATM, but that's not good)
	m_streamingThread->Start();

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
	
	debugOutput( DEBUG_LEVEL_VERBOSE, " Sync Source StreamProcessor...\n");
        if (!m_SyncSource->prepareForEnable()) {
		debugFatal("Could not prepare Sync Source StreamProcessor for enable()...\n");
		return false;
	}
	
        m_SyncSource->enable();

	debugOutput( DEBUG_LEVEL_VERBOSE, " All StreamProcessors...\n");
        if (!enableStreamProcessors()) {
		debugFatal("Could not enable StreamProcessors...\n");
		return false;
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
			if(!(*it)->prepareForStop()) allReady = false;
		}
	
		for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
			it != m_TransmitProcessors.end();
			++it ) {
			if(!(*it)->prepareForStop()) allReady = false;
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

/**
 * Enables the registered StreamProcessors
 * @return true if successful, false otherwise
 */
bool StreamProcessorManager::enableStreamProcessors() {
	// and we enable the streamprocessors
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {		
		(*it)->prepareForEnable();
		(*it)->enable();
	}

	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->prepareForEnable();
		(*it)->enable();
	}
	return true;
}

/**
 * Disables the registered StreamProcessors
 * @return true if successful, false otherwise
 */
bool StreamProcessorManager::disableStreamProcessors() {
	// and we disable the streamprocessors
	for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
		it != m_ReceiveProcessors.end();
		++it ) {
		(*it)->prepareForDisable();
		(*it)->disable();
	}

	for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
		it != m_TransmitProcessors.end();
		++it ) {
		(*it)->prepareForDisable();
		(*it)->disable();
	}
	return true;
}

/**
 * Called upon Xrun events. This brings all StreamProcessors back
 * into their starting state, and then carries on streaming. This should
 * have the same effect as restarting the whole thing.
 *
 * @return true if successful, false otherwise
 */
bool StreamProcessorManager::handleXrun() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Handling Xrun ...\n");

	/* 
	 * Reset means:
	 * 1) Disabling the SP's, so that they don't process any packets
	 *    note: the isomanager does keep on delivering/requesting them
	 * 2) Bringing all buffers & streamprocessors into a know state
	 *    - Clear all capture buffers
	 *    - Put nb_periods*period_size of null frames into the playback buffers
	 * 3) Re-enable the SP's
	 */
	debugOutput( DEBUG_LEVEL_VERBOSE, "Disabling StreamProcessors...\n");
        if (!disableStreamProcessors()) {
		debugFatal("Could not disable StreamProcessors...\n");
		return false;
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting Processors...\n");
	
	// now we reset the streamprocessors
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
	
	debugOutput( DEBUG_LEVEL_VERBOSE, " Sync Source StreamProcessor...\n");
        if (!m_SyncSource->prepareForEnable()) {
		debugFatal("Could not prepare Sync Source StreamProcessor for enable()...\n");
		return false;
	}
	
        m_SyncSource->enable();

	debugOutput( DEBUG_LEVEL_VERBOSE, " All StreamProcessors...\n");
        if (!enableStreamProcessors()) {
		debugFatal("Could not enable StreamProcessors...\n");
		return false;
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Xrun handled...\n");
	
	return true;
}

/**
 * @brief Waits until the next period of samples is ready
 *
 * This function does not return until a full period of samples is (or should be)
 * ready to be transferred. 
 *
 * @return true if the period is ready, false if an xrun occurred
 */
bool StreamProcessorManager::waitForPeriod() {
    int time_till_next_period;
    
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");

    assert(m_SyncSource);
    
    time_till_next_period=m_SyncSource->getTimeUntilNextPeriodUsecs();
    
    while(time_till_next_period > 0) {
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "waiting for %d usecs...\n", time_till_next_period);
    
        // wait for the period
        usleep(time_till_next_period);
        
        // check if it is still true
        time_till_next_period=m_SyncSource->getTimeUntilNextPeriodUsecs();
    }
    
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "delayed for %d usecs...\n", time_till_next_period);
    
    // this is to notify the client of the delay 
    // that we introduced 
    m_delayed_usecs=time_till_next_period;
    
    // we save the 'ideal' time of the transfer at this point,
    // because we can have interleaved read - process - write 
    // cycles making that we modify a receiving stream's buffer
    // before we get to writing.
    // NOTE: before waitForPeriod() is called again, both the transmit
    //       and the receive processors should have done their transfer.
    m_time_of_transfer=m_SyncSource->getTimeAtPeriod();
    debugOutput( DEBUG_LEVEL_VERBOSE, "transfer at %llu ticks...\n", 
        m_time_of_transfer);
    
    // check if xruns occurred on the Iso side.
    // also check if xruns will occur should we transfer() now
    bool xrun_occurred=false;
    
    for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
          it != m_ReceiveProcessors.end();
          ++it ) {
        // a xrun has occurred on the Iso side
        xrun_occurred |= (*it)->xrunOccurred();
        
        // if this is true, a xrun will occur
        xrun_occurred |= !((*it)->canClientTransferFrames(m_period));
        
#ifdef DEBUG
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on RECV SP %p due to ISO xrun\n",*it);
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugWarning("Xrun on RECV SP %p due to buffer xrun\n",*it);
        }
#endif
        
    }
    for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
          it != m_TransmitProcessors.end();
          ++it ) {
        // a xrun has occurred on the Iso side
        xrun_occurred |= (*it)->xrunOccurred();
        
        // if this is true, a xrun will occur
        xrun_occurred |= !((*it)->canClientTransferFrames(m_period));
        
#ifdef DEBUG
        if ((*it)->xrunOccurred()) {
            debugWarning("Xrun on XMIT SP %p due to ISO xrun\n",*it);
        }
        if (!((*it)->canClientTransferFrames(m_period))) {
            debugWarning("Xrun on XMIT SP %p due to buffer xrun\n",*it);
        }
#endif        
    }
    
    // now we can signal the client that we are (should be) ready
    return !xrun_occurred;
}

/**
 * @brief Transfer one period of frames for both receive and transmit StreamProcessors
 *
 * Transfers one period of frames from the client side to the Iso side and vice versa.
 *
 * @return true if successful, false otherwise (indicates xrun).
 */
bool StreamProcessorManager::transfer() {
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Transferring period...\n");

    if (!transfer(StreamProcessor::E_Receive)) return false;
    if (!transfer(StreamProcessor::E_Transmit)) return false;

    return true;
}

/**
 * @brief Transfer one period of frames for either the receive or transmit StreamProcessors
 *
 * Transfers one period of frames from the client side to the Iso side or vice versa.
 *
 * @param t The processor type to tranfer for (receive or transmit)
 * @return true if successful, false otherwise (indicates xrun).
 */

bool StreamProcessorManager::transfer(enum StreamProcessor::EProcessorType t) {
    int64_t time_of_transfer;
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Transferring period...\n");
    
    // first we should find out on what time this transfer is
    // supposed to be happening. this time will become the time
    // stamp for the transmitted buffer.
    // NOTE: maybe we should include the transfer delay here, that
    //       would make it equal for all types of SP's
    time_of_transfer=m_time_of_transfer;
    
    // a static cast could make sure that there is no performance
    // penalty for the virtual functions (to be checked)
    if (t==StreamProcessor::E_Receive) {
        for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
                it != m_ReceiveProcessors.end();
                ++it ) {
            uint64_t buffer_tail_ts;
            uint64_t fc;
            int64_t ts;
        
            (*it)->getBufferTailTimestamp(&buffer_tail_ts,&fc);
            ts =  buffer_tail_ts;
            ts += (int64_t)((-(int64_t)fc) * m_SyncSource->getTicksPerFrame());
            // NOTE: ts can be negative due to wraparound, it is the responsability of the 
            //       SP to deal with that.
            
            float tmp=m_SyncSource->getTicksPerFrame();
            
            debugOutput(DEBUG_LEVEL_VERBOSE, "=> TS=%11lld, BLT=%11llu, FC=%5d, TPF=%f\n",
                ts, buffer_tail_ts, fc, tmp
                );
            debugOutput(DEBUG_LEVEL_VERBOSE, "   TPF=%f\n", tmp);
             
            #ifdef DEBUG
            {
                uint64_t ts_tail=0;
                uint64_t fc_tail=0;
                
                uint64_t ts_head=0;
                uint64_t fc_head=0;
                
                int cnt=0;
                
                (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
                
                while((fc_head != fc_tail) && (cnt++ < 10)) {
                    (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                    (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
                }
                
                debugOutput(DEBUG_LEVEL_VERBOSE,"R *  HEAD: TS=%llu, FC=%llu | TAIL: TS=%llu, FC=%llu, %d\n",
                    ts_tail, fc_tail, ts_head, fc_head, cnt);
            }
            #endif
    
            if(!(*it)->getFrames(m_period, ts)) {
                    debugOutput(DEBUG_LEVEL_VERBOSE,"could not getFrames(%u) from stream processor (%p)",
                            m_period,*it);
                    return false; // buffer underrun
            }
            
            #ifdef DEBUG
            {
                uint64_t ts_tail=0;
                uint64_t fc_tail=0;
                
                uint64_t ts_head=0;
                uint64_t fc_head=0;
                
                int cnt=0;
                
                (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
            
                while((fc_head != fc_tail) && (cnt++ < 10)) {
                    (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                    (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
                }
                
                debugOutput(DEBUG_LEVEL_VERBOSE,"R > HEAD: TS=%llu, FC=%llu | TAIL: TS=%llu, FC=%llu, %d\n",
                    ts_tail, fc_tail, ts_head, fc_head, cnt);            
            }
            #endif
    
        }
    } else {
        for ( StreamProcessorVectorIterator it = m_TransmitProcessors.begin();
                it != m_TransmitProcessors.end();
                ++it ) {
                
            #ifdef DEBUG
            {
                uint64_t ts_tail=0;
                uint64_t fc_tail=0;
                
                uint64_t ts_head=0;
                uint64_t fc_head=0;
                
                int cnt=0;
                
                (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
                
                while((fc_head != fc_tail) && (cnt++ < 10)) {
                    (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                    (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
                }
                
                debugOutput(DEBUG_LEVEL_VERBOSE,"T *  HEAD: TS=%llu, FC=%llu | TAIL: TS=%llu, FC=%llu, %d\n",
                    ts_tail, fc_tail, ts_head, fc_head, cnt);
            }
            #endif
                
            if(!(*it)->putFrames(m_period,time_of_transfer)) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "could not putFrames(%u,%llu) to stream processor (%p)",
                        m_period, time_of_transfer, *it);
                return false; // buffer overrun
            }
            
            #ifdef DEBUG
            {
                uint64_t ts_tail=0;
                uint64_t fc_tail=0;
                
                uint64_t ts_head=0;
                uint64_t fc_head=0;
                
                int cnt=0;
                
                (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
            
                while((fc_head != fc_tail) && (cnt++ < 10)) {
                    (*it)->getBufferHeadTimestamp(&ts_head,&fc_head);
                    (*it)->getBufferTailTimestamp(&ts_tail,&fc_tail);
                }
                
                debugOutput(DEBUG_LEVEL_VERBOSE,"T > HEAD: TS=%llu, FC=%llu | TAIL: TS=%llu, FC=%llu, %d\n",
                    ts_tail, fc_tail, ts_head, fc_head, cnt);            
            }
            #endif
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
