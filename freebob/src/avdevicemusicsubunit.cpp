/* avdevicemusicsubunit.cpp
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
#include <errno.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include "avdevice.h"
#include "avdevicesubunit.h"
#include "avdevicemusicsubunit.h"
#include "avmusicstatusdescriptor.h"
#include "avmusicidentifierdescriptor.h"
#include "avpluginfoblock.h"
#include "avclusterinfoblock.h"

#define CORRECT_INTEGER_ENDIANNESS(value) (((((value)>>8) & 0xFF)) | ((((value)) & 0xFF)<<8))

void AvDeviceMusicSubunit::test() {
//	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;

	unsigned char ipcr=0;

	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Input Select status (iso channels):\n");
	for(ipcr=0;ipcr<0x1E;ipcr++) {
		request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
						| (0x1B << 8) | 0xFF;
		//request[1] = ((iTarget & 0x1F) << 27) | ((iId & 0x07) << 24) | 0x00FF7F;
		request[1]=0xFFFFFFFF;
		request[2]= (ipcr<<24)| 0xFFFEFF;
		response = Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 3, 4);

		if ((response != NULL) && ((response[0]&0xFF000000)==0x0C000000)) {
			unsigned char output_status=(response[1]&0xF0000000) >> 28;
			unsigned char output_plug=(response[1]&0xFF);
			unsigned char input_plug=((response[2]>>24)&0xFF);

			unsigned int node_id=((response[1]>>8)&0xFFFF);
			unsigned int signal_destination=((response[2]>>8)&0xFFFF);
			debugPrint(DEBUG_LEVEL_SUBUNIT,"  iPCR %d: output_status=0x%01X,node_id=%02d,output_plug=0x%02X,input_plug=0x%02X,signal_destination=0x%04X\n",ipcr,output_status,node_id,output_plug,input_plug,signal_destination);
		}
	}
}


void AvDeviceMusicSubunit::printMusicPlugConfigurations() {
//	unsigned char byte;
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
		response = Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 1, 4);
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
		response = Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 1, 4);
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
//	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;

	debugPrint(DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: Plug Information\n");

	// get music plug info (verified & working)
	request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
					| (0xC0 << 8) | 0xFF;
	response = Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 1, 4);

	if (response != NULL) {
		unsigned char *buffer=(unsigned char *) response;
		buffer += 4;
		unsigned char nb_items=*buffer;
		buffer += 1;
		for (unsigned int i=0;i<nb_items;i++) {
			unsigned int nb_output_plugs=CORRECT_INTEGER_ENDIANNESS(*((unsigned int *)(buffer + 3)));
			unsigned int nb_input_plugs=CORRECT_INTEGER_ENDIANNESS(*((unsigned int *)(buffer + 1)));
			debugPrint(DEBUG_LEVEL_SUBUNIT," Music plug type 0x%02X: %d input plugs / %d output plugs\n",*buffer,nb_input_plugs,nb_output_plugs);
			buffer +=5;
		}
	}

}

void AvDeviceMusicSubunit::printSourcePlugConnections(unsigned char plug) {
	if(cStatusDescriptor) {
		AvPlugInfoBlock *plugInfo=cStatusDescriptor->getSourcePlugInfoBlock(plug);

		if(plugInfo) {
			plugInfo->printConnections();
		} else {
			debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: could not find source plug 0x%02X in the descriptor.\n",plug);
		}


	} else {
		debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: no Status Descriptor present!\n");
	}

}



AvDeviceMusicSubunit::AvDeviceMusicSubunit(AvDevice *parent, unsigned char  id) : AvDeviceSubunit(parent,0x0C,id)
{
    setDebugLevel( DEBUG_LEVEL_ALL );

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
