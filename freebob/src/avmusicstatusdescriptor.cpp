/* avmusicstatusdescriptor.cpp
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

#include "avdevice.h"
#include "avdescriptor.h"
#include "avmusicstatusdescriptor.h"

AvMusicStatusDescriptor::AvMusicStatusDescriptor(AvDevice *parent) : AvDescriptor(parent, AVC1394_SUBUNIT_TYPE_MUSIC | AVC1394_SUBUNIT_ID_0,0x80) {

}

AvMusicStatusDescriptor::~AvMusicStatusDescriptor()  {

}

void AvMusicStatusDescriptor::printCapabilities() {
	if (!(this->AvDescriptor::isPresent())) {
		fprintf(stderr,"AvMusicStatusDescriptor: Descriptor not present!\n");
		return;
	}
	
	if (!(this->AvDescriptor::isOpen())) {
		fprintf(stderr,"AvMusicStatusDescriptor: Opening descriptor...\n");
		this->AvDescriptor::OpenReadOnly();
		if (!(this->AvDescriptor::isOpen()) ) {
			fprintf(stderr,"AvMusicStatusDescriptor:   Failed!\n");
			return;
		}
	}
	
	if (!(this->AvDescriptor::isLoaded())) {
		fprintf(stderr,"AvMusicStatusDescriptor: Loading descriptor...\n");
		this->AvDescriptor::Load();
		if (!(this->AvDescriptor::isLoaded())) {
			fprintf(stderr,"AvMusicStatusDescriptor:   Failed!\n");
			return;
		}
	}
	
	fprintf(stderr,"AvMusicStatusDescriptor: \n");
	// PP: calculate the offset to accomodate for the presence of root lists [not implemented]
	
	//int offset=2; // update offset when beginning at a new table in the specs for easy reading
	
	// start parsing the optional capability stuff
	
}
