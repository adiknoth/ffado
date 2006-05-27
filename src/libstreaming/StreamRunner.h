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
#ifndef __FREEBOB_STREAMRUNNER__
#define __FREEBOB_STREAMRUNNER__

#include "../debugmodule/debugmodule.h"
#include "FreebobThread.h"

namespace FreebobStreaming {

class StreamProcessorManager;
/*!
\brief Runs the isomanager and the processor manager
 
*/
class StreamRunner : public FreebobRunnableInterface {

public:

	StreamRunner(StreamProcessorManager *p) 
	    : m_processorManager(p)
	    {};

	virtual ~StreamRunner();

    // FreebobRunnableInterface interface
	bool Execute(); // note that this is called in we while(running) loop
	bool Init();

 	void setVerboseLevel(int l) { setDebugLevel(l);  };

protected:

	StreamProcessorManager *m_processorManager;

    DECLARE_DEBUG_MODULE;

};

}

#endif /* __FREEBOB_STREAMRUNNER__ */


