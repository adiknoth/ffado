/* avmusicstatusdescriptor.cpp
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

#include "avdevice.h"
#include "avdescriptor.h"
#include "avmusicidentifierdescriptor.h"
#include "avmusicstatusdescriptor.h"
#include "avinfoblock.h"
#include "avgeneralmusicstatusinfoblock.h"
#include "avnameinfoblock.h"
#include "avaudioinfoblock.h"
#include "avmidiinfoblock.h"
#include "avaudiosyncinfoblock.h"
#include "avsourcepluginfoblock.h"
#include "avoutputplugstatusinfoblock.h"
#include "avroutingstatusinfoblock.h"
#include "avpluginfoblock.h"

AvMusicStatusDescriptor::AvMusicStatusDescriptor(AvDevice *parent, unsigned char id) : AvDescriptor(parent, AVC1394_SUBUNIT_TYPE_MUSIC | (id << 16),0x80) {
	if (!(this->AvDescriptor::isPresent())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicStatusDescriptor: Descriptor not present!\n");
		return;
	}
	
	if (!(this->AvDescriptor::isOpen())) {
		debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor: Opening descriptor...\n");
		this->AvDescriptor::OpenReadOnly();
		if (!(this->AvDescriptor::isOpen()) ) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicStatusDescriptor:   Failed!\n");
			bValid=false;
			return;
		}
	}
	
	if (!(this->AvDescriptor::isLoaded())) {
		debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor: Loading descriptor...\n");
		this->AvDescriptor::Load();
		if (!(this->AvDescriptor::isLoaded())) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicStatusDescriptor:   Failed!\n");
			bValid=false;
			return;
		}
	}
	
	unsigned int offset=0; // update offset when beginning at a new table in the specs for easy reading
	cGeneralMusicInfoBlock=NULL;
	cOutputPlugStatusInfoBlock=NULL;
	
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor: Creating AvGeneralMusicStatusInfoBlock... (offset=0x%04X)\n",offset);
	cGeneralMusicInfoBlock=new AvGeneralMusicInfoBlock(this,offset);
	if (!(cGeneralMusicInfoBlock) || !(cGeneralMusicInfoBlock->isValid())) {
		debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  AvGeneralMusicStatusInfoBlock not found!\n");
		bValid=false;
		return;
	}
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  AvGeneralMusicStatusInfoBlock found: length=0x%04X\n",cGeneralMusicInfoBlock->getLength());
	
	offset += cGeneralMusicInfoBlock->getLength()+2;
	
	
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor: Creating AvOutputPlugStatusInfoBlock... (offset=0x%04X)\n",offset);
	cOutputPlugStatusInfoBlock=new AvOutputPlugStatusInfoBlock(this,offset);
	if (!(cOutputPlugStatusInfoBlock) || !(cOutputPlugStatusInfoBlock->isValid())) {
		debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  AvOutputPlugStatusInfoBlock not found!\n");
		bValid=false;
		return;
	}
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  AvOutputPlugStatusInfoBlock found: length=0x%04X\n",cOutputPlugStatusInfoBlock->getLength());
	
	offset += cOutputPlugStatusInfoBlock->getLength()+2;
	
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor: Creating AvRoutingStatusInfoBlock... (offset=0x%04X)\n",offset);
	cRoutingStatusInfoBlock=new AvRoutingStatusInfoBlock(this,offset);
	if (!(cRoutingStatusInfoBlock) || !(cRoutingStatusInfoBlock->isValid())) {
		debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  AvRoutingStatusInfoBlock not found!\n");
		bValid=false;
		return;
	}
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  AvRoutingStatusInfoBlock found: length=0x%04X\n",cRoutingStatusInfoBlock->getLength());
	
	offset += cRoutingStatusInfoBlock->getLength()+2;
	
	// start parsing the optional infoblock(s)
	AvInfoBlock *tmpInfoBlock = NULL;
	debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor: Parsing optional infoblocks...\n");
	
	while (offset < getLength()) {
		debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  Optional block found...\n");
		tmpInfoBlock = new AvInfoBlock(this,offset);
		if (tmpInfoBlock && tmpInfoBlock->isValid()) {
			debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  Optional block of type 0x%04X with length 0x%04X found\n",tmpInfoBlock->getType(), tmpInfoBlock->getLength());
			
			offset += tmpInfoBlock->getLength()+2;
		} else {
			if (tmpInfoBlock) {
				delete tmpInfoBlock;
				tmpInfoBlock=NULL;
			}
			debugPrint (DEBUG_LEVEL_DESCRIPTOR, "AvMusicStatusDescriptor:  error parsing optional infoblock\n");
			bValid=false;
			return;
		}
		
	}

}
	
AvPlugInfoBlock *AvMusicStatusDescriptor::getSourcePlugInfoBlock(unsigned char plug) {
	if (cRoutingStatusInfoBlock) {
		return cRoutingStatusInfoBlock->getSourcePlugInfoBlock(plug);
	} else return NULL;
}

AvPlugInfoBlock *AvMusicStatusDescriptor::getDestinationPlugInfoBlock(unsigned char plug) {
	if (cRoutingStatusInfoBlock) {
		return cRoutingStatusInfoBlock->getDestinationPlugInfoBlock(plug);
	} else return NULL;

}

AvMusicStatusDescriptor::~AvMusicStatusDescriptor()  {
	if (cGeneralMusicInfoBlock) delete cGeneralMusicInfoBlock;
	if (cOutputPlugStatusInfoBlock) delete cOutputPlugStatusInfoBlock;
}

void AvMusicStatusDescriptor::printCapabilities() {
	
	
}
