/* avdevice.cpp
 * Copyright (C) 2004 by Daniel Wagner, Pieter Palmers
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
#include "avdevicemusicsubunit.h"

AvDevice::AvDevice(int port, int node)
{
	iNodeId=node;
	m_iPort=port;

	// check to see if a device is really present?

	// probably initialisation would be better done here

        // Port and node id are not distinct.  The node id
        // can change after a bus reset, therefore the
        // device id has to be taken for identifiction.
}

FBReturnCodes
AvDevice::Initialize() {
   if (!m_bInitialised) {

	m_handle = raw1394_new_handle();
	if ( !m_handle ) {
	    if ( !errno ) {
		debugPrint(DEBUG_LEVEL_DEVICE,  "libraw1394 not compatible.\n" );
	    } else {
		perror ("Could not get 1394 handle");
		debugPrint(DEBUG_LEVEL_DEVICE, "Is ieee1394 and raw1394 driver loaded?\n");
	    }
	    return eFBRC_Creating1394HandleFailed;
	}

	raw1394_set_userdata( m_handle, this );

	if ( raw1394_set_port( m_handle,  m_iPort ) < 0 ) {
	    perror( "Could not set port" );
	    return eFBRC_Setting1394PortFailed;
	}
   }

   // enumerate the subunits present in this device, create an AvDeviceSubunit for them, and add this object to the cSubUnits vector
	unsigned char table_entry;
	unsigned char subunit_maxid;
	unsigned char subunit_type;
	quadlet_t table_entries; // buffer these table entries, because the memory content pointed to by
				 // the response pointer can change due to other libraw operations on this handle

	quadlet_t request[6];
	quadlet_t *response;
	AvDeviceSubunit *tmpAvDeviceSubunit=NULL;

	// check the number of I/O plugs

	request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
					| AVC1394_COMMAND_PLUG_INFO | 0x00;
	request[1] = 0xFFFFFFFF;
	response = avcExecuteTransaction(request, 2, 2);
	if (response != NULL) {
		iNbIsoDestinationPlugs= (unsigned char) ((response[1]>>24) & 0xff);
		iNbIsoSourcePlugs= (unsigned char) ((response[1]>>16) & 0xff);
		iNbExtDestinationPlugs= (unsigned char) ((response[1]>>8) & 0xff);
		iNbExtSourcePlugs= (unsigned char) ((response[1]>>0) & 0xff);
	}
	request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
					| AVC1394_COMMAND_PLUG_INFO | 0x01;
	request[1] = 0xFFFFFFFF;
	response = avcExecuteTransaction(request, 2, 2);
	if (response != NULL) {
		iNbAsyncDestinationPlugs= (unsigned char) ((response[1]>>24) & 0xff);
		iNbAsyncSourcePlugs= (unsigned char) ((response[1]>>16) & 0xff);
	}

	debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: %d Isochronous source plugs, %d Isochronous destination plugs\n",iNbIsoSourcePlugs,iNbIsoDestinationPlugs);
	debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: %d External source plugs, %d External destination plugs\n",iNbExtSourcePlugs,iNbExtDestinationPlugs);
	debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: %d Asynchronous source plugs, %d Asynchronous destination plugs\n",iNbAsyncSourcePlugs,iNbAsyncDestinationPlugs);

	// create the subunits
	for (unsigned int i=0;i<8;i++) { // cycle through the 8 pages (max 32 subunits; 4 subunits/page)
		request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
				| AVC1394_COMMAND_SUBUNIT_INFO | ((i<<4) & 0xF0) | 0x07;
		request[1] = 0xFFFFFFFF;
		response = avcExecuteTransaction(request, 6, 2);

		table_entries=response[1];

		if (response != NULL) {
			// this way of processing the table entries assumes that the subunit type is not "extended"

			// stop processing when a "not implemented" is received (according to spec)
			if((response[0]&0xFF000000) == AVC1394_RESPONSE_NOT_IMPLEMENTED) break;

			// warning: don't do unsigned int j! comparison >= 0 is always true for uint
			for (int j=3;j>=0;j--) { // cycle through the 8 pages (max 32 subunits; 4 subunits/page)
				table_entry=(table_entries >> (j*8)) & 0xFF;
				subunit_maxid=table_entry& 0x07;
				subunit_type=(table_entry >> 3) & 0x1F;
				//debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: Page %d, item %d: Table entry=0x%02X, subunit_maxid=0x%02X, subunit_type=0x%02X\n",i,j,table_entry,subunit_maxid,subunit_type);

				// according to spec we could stop processing
				// at the first 0xFF entry, but doing it this way
				// is a little more robust
				if (table_entry != 0xFF) {
					for (unsigned char subunit_id=0;subunit_id<subunit_maxid+1;subunit_id++) {

						// only two types of specific subunits are supported: audio and music
						switch (subunit_type) {
							case 0x01: // audio subunit
								tmpAvDeviceSubunit=new AvDeviceAudioSubunit(this,subunit_id);
								if (tmpAvDeviceSubunit) { // test code
									//tmpAvDeviceSubunit->printOutputPlugConnections();
								}
							break;
							case 0x0C: // music subunit
								tmpAvDeviceSubunit=new AvDeviceMusicSubunit(this,subunit_id);
								/*{ // just a test
								AvDeviceMusicSubunit tmpAvDeviceSubunit2(this,subunit_id);
								tmpAvDeviceSubunit2.printMusicPlugInfo();
								tmpAvDeviceSubunit2.printMusicPlugConfigurations();
								tmpAvDeviceSubunit2.printOutputPlugConnections();
								tmpAvDeviceSubunit2.test();
								}*/
							break;

							default: // generic
								tmpAvDeviceSubunit=new AvDeviceSubunit(this,subunit_type,subunit_id);
							break;
						}

						if(tmpAvDeviceSubunit && tmpAvDeviceSubunit->isValid()) {
							cSubUnits.push_back(tmpAvDeviceSubunit);


						} else {
							if (tmpAvDeviceSubunit) {
								debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: Unsupported AvDeviceSubunit encountered. Page %d, item %d: Table entry=0x%02X, subunit_maxid=0x%02X, subunit_type=0x%02X, subunit_id=%0x02X\n",i,j,table_entry,subunit_maxid,subunit_type,subunit_id);

								delete tmpAvDeviceSubunit;
							} else {
								debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: Could not create AvDeviceSubunit object.\n");
							}
						}
					}
				}
			}
		}
	}

   m_bInitialised = true;
   return eFBRC_Success;

}

bool AvDevice::isInitialised() {
	return m_bInitialised;
}

AvDevice::~AvDevice()
{

	vector<AvDeviceSubunit *>::iterator it;
	for( it = cSubUnits.begin(); it != cSubUnits.end(); it++ ) {
		delete *it;
	}

    if ( m_handle ) {
	raw1394_destroy_handle( m_handle );
	m_handle = 0;
    }
}

/* Function to execute an AVC transaction, i.e. send command/status and get response
 * main purpose is wrapping the avc1394 function call to output some debugging comments.
 */
quadlet_t * AvDevice::avcExecuteTransaction(quadlet_t *request, unsigned int request_len, unsigned int response_len) {
	quadlet_t *response;
	unsigned char *request_pos;
	unsigned int i;
	response = avc1394_transaction_block(m_handle, iNodeId, request, request_len, 2);
	if (request != NULL) {
            	debugPrint (DEBUG_LEVEL_TRANSFERS, "\n------- TRANSACTION START -------\n");
		debugPrint (DEBUG_LEVEL_TRANSFERS,"  REQUEST:     ");
		/* request is in machine byte order. this function is for intel architecure */
		for (i=0;i<request_len;i++) {
			request_pos=(unsigned char *)(request+i);
			debugPrint (DEBUG_LEVEL_TRANSFERS, "0x%02X%02X%02X%02X ", *(request_pos),*(request_pos+1),*(request_pos+2),*(request_pos+3));
		}
		debugPrint (DEBUG_LEVEL_TRANSFERS,"\n");
		debugPrint (DEBUG_LEVEL_TRANSFERS,"      => ");
		debugPrint (DEBUG_LEVEL_TRANSFERS,"                     ");
		request_pos=(unsigned char *)(request);
		debugPrint (DEBUG_LEVEL_TRANSFERS, "subunit_type=%02X  subunit_id=%02X  opcode=%02X",((*(request_pos+1))>>3)&0x1F,(*(request_pos+1))&0x07,(*(request_pos+2))&0xFF);
		debugPrint (DEBUG_LEVEL_TRANSFERS,"\n");
	}
	if (response != NULL) {
		/* response is in order of receiving, i.e. msb first */
		debugPrint (DEBUG_LEVEL_TRANSFERS,"  -> RESPONSE: ");
		for (i=0;i<response_len;i++) {
			debugPrint (DEBUG_LEVEL_TRANSFERS, "0x%08X ", response[i]);
		}
		debugPrint (DEBUG_LEVEL_TRANSFERS,"\n");
		debugPrint (DEBUG_LEVEL_TRANSFERS,"      => ");
		switch (response[0]&0xFF000000) {
			case AVC1394_RESPONSE_NOT_IMPLEMENTED:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Not Implemented      ");
			break;
			case AVC1394_RESPONSE_ACCEPTED:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Accepted             ");
			break;
			case AVC1394_RESPONSE_REJECTED:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Rejected             ");
			break;
			case AVC1394_RESPONSE_IN_TRANSITION:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"In Transition        ");
			break;
			case AVC1394_RESPONSE_IMPLEMENTED:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Implemented / Stable ");
			break;
			case AVC1394_RESPONSE_CHANGED:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Changed              ");
			break;
			case AVC1394_RESPONSE_INTERIM:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Interim              ");
			break;
			default:
				debugPrint (DEBUG_LEVEL_TRANSFERS,"Unknown response     ");
			break;
		}
		debugPrint (DEBUG_LEVEL_TRANSFERS, "subunit_type=%02X  subunit_id=%02X  opcode=%02X",(response[0]>>19)&0x1F,(response[0]>>16)&0x07,(response[0]>>8)&0xFF);
		debugPrint (DEBUG_LEVEL_TRANSFERS,"\n");
	}
        debugPrint (DEBUG_LEVEL_TRANSFERS, "------- TRANSACTION END -------\n");
	return response;

}
