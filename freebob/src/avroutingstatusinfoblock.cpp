/* avroutingstatusinfoblock.cpp
 * Copyright (C) 2004,05 by Pieter Palmers
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

#include "avdescriptor.h"
#include "avinfoblock.h"
#include "avnameinfoblock.h"
#include "avroutingstatusinfoblock.h"
#include "avpluginfoblock.h"
#include "avmusicpluginfoblock.h"

AvRoutingStatusInfoBlock::AvRoutingStatusInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
    setDebugLevel( DEBUG_LEVEL_ALL );
	// do some more valid checks
	if (getType() != 0x8108) {
		bValid=false;
	}

	unsigned int next_block_position=address+0x0A;
	unsigned int nb_dest_plugs=readByte(0x06);
	unsigned int nb_source_plugs=readByte(0x07);
	unsigned int nb_music_plugs=readWord(0x08);
	AvPlugInfoBlock *tmpAvPlugInfoBlock=NULL;
	AvMusicPlugInfoBlock *tmpAvMusicPlugInfoBlock=NULL;

	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: Creating... length=0x%04X\n",getLength());
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock:   Number of source plugs=%d\n",nb_source_plugs);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock:   Number of destination plugs=%d\n",nb_dest_plugs);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock:   Number of music plugs=%d\n",nb_music_plugs);

	if (nb_dest_plugs>0) {
		for (unsigned int i=0;i<nb_dest_plugs;i++) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: destination plug=%d\n",i);
			tmpAvPlugInfoBlock=new AvPlugInfoBlock(parent, next_block_position);

			if (tmpAvPlugInfoBlock && (tmpAvPlugInfoBlock->isValid())) {
				//debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvOutputPlugStatusInfoBlock: source plug type=0x%04X\n",tmpAvSourcePlugInfoBlock->getType());
				next_block_position+=tmpAvPlugInfoBlock->getLength()+2; // the 2 is due to the fact that the length of the length field of the infoblock isn't accounted for;

				// add to child list
				cDestinationPlugInfoBlocks.push_back(tmpAvPlugInfoBlock);

				//tmpAvPlugInfoBlock->printContents();

			} else {
				debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: Invalid block... parse error!\n");
				if(tmpAvPlugInfoBlock) {
					delete tmpAvPlugInfoBlock;
					tmpAvPlugInfoBlock=NULL;
				}
				bValid=false;
				break; // what to do now?
			}

		}
	}

	if (nb_source_plugs>0) {
		for (unsigned int i=0;i<nb_source_plugs;i++) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: source plug=%d\n",i);
			tmpAvPlugInfoBlock=new AvPlugInfoBlock(parent, next_block_position);

			if (tmpAvPlugInfoBlock && (tmpAvPlugInfoBlock->isValid())) {
				//debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvOutputPlugStatusInfoBlock: source plug type=0x%04X\n",tmpAvSourcePlugInfoBlock->getType());
				next_block_position+=tmpAvPlugInfoBlock->getLength()+2; // the 2 is due to the fact that the length of the length field of the infoblock isn't accounted for;

				// add to child list
				cSourcePlugInfoBlocks.push_back(tmpAvPlugInfoBlock);

				//tmpAvPlugInfoBlock->printContents();

			} else {
				debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: Invalid block... parse error!\n");
				if(tmpAvPlugInfoBlock) {
					delete tmpAvPlugInfoBlock;
					tmpAvPlugInfoBlock=NULL;
				}
				bValid=false;
				break; // what to do now?
			}

		}
	}

	if (nb_music_plugs>0) {
		for (unsigned int i=0;i<nb_music_plugs;i++) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: music plug=%d\n",i);
			tmpAvMusicPlugInfoBlock=new AvMusicPlugInfoBlock(parent, next_block_position);

			if (tmpAvMusicPlugInfoBlock && (tmpAvMusicPlugInfoBlock->isValid())) {
				//debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvOutputPlugStatusInfoBlock: source plug type=0x%04X\n",tmpAvSourcePlugInfoBlock->getType());
				next_block_position+=tmpAvMusicPlugInfoBlock->getLength()+2; // the 2 is due to the fact that the length of the length field of the infoblock isn't accounted for;

				// add to child list
				cMusicPlugInfoBlocks.push_back(tmpAvMusicPlugInfoBlock);

				//tmpAvPlugInfoBlock->printContents();

			} else {
				debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: Invalid block... parse error!\n");
				if(tmpAvMusicPlugInfoBlock) {
					delete tmpAvMusicPlugInfoBlock;
					tmpAvMusicPlugInfoBlock=NULL;
				}
				bValid=false;
				break; // what to do now?
			}

		}
	}


	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvRoutingStatusInfoBlock: Created\n");

}

AvRoutingStatusInfoBlock::~AvRoutingStatusInfoBlock() {
	vector<AvPlugInfoBlock *>::iterator it;
	vector<AvMusicPlugInfoBlock *>::iterator it2;
	for( it = cDestinationPlugInfoBlocks.begin(); it != cDestinationPlugInfoBlocks.end(); it++ ) {
		delete *it;
	}
	for( it = cSourcePlugInfoBlocks.begin(); it != cSourcePlugInfoBlocks.end(); it++ ) {
		delete *it;
	}
	for( it2 = cMusicPlugInfoBlocks.begin(); it2 != cMusicPlugInfoBlocks.end(); it2++ ) {
		delete *it2;
	}

}

AvPlugInfoBlock * AvRoutingStatusInfoBlock::getSourcePlugInfoBlock(unsigned char plug) {
	vector<AvPlugInfoBlock *>::iterator it;
	for( it = cSourcePlugInfoBlocks.begin(); it != cSourcePlugInfoBlocks.end(); it++ ) {
		if((*it) && ((*it)->getPlugId() == plug)) {
			return *it;
		}
	}
	return NULL;

}

AvPlugInfoBlock * AvRoutingStatusInfoBlock::getDestinationPlugInfoBlock(unsigned char plug) {
	vector<AvPlugInfoBlock *>::iterator it;
	for( it = cDestinationPlugInfoBlocks.begin(); it != cDestinationPlugInfoBlocks.end(); it++ ) {
		if((*it) && ((*it)->getPlugId() == plug)) {
			return *it;
		}
	}
	return NULL;

}
