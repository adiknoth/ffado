/* avgeneralmusicstatusinfoblock.cpp
 * Copyright (C) 2004 by Pieter Palmers
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "avdescriptor.h"
#include "avinfoblock.h"
#include "avgeneralmusicstatusinfoblock.h"

#include <string.h>
#include <errno.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include "debugmodule.h"

AvGeneralMusicInfoBlock::AvGeneralMusicInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8100) {
		bValid=false;
	}
}

AvGeneralMusicInfoBlock::~AvGeneralMusicInfoBlock() {

}

bool AvGeneralMusicInfoBlock::canTransmitBlocking() {
	if(isValid()) {
		unsigned char transmit_capability=readByte(6);
		return (transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING);
	} else {
		return false;
	}
}

bool AvGeneralMusicInfoBlock::canTransmitNonblocking() {
	if(isValid()) {
		unsigned char transmit_capability=readByte(6);
		return (transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING);
	
	} else {
		return false;
	}

}

bool AvGeneralMusicInfoBlock::canReceiveBlocking() {
	if(isValid()) {
		unsigned char receive_capability=readByte(7);
		return (receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING);
	
	} else {
		return false;
	}

}

bool AvGeneralMusicInfoBlock::canReceiveNonblocking() {
	if(isValid()) {
		unsigned char receive_capability=readByte(7);
		return (receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING);
	
	} else {
		return false;
	}

}

