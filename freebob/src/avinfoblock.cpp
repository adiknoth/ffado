/* avinfoblock.cpp
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

#include "avdescriptor.h"
#include "avinfoblock.h"
#include <string.h>
#include <errno.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include "debugmodule.h"

AvInfoBlock::AvInfoBlock(AvDescriptor *parent, int address) {
	if (parent) {
		cParent=parent;
		iLength=cParent->readWord(address);
		iBaseAddress=address;
		iType=cParent->readWord(address+2);
	} else {
		iLength=0;
	}
	if (iLength>0) {
		bValid=true;
	} else {		
		bValid=false;
	}

}

AvInfoBlock::~AvInfoBlock() {

}

bool AvInfoBlock::isValid() {
	return bValid;
}

unsigned int AvInfoBlock::getLength() {
	return iLength;
}

unsigned int AvInfoBlock::getType() {
	return iType;
}

unsigned char AvInfoBlock::readByte(unsigned int address) {
	if (cParent) {
		return cParent->readByte(iBaseAddress+address);
	}
	return 0;
}

unsigned int AvInfoBlock::readWord(unsigned int address) {
	if (cParent) {
		return cParent->readWord(iBaseAddress+address);
	}
	return 0;
}

unsigned int AvInfoBlock::readBuffer(unsigned int address, unsigned int length, unsigned char *buffer) {
	if (cParent) {
		return cParent->readBuffer(iBaseAddress+address,length,buffer);
	}
	return 0;

}
