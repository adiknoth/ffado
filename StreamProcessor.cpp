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

namespace FreebobStreaming {

IMPL_DEBUG_MODULE( StreamProcessor, StreamProcessor, DEBUG_LEVEL_NORMAL );

StreamProcessor::StreamProcessor(enum IsoStream::EStreamType type, int channel) 
	: IsoStream(type, channel) {

}

StreamProcessor::~StreamProcessor() {

}

int StreamProcessor::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {
	int retval;

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );

	return 0;
}

int StreamProcessor::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped, unsigned int max_length) {
	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "sending packet: length=%d, cycle=%d\n",
	             *length, cycle );

	int retval;
	*length=sizeof(cycle);
	*tag = 1;
	*sy = 0;

	return 0;
}




}
