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

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessorManager, StreamProcessorManager, DEBUG_LEVEL_NORMAL );

StreamProcessorManager::StreamProcessorManager(unsigned int period) {

}

StreamProcessorManager::~StreamProcessorManager() {

}

int StreamProcessorManager::registerProcessor(StreamProcessor *processor)
{
	assert(processor);

	if (processor->getType()==StreamProcessor::E_Receive) {
		m_ReceiveProcessors.push_back(processor);
		return 0;
	}
	
	if (processor->getType()==StreamProcessor::E_Transmit) {
		m_TransmitProcessors.push_back(processor);
		return 0;
	}

	return -1;
}

int StreamProcessorManager::unregisterProcessor(StreamProcessor *processor)
{
	assert(processor);

	if (processor->getType()==StreamProcessor::E_Receive) {

		for ( StreamProcessorVectorIterator it = m_ReceiveProcessors.begin();
			it != m_ReceiveProcessors.end();
			++it ) {

			if ( *it == processor ) { 
					m_ReceiveProcessors.erase(it);
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
					return 0;
				}
		}
	}

	return -1; //not found

}

bool StreamProcessorManager::Init()
{
	return true;
}


bool StreamProcessorManager::Execute()
{

}

} // end of namespace
