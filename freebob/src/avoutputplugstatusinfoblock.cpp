/* avoutputplugstatusinfoblock.cpp
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
#include "avoutputplugstatusinfoblock.h"

AvOutputPlugStatusInfoBlock::AvOutputPlugStatusInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8101) {
		bValid=false;
	}
	
	unsigned int next_block_position=address+7;
	unsigned int nb_sourceplugs=readByte(6);
	AvSourcePlugInfoBlock *tmpAvSourcePlugInfoBlock=NULL;

	debugPrint(DEBUG_LEVEL_INFO,"AvOutputPlugStatusInfoBlock: Creating... length=0x%04X\n",getLength());
		
	if (nb_sourceplugs>0) {
		for (unsigned int i=0;i<nb_sourceplugs;i++) {
			tmpAvSourcePlugInfoBlock=new AvSourcePlugInfoBlock(parent, next_block_position);
			
			if (tmpAvSourcePlugInfoBlock && (tmpAvSourcePlugInfoBlock->isValid())) {
				next_block_position+=tmpAvSourcePlugInfoBlock->getLength()+2; // the 2 is due to the fact that the length of the length field of the infoblock isn't accounted for;
				
				// add to child list
				cSourcePlugs.push_back(tmpAvSourcePlugInfoBlock);
			} else {
				debugPrint(DEBUG_LEVEL_INFO,"AvOutputPlugStatusInfoBlock: Invalid block... parse error!\n");
				bValid=false;
				break; // what to do now?
			}
			
		}
	}
	
	debugPrint(DEBUG_LEVEL_INFO,"AvOutputPlugStatusInfoBlock: Created\n");
	
}

AvOutputPlugStatusInfoBlock::~AvOutputPlugStatusInfoBlock() {
	vector<AvSourcePlugInfoBlock *>::iterator it;
	for( it = cSourcePlugs.begin(); it != cSourcePlugs.end(); it++ ) {
		delete *it;
	}

}

unsigned int AvOutputPlugStatusInfoBlock::getNbSourcePlugs() {
	if(isValid()) {
		return readByte(6);
	} else {
		return 0;
	}
}
