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

AvMusicIdentifierDescriptor::AvMusicIdentifierDescriptor(AvDevice *parent, unsigned char id) : AvDescriptor(parent, AVC1394_SUBUNIT_TYPE_MUSIC | (id<<16),0x00) {
	if (!(this->AvDescriptor::isPresent())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicIdentifierDescriptor: Descriptor not present!\n");
		return;
	}
	
	if (!(this->AvDescriptor::isOpen())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicIdentifierDescriptor: Opening descriptor...\n");
		this->AvDescriptor::OpenReadOnly();
		if (!(this->AvDescriptor::isOpen()) ) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicIdentifierDescriptor:   Failed!\n");
			return;
		}
	}
	
	if (!(this->AvDescriptor::isLoaded())) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicIdentifierDescriptor: Loading descriptor...\n");
		this->AvDescriptor::Load();
		if (!(this->AvDescriptor::isLoaded())) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicIdentifierDescriptor:   Failed!\n");
			return;
		}
	}
}

AvMusicIdentifierDescriptor::~AvMusicIdentifierDescriptor()  {

}

void AvMusicIdentifierDescriptor::printCapabilities() {

	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"AvMusicIdentifierDescriptor: len=%X\n",iLength);
	// PP: calculate the offset to accomodate for the presence of root lists [not implemented]
hexDump(aContents, iLength);
		
	int offset=8; // update offset when beginning at a new table in the specs for easy reading
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t music_subunit_dependant_info_fields_length:     0x%04X %03d\n",readWord(offset+0),readWord(offset+0));
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t attributes:                                       0x%02X %03d\n",readByte(offset+2),readByte(offset+2));
	
	// PP: read the optional extra attribute bytes [not implemented]
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t music_subunit_version:                            0x%02X %03d\n",readByte(offset+3),readByte(offset+3));
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t music_subunit_specific_information_length:      0x%04X %03d\n",readWord(offset+4),readWord(offset+4));
	
	offset=14;
	unsigned char capability_attributes=readByte(offset+0);
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t capability_attributes:                            0x%02X %03d\n",capability_attributes,capability_attributes);
	
	// PP: #defines in ieee1394service.h for the moment
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   -> capabilities: ");
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_GENERAL)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," general ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIO)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," audio ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_MIDI)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," midi ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SMPTE)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," smtpe ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SAMPLECOUNT)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," samplecount ");
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC)) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR," audiosync ");
	}
	debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,"\n");
	
	// start parsing the optional capability stuff
	offset=15;
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_GENERAL)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   general capabilities:\n");
		
		unsigned char transmit_capability=readByte(offset+1);
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      transmit: %s%s\n",
			(transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING)?"blocking ":"",
			(transmit_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING)?"non-blocking ":"");
		
		unsigned char receive_capability=readByte(offset+2);
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      receive:  %s%s\n",
			(receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_BLOCKING)?"blocking ":"",
			(receive_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_NONBLOCKING)?"non-blocking ":"");

		// PP: skip undefined latency cap
			
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIO)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   audio capabilities: (%d entries)\n",readByte(offset+1));
		for (int i=0;i<readByte(offset+1);i++) {
			debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      %d: #inputs=%02d, #outputs=%02d, format=0x%04X)\n",i+1,readWord(offset+2+i*6),readWord(offset+4+i*6),readWord(offset+6+i*6));
		}
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_MIDI)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   midi capabilities:\n");
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      midi version:             %d.%d\n",((readByte(offset+1) & 0xF0) >> 4),(readByte(offset+1) & 0x0F));
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      adaptation layer version: %d.%d\n",readByte(offset+2));
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      #inputs=%02d, #outputs=%02d\n",readWord(offset+3),readWord(offset+5));
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SMPTE)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   smtpe capabilities:\n");
		// PP: not present on my quatafire
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_SAMPLECOUNT)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   samplecount capabilities:\n");
		// PP: not present on my quatafire
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
	if ((capability_attributes & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t   audiosync capabilities:\n");
		
		unsigned char audiosync_capability=readByte(offset+1);
		
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"\t      sync from:  %s%s\n",
			(audiosync_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_BUS)?" 1394 bus ":"",
			(audiosync_capability & AVC1394_SUBUNIT_MUSIC_CAPABILITY_AUDIOSYNC_EXTERNAL)?"  external source ":"");
		
		offset += readByte(offset)+1; // update offset with length of this block incl the length field
	}
}
