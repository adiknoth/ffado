/* avdevice.cpp
 * Copyright (C) 2004 by Daniel Wagner
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

AvDevice::AvDevice(int port, int node)
{
	iNodeId=node;
	m_iPort=port;

	// check to see if a device is really present?
}

FBReturnCodes
AvDevice::Initialize() {
   if (!m_bInitialised) {

	m_handle = raw1394_new_handle();
        if ( !m_handle ) {
            if ( !errno ) {
                fprintf( stderr,  "libraw1394 not compatible.\n" );
            } else {
                perror ("Could not get 1394 handle");
                fprintf (stderr, "Is ieee1394 and raw1394 driver loaded?\n");
            }
            return eFBRC_Creating1394HandleFailed;
        }
        
	raw1394_set_userdata( m_handle, this );
        
	if ( raw1394_set_port( m_handle,  m_iPort ) < 0 ) {
            perror( "Could not set port" );
            return eFBRC_Setting1394PortFailed;
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
            	debugPrint (DEBUG_LEVEL_TRANSFERS, "    Created...\n");
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
	return response;

}
