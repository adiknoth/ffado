/* avmidiinfoblock.cpp
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
#include "avmidiinfoblock.h"

AvMidiInfoBlock::AvMidiInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8104) {
		bValid=false;
	}
	
	unsigned int nb_streams=readByte(6);
	unsigned int position=0;
	AvNameInfoBlock *tmpNameBlock;
	
	debugPrint(DEBUG_LEVEL_INFO,"AvMidiInfoBlock: Creating... length=0x%04X\n",getLength());
	
	if (nb_streams>0) {
		position=address+7;
		for (unsigned int i=0;i<nb_streams;i++) {
			
			tmpNameBlock=new AvNameInfoBlock(parent, position);
			
			if (tmpNameBlock && (tmpNameBlock->isValid())) {
				position+=tmpNameBlock->getLength()+2; // the 2 is due to the fact that the length of the length field of the infoblock isn't accounted for;
				
				// add to child list
				cNameInfoBlocks.push_back(tmpNameBlock);
			} else {
				debugPrint(DEBUG_LEVEL_INFO,"Invalid name block in Midi info Block...\n");
				bValid=false;
				break; // what to do now?
			}
		}
		
	}
	debugPrint(DEBUG_LEVEL_INFO,"AvMidiInfoBlock: Created\n");
	
	// no optional info blocks please...
	
}

AvMidiInfoBlock::~AvMidiInfoBlock() {
	
	vector<AvNameInfoBlock *>::iterator it;
	for( it = cNameInfoBlocks.begin(); it != cNameInfoBlocks.end(); it++ ) {
		delete *it;
	}

}

unsigned int AvMidiInfoBlock::getNbStreams() {
	if(isValid()) {
		return readByte(6);
	} else {
		return 0;
	}
}

unsigned char *AvMidiInfoBlock::getName(unsigned int streamIdx) {
	AvNameInfoBlock *tmpNameBlock;
	
	if ((streamIdx<getNbStreams()) && (streamIdx<cNameInfoBlocks.size())) {
		tmpNameBlock =cNameInfoBlocks.at(streamIdx);
		if (tmpNameBlock) {
			return tmpNameBlock->getName();
		} else {
			return NULL;
		}
	}
	return NULL;
}
