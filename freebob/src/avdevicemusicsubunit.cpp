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


void AvDeviceMusicSubunit::test() {
	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;
	
	request[0] = AVC1394_CTYPE_STATUS | ((iTarget & 0x1F) << 19) | ((iId & 0x07) << 16)
					| AVC1394_COMMAND_PLUG_INFO | 0x00;
	request[1] = 0xFFFFFFFF;
	response = cParent->avcExecuteTransaction(request, 2, 2);
	if (response != NULL) {
		iNbDestinationPlugs= (unsigned char) ((response[1]>>24) & 0xff);
		iNbSourcePlugs= (unsigned char) ((response[1]>>16) & 0xff);
	}
	debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceSubunit: %d source plugs, %d destination plugs\n",iNbSourcePlugs,iNbDestinationPlugs);
	

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
