/* avaudiosyncinfoblock.cpp
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


#include <string.h>
#include <errno.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include "debugmodule.h"

#include "avdescriptor.h"
#include "avinfoblock.h"
#include "avaudiosyncinfoblock.h"

AvAudioSyncInfoBlock::AvAudioSyncInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8107) {
		bValid=false;
	}
}

AvAudioSyncInfoBlock::~AvAudioSyncInfoBlock() {

}

bool AvAudioSyncInfoBlock::canSyncBus() {
	if(isValid()) {
		unsigned char capability=readByte(6);
		return (capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_BUS);
	} else {
		return false;
	}
}

bool AvAudioSyncInfoBlock::canSyncExternal() {
	if(isValid()) {
		unsigned char capability=readByte(6);
		return (capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_EXTERNAL);
	
	} else {
		return false;
	}

}
