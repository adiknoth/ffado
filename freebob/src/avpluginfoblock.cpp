/* avpluginfoblock.cpp
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
#include "avpluginfoblock.h"
#include "avclusterinfoblock.h"

AvPlugInfoBlock::AvPlugInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x8109) {
		bValid=false;
	}
	


	unsigned int next_block_position=address+0x0E;
	unsigned int subunit_plug_id=readByte(0x06);
	unsigned int signal_format=readWord(0x07);
	unsigned int plug_type=readByte(0x09);
	unsigned int nb_clusters=readWord(0x0A);
	unsigned int nb_channels=readWord(0x0C);
	
	AvClusterInfoBlock *tmpAvClusterInfoBlock=NULL;
	AvInfoBlock *tmpInfoBlock=NULL;
	
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock: Creating... length=0x%04X\n",getLength());
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   Subunit plug id=0x%02X\n",subunit_plug_id);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   Signal format=0x%04X\n",signal_format);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   Plug type=0x%02X\n",plug_type);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   Number of clusters=%d\n",nb_clusters);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   Number of channels=%d\n",nb_channels);

		
	if (nb_clusters>0) {
		for (unsigned int i=0;i<nb_clusters;i++) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock: cluster=%d\n",i);
			tmpAvClusterInfoBlock=new AvClusterInfoBlock(parent, next_block_position);
			
			if (tmpAvClusterInfoBlock && (tmpAvClusterInfoBlock->isValid())) {

				next_block_position+=tmpAvClusterInfoBlock->getLength()+2; // the 2 is due to the fact that the length of the length field of the infoblock isn't accounted for;
				
				// add to child list
				cClusterInfoBlocks.push_back(tmpAvClusterInfoBlock);
				
				//tmpAvPlugInfoBlock->printContents();
				
			} else {
				debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock: Invalid block... parse error!\n");
				if(tmpAvClusterInfoBlock) {
					delete tmpAvClusterInfoBlock;
					tmpAvClusterInfoBlock=NULL;
				}
				bValid=false;
				break; // what to do now?
			}
			
		}
	}
	
	if(next_block_position<address+getLength()) {
		// parse the optional name block
		tmpInfoBlock=new AvInfoBlock(parent,next_block_position);
		if (tmpInfoBlock && (tmpInfoBlock->isValid())) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   extra block of type=0x%04X present\n",tmpInfoBlock->getType());
		
		} else {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock:   invalid extra block present\n");
		}
		if(tmpInfoBlock) {
			delete tmpInfoBlock;
			tmpInfoBlock=NULL;
		}
	
	}
	
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvPlugInfoBlock: Created\n");
	
}

AvPlugInfoBlock::~AvPlugInfoBlock() {
	vector<AvClusterInfoBlock *>::iterator it;
	for( it = cClusterInfoBlocks.begin(); it != cClusterInfoBlocks.end(); it++ ) {
		delete *it;
	}


}
	
AvClusterInfoBlock *AvPlugInfoBlock::getCluster(unsigned int idx) {
	if ((idx < getNbClusters()) && (idx < cClusterInfoBlocks.size())) {
		return cClusterInfoBlocks.at(idx);
	} else return NULL;
}

void AvPlugInfoBlock::printConnections() {
	for (unsigned int i=0; i<getNbClusters();i++) {
		AvClusterInfoBlock *tmpCluster=getCluster(i);
		if (tmpCluster) {
			tmpCluster->printName();
		}
	}
}