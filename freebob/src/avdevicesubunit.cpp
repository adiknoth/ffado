/* avdevicesubunit.cpp
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


void AvDeviceSubunit::test() {

}


AvDeviceSubunit::AvDeviceSubunit(AvDevice *parent, unsigned char target, unsigned char  id)
{
	unsigned char byte;
	quadlet_t request[6];
	quadlet_t *response;
	
	cParent=parent;
	iTarget=target;
	iId=id;
	iNbDestinationPlugs=0;
	iNbSourcePlugs=0;
	
	debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceSubunit target=%02X id=%02X created\n",target,id);
	
	if (!cParent) {
		debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceSubunit: parent == NULL!\n");
		bValid=false;
		return;
	}
	
	bValid=true;
	
	// check the number of I/O plugs
	
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

AvDeviceSubunit::~AvDeviceSubunit()
{

}

bool AvDeviceSubunit::isValid()
{
	return bValid;
}

unsigned char AvDeviceSubunit::getNbDestinationPlugs() {
	return iNbDestinationPlugs;
}

unsigned char AvDeviceSubunit::getNbSourcePlugs() {
	return iNbSourcePlugs;
}

