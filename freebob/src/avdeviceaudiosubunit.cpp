/* avdeviceaudiosubunit.cpp
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
#include "avdeviceaudiosubunit.h"
#include "avaudiosubunitidentifierdescriptor.h"

AvDeviceAudioSubunit::AvDeviceAudioSubunit(AvDevice *parent, unsigned char  id) : AvDeviceSubunit(parent,0x01,id)
{
    	setDebugLevel( DEBUG_LEVEL_ALL );
	debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceAudioSubunit id=%02X created\n",id);

	bValid=false;

	// parse descriptors
	//cStatusDescriptor=NULL;
	cIdentifierDescriptor=NULL;
/*
	cStatusDescriptor = new AvMusicStatusDescriptor(parent,id);

	if(cStatusDescriptor) {
		cStatusDescriptor->printCapabilities();
	} else {
		debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceMusicSubunit: could not create AvMusicStatusDescriptor object\n");
	}
*/
	cIdentifierDescriptor = new AvAudioSubunitIdentifierDescriptor(parent,id);
	if(cIdentifierDescriptor) {
		cIdentifierDescriptor->printCapabilities();

	} else {
		debugPrint (DEBUG_LEVEL_SUBUNIT,"AvDeviceAudioSubunit: could not create AvAudioSubunitIdentifierDescriptor object\n");
	}

	bValid=true;


}

AvDeviceAudioSubunit::~AvDeviceAudioSubunit()
{
	if(cIdentifierDescriptor) {
		delete cIdentifierDescriptor;
	}
}
