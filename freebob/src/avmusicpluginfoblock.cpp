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
	

	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock: Creating... length=0x%04X\n",getLength());

	
	debugPrint(DEBUG_LEVEL_INFOBLOCK,"AvMusicPlugInfoBlock: Created\n");
	
}

AvMusicPlugInfoBlock::~AvMusicPlugInfoBlock() {


}

