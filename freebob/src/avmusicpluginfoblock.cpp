/* avmusicpluginfoblock.cpp
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
#include "avmusicpluginfoblock.h"

AvMusicPlugInfoBlock::AvMusicPlugInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {
	// do some more valid checks
	if (getType() != 0x810B) {
		bValid=false;
	}
	
	unsigned int next_block_position=address+0x06+readWord(0x04);
	unsigned int plug_type=readByte(0x06);
	unsigned int plug_id=readWord(0x07);
	unsigned int routing_support=readByte(0x09);
	
	unsigned int source_plug_function_type=readByte(0x0A);
	unsigned int source_plug_id=readByte(0x0B);
	unsigned int source_plug_function_block_ID=readByte(0x0C);
	unsigned int source_stream_position=readByte(0x0D);
	unsigned int source_stream_location=readByte(0x0E);
	unsigned int destination_plug_function_type=readByte(0x0F);
	unsigned int destination_plug_id=readByte(0x10);
	unsigned int destination_plug_function_block_ID=readByte(0x11);
	unsigned int destination_stream_position=readByte(0x12);
	unsigned int destination_stream_location=readByte(0x13);

	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock: Creating... length=0x%04X\n",getLength());
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   Music plug id=0x%04X\n",plug_id);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   Plug type=0x%02X\n",plug_type);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   routing support=0x%02X\n",routing_support);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   source plug: type=0x%02X - id=0x%02X - fb_id=0x%02X - pos=%d - loc=%d\n",source_plug_function_type,
			source_plug_id,source_plug_function_block_ID,source_stream_position,source_stream_location);
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   dest plug: type=0x%02X - id=0x%02X - fb_id=0x%02X - pos=%d - loc=%d\n",destination_plug_function_type,
			destination_plug_id,destination_plug_function_block_ID,destination_stream_position,destination_stream_location);

	
	if(next_block_position<address+getLength()) {
		// parse the optional name block
		AvNameInfoBlock *tmpNameInfoBlock=new AvNameInfoBlock(parent,next_block_position);
		if (tmpNameInfoBlock && (tmpNameInfoBlock->isValid())) {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   Name info=%s\n",tmpNameInfoBlock->getName());
		
		} else {
			debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock:   invalid extra name block present\n");
		}
		if(tmpNameInfoBlock) {
			delete tmpNameInfoBlock;
			tmpNameInfoBlock=NULL;
		}
	}
	
	
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock: Created\n");
	
}

AvMusicPlugInfoBlock::~AvMusicPlugInfoBlock() {


}

