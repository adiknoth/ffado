/* avdevicemusicsubunit.cpp
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
#include <errno.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include "debugmodule.h"
#include "avdevice.h"
#include "avdevicesubunit.h"
#include "avdevicemusicsubunit.h"
#include "avmusicstatusdescriptor.h"
#include "avmusicidentifierdescriptor.h"

#define CORRECT_INTEGER_ENDIANNESS(value) (((((value)>>8) & 0xFF)) | ((((value)) & 0xFF)<<8))

void AvDeviceMusicSubunit::test() {
	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;
	
	unsigned char opcr=0;
	
	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Serial bus iso output plug connections:\n");
	for(opcr=0;opcr<0x1E;opcr++) {	
		request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
						| (0x1A << 8) | 0xFF;
		//request[1] = ((iTarget & 0x1F) << 27) | ((iId & 0x07) << 24) | 0x00FF7F;
		request[1]=0xFFFEFF00 | opcr;
		response = cParent->avcExecuteTransaction(request, 2, 4);
		if ((response != NULL) && ((response[0]&0xFF000000)==0x0C000000)) {
			unsigned char output_status=(response[0]&0xE0) >> 5;
			unsigned char conv=(response[0]&0x10) >> 4;
			unsigned char signal_status=(response[0]&0x0F);
			
			unsigned int signal_source=((response[1]>>16)&0xFFFF);
			unsigned int signal_destination=((response[1])&0xFFFF);
			debugPrint(DEBUG_LEVEL_SUBUNIT,"  oPCR %d: output_status=%d,conv=%d,signal_status=%d,signal_source=0x%04X,signal_destination=0x%04X\n",opcr,output_status,conv,signal_status,signal_source,signal_destination);
		}
	}
	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Serial bus external output plug connections:\n");
	for(opcr=0x80;opcr<0x9E;opcr++) {	
		request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
						| (0x1A << 8) | 0xFF;
		//request[1] = ((iTarget & 0x1F) << 27) | ((iId & 0x07) << 24) | 0x00FF7F;
		request[1]=0xFFFEFF00 | opcr;
		response = cParent->avcExecuteTransaction(request, 2, 4);
		if ((response != NULL) && ((response[0]&0xFF000000)==0x0C000000)) {
			unsigned char output_status=(response[0]&0xE0) >> 5;
			unsigned char conv=(response[0]&0x10) >> 4;
			unsigned char signal_status=(response[0]&0x0F);
			
			unsigned int signal_source=((response[1]>>16)&0xFFFF);
			unsigned int signal_destination=((response[1])&0xFFFF);
			debugPrint(DEBUG_LEVEL_SUBUNIT,"  oPCR %02X: output_status=%d,conv=%d,signal_status=%d,signal_source=0x%04X,signal_destination=0x%04X\n",opcr,output_status,conv,signal_status,signal_source,signal_destination);
		}
	}

}
void AvDeviceMusicSubunit::printMusicPlugConfigurations() {
	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;
	
	unsigned int subunit_plug_id;
	
	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Source Plug configurations:\n");
		
	for (subunit_plug_id=0;subunit_plug_id < getNbSourcePlugs() ;subunit_plug_id++) {
		// get source plug configure status
		request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
						| (0x43 << 8) | subunit_plug_id;
		request[1] = 0xFFFFFF00;
		request[2] = 0x0000FFFF;
		request[3] = 0xFFFFFFFF;
		response = cParent->avcExecuteTransaction(request, 1, 4);
		if (response != NULL) {
			
			unsigned int start_of_music_plug_ID=CORRECT_INTEGER_ENDIANNESS(response[1]);
			unsigned int end_of_music_plug_ID=CORRECT_INTEGER_ENDIANNESS(response[1]>>16);
			
			debugPrint(DEBUG_LEVEL_SUBUNIT," Subunit source plug %d: start_of_music_plug_ID=%d, end_of_music_plug_ID=%d\n",subunit_plug_id,start_of_music_plug_ID,end_of_music_plug_ID);
			
			unsigned char *table_pointer=((unsigned char *)response)+8;
			
			for(unsigned int i=0;i<(end_of_music_plug_ID-start_of_music_plug_ID)+1;i++) {
				unsigned char music_plug_type=(*(table_pointer)) & 0xFF;
				unsigned int music_plug_ID=((*(table_pointer+1))<<8)+((*(table_pointer+2)));
 				unsigned int stream_position=((*(table_pointer+3))<<8)+((*(table_pointer+4)));
				
				debugPrint(DEBUG_LEVEL_SUBUNIT,"  %02d: Type=0x%02X, ID=%02d, Position=0x%04X\n",i+start_of_music_plug_ID,music_plug_type,music_plug_ID,stream_position);
				
				table_pointer+=5;
			}
		}
	}
	
	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Destination Plug configurations:\n");
	for (subunit_plug_id=0;subunit_plug_id < getNbDestinationPlugs() ;subunit_plug_id++) {
		// get source plug configure status
		request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
						| (0x42 << 8) | subunit_plug_id;
		request[1] = 0xFFFFFF00;
		request[2] = 0x0000FFFF;
		request[3] = 0xFFFFFFFF;
		response = cParent->avcExecuteTransaction(request, 1, 4);
		if (response != NULL) {
			
			unsigned int start_of_music_plug_ID=CORRECT_INTEGER_ENDIANNESS(response[1]);
			unsigned int end_of_music_plug_ID=CORRECT_INTEGER_ENDIANNESS(response[1]>>16);
			
			debugPrint(DEBUG_LEVEL_SUBUNIT," Subunit destination plug %d: start_of_music_plug_ID=%d, end_of_music_plug_ID=%d\n",subunit_plug_id,start_of_music_plug_ID,end_of_music_plug_ID);
			
			unsigned char *table_pointer=((unsigned char *)response)+8;
			
			for(unsigned int i=0;i<(end_of_music_plug_ID-start_of_music_plug_ID)+1;i++) {
				unsigned char music_plug_type=(*(table_pointer)) & 0xFF;
				unsigned int music_plug_ID=((*(table_pointer+1))<<8)+((*(table_pointer+2)));
 				unsigned int stream_position=((*(table_pointer+3))<<8)+((*(table_pointer+4)));
				
				debugPrint(DEBUG_LEVEL_SUBUNIT,"  %02d: Type=0x%02X, ID=%02d, Position=0x%04X\n",i+start_of_music_plug_ID,music_plug_type,music_plug_ID,stream_position);
				
				table_pointer+=5;
			}
		}
	}	

}

void AvDeviceMusicSubunit::printMusicPlugInfo() {
	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;
	
	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Plug Information\n");
		
	// get music plug info (verified & working)
	request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
					| (0xC0 << 8) | 0xFF;
	response = cParent->avcExecuteTransaction(request, 1, 4);
	
	if (response != NULL) {
		unsigned char *buffer=(unsigned char *) response;
		buffer += 4;
		unsigned char nb_items=*buffer;
		buffer += 1;
		for (unsigned int i=0;i<nb_items;i++) {
			unsigned int nb_output_plugs=CORRECT_INTEGER_ENDIANNESS(*((unsigned int *)(buffer + 3)));
			unsigned int nb_input_plugs=CORRECT_INTEGER_ENDIANNESS(*((unsigned int *)(buffer + 1)));
			debugPrint(DEBUG_LEVEL_SUBUNIT," Music plug type 0x%02X: %d input plugs / %d output plugs\n",*buffer,nb_input_plugs,nb_output_plugs)
			buffer +=5;
		}
	}

}


AvDeviceMusicSubunit::AvDeviceMusicSubunit(AvDevice *parent, unsigned char  id) : AvDeviceSubunit(parent,0x0C,id)
{
	debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: id=%02X constructor\n",id);
	bValid=false;
	
	// parse descriptors
	cStatusDescriptor=NULL;
	cIdentifierDescriptor=NULL;
	
	cStatusDescriptor = new AvMusicStatusDescriptor(parent,id);
	
	if(cStatusDescriptor) {
		cStatusDescriptor->printCapabilities();
	} else {
		debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: could not create AvMusicStatusDescriptor object\n");
	}
	
	cIdentifierDescriptor = new AvMusicIdentifierDescriptor(parent,id);
	if(cIdentifierDescriptor) {
		cIdentifierDescriptor->printCapabilities();
		
	} else {
		debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: could not create AvMusicIdentifierDescriptor object\n");
	}
	
	bValid=true;	
	
}

AvDeviceMusicSubunit::~AvDeviceMusicSubunit()
{
	if(cStatusDescriptor) delete cStatusDescriptor;
	if(cIdentifierDescriptor) delete cIdentifierDescriptor;
}
