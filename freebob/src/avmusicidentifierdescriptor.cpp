/* avmusicidentifierdescriptor.cpp
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
#include "avmusicidentifierdescriptor.h"

AvMusicIdentifierDescriptor::AvMusicIdentifierDescriptor(AvDevice *parent) : AvDescriptor(parent, AVC1394_SUBUNIT_TYPE_MUSIC | AVC1394_SUBUNIT_ID_0,0x00) {

}

AvMusicIdentifierDescriptor::~AvMusicIdentifierDescriptor()  {

}

void AvMusicIdentifierDescriptor::printCapabilities() {
	if (!(this->AvDescriptor::isPresent())) {
		fprintf(stderr,"AvMusicIdentifierDescriptor: Descriptor not present!\n");
		return;
	}
	
	if (!(this->AvDescriptor::isOpen())) {
		fprintf(stderr,"AvMusicIdentifierDescriptor: Opening descriptor...\n");
		this->AvDescriptor::OpenReadOnly();
		if (!(this->AvDescriptor::isOpen()) ) {
			fprintf(stderr,"AvMusicIdentifierDescriptor:   Failed!\n");
			return;
		}
	}
	
	if (!(this->AvDescriptor::isLoaded())) {
		fprintf(stderr,"AvMusicIdentifierDescriptor: Loading descriptor...\n");
		this->AvDescriptor::Load();
		if (!(this->AvDescriptor::isLoaded())) {
			fprintf(stderr,"AvMusicIdentifierDescriptor:   Failed!\n");
			return;
		}
	}
	
	fprintf(stderr,"AvMusicIdentifierDescriptor: \n");
	// PP: calculate the offset to accomodate for the presence of root lists [not implemented]
	
	int offset=8; // update offset when beginning at a new table in the specs for easy reading
	fprintf(stderr,"\t music_subunit_dependant_info_fields_length:     0x%04X %03d\n",readWord(offset+0),readWord(offset+0));
	fprintf(stderr,"\t attributes:                                       0x%02X %03d\n",readByte(offset+2),readByte(offset+2));
	
	// PP: read the optional extra attribute bytes [not implemented]
	
	fprintf(stderr,"\t music_subunit_version:                            0x%02X %03d\n",readByte(offset+3),readByte(offset+3));
	
	fprintf(stderr,"\t music_subunit_specific_information_length:      0x%04X %03d\n",readWord(offset+4),readWord(offset+4));
	
	offset=14;
	unsigned char capability_attributes=readByte(offset+0);
	fprintf(stderr,"\t capability_attributes:                            0x%02X %03d\n",capability_attributes,capability_attributes);
	
	// PP: #defines in ieee1394service.h for the moment
	fprintf(stderr,"\t   -> capabilities: ");
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_GENERAL)) {
		fprintf(stderr," general ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIO)) {
		fprintf(stderr," audio ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_MIDI)) {
		fprintf(stderr," midi ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SMPTE)) {
		fprintf(stderr," smtpe ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SAMPLECOUNT)) {
		fprintf(stderr," samplecount ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC)) {
		fprintf(stderr," audiosync ");
	}
	fprintf(stderr,"\n");
	
	// start parsing the optional capability stuff
	offset=15;
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_GENERAL)) {
		fprintf(stderr,"\t   general capabilities:\n");
		
		unsigned char transmit_capability=readByte(offset+1);
		fprintf(stderr,"\t      transmit: %s%s\n",
			(transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING)?"blocking ":"",
			(transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING)?"non-blocking ":"");
		
		unsigned char receive_capability=readByte(offset+2);
		fprintf(stderr,"\t      receive:  %s%s\n",
			(receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING)?"blocking ":"",
			(receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING)?"non-blocking ":"");

		// PP: skip undefined latency cap
			
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIO)) {
		fprintf(stderr,"\t   audio capabilities: (%d entries)\n",readByte(offset+1));
		for (int i=0;i<readByte(offset+1);i++) {
			fprintf(stderr,"\t      %d: #inputs=%02d, #outputs=%02d, format=0x%04X)\n",i+1,readWord(offset+2+i*6),readWord(offset+4+i*6),readWord(offset+6+i*6));
		}
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_MIDI)) {
		fprintf(stderr,"\t   midi capabilities:\n");
		fprintf(stderr,"\t      midi version:             %d.%d\n",((readByte(offset+1) & 0xF0) >> 4),(readByte(offset+1) & 0x0F));
		fprintf(stderr,"\t      adaptation layer version: %d.%d\n",readByte(offset+2));
		fprintf(stderr,"\t      #inputs=%02d, #outputs=%02d\n",readWord(offset+3),readWord(offset+5));
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SMPTE)) {
		fprintf(stderr,"\t   smtpe capabilities:\n");
		// PP: not present on my quatafire
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SAMPLECOUNT)) {
		fprintf(stderr,"\t   samplecount capabilities:\n");
		// PP: not present on my quatafire
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC)) {
		fprintf(stderr,"\t   audiosync capabilities:\n");
		
		unsigned char audiosync_capability=readByte(offset+1);
		
		fprintf(stderr,"\t      sync from:  %s%s\n",
			(audiosync_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_BUS)?" 1394 bus ":"",
			(audiosync_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_EXTERNAL)?"  external source ":"");
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	
	
	
}
