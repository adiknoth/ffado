/* avpluginfoblock.h
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
#include <vector>
using std::vector;

#include "debugmodule.h"

#include "avdevice.h"
#include "avdescriptor.h"
#include "avinfoblock.h"
#include "avaudioinfoblock.h"
#include "avmidiinfoblock.h"
#include "avaudiosyncinfoblock.h"
#include "avnameinfoblock.h"
#include "avsourcepluginfoblock.h"

#ifndef AVPLUGINFOBLOCK_H
#define AVPLUGINFOBLOCK_H

class AvClusterInfoBlock;

class AvPlugInfoBlock : public AvInfoBlock {
public:
	AvPlugInfoBlock(AvDescriptor *parent, int address); // read an infoblock from a parent starting from a specific position
	virtual ~AvPlugInfoBlock();

	unsigned char getPlugId() { return readByte(0x06); };
	unsigned int getSignalFormat() { return readWord(0x07); };
	unsigned char getPlugType() { return readByte(0x09); };
	unsigned int getNbClusters() { return readWord(0x0A); };
	unsigned int getNbChannels() { return readWord(0x0C); };
	
	AvClusterInfoBlock *getCluster(unsigned int idx);
	
	void printConnections();
	
protected:
	vector<AvClusterInfoBlock *> cClusterInfoBlocks;
private:	
	
};

#endif
