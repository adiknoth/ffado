/* avnameinfoblock.cpp
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

AvNameInfoBlock::AvNameInfoBlock(AvDescriptor *parent, int address) : AvInfoBlock(parent,address) {

    setDebugLevel( DEBUG_LEVEL_ALL );
// do some more valid checks
	if (getType() != 0x000B) {
		bValid=false;
	}
	nameBuffer=NULL;

}

AvNameInfoBlock::~AvNameInfoBlock() {

	if(nameBuffer) {
		delete[] nameBuffer;
	}
}

unsigned char * AvNameInfoBlock::getName() {

	// PP: Can't find this in the specs I have... going to look around a bit for the spec this is in.
	//     (AV/C Information Block Types Specification Version 1.0)
	unsigned int readlen=0;
	if (isValid()) {
		// PP: we should parse some raw_text_info_blocks here, but the music spec indicates that there are maximum 3 of these
		//     so I do it manually here.
		// PP: for my device apparently one block is sufficient.
		// PP: one strange thing remains, that is that there are some NULL characters in the buffer.
		// unsigned int length_of_textblock_1=readWord(0x0A);
		// unsigned int type_of_textblock_1=readWord(0x0C); // = 0x000A ?
		unsigned int primary_field_length_textblock_1=readWord(0x0E); // ?? I guess ??

		if (nameBuffer) {
			delete nameBuffer;
		}
		nameBuffer=new unsigned char[primary_field_length_textblock_1];

		if((readlen=readBuffer(0x10,primary_field_length_textblock_1,nameBuffer))<primary_field_length_textblock_1) {
           		debugPrint (DEBUG_LEVEL_INFOBLOCK, "      truncated????\n");

			nameBuffer[primary_field_length_textblock_1-1]=0; // make sure the string converter doesn't read past the buffer
		}

		return nameBuffer;
	} else {
		return NULL;
	}
}
