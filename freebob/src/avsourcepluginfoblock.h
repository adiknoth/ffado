/* avsourcepluginfoblock.h
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
//#include <vector>
//using std::vector;

#include "debugmodule.h"

#include "avdevice.h"
#include "avdescriptor.h"
#include "avinfoblock.h"
#include "avaudioinfoblock.h"
#include "avmidiinfoblock.h"
#include "avaudiosyncinfoblock.h"
#include "avnameinfoblock.h"

#ifndef AVSOURCEPLUGINFOBLOCK_H
#define AVSOURCEPLUGINFOBLOCK_H

class AvSourcePlugInfoBlock : public AvInfoBlock {
public:
	AvSourcePlugInfoBlock(AvDescriptor *parent, int address); // read an infoblock from a parent starting from a specific position
	virtual ~AvSourcePlugInfoBlock();
	unsigned int getPlugNumber();

	void printContents(); // to debug the parse process
	
protected:
	//vector<AvNameInfoBlock*> cNameInfoBlocks;
	
	AvAudioInfoBlock * cAudioInfoBlock;
	AvMidiInfoBlock * cMidiInfoBlock;
	AvAudioSyncInfoBlock * cAudioSyncInfoBlock;
	
	DECLARE_DEBUG_MODULE;
};

#endif
