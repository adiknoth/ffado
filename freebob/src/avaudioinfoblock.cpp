/* avaudioinfoblock.cpp
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
#include "avnameinfoblock.h"
#include "avaudioinfoblock.h"

AvAudioInfoBlock::AvAudioInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8103) {
		bValid=false;
	}	
	debugPrint(DEBUG_LEVEL_INFO,"AvAudioInfoBlock: Creating... length=0x%04X\n",getLength());
	
	// PP: I assume that there is only an audio block, no optional blocks.
	cNameInfoBlock=new AvNameInfoBlock(parent, address+7);
	debugPrint(DEBUG_LEVEL_INFO,"AvAudioInfoBlock: Created\n");
	
}

AvAudioInfoBlock::~AvAudioInfoBlock() {

	if(cNameInfoBlock) {
		delete cNameInfoBlock;
	}
}

unsigned int AvAudioInfoBlock::getNbStreams() {
	if(isValid()) {
		return readByte(6);
	} else {
		return 0;
	}
}

unsigned char *AvAudioInfoBlock::getName() {
	if (cNameInfoBlock && cNameInfoBlock->isValid()) {
		unsigned char * name=cNameInfoBlock->getName();
		return name;
	}
	return NULL;
}
