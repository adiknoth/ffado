/* avnameinfoblock.h
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
#include "debugmodule.h"

#include "avdevice.h"
#include "avdescriptor.h"
#include "avinfoblock.h"

#ifndef AVNAMEINFOBLOCK_H
#define AVNAMEINFOBLOCK_H

class AvNameInfoBlock : public AvInfoBlock {
public:
	AvNameInfoBlock(AvDescriptor *parent, int address); // read an infoblock from a parent starting from a specific position
	virtual ~AvNameInfoBlock();
	unsigned char * getName();

protected:
	unsigned char *nameBuffer;
	DECLARE_DEBUG_MODULE;
	
};

#endif
