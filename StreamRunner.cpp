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

#include "StreamRunner.h"
#include "StreamProcessorManager.h"
#include "IsoHandlerManager.h"
#include <assert.h>


namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamRunner, StreamRunner, DEBUG_LEVEL_NORMAL );

StreamRunner::~StreamRunner() {

}

bool StreamRunner::Execute() {
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "enter...\n");
	// note that this is called in we while(running) loop
	
	return m_processorManager->Execute();

}

bool StreamRunner::Init() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	if(!m_processorManager) {
		debugFatal("Not a valid StreamProcessorManager\n");
		return false;
	}
	

	return true;

}


} // end of namespace FreebobStreaming
