/* avclusterinfoblock.cpp
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
#include "avclusterinfoblock.h"

AvClusterInfoBlock::AvClusterInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
    setDebugLevel( DEBUG_LEVEL_ALL );
	// do some more valid checks
	if (getType() != 0x810A) {
		bValid=false;
	}

	unsigned int next_block_position=address+0x06+readWord(0x04);

	unsigned int stream_format=readByte(0x06);
	unsigned int port_type=readByte(0x07);
	unsigned int nb_signals=readByte(0x08);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock: Creating... length=0x%04X\n",getLength());
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Stream format=0x%02X\n",stream_format);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Port type=0x%02X\n",port_type);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Number of signals=%d\n",nb_signals);

	for(unsigned int i=0;i<nb_signals;i++) {
		unsigned int plug_info=readWord(0x09+i*4);
		unsigned char stream_position=readByte(0x09+i*4+2);
		unsigned char stream_location=readByte(0x09+i*4+3);
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Signal %d: plug_info=0x%04X, stream_position=0x%02X, stream_location=0x%02X\n",i,plug_info,stream_position,stream_location);

	}

	if(next_block_position<address+getLength()) {
		// parse the optional name block
		AvNameInfoBlock *tmpNameInfoBlock=new AvNameInfoBlock(parent,next_block_position);
		if (tmpNameInfoBlock && (tmpNameInfoBlock->isValid())) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Name info=%s\n",tmpNameInfoBlock->getName());

		} else {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   invalid extra name block present\n");
		}
		if(tmpNameInfoBlock) {
			delete tmpNameInfoBlock;
			tmpNameInfoBlock=NULL;
		}
	}


	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock: Created\n");

}

AvClusterInfoBlock::~AvClusterInfoBlock() {


}

void AvClusterInfoBlock::printSignalInfo(unsigned char idx) {
	if ( ( idx < getNbSignals() ) && ( ( unsigned int )( 0x09+idx*4 ) < getLength() ) ) {
		unsigned int plug_info=readWord(0x09+idx*4);
		unsigned char stream_position=readByte(0x09+idx*4+2);
		unsigned char stream_location=readByte(0x09+idx*4+3);
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Signal %d: plug_info=0x%04X, stream_position=0x%02X, stream_location=0x%02X\n",idx,plug_info,stream_position,stream_location);
	} else {
		debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Signal %d not present!\n",idx);

	}
}

void AvClusterInfoBlock::printName() {
	unsigned int next_block_position=iBaseAddress+0x06+readWord(0x04);

	if(next_block_position<iBaseAddress+getLength()) {
		// parse the optional name block
		AvNameInfoBlock *tmpNameInfoBlock=new AvNameInfoBlock(cParent,next_block_position);
		if (tmpNameInfoBlock && (tmpNameInfoBlock->isValid())) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   Name: %s\n",tmpNameInfoBlock->getName());

		} else {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvClusterInfoBlock:   no name block present\n");
		}
		if(tmpNameInfoBlock) {
			delete tmpNameInfoBlock;
			tmpNameInfoBlock=NULL;
		}
	}

}
