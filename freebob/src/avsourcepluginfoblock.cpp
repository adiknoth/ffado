/* avsourcepluginfoblock.cpp
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
#include "avsourcepluginfoblock.h"

AvSourcePlugInfoBlock::AvSourcePlugInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8102) {
		bValid=false;
	}
	
	unsigned int next_block_position=address+7;
	
	AvInfoBlock *tmpInfoBlock=NULL;
	
	cAudioInfoBlock=NULL;
	cMidiInfoBlock=NULL;
	cAudioSyncInfoBlock=NULL;
	
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Creating... length=0x%04X\n",getLength());

	// parse the child info blocks
	while ((next_block_position<address+getLength())) {
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Creating tmpInfoBlock\n");

		tmpInfoBlock=new AvInfoBlock(parent,next_block_position);
		
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: testing tmpInfoBlock\n");
		if (tmpInfoBlock && tmpInfoBlock->isValid()) {
			// read the type of the block
			// note: only one block instance per type is supported (according to the specs)
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: position=0x%04X  type=0x%04X\n",next_block_position,tmpInfoBlock->getType());
			switch (tmpInfoBlock->getType()) {
				case 0x8103:
				debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Creating AudioInfoBlock\n");
					if(cAudioInfoBlock) {
						debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: deleting duplicate cAudioInfoBlock. Non-conformant info block!\n");
						delete cAudioInfoBlock;
					}
					cAudioInfoBlock=new AvAudioInfoBlock(parent, next_block_position);
				break;
				case 0x8104:
					debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Creating MidiInfoBlock\n");
					if(cMidiInfoBlock) {
						debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: deleting duplicate cMidiInfoBlock. Non-conformant info block!\n");
						delete cMidiInfoBlock;
					}
					cMidiInfoBlock=new AvMidiInfoBlock(parent, next_block_position);
				break;
				case 0x8105:
					debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Skipping SMPTE block, unsupported.\n");
				break;
				case 0x8106:
					debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Skipping SampleCount block, unsupported.\n");
				break;
				case 0x8107:
					debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Creating AudioSyncInfoBlock\n");
					if(cAudioSyncInfoBlock) {
						debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: deleting duplicate cAudioSyncInfoBlock. Non-conformant info block!\n");
						delete cAudioSyncInfoBlock;
					}
					cAudioSyncInfoBlock=new AvAudioSyncInfoBlock(parent, next_block_position);
				break;
				default:
					debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Skipping unknown block\n");
				break;
				
			}
			// update the block position pointer
			next_block_position+=tmpInfoBlock->getLength()+2;
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Advancing to position=0x%04X\n",next_block_position);
			
		} else {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Parse error!\n");
			bValid=false;
			break;
		}
		
		if (tmpInfoBlock) {
			delete tmpInfoBlock;
			tmpInfoBlock=NULL;
		}
	}
	
	if (tmpInfoBlock) {
		delete tmpInfoBlock;
		tmpInfoBlock=NULL;
	}
	
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: Created\n");
	
}

AvSourcePlugInfoBlock::~AvSourcePlugInfoBlock() {
	if(cAudioInfoBlock) delete cAudioInfoBlock;
	if(cMidiInfoBlock) delete cMidiInfoBlock;
	if(cAudioSyncInfoBlock) delete cAudioSyncInfoBlock;
}

unsigned int AvSourcePlugInfoBlock::getPlugNumber() {
	if(isValid()) {
		return readByte(6);
	} else {
		return 0;
	}
}

void AvSourcePlugInfoBlock::printContents() {
	if(cAudioInfoBlock) {
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: AudioInfoBlock: %s\n",cAudioInfoBlock->getName());
	}
	if(cMidiInfoBlock) {
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: MidiInfoBlock %d streams\n",cMidiInfoBlock->getNbStreams());
		for (unsigned int i=0;i<cMidiInfoBlock->getNbStreams();i++) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: MidiInfoBlock stream %d: %s\n",i,cMidiInfoBlock->getName(i));
		}
	}
	if(cAudioSyncInfoBlock) {
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvSourcePlugInfoBlock: AudioSyncInfoBlock can sync to: Bus? %s / External? %s\n",
			cAudioSyncInfoBlock->canSyncBus()?"yes":"no",
			cAudioSyncInfoBlock->canSyncExternal()?"yes":"no"
			);
	}
}
