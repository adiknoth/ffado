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
#ifndef __FREEBOB_STREAMPROCESSOR__
#define __FREEBOB_STREAMPROCESSOR__

#include "../debugmodule/debugmodule.h"

#include "IsoStream.h"
#include "PortManager.h"

namespace FreebobStreaming {

class StreamProcessor : public IsoStream, 
                        public PortManager {

public:
	enum EProcessorType {
		E_Receive,
		E_Transmit
	};

	StreamProcessor(enum IsoStream::EStreamType type, int channel);
	virtual ~StreamProcessor();

	int 
		putPacket(unsigned char *data, unsigned int length, 
	              unsigned char channel, unsigned char tag, unsigned char sy, 
		          unsigned int cycle, unsigned int dropped);
	int 
		getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length);

	enum EProcessorType getType() { return m_type;};

	void setPeriodSize(unsigned int period);
	void setPeriodSize(unsigned int period, unsigned int nb_buffers);
	int getPeriodSize() {return m_period;};

	void setNbBuffers(unsigned int nb_buffers);
	int getNbBuffers() {return m_nb_buffers;};

	bool xrunOccurred() { return (m_xruns>0);};

	bool periodDone() { return (m_framecounter > m_period); };
	unsigned int getNbPeriodsReady() { if(m_period) return m_framecounter/m_period; else return 0;};


protected:
	
	unsigned int m_nb_buffers;
	unsigned int m_period;

	unsigned int m_xruns;
	unsigned int m_framecounter;

	enum EProcessorType m_type;

    DECLARE_DEBUG_MODULE;


};

}

#endif /* __FREEBOB_STREAMPROCESSOR__ */


