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

#include "AmdtpPort.h"
#include <assert.h>

namespace FreebobStreaming {

AmdtpMidiPort::AmdtpMidiPort(std::string name, 
	                   enum E_Direction direction,
		           int position, 
		           int location, 
		           enum E_Formats format, 
		           int type)
	: MidiPort(name, direction),
	  AmdtpPortInfo(name, position, location, format, type)
	  , m_countdown(0)
{
	m_ringbuffer=freebob_ringbuffer_create(m_buffersize * getEventSize());
	
	if(!m_ringbuffer) {
		debugFatal("Could not allocate ringbuffer\n");
		m_buffersize=0;
	}
	
}

AmdtpMidiPort::~AmdtpMidiPort() {
	if (m_ringbuffer) freebob_ringbuffer_free(m_ringbuffer);
	
}

/**
 * The problem with MIDI ports is that there is no guaranteed availability of data.
 * This function will return true if: 
 *  (1) there is a byte ready in the buffer
 *  (2) we are allowed to send a byte
 *
 * it will also assume that you actually are sending a byte, and it will reset
 * the countdown
 *
 * note on (2): the midi over 1394 spec limits the speed of sending midi data bytes.
 *              For every (time muxed) channel, you can send only one midi byte every
 *              320 microseconds. The packet rate is 8000pkt/sec, or 125us. Therefore
 *              we wait (at least) two packets before sending another byte. This comes
 *              down to 375us, so there is a slight limiting of the bandwidth.
 *
 * \todo fix the too long delay (375us instead of 320us)
 *
 * @return true if you can send a midi byte
 */
bool AmdtpMidiPort::canSend() {
	bool byte_present_in_buffer;
	assert(m_ringbuffer);
	
	byte_present_in_buffer=(freebob_ringbuffer_read_space(m_ringbuffer)>=sizeof(char));
	
	if(byte_present_in_buffer && (m_countdown < 0)) {
		m_countdown=2;
		return true;
	}
	return false;
}

} // end of namespace FreebobStreaming
