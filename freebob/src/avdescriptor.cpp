/* avdescriptor.cpp
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

#include "avdescriptor.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <netinet/in.h>
#include <assert.h>

/* should probably be attached to an AVC device,
   a descriptor is attached to an AVC device anyway.

   a descriptor also always has a type
*/

AvDescriptor::AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type)
    : cParent( parent )
    , iType( type )
    , aContents( 0 )
    , bLoaded( false )
    , bOpen( false )
    , bValid( false )  // don't know yet what I'm going to do with this in the generic descriptor
    , iAccessType( 0 )
    , iLength( 0 )
    , qTarget( target )
{
    setDebugLevel( DEBUG_LEVEL_ALL );
}

AvDescriptor::AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type, unsigned int list_id, unsigned int list_id_size) 
    : cParent( parent )
    , iType( type )
    , aContents( 0 )
    , bLoaded( false )
    , bOpen( false )
    , bValid( false )  // don't know yet what I'm going to do with this in the generic descriptor
    , iAccessType( 0 )
    , iLength( 0 )
    , qTarget( target )
    , iListId( list_id )
    , iListIdSize( list_id_size )
{
    setDebugLevel( DEBUG_LEVEL_ALL );
}

AvDescriptor::AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type, unsigned int list_id, unsigned int list_id_size,
                                                     unsigned int object_id, unsigned int object_id_size,
                                                     unsigned int position, unsigned int position_size)
    : cParent( parent )
    , iType( type )
    , aContents( 0 )
    , bLoaded( false )
    , bOpen( false )
    , bValid( false )  // don't know yet what I'm going to do with this in the generic descriptor
    , iAccessType( 0 )
    , iLength( 0 )
    , qTarget( target )
    , iListId( list_id )
    , iListIdSize( list_id_size )
	, iObjectId( object_id )
	, iObjectIdSize( object_id_size) 
	, iPosition( position )
	, iPositionSize( position_size) 
{
    setDebugLevel( DEBUG_LEVEL_ALL );
}

AvDescriptor::~AvDescriptor()
{
	if (bOpen) {
		Close();
	}
	if (aContents) {
		delete aContents;
	}

}

int AvDescriptor::ConstructDescriptorSpecifier(quadlet_t *request) {
	int specifier_len=0;
	
	assert(request);
	
	switch(iType) { // only a subset of the spec is supported
		case AVC_DESCRIPTOR_SPECIFIER_TYPE_IDENTIFIER:
			request[1] = 0xFFFFFFFF;
			specifier_len=1;
			break;
		case AVC_DESCRIPTOR_SPECIFIER_TYPE_LIST_BY_ID:
			request[1] = 0x00000000;
			switch (iListIdSize) {
				case 1:
					request[1]=request[1] | ((iListId & 0xFF) << 24);
					specifier_len=2;
					break;				
				case 2:
					request[1]=request[1] | ((iListId & 0xFFFF) << 16);
					specifier_len=3;
					break;				
				case 3:
					request[1]=request[1] | ((iListId & 0xFFFFFF)<<8);
					specifier_len=4;
					break;				
				default: // we don't support more than 3 byte long ID's (yet?)
					specifier_len=0;
					debugError("Invalid list_id size!\n");
					break;				
			}
		
			break;
		case AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_POSITION_IN_LIST:
			request[1] = 0x00000000;
			switch (iListIdSize) {
				case 1:
					request[1]=request[1] | ((iListId & 0xFF) << 24);
					switch(iPositionSize) {
						case 1:
							request[1]=request[1] | ((iPosition & 0xFF) << 16);
							specifier_len=3;
							break;
						case 2:
							request[1]=request[1] | ((iPosition & 0xFFFF)<<8);
							specifier_len=4;
							break;
						case 3:
							request[1]=request[1] | ((iPosition & 0xFFFFFF));
							specifier_len=5;
							break;
						default:
							debugError("Invalid position size!\n");
							specifier_len=0;
							break;
					}
					break;				
				case 2:
					request[1]=request[1] | ((iListId & 0xFFFF) << 16);
					switch(iPositionSize) {
						case 1:
							request[1]=request[1] | ((iPosition & 0xFF)<<8);
							specifier_len=4;
							break;
						case 2:
							request[1]=request[1] | (((iPosition) & 0xFFFF));
							specifier_len=5;
							break;
						case 3:
							request[1]=request[1] | (((iPosition >> 8) & 0xFFFF));
							request[2]=0x00000000 | ((iPosition & 0xFF) << 24);
							specifier_len=6;
							break;
						default:
							specifier_len=0;
							debugError("Invalid position size!\n");
							break;
					}
					break;				
				case 3:
					request[1]=request[1] | ((iListId & 0xFFFFFF)<<8);
					switch(iPositionSize) {
						case 1:
							request[1]=request[1] | ((iPosition & 0xFF));
							specifier_len=5;
							break;
						case 2:
							request[1]=request[1] | (((iPosition >> 8) & 0xFF));
							request[2]=0x00000000 | ((iPosition & 0xFF) << 24);
							specifier_len=6;
							break;
						case 3:
							request[1]=request[1] | (((iPosition >> 16) & 0xFF));
							request[2]=0x00000000 | ((iPosition & 0xFFFF) << 16);
							specifier_len=7;
							break;
						default:
							specifier_len=0;
							debugError("Invalid position size!\n");
							break;
					}
					break;				
				default: // we don't support more than 3 byte long ID's (yet?)
					specifier_len=0;
					debugError("Invalid object_id size!\n");
					break;				
			}
			break;
			
		case AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_ID:
			request[1] = 0x00000000;
			switch (iObjectIdSize) {
				case 1:
					request[1]=request[1] | ((iObjectId & 0xFF) << 24);
					specifier_len=2;
					break;				
				case 2:
					request[1]=request[1] | ((iObjectId & 0xFFFF) << 16);
					specifier_len=3;
					break;				
				case 3:
					request[1]=request[1] | ((iObjectId & 0xFFFFFF)<<8);
					specifier_len=4;
					break;				
				default: // we don't support more than 3 byte long ID's (yet?)
					specifier_len=0;
					debugError("Invalid object_id size!\n");
					break;				
			}
			break;
		default:
			specifier_len=1;
			request[1] = 0xFFFFFFFF;
			break;
	}
	return specifier_len;
	
}

void AvDescriptor::OpenReadOnly() {
	quadlet_t *response;
	quadlet_t request[5];
	int specifier_len;
	
	if (!cParent) {
		return;
	}

	if(isOpen()) {
		Close();
	}

	request[0] = AVC1394_CTYPE_CONTROL | qTarget
					| AVC1394_COMMAND_OPEN_DESCRIPTOR | (iType & 0xFF);
	
	// construct the description specifier structure
	specifier_len=ConstructDescriptorSpecifier(request);
	
	// set the subfunction
	switch(specifier_len % 4) { 
	case 0:
		request[specifier_len/4] &= 0xFFFFFF00;
		request[specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY);
		break;
	case 1:
		request[1+specifier_len/4] &= 0x00FFFFFF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY << 24);
		break;
	case 2:
		request[1+specifier_len/4] &= 0xFF00FFFF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY << 16);
		break;
	case 3:
		request[1+specifier_len/4] &= 0xFFFF00FF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY << 8);
		break;
	}	
	
	// set the subfunction
	//request[1] |= 0x00FFFFFF | (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY << 24);

	
	response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 2, 2);

	if (response && ((response[0]&0xFF000000)==AVC1394_RESPONSE_ACCEPTED)) {
		bOpen=true;
		iAccessType=AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY;
	} else {
		// try and figure out the problem:
		request[0] = AVC1394_CTYPE_STATUS | qTarget
						| AVC1394_COMMAND_OPEN_DESCRIPTOR | (iType & 0xFF);
						
		specifier_len=ConstructDescriptorSpecifier(request);
		
		// set the subfunction
		switch(specifier_len % 4) { 
		case 0:
			request[specifier_len/4] &= 0xFFFFFF00;
			request[specifier_len/4] |= (0xFF);
			request[specifier_len/4+1] = (0xFFFFFF00);
			break;
		case 1:
			request[1+specifier_len/4] &= 0x00000000;
			request[1+specifier_len/4] |= (0xFF << 24) | 0xFFFFFF;
			break;
		case 2:
			request[1+specifier_len/4] &= 0xFF000000;
			request[1+specifier_len/4] |= (0xFF << 16) | 0xFFFF;
			request[1+specifier_len/4+1] = (0xFF << 24);
			break;
		case 3:
			request[1+specifier_len/4] &= 0xFFFF0000;
			request[1+specifier_len/4] |= (0xFF << 8) | 0xFF;
			request[1+specifier_len/4+1] = (0xFFFF << 16);
			break;
		}				
		//request[1] |= 0x00FFFFFF | (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE << 24);
		//fprintf(stderr, "Opening descriptor\n");
	
		response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, specifier_len/4+2, specifier_len/4+2);
	
		Close();
		bOpen=false;
	}
}

void AvDescriptor::OpenReadWrite() {
	quadlet_t *response;
	quadlet_t request[5];
	int specifier_len;
	
	if (!cParent) {
		return;
	}

	if(isOpen()) {
		Close();
	}

	request[0] = AVC1394_CTYPE_CONTROL | qTarget
					| AVC1394_COMMAND_OPEN_DESCRIPTOR | (iType & 0xFF);
					
	specifier_len=ConstructDescriptorSpecifier(request);
	
	// set the subfunction
	switch(specifier_len % 4) { 
	case 0:
		request[specifier_len/4] &= 0xFFFFFF00;
		request[specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE);
		break;
	case 1:
		request[1+specifier_len/4] &= 0x00FFFFFF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE << 24);
		break;
	case 2:
		request[1+specifier_len/4] &= 0xFF00FFFF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE << 16);
		break;
	case 3:
		request[1+specifier_len/4] &= 0xFFFF00FF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE << 8);
		break;
	}				
	//request[1] |= 0x00FFFFFF | (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE << 24);
	//fprintf(stderr, "Opening descriptor\n");

    response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 2, 2);

	if (response && ((response[0]&0xFF000000)==AVC1394_RESPONSE_ACCEPTED)) {
		bOpen=true;
		iAccessType=AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE;
	} else {
		// try and figure out the problem:
		request[0] = AVC1394_CTYPE_STATUS | qTarget
						| AVC1394_COMMAND_OPEN_DESCRIPTOR | (iType & 0xFF);
						
		specifier_len=ConstructDescriptorSpecifier(request);
		
		// set the subfunction
		switch(specifier_len % 4) { 
		case 0:
			request[specifier_len/4] &= 0xFFFFFF00;
			request[specifier_len/4] |= (0xFF);
			request[specifier_len/4+1] = (0xFFFFFF00);
			break;
		case 1:
			request[1+specifier_len/4] &= 0x00000000;
			request[1+specifier_len/4] |= (0xFF << 24) | 0xFFFFFF;
			break;
		case 2:
			request[1+specifier_len/4] &= 0xFF000000;
			request[1+specifier_len/4] |= (0xFF << 16) | 0xFFFF;
			request[1+specifier_len/4+1] = (0xFF << 24);
			break;
		case 3:
			request[1+specifier_len/4] &= 0xFFFF0000;
			request[1+specifier_len/4] |= (0xFF << 8) | 0xFF;
			request[1+specifier_len/4+1] = (0xFFFF << 16);
			break;
		}				
		//request[1] |= 0x00FFFFFF | (AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE << 24);
		//fprintf(stderr, "Opening descriptor\n");
	
		response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, specifier_len/4+2, specifier_len/4+2);
	
	
		Close();
		bOpen=false;
	}
}

bool AvDescriptor::canWrite() {
	if(bOpen && (iAccessType==AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE) && cParent) {
		return true;
	} else {
		return false;
	}
}

void AvDescriptor::Close() {
	quadlet_t *response;
	quadlet_t request[5];
	
	int specifier_len;
	
	if (!cParent) {
		return;
	}
	request[0] = AVC1394_CTYPE_CONTROL | qTarget
					| AVC1394_COMMAND_OPEN_DESCRIPTOR | (iType & 0xFF);
	
	specifier_len=ConstructDescriptorSpecifier(request);
	
	// set the subfunction
	switch(specifier_len % 4) { 
	case 0:
		request[specifier_len/4] &= 0xFFFFFF00;
		request[specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_CLOSE);
		break;
	case 1:
		request[1+specifier_len/4] &= 0x00FFFFFF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_CLOSE << 24);
		break;
	case 2:
		request[1+specifier_len/4] &= 0xFF00FFFF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_CLOSE << 16);
		break;
	case 3:
		request[1+specifier_len/4] &= 0xFFFF00FF;
		request[1+specifier_len/4] |= (AVC_DESCRIPTOR_SUBFUNCTION_CLOSE << 8);
		break;
	}	
	
	
	//fprintf(stderr, "Opening descriptor\n");

        response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 2, 2);

	if (response && ((response[0]&0xFF000000)==AVC1394_RESPONSE_ACCEPTED)) {
		bOpen=false;
		iAccessType=AVC_DESCRIPTOR_SUBFUNCTION_CLOSE;
	}
}

/* Load the descriptor
 * no error checking yet
 */

#define MAX_RETRIES 10

void AvDescriptor::Load() {
	quadlet_t *response;
	quadlet_t request[8];
	unsigned int i=0;
	unsigned int bytes_read=0;
	unsigned int bytes_read_this_loop=0;
	unsigned char *databuffer;
	unsigned char read_result_status;
	unsigned int data_length_read=0;
	int specifier_len=0;
	int bytes_to_copy;
	quadlet_t *databuffer_quadlets;
	size_t aContents_size;
	
	int retries=0;
	int data_length=0;
	int address=0;

	if (!cParent) {
		return;
	}

	/* First determine the descriptor length */
	request[0] = AVC1394_CTYPE_CONTROL | qTarget
					| AVC1394_COMMAND_READ_DESCRIPTOR | (iType & 0xFF);
	
	
	data_length=8;
	address=0;
	specifier_len=ConstructDescriptorSpecifier(request);
	
	switch (specifier_len) { // can be generalised I guess, but not yet...
		case 0:
		break;
		
		case 1:
			request[1] = 0xFFFF0000 | (data_length);
			request[2] = (((address)&0xFFFF) << 16) |0x0000FFFF;
		break;
		
		case 2:
			request[1] |= 0x00FFFF00 | ((data_length>>8) & 0xFF);
			request[2] = (((data_length) & 0xFF)<<24) | (((address)&0xFFFF) << 8) | 0x000000FF;
		break;
		
		case 3:
			request[1] |= 0x0000FFFF;
			request[2] = ((data_length & 0xFFFF)<<16) | (((address)&0xFFFF));
		break;
		case 4:
			request[1] |= 0x000000FF;
			request[2] = 0xFF000000 | ((data_length & 0xFFFF)<<8) | (((address>>8)&0xFF));
			request[3] = (((address)&0xFF)<<24) | 0x00FFFFFF;
		break;
		case 5:
		
			request[2] = 0xFFFF0000 | (data_length);
			request[3] = (((address)&0xFFFF) << 16) |0x0000FFFF;
		break;
		case 6:
			request[2] |= 0x00FFFF00 | ((data_length>>8) & 0xFF);
			request[3] = (((data_length) & 0xFF)<<24) | (((address)&0xFFFF) << 8) | 0x000000FF;
		break;
		case 7:
			request[2] |= 0x0000FFFF;
			request[3] = ((data_length & 0xFFFF)<<16) | (((address)&0xFFFF));
		break;
	}

	response=0;
	retries=0;
		
	while (((response == 0) || ((response[0]  & 0xFF000000) != 0x09000000)) && retries<MAX_RETRIES) {
		response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 3, 4);
		retries++;
	}
	if ((response == 0) || ((response[0]  & 0xFF000000) !=0x09000000)) {
		debugError("Could not read descriptor! Max retries exceeded.");
		bLoaded=false;
		return;
	}
	
	// this is a more general approach
	// assumes that integer division is floored (not rounded)
	switch(specifier_len % 4) { 
	case 0:
		iLength=((response[2+specifier_len/4]>>8) & 0xFFFF);
		break;
	case 1:
		iLength=response[2+specifier_len/4] & 0xFFFF;
		break;
	case 2:
		iLength=(((response[2+specifier_len/4]) & 0xFF)<<8)|(((response[2+(specifier_len/4)+1]) >> 24) & 0xFF);
		break;
	case 3:
		iLength=(((response[2+(specifier_len/4)+1]) >> 16) & 0xFFFF);
		break;
	}
	
	debugPrint(DEBUG_LEVEL_DESCRIPTOR,"Descriptor length=0x%04X %d (0x%04X) %d %d ",iLength,iLength,(response[1]&0xFFFF),specifier_len/4,specifier_len);

	// now get the rest of the descriptor
	// allocate space for the decriptor
	// we want a multiple of sizeof(quadlet_t's)
	aContents_size = (iLength/sizeof(quadlet_t)+1)*sizeof(quadlet_t);
	aContents=new unsigned char[aContents_size];

	/* there is a gap in the read data, because of the two bytes
	 * that are before the third quadlet (why did 1394TA do that?)
	 *
	 * proposed solution:
	 * 1) the first read is OK, because the length of the descriptor is in these two bytes,
	 *    so execute a read from address 0 with length=iLength
	 * 2) process the bytes starting from response[3]
	 * 3) the next reads should start from the end of the previous read minus 2. this way we
	 *    can discard these two problem bytes, because they are already read in the previous
	 *    run.
	 */

	// first read
	int data_buffer_boundary_offset=0;
	
	if(bytes_read<iLength) {
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,".");
		// apparently the lib modifies the request, so redefine it completely
		request[0] = AVC1394_CTYPE_CONTROL | qTarget
						| AVC1394_COMMAND_READ_DESCRIPTOR | (iType & 0xFF);
		// the amount of bytes we want to read is:
		//  total descriptor length 
		
		data_length=iLength;
		if (data_length < 4) {
			data_length=4;
		}
		
		
		
		specifier_len=ConstructDescriptorSpecifier(request);
		
		// plz see below if you want to understand this
		// basically what this does is skip the length field (first two bytes)
		// in the descriptor, with arbitrary specifier_len values.
		switch(specifier_len %4) {
			case 0:
				//address=1;
				debugError("unsupported");
				address=0;
				
				break;
			case 1:
				address=0;
				break;
			case 2:
				address=1;
				break;
			case 3:
				address=2;
			
				break;
		}
		//address=(((4-((3+specifier_len+1+1+2+2)%4))%4)-2)+0;
		
		switch (specifier_len) { // can be generalised I guess, but not yet...
			case 0:
			break;
			
			case 1:
				request[1] = 0xFFFF0000 | (data_length);
				request[2] = (((address)&0xFFFF) << 16) |0x0000FFFF;
			break;
			
			case 2:		
				request[1] |= 0x00FFFF00 | ((data_length>>8) & 0xFF);
				request[2] = (((data_length) & 0xFF)<<24) | (((address)&0xFFFF) << 8) | 0x000000FF;
			break;
			
			case 3:
				request[1] |= 0x0000FFFF;
				request[2] = ((data_length & 0xFFFF)<<16) | (((address)&0xFFFF));
			break;
			case 4:
				request[1] |= 0x000000FF;
				request[2] = 0xFF000000 | ((data_length & 0xFFFF)<<8) | (((address>>8)&0xFF));
				request[3] = (((address)&0xFF)<<24) | 0x00FFFFFF;
			break;
			case 5:
			
				request[2] = 0xFFFF0000 | (data_length);
				request[3] = (((address)&0xFFFF) << 16) |0x0000FFFF;
			break;
			case 6:
				request[2] |= 0x00FFFF00 | ((data_length>>8) & 0xFF);
				request[3] = (((data_length) & 0xFF)<<24) | (((address)&0xFFFF) << 8) | 0x000000FF;
			break;
			case 7:
				request[2] |= 0x0000FFFF;
				request[3] = ((data_length & 0xFFFF)<<16) | (((address)&0xFFFF));
			break;
		}

		response=0;
		retries=0;
		
		while (((response == 0) || ((response[0]  & 0xFF000000) != 0x09000000)) && retries<MAX_RETRIES) {
			response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 3, 3+data_length/sizeof(quadlet_t));
			retries++;
		}
		if ((response == 0) || ((response[0] & 0xFF000000) !=0x09000000)) {
			debugError("Could not read descriptor! Max retries exceeded.");
			bLoaded=false;
			return;
		}
			hexDumpQuadlets(response,10);
		
		switch(specifier_len % 4) { 
		case 0:
			data_length_read=((response[1+specifier_len/4]>>8) & 0xFFFF);
			read_result_status=((response[specifier_len/4])&0xFF);
			break;
		case 1:
			data_length_read=response[1+specifier_len/4] & 0xFFFF;
			read_result_status=((response[1+specifier_len/4]>>24)&0xFF);
			break;
		case 2:
			data_length_read=(((response[1+specifier_len/4]) & 0xFF)<<8)|(((response[1+(specifier_len/4)+1]) >> 24) & 0xFF);
			read_result_status=((response[1+specifier_len/4]>>16)&0xFF);
			break;
		case 3:
			data_length_read=(((response[1+(specifier_len/4)+1]) >> 16) & 0xFFFF);
			read_result_status=((response[1+specifier_len/4]>>8)&0xFF);
			break;
		}
		/* This is a very messy part, let's describe it a bit:
		 * 
		 * The data we get from the avcExecuteTransaction is an array of quadlet_t's in host order
		 * what we want is an byte adressable array aContents. Therefore we have to convert the
		 * host order into the byte addressable (big endian).
		 * To make things worse, the specifier_len's can be any size (also non-even)
		 * We solve this by overlapping between reads so that the data we need
		 * starts on a quadlet_t boundary
		 */
		
		// this is addressed in bytes, so accounting for the specifier_len is easier.
		databuffer=(unsigned char *)(response);
		// 3 bytes for the response header
		// the length of the specifier (incl type)
		// 1 byte for read_result_status
		// 1 reserved byte
		// 2 bytes for data length
		// 2 bytes for address
		databuffer+=3+specifier_len+1+1+2+2;
		
		// we want to start copying the buffer on a quadlet boundary
		// in order to facilitate endian conversion.
		// this means that we have to add (4-((3+specifier_len+1+1+2+2)%4))%4 bytes extra
		data_buffer_boundary_offset=(4-((3+specifier_len+1+1+2+2)%4))%4;
		databuffer += data_buffer_boundary_offset;
		
		//data_length_read=(response[1]&0xFFFF);
		//read_result_status=((response[1]>>24)&0xFF);
		//databuffer=(unsigned char *)(response+3);

		bytes_to_copy=0;
		bytes_to_copy=data_length_read-data_buffer_boundary_offset;
		if(bytes_read+bytes_to_copy>iLength) {
			bytes_to_copy=iLength-bytes_read;
		}
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"data_length_read %d, data_buffer_boundary_offset %d, bytes_to_copy %d, specifier_len %d",data_length_read,data_buffer_boundary_offset, bytes_to_copy, specifier_len);
		
		databuffer_quadlets=(quadlet_t *)databuffer;
		
		bytes_read_this_loop=0;
		
		for (i=0;i<bytes_to_copy/sizeof(quadlet_t);i++) {
			quadlet_t *dst=(quadlet_t*)(aContents+bytes_read+i*sizeof(quadlet_t));
			if(!((unsigned char *)dst<=(aContents+aContents_size))) {
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"%p %p",dst,aContents+aContents_size);
				
				assert(0);
			};
			*dst=ntohl(*(databuffer_quadlets + i));
			bytes_read_this_loop+=sizeof(quadlet_t);
		}
		
		if(bytes_to_copy-bytes_read_this_loop > 0) {
			quadlet_t *dst=(quadlet_t*)(aContents+bytes_read+(bytes_to_copy/sizeof(quadlet_t))*sizeof(quadlet_t));
			assert(bytes_to_copy/sizeof(quadlet_t) == i);
			if(!((unsigned char *)dst<=(aContents+aContents_size))) {
				debugPrint(DEBUG_LEVEL_DESCRIPTOR,"%p %p",dst,aContents+aContents_size);
				
				assert(0);
			};
			*dst=ntohl(*(databuffer_quadlets + i));
			bytes_read_this_loop+=sizeof(quadlet_t);		
		}
		
		if(bytes_read+bytes_read_this_loop<iLength) {
			bytes_read+=bytes_read_this_loop;
		} else {
			bytes_read=iLength;
		}
		// OLD: the buffer starting at databuffer is data_buffer_boundary_offset bytes smaller that the amount of bytes read
		/*
		for (i=0;(i<data_length_read-data_buffer_boundary_offset) && (bytes_read < iLength);i++) {
			*(aContents+bytes_read)=(*(databuffer+i));
			bytes_read++;
		}*/

	}

	// now do the remaining reads
	while(bytes_read<iLength) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"bytes_read %d, iLength %d ",bytes_read,iLength);
		debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,".");
		// apparently the lib modifies the request, so redefine it completely
		request[0] = AVC1394_CTYPE_CONTROL | qTarget
						| AVC1394_COMMAND_READ_DESCRIPTOR | (iType & 0xFF);
		// the amount of bytes we want to read is:
		//  (total descriptor length - number of bytes already read) + data_buffer_boundary_offset overlap with previous read

		data_length=(iLength-bytes_read+data_buffer_boundary_offset);
		if (data_length < 4) {
			data_length=4;
		}
		specifier_len=ConstructDescriptorSpecifier(request);
		
		switch(specifier_len %4) {
			case 0:
				//address=1;
				debugError("unsupported");
				address=0;
				
				break;
			case 1:
				address=0+bytes_read;
				break;
			case 2:
				address=1+bytes_read;
				break;
			case 3:
				address=2+bytes_read;
			
				break;
		}
		//address=((4-((3+specifier_len+1+1+2+2)%4))%4-2)+bytes_read;
		
		switch (specifier_len) { // can be generalised I guess, but not yet...
			case 0:
			break;
			
			case 1:
				request[1] = 0xFFFF0000 | (data_length);
				request[2] = (((address)&0xFFFF) << 16) |0x0000FFFF;
			break;
			
			case 2:		
				request[1] |= 0x00FFFF00 | ((data_length>>8) & 0xFF);
				request[2] = (((data_length) & 0xFF)<<24) | (((address)&0xFFFF) << 8) | 0x000000FF;
			break;
			
			case 3:
				request[1] |= 0x0000FFFF;
				request[2] = ((data_length & 0xFFFF)<<16) | (((address)&0xFFFF));
			break;
			case 4:
				request[1] |= 0x000000FF;
				request[2] = 0xFF000000 | ((data_length & 0xFFFF)<<8) | (((address>>8)&0xFF));
				request[3] = (((address)&0xFF)<<24) | 0x00FFFFFF;
			break;
			case 5:
			
				request[2] = 0xFFFF0000 | (data_length);
				request[3] = (((address)&0xFFFF) << 16) |0x0000FFFF;
			break;
			case 6:
				request[2] |= 0x00FFFF00 | ((data_length>>8) & 0xFF);
				request[3] = (((data_length) & 0xFF)<<24) | (((address)&0xFFFF) << 8) | 0x000000FF;
			break;
			case 7:
				request[2] |= 0x0000FFFF;
				request[3] = ((data_length & 0xFFFF)<<16) | (((address)&0xFFFF));
			break;
		}				
		//debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,"%08X %08X %08X \n", request[0],request[1],request[2]);
	
		response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 3, 3+data_length/sizeof(quadlet_t));
		
		if (((response[0]>>24)&0xFF) == 0x09) {
			switch(specifier_len % 4) { 
			case 0:
				data_length_read=((response[1+specifier_len/4]>>8) & 0xFFFF);
				read_result_status=((response[specifier_len/4])&0xFF);
				break;
			case 1:
				data_length_read=response[1+specifier_len/4] & 0xFFFF;
				read_result_status=((response[1+specifier_len/4]>>24)&0xFF);
				break;
			case 2:
				data_length_read=(((response[1+specifier_len/4]) & 0xFF)<<8)|(((response[1+(specifier_len/4)+1]) >> 24) & 0xFF);
				read_result_status=((response[1+specifier_len/4]>>16)&0xFF);
				break;
			case 3:
				data_length_read=(((response[1+(specifier_len/4)+1]) >> 16) & 0xFFFF);
				read_result_status=((response[1+specifier_len/4]>>8)&0xFF);
				break;
			}
			/* This is a very messy part, let's describe it a bit:
			* 
			* The data we get from the avcExecuteTransaction is an array of quadlet_t's in host order
			* what we want is an byte adressable array aContents. Therefore we have to convert the
			* host order into the byte addressable (big endian).
			* To make things worse, the specifier_len's can be any size (also non-even)
			* We solve this by overlapping between reads so that the data we need
			* starts on a quadlet_t boundary
			*/
			
			// this is addressed in bytes, so accounting for the specifier_len is easier.
			databuffer=(unsigned char *)(response);
			// 3 bytes for the response header
			// the length of the specifier (incl type)
			// 1 byte for read_result_status
			// 1 reserved byte
			// 2 bytes for data length
			// 2 bytes for address
			databuffer+=3+specifier_len+1+1+2+2;
			
			// we want to start copying the buffer on a quadlet boundary
			// in order to facilitate endian conversion.
			// this means that we have to add 4-((3+specifier_len+1+1+2+2)%4) bytes extra
			data_buffer_boundary_offset=(4-((3+specifier_len+1+1+2+2)%4))%4;
			databuffer += data_buffer_boundary_offset;
			
			//data_length_read=(response[1]&0xFFFF);
			//read_result_status=((response[1]>>24)&0xFF);
			//databuffer=(unsigned char *)(response+3);
	
			bytes_to_copy=0;
			bytes_to_copy=data_length_read-data_buffer_boundary_offset;
			//debugPrint(DEBUG_LEVEL_DESCRIPTOR,"data_length_read %d, data_buffer_boundary_offset %d, bytes_to_copy %d, specifier_len %d",data_length_read,data_buffer_boundary_offset, bytes_to_copy, specifier_len);

			
			databuffer_quadlets=(quadlet_t *)databuffer;
			bytes_read_this_loop=0;
			
			for (i=0;i<bytes_to_copy/sizeof(quadlet_t);i++) {
				quadlet_t *dst=(quadlet_t*)(aContents+bytes_read+i*sizeof(quadlet_t));
				if(!((unsigned char *)dst<=(aContents+aContents_size))) {
					debugPrint(DEBUG_LEVEL_DESCRIPTOR,"%p %p",dst,aContents+aContents_size);
					
					assert(0);
				};
				*dst=ntohl(*(databuffer_quadlets + i));
				bytes_read_this_loop+=sizeof(quadlet_t);
			}
			if(bytes_to_copy-bytes_read_this_loop > 0) {
				quadlet_t *dst=(quadlet_t*)(aContents+bytes_read+(bytes_to_copy/sizeof(quadlet_t))*sizeof(quadlet_t));
				assert(bytes_to_copy/sizeof(quadlet_t) == i);
				if(!((unsigned char *)dst<=(aContents+aContents_size))) {
					debugPrint(DEBUG_LEVEL_DESCRIPTOR,"%p %p",dst,aContents+aContents_size);
					
					assert(0);
				};
				*dst=ntohl(*(databuffer_quadlets + i));
				bytes_read_this_loop+=sizeof(quadlet_t);		
			}
			if(bytes_read+bytes_read_this_loop<iLength) {
				bytes_read+=bytes_read_this_loop;
			} else {
				bytes_read=iLength;
			}
			
			retries=0;
		} else {
			if(retries<MAX_RETRIES) {
				retries++;
			} else {
				debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,"Maximum number of read retries exceeded!\n");
				bLoaded=false;
				delete aContents;
				aContents=NULL;
				return;
			}
		}
		
	}
	debugPrintShort(DEBUG_LEVEL_DESCRIPTOR,"done\n");
	hexDump(aContents,iLength);
	
	bLoaded=true;
}

bool AvDescriptor::isPresent() {
	quadlet_t *response;
	quadlet_t request[2];

	if (!cParent) {
		return false;
	}

	request[0] = AVC1394_CTYPE_STATUS | qTarget | AVC1394_COMMAND_OPEN_DESCRIPTOR | (iType & 0xFF);
	request[1] = 0xFFFFFFFF;
	response =  Ieee1394Service::instance()->avcExecuteTransaction(cParent->getNodeId(), request, 2, 2);

	if (((response[0] & 0xFF000000)==AVC1394_RESPONSE_NOT_IMPLEMENTED) || ((response[1] & 0xFF000000)==0x04)) {
		debugPrint(DEBUG_LEVEL_DESCRIPTOR,"Descriptor not present.\n");
		bValid=false;
		return false;
	}
	return true;
}


bool AvDescriptor::isOpen() {
	// maybe we should check this on the device instead of mirroring locally...
	// after all it can time out
	return bOpen;
}

bool AvDescriptor::isLoaded() {
	return bLoaded;
}

bool AvDescriptor::isValid() {
	return bValid;
}


unsigned int AvDescriptor::getLength() {
	return iLength;
}

unsigned char AvDescriptor::readByte(unsigned int address) {
	if(cParent && bLoaded && aContents) {
		return *(aContents+address);
	} else {
		return 0; // what to do with this?
	}
}

unsigned int AvDescriptor::readWord(unsigned int address) {
	unsigned int word;

	if(cParent && bLoaded && aContents) {
		word=(*(aContents+address)<<8)+*(aContents+address+1);
		return word;
	} else {
		return 0; // what to do with this?
	}
}

unsigned int AvDescriptor::readBuffer(unsigned int address, unsigned int length, unsigned char *buffer) {
	if(cParent && bLoaded && aContents && address < iLength) {
		if(address+length>iLength) {
			length=iLength-address;
		}
		memcpy((void*)buffer, (void*)(aContents+address), length);
		return length;

	} else {
		return 0;
	}
}

