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
#include <stdio.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include "threads.h"
#include "avdevice.h"
#include "avdevicepool.h"
#include "avdevicesubunit.h"
#include "avdeviceaudiosubunit.h"
#include "avdevicemusicsubunit.h"
#include "avmusicstatusdescriptor.h"
#include "avclusterinfoblock.h"
#include "avpluginfoblock.h"

#include "cmhandler.h"

#undef AVC1394_GET_RESPONSE_OPERAN
#define AVC1394_GET_RESPONSE_OPERAND(x, n) (((x) & (0xFF000000 >> (((n)%4)*8))) >> (((3-(n))%4)*8))

AvDevice::AvDevice(ConfigRom& configRom)
    : m_configRom( ConfigRom( configRom ) )
    , m_iNodeId( -1 )
    , m_iPort( -1 )
    , m_bInitialised( false )
    , m_iGeneration( 0 )
    , cSubUnits( 0 )
    , m_iNbAsyncDestinationPlugs( 0 )
    , m_iNbAsyncSourcePlugs( 0 )
    , m_iNbIsoDestinationPlugs( 0 )
    , m_iNbIsoSourcePlugs( 0 )
    , m_iNbExtDestinationPlugs( 0 )
    , m_iNbExtSourcePlugs( 0 )
{
    setDebugLevel( DEBUG_LEVEL_MODERATE );
    AvDevicePool::instance()->registerAvDevice( this );
}


AvDevice::~AvDevice()
{
    vector<AvDeviceSubunit *>::iterator it;
    for( it = cSubUnits.begin(); it != cSubUnits.end(); it++ ) {
	delete *it;
    }
    AvDevicePool::instance()->unregisterAvDevice( this );
}

void
AvDevice::execute( EStates state )
{
    switch ( state ) {
    case eDeviceDiscovery:
        if ( initialize() == eFBRC_Success ) {
            debugPrint( DEBUG_LEVEL_INFO, "Device discovery successful\n" );

            // Put ourself to sleep until a something happends
            sleepCall( this, &AvDevice::execute, eCheckState );
        } else {
            debugError( "Device discovery failed\n" );

            asyncCall( this, &AvDevice::execute, eDestroy );
        }
        break;
    case eCheckState:
        {
            if ( m_iGeneration
                 != Ieee1394Service::instance()->getGenerationCount() ) {
                asyncCall( this, &AvDevice::execute, eDestroy );
            }
        }
        break;
    case eDestroy:
        destroyCall( this );
        break;
    default:
        debugError( "Invalid state: %d\n", state );
    }
}

FBReturnCodes
AvDevice::initialize()
{
    if ( !m_bInitialised ) {
	FBReturnCodes eStatus = enumerateSubUnits();
	if ( eStatus != eFBRC_Success ) {
	    debugError( "Could not enumerate SubUnits\n" );
	    return eStatus;
	}

	m_bInitialised = true;
    }
    return eFBRC_Success;
}

bool AvDevice::isInitialised()
{
    return m_bInitialised;
}

FBReturnCodes
AvDevice::enumerateSubUnits()
{
    // enumerate the subunits present in this device, create an
    // AvDeviceSubunit for them, and add this object to the cSubUnits
    // vector
    unsigned char table_entry;
    unsigned char subunit_maxid;
    unsigned char subunit_type;
    // buffer these table entries, because the memory content pointed
    // to by the response pointer can change due to other libraw
    // operations on this handle
    quadlet_t table_entries;
    quadlet_t request[6];
    quadlet_t *response;
    AvDeviceSubunit *tmpAvDeviceSubunit=NULL;

    Ieee1394Service* p1394Service = Ieee1394Service::instance();

    // check the number of I/O plugs
    request[0] = AVC1394_CTYPE_STATUS
		 | AVC1394_SUBUNIT_TYPE_UNIT
		 | AVC1394_SUBUNIT_ID_IGNORE
		 | AVC1394_COMMAND_PLUG_INFO
		 | 0x0000;
    request[1] = 0xFFFFFFFF;
    response = p1394Service->avcExecuteTransaction( m_iNodeId, request, 2, 2 );
    request[1] = 0x02020606;
    response = request;
    if ( response ) {
	m_iNbIsoDestinationPlugs
	    = AVC1394_GET_RESPONSE_OPERAND( response[1], 0 );
	m_iNbIsoSourcePlugs
	    = AVC1394_GET_RESPONSE_OPERAND( response[1], 1 );
	m_iNbExtDestinationPlugs
	    = AVC1394_GET_RESPONSE_OPERAND( response[1], 2 );
	m_iNbExtSourcePlugs
	    = AVC1394_GET_RESPONSE_OPERAND( response[1], 3 );
    }

    request[0] = AVC1394_CTYPE_STATUS
		 | AVC1394_SUBUNIT_TYPE_UNIT
		 | AVC1394_SUBUNIT_ID_IGNORE
		 | AVC1394_COMMAND_PLUG_INFO
		 | 0x01;
    request[1] = 0xFFFFFFFF;
    response = p1394Service->avcExecuteTransaction( m_iNodeId, request, 2, 2 );
    if ( response != NULL ) {
	m_iNbAsyncDestinationPlugs
            = AVC1394_GET_RESPONSE_OPERAND( response[1], 0 );
	m_iNbAsyncSourcePlugs
            = AVC1394_GET_RESPONSE_OPERAND( response[1], 1 );
    }

    debugPrint( DEBUG_LEVEL_DEVICE,
		"AvDevice: %d Isochronous source plugs, "
		"%d Isochronous destination plugs\n",
		m_iNbIsoSourcePlugs, m_iNbIsoDestinationPlugs);
    debugPrint( DEBUG_LEVEL_DEVICE,
		"AvDevice: %d External source plugs, "
		"%d External destination plugs\n",
		m_iNbExtSourcePlugs, m_iNbExtDestinationPlugs);
    debugPrint( DEBUG_LEVEL_DEVICE,
		"AvDevice: %d Asynchronous source plugs, "
		"%d Asynchronous destination plugs\n",
		m_iNbAsyncSourcePlugs, m_iNbAsyncDestinationPlugs);


    // create the subunits
    for (unsigned int i = 0; i < 8; i++ ) {
	// cycle through the 8 pages (max 32 subunits; 4
	// subunits/page)


	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_SUBUNIT_INFO
		     | ((i<<4) & 0xF0) | 0x07;
	request[1] = 0xFFFFFFFF;
	response = p1394Service->avcExecuteTransaction( m_iNodeId, request, 6, 2 );

	table_entries=response[1]; /// XXX buggy code! response could be 0!

	if ( response != NULL ) {
	    // this way of processing the table entries assumes that
	    // the subunit type is not "extended"

	    // stop processing when a "not implemented" is received
	    // (according to spec)
	    if ( (response[0]&0xFF000000) == AVC1394_RESPONSE_NOT_IMPLEMENTED) {
		break;
	    }

	    // warning: don't do unsigned int j!
	    // comparison >= 0 is always true for uint
	    for ( int j = 3; j >= 0; j-- ) {
		// cycle through the 8 pages (max 32
		// subunits; 4 subunits/page)
		table_entry   = (table_entries >> (j*8)) & 0xFF;
		subunit_maxid = table_entry & 0x07;
		subunit_type  = (table_entry >> 3) & 0x1F;

		//debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: Page %d, item %d: Table entry=0x%02X, subunit_maxid=0x%02X, subunit_type=0x%02X\n",i,j,table_entry,subunit_maxid,subunit_type);

		// according to spec we could stop processing at the
		// first 0xFF entry, but doing it this way is a little
		// more robust

		if ( table_entry != 0xFF ) {
		    for ( unsigned char subunit_id = 0;
			  subunit_id < subunit_maxid+1;
			  subunit_id++ )
		    {

			// only two types of specific subunits are
			// supported: audio and music
			switch ( subunit_type ) {
			case 0x01: // audio subunit
			    tmpAvDeviceSubunit
				= new AvDeviceAudioSubunit( this,  subunit_id);
			    if ( tmpAvDeviceSubunit ) {
				// test code
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

			if ( tmpAvDeviceSubunit
			     && tmpAvDeviceSubunit->isValid() )
			{
			    cSubUnits.push_back(tmpAvDeviceSubunit);
			    //setDebugLevel(DEBUG_LEVEL_ALL);
			    /*
			    debugPrint( DEBUG_LEVEL_DEVICE,
					"Trying to reserve the "
					"subunit...\n" );
			    tmpAvDeviceSubunit->reserve( 0x01 );
			    debugPrint( DEBUG_LEVEL_DEVICE,
					"  isReserved?: %d\n",
					tmpAvDeviceSubunit->isReserved());
			    tmpAvDeviceSubunit->unReserve();
			    */
			    //setDebugLevel(DEBUG_LEVEL_MODERATE);
			} else {
			    if ( tmpAvDeviceSubunit ) {
				debugPrint( DEBUG_LEVEL_DEVICE,  "AvDevice: Unsupported AvDeviceSubunit encountered. Page %d, item %d: Table entry=0x%02X, subunit_maxid=0x%02X, subunit_type=0x%02X, subunit_id=%0x02X\n",i,j,table_entry,subunit_maxid,subunit_type,subunit_id);

				delete tmpAvDeviceSubunit;
			    } else {
				debugPrint( DEBUG_LEVEL_DEVICE,
					    "AvDevice: Could not create AvDeviceSubunit object.\n");
			    }
			}
		    }
		}
	    }
	}
    }
    return eFBRC_Success;
}

#define AVC1394_COMMAND_STREAM_FORMAT_SUPPORT 0x00002F00

#define AVC1394_COMMAND_STREAM_FORMAT_SUPPORT_INPUT            0x00000000
#define AVC1394_COMMAND_STREAM_FORMAT_SUPPORT_OUTPUT           0x00000001
#define AVC1394_COMMAND_STREAM_FORMAT_SUPPORT_SINGLE_REQUEST   0x000000C0
#define AVC1394_COMMAND_STREAM_FORMAT_SUPPORT_LIST_REQUEST     0x000000C1

#define AVC1394_STREAM_FORMAT_DVCR         0x80
#define AVC1394_STREAM_FORMAT_AUDIO_MUSIC  0x90

#define AVC1394_STREAM_FORMAT_AM_AM824          0x00
#define AVC1394_STREAM_FORMAT_AM_AUDIOPACK      0x01
#define AVC1394_STREAM_FORMAT_AM_FLOAT          0x02
#define AVC1394_STREAM_FORMAT_AM_AM824_COMPOUND 0x40

#define AVC1394_STREAM_FORMAT_AM_AM824C_22K05 0x00
#define AVC1394_STREAM_FORMAT_AM_AM824C_24K   0x01
#define AVC1394_STREAM_FORMAT_AM_AM824C_32K   0x02
#define AVC1394_STREAM_FORMAT_AM_AM824C_44K1  0x03
#define AVC1394_STREAM_FORMAT_AM_AM824C_48K   0x04
#define AVC1394_STREAM_FORMAT_AM_AM824C_96K   0x05
#define AVC1394_STREAM_FORMAT_AM_AM824C_176K4 0x06
#define AVC1394_STREAM_FORMAT_AM_AM824C_192K  0x07
#define AVC1394_STREAM_FORMAT_AM_AM824C_88K2  0x0A

#define AVC1394_STREAM_FORMAT_AM_AM824C_CBR_SUPPORTED     0x00
#define AVC1394_STREAM_FORMAT_AM_AM824C_CBR_NOT_SUPPORTED 0x02
#define AVC1394_STREAM_FORMAT_AM_AM824C_CBR_DONT_CARE     0x01

FBReturnCodes AvDevice::setInputPlugSignalFormat(unsigned char plug, unsigned char fmt, quadlet_t fdf) {
	quadlet_t request[6];
	quadlet_t *response;

	request[0] = AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
					| AVC1394_COMMAND_STREAM_FORMAT_SUPPORT 
					| AVC1394_COMMAND_STREAM_FORMAT_SUPPORT_SINGLE_REQUEST;
	request[1] = 0x00000000;
	request[2] = 0xFFFF0000 | (AVC1394_STREAM_FORMAT_AUDIO_MUSIC << 8) | AVC1394_STREAM_FORMAT_AM_AM824_COMPOUND;
	
	response = Ieee1394Service::instance()->avcExecuteTransaction( m_iNodeId, request, 2, 2);
	if (response != NULL) {

	}
	return eFBRC_Success;
}

FBReturnCodes AvDevice::getInputPlugSignalFormat(unsigned char plug, unsigned char *fmt, quadlet_t *fdf) {
	quadlet_t request[6];
	quadlet_t *response;

	request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
					| AVC1394_COMMAND_INPUT_PLUG_SIGNAL_FORMAT | plug;
	request[1] = 0xFFFFFFFF;
	response = Ieee1394Service::instance()->avcExecuteTransaction( m_iNodeId, request, 2, 2);
	if (response != NULL) {
		*fmt=((response[1] >> 24) & 0x3F);
		*fdf=response[1]& 0x00FFFFFF;
	}
	return eFBRC_Success;
}

FBReturnCodes AvDevice::setOutputPlugSignalFormat(unsigned char plug, unsigned char fmt, quadlet_t fdf) {
	quadlet_t request[6];
	quadlet_t *response;

	request[0] = AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
					| AVC1394_COMMAND_OUTPUT_PLUG_SIGNAL_FORMAT | plug;
	request[1] = (0x80000000) | ((fmt & 0x3F)<<24) | (fdf & 0x00FFFFFF);
	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 2, 2);
	if (response != NULL) {

	}
	return eFBRC_Success;
}

FBReturnCodes AvDevice::getOutputPlugSignalFormat(unsigned char plug, unsigned char *fmt, quadlet_t *fdf) {
	quadlet_t request[6];
	quadlet_t *response;

	request[0] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE
					| AVC1394_COMMAND_OUTPUT_PLUG_SIGNAL_FORMAT | plug;
	request[1] = 0xFFFFFFFF;
	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 2, 2);
	if (response != NULL) {
		*fmt=((response[1] >> 24) & 0x3F);
		*fdf=response[1]& 0x00FFFFFF;
	}
	return eFBRC_Success;
}

AvDeviceSubunit *AvDevice::getSubunit(unsigned char type, unsigned char id) {
	vector<AvDeviceSubunit *>::iterator it;
	for( it = cSubUnits.begin(); it != cSubUnits.end(); it++ ) {
		if ((*it) && ((*it)->getType()==type) && ((*it)->getId()==id)) {
			return *it;
		}
	}
	return NULL;
}

#define AVC1394_COMMAND_SIGNAL_SOURCE 0x00001A00

void AvDevice::printConnections() {
    quadlet_t request[6];
    quadlet_t *response;
    //setDebugLevel(DEBUG_LEVEL_ALL);

    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: ISO source connections:\n");

    for (unsigned int i=0;i<getNbIsoSourcePlugs();i++) {
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_SIGNAL_SOURCE
		     | 0x00FF;
	request[1]=0xFFFEFF00 | ((i & 0xFF));

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 2, 2);

	if ( response ) {
	    unsigned char output_status=(response[0]&0xE0) >> 5;
	    unsigned char conv=(response[0]&0x10) >> 4;
	    unsigned char signal_status=(response[0]&0x0F);

	    unsigned int signal_source=((response[1]>>16)&0xFFFF);

	    unsigned char source_subunit_type=(signal_source>>11)&0x1F;
	    unsigned char source_subunit_id=(signal_source>>8)&0x07;
	    unsigned char source_plug=signal_source&0xFF;

	    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice:   OPCR 0x%02X <- subunit: 0x%02X/0x%02X, plug: 0x%02X (0x%02X / %d / 0x%02X)\n",i, source_subunit_type,source_subunit_id,source_plug,output_status,conv,signal_status);
	    // find the subunit this plug is connected to
	    AvDeviceSubunit *tmpSubunit=getSubunit(source_subunit_type,source_subunit_id);
	    if ( tmpSubunit ) {
		tmpSubunit->printSourcePlugConnections(source_plug);
	    }

	}
    }

    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: External source connections:\n");

    for (unsigned int i=0;i<getNbExtSourcePlugs();i++) {
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_SIGNAL_SOURCE
		     | 0x00FF;
	request[1]=0xFFFEFF00 | ((i & 0xFF)|0x80);

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 2, 2);

	if ( response ) {
	    unsigned char output_status=(response[0]&0xE0) >> 5;
	    unsigned char conv=(response[0]&0x10) >> 4;
	    unsigned char signal_status=(response[0]&0x0F);

	    unsigned int signal_source=((response[1]>>16)&0xFFFF);

	    unsigned char source_subunit_type=(signal_source>>11)&0x1F;
	    unsigned char source_subunit_id=(signal_source>>8)&0x07;
	    unsigned char source_plug=signal_source&0xFF;

	    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice:   EXTOUT 0x%02X <- subunit: 0x%02X/0x%02X, plug: 0x%02X (0x%02X / %d / 0x%02X)\n",i, source_subunit_type,source_subunit_id,source_plug,output_status,conv,signal_status);

	    // find the subunit this plug is connected to
	    AvDeviceSubunit *tmpSubunit=getSubunit(source_subunit_type,source_subunit_id);
	    if ( tmpSubunit ) {
		tmpSubunit->printSourcePlugConnections(source_plug);
	    }
	}
    }

    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: ISO sink connections:\n");

    for (unsigned int i=0;i<getNbIsoDestinationPlugs();i++) {
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_SIGNAL_SOURCE
		     | 0x00FF;
	request[1] = 0xFFFEFF00 | ((i & 0xFF));

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 2, 2);

	if ( response ) {
	    unsigned char output_status=(response[0]&0xE0) >> 5;
	    unsigned char conv=(response[0]&0x10) >> 4;
	    unsigned char signal_status=(response[0]&0x0F);

	    unsigned int signal_source=((response[1]>>16)&0xFFFF);

	    unsigned char source_subunit_type=(signal_source>>11)&0x1F;
	    unsigned char source_subunit_id=(signal_source>>8)&0x07;
	    unsigned char source_plug=signal_source&0xFF;

	    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice:   OPCR 0x%02X <- subunit: 0x%02X/0x%02X, plug: 0x%02X (0x%02X / %d / 0x%02X)\n",i, source_subunit_type,source_subunit_id,source_plug,output_status,conv,signal_status);
	    // find the subunit this plug is connected to
	    AvDeviceSubunit *tmpSubunit
		= getSubunit(source_subunit_type,source_subunit_id);
	    if ( tmpSubunit ) {
		//tmpSubunit->printDestinationPlugConnections(source_plug);
	    }

	}
    }

    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice: External sink connections:\n");

    for (unsigned int i=0;i<getNbExtDestinationPlugs();i++) {
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_SIGNAL_SOURCE
		     | 0x00FF;
	request[1]=0xFFFEFF00 | ((i & 0xFF)|0x80);

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 2, 2);

	if ( response ) {
	    unsigned char output_status=(response[0]&0xE0) >> 5;
	    unsigned char conv=(response[0]&0x10) >> 4;
	    unsigned char signal_status=(response[0]&0x0F);

	    unsigned int signal_source=((response[1]>>16)&0xFFFF);

	    unsigned char source_subunit_type=(signal_source>>11)&0x1F;
	    unsigned char source_subunit_id=(signal_source>>8)&0x07;
	    unsigned char source_plug=signal_source&0xFF;

	    debugPrint (DEBUG_LEVEL_DEVICE,"AvDevice:   EXTOUT 0x%02X <- subunit: 0x%02X/0x%02X, plug: 0x%02X (0x%02X / %d / 0x%02X)\n",i, source_subunit_type,source_subunit_id,source_plug,output_status,conv,signal_status);

	    // find the subunit this plug is connected to
	    AvDeviceSubunit *tmpSubunit
		= getSubunit(source_subunit_type,source_subunit_id);
	    if ( tmpSubunit ) {
		//tmpSubunit->printDestinationPlugConnections(source_plug);
	    }
	}
    }
}
void AvDevice::test() {
    quadlet_t request[6];
    quadlet_t *response;
    setDebugLevel(DEBUG_LEVEL_ALL);


	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x00000000;
	request[2]=0xFF000000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}
	
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x00000001;
	request[2]=0xFF000000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}

	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x00000000;
	request[2]=0xFF060000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}
	
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x00000001;
	request[2]=0xFF060000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x10000000;
	request[2]=0xFF050000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}
	
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x10000001;
	request[2]=0xFF050000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}
	
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x00000000;
	request[2]=0xFF010000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}
	
	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
	request[1]=0x00000001;
	request[2]=0xFF010000;

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 20);

	if ( response ) {

	}	
}

std::string AvDevice::getPlugChannelName(unsigned int direction, unsigned int plug, unsigned int channel) {
    quadlet_t request[6];
    quadlet_t *response;
	std::string str;

	request[0] = AVC1394_CTYPE_STATUS
		     | AVC1394_SUBUNIT_TYPE_UNIT
		     | AVC1394_SUBUNIT_ID_IGNORE
		     | AVC1394_COMMAND_PLUG_INFO
		     | 0xC0;
		   
	request[1]=0x00000000 | ((direction &0xFF)<<24) | (plug & 0xFF);
	request[2]=0xFF040000 | ((channel & 0xFF) << 8);

	response = Ieee1394Service::instance()->avcExecuteTransaction(m_iNodeId, request, 3, 3);

	if ( response ) {
		unsigned namelen=response[3]&0xFF;
		char *buff=NULL;
		
		buff=new char[namelen+1];
		if (!buff) {
			debugError( "Could not allocate memory for temporary name buffer\n" );
			return str;
		}
		
		if(!strncpy(buff, (const char *)(response+3), namelen)) {
			buff[0]=0;
			debugError( "Could not copy name\n" );
			str="";
		} else {
			buff[namelen]=0;
			str=buff;
		}
		delete buff;
	}

	return str;
}


#define FREEBOB_DIRTY_XML_CHILD_ADD(XparentX,XnodeX,XvalueX)  if ( !xmlNewChild( XparentX, 0, BAD_CAST XnodeX, BAD_CAST XvalueX ) ) { debugError( "Couldn't create " XnodeX "node\n" ); return eFBRC_CreatingXMLDocFailed; }

FBReturnCodes
AvDevice::addConnectionsToXml( xmlNodePtr root )    
{
	char tmpbuff[256];
	std::string name;
	
	// At this point only plug 0 is used by the firmware for audio i/o
	int plug=0;
	
	// obtain the music subunit (no beauty contests for this one :)
	AvDeviceSubunit *tmpSubunit=getSubunit(0x0C,0);
	AvDeviceMusicSubunit *musicSubunit;
	AvMusicStatusDescriptor *statusDescriptor;
	AvPlugInfoBlock* plugInfo;
	
	if(tmpSubunit && (tmpSubunit->getType()==0x0C)) {	
		musicSubunit=(AvDeviceMusicSubunit *)tmpSubunit;
	} else {
		debugError( "Could not find music subunit\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	statusDescriptor=musicSubunit->getMusicStatusDescriptor();
	
	if(!statusDescriptor) {
		debugError( "Could not find music status descriptor\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	plugInfo=statusDescriptor->getDestinationPlugInfoBlock(plug);
	if(!plugInfo) {
		debugError( "Could not find destination plug info block\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	
    // only one connection in each direction for the moment
    
    // prepare the device->host connection
    xmlNodePtr connectionSet = xmlNewChild( root, 0, BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "Couldn't create connection set node for direction 1 (playback)\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST "1" ) ) {
        debugError( "Couldn't create direction node\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    xmlNodePtr connection = xmlNewChild( connectionSet, 0, BAD_CAST "Connection", 0 );
    if ( !connection ) {
        debugError( "Couldn't create connection node for direction 1 (playback)\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    sprintf(tmpbuff,"%d",getGuid());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Id",tmpbuff);
    
    sprintf(tmpbuff,"%d",getPort());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Port",tmpbuff);
    
    sprintf(tmpbuff,"%d",getNodeId());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Node",tmpbuff);
   
    sprintf(tmpbuff,"%d",plug); 
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Plug",tmpbuff);
    
    sprintf(tmpbuff,"%d",48000);
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Samplerate",tmpbuff);
 
    sprintf(tmpbuff,"%d",plugInfo->getNbChannels());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Dimension",tmpbuff);

    xmlNodePtr streams=xmlNewChild( connection, 0, BAD_CAST "Streams", 0 );
    if ( !streams ) {
	debugError( "Couldn't create streams node for direction 1 (playback)\n");
	return eFBRC_CreatingXMLDocFailed;
    }
    
	for (unsigned int i=0; i<plugInfo->getNbClusters(); i++) {
		AvClusterInfoBlock *clusterInfo=plugInfo->getCluster(i);

		if (!clusterInfo) {
			debugError( "Could not find destination plug routing cluster %d\n",i );
			return eFBRC_CreatingXMLDocFailed;
		}	
		for (unsigned int j=0;j<clusterInfo->getNbSignals();j++) {
			xmlNodePtr stream = xmlNewChild( streams, 0, BAD_CAST "Stream", 0 );
			if ( !stream ) {
				debugError( "Couldn't create stream node for direction 1 (playback), cluster %d, signal %d\n",i ,j);
				return eFBRC_CreatingXMLDocFailed;
			}
			
			sprintf(tmpbuff,"%d",clusterInfo->getStreamFormat());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Format",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPortType());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Type",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPosition(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Position",tmpbuff);

			sprintf(tmpbuff,"%d",clusterInfo->getLocation(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Location",tmpbuff);
			
			sprintf(tmpbuff,"%d",0);
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "DestinationPort",tmpbuff);
			
			// apparently this can fail 
			name=getInputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			if (name=="") { // retry
				name=getInputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			}
			if (name=="") { // give up
				sprintf(tmpbuff,"out_%d_%d",clusterInfo->getPosition(j),clusterInfo->getLocation(j));
				name=tmpbuff;
			}
			debugError( " --> %s\n",name.c_str());

			xmlChar *enc_name=xmlEncodeEntitiesReentrant(root->doc, BAD_CAST name.c_str());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Name",enc_name);
			xmlFree(enc_name);
		}
	}
    // prepare the host->device connection
	plugInfo=statusDescriptor->getSourcePlugInfoBlock(plug);
	if(!plugInfo) {
		debugError( "Could not find destination plug info block\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	
    // only one connection in each direction for the moment
    
    connectionSet = xmlNewChild( root, 0, BAD_CAST "ConnectionSet", 0 );
    if ( !connectionSet ) {
        debugError( "Couldn't create connection set node for direction 0 (record)\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST "0" ) ) {
        debugError( "Couldn't create direction node\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    connection = xmlNewChild( connectionSet, 0, BAD_CAST "Connection", 0 );
    if ( !connection ) {
        debugError( "Couldn't create connection node for direction 0 (record)\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    sprintf(tmpbuff,"%d",getGuid());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Id",tmpbuff);
    
    sprintf(tmpbuff,"%d",getPort());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Port",tmpbuff);
    
    sprintf(tmpbuff,"%d",getNodeId());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Node",tmpbuff);
   
    sprintf(tmpbuff,"%d",plug); 
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Plug",tmpbuff);
    
    sprintf(tmpbuff,"%d",48000);
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Samplerate",tmpbuff);
 
    sprintf(tmpbuff,"%d",plugInfo->getNbChannels());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Dimension",tmpbuff);

    streams=xmlNewChild( connection, 0, BAD_CAST "Streams", 0 );
    if ( !streams ) {
	debugError( "Couldn't create streams node for direction 1 (playback)\n");
	return eFBRC_CreatingXMLDocFailed;
    }
    
	for (unsigned int i=0; i<plugInfo->getNbClusters(); i++) {
		AvClusterInfoBlock *clusterInfo=plugInfo->getCluster(i);

		if (!clusterInfo) {
			debugError( "Could not find destination plug routing cluster %d\n",i );
			return eFBRC_CreatingXMLDocFailed;
		}	
		for (unsigned int j=0;j<clusterInfo->getNbSignals();j++) {
			xmlNodePtr stream = xmlNewChild( streams, 0, BAD_CAST "Stream", 0 );
			if ( !stream ) {
				debugError( "Couldn't create stream node for direction 1 (playback), cluster %d, signal %d\n",i ,j);
				return eFBRC_CreatingXMLDocFailed;
			}
			
			sprintf(tmpbuff,"%d",clusterInfo->getStreamFormat());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Format",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPortType());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Type",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPosition(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Position",tmpbuff);

			sprintf(tmpbuff,"%d",clusterInfo->getLocation(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Location",tmpbuff);
			
			sprintf(tmpbuff,"%d",0);
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "DestinationPort",tmpbuff);
			
			// apparently this can fail 
			name=getOutputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			if (name=="") { // retry
				name=getInputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			}
			if (name=="") { // give up
				sprintf(tmpbuff,"in_%d_%d",clusterInfo->getPosition(j),clusterInfo->getLocation(j));
				name=tmpbuff;
			}

			debugError( " --> %s\n",name.c_str());
			
			xmlChar *enc_name=xmlEncodeEntitiesReentrant(root->doc, BAD_CAST name.c_str());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Name",enc_name);
			xmlFree(enc_name);
		}
	}

    return eFBRC_Success;
}

FBReturnCodes
AvDevice::addPlaybackConnectionsToXml( xmlNodePtr connectionSet )    
{
	char tmpbuff[256];
	std::string name;
	
	// At this point only plug 0 is used by the firmware for audio i/o
	int plug=0;
	
	// obtain the music subunit (no beauty contests for this one :)
	AvDeviceSubunit *tmpSubunit=getSubunit(0x0C,0);
	AvDeviceMusicSubunit *musicSubunit;
	AvMusicStatusDescriptor *statusDescriptor;
	AvPlugInfoBlock* plugInfo;
	
	if(tmpSubunit && (tmpSubunit->getType()==0x0C)) {	
		musicSubunit=(AvDeviceMusicSubunit *)tmpSubunit;
	} else {
		debugError( "Could not find music subunit\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	statusDescriptor=musicSubunit->getMusicStatusDescriptor();
	
	if(!statusDescriptor) {
		debugError( "Could not find music status descriptor\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	plugInfo=statusDescriptor->getDestinationPlugInfoBlock(plug);
	if(!plugInfo) {
		debugError( "Could not find destination plug info block\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	
    xmlNodePtr connection = xmlNewChild( connectionSet, 0, BAD_CAST "Connection", 0 );
    if ( !connection ) {
        debugError( "Couldn't create connection node for direction 1 (playback)\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    sprintf(tmpbuff,"%d",getGuid());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Id",tmpbuff);
    
    sprintf(tmpbuff,"%d",getPort());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Port",tmpbuff);
    
    sprintf(tmpbuff,"%d",getNodeId());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Node",tmpbuff);
   
    sprintf(tmpbuff,"%d",plug); 
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Plug",tmpbuff);
    
    sprintf(tmpbuff,"%d",48000);
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Samplerate",tmpbuff);
 
    sprintf(tmpbuff,"%d",plugInfo->getNbChannels());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Dimension",tmpbuff);

    xmlNodePtr streams=xmlNewChild( connection, 0, BAD_CAST "Streams", 0 );
    if ( !streams ) {
	debugError( "Couldn't create streams node for direction 1 (playback)\n");
	return eFBRC_CreatingXMLDocFailed;
    }
    
	for (unsigned int i=0; i<plugInfo->getNbClusters(); i++) {
		AvClusterInfoBlock *clusterInfo=plugInfo->getCluster(i);

		if (!clusterInfo) {
			debugError( "Could not find destination plug routing cluster %d\n",i );
			return eFBRC_CreatingXMLDocFailed;
		}	
		for (unsigned int j=0;j<clusterInfo->getNbSignals();j++) {
			xmlNodePtr stream = xmlNewChild( streams, 0, BAD_CAST "Stream", 0 );
			if ( !stream ) {
				debugError( "Couldn't create stream node for direction 1 (playback), cluster %d, signal %d\n",i ,j);
				return eFBRC_CreatingXMLDocFailed;
			}
			
			sprintf(tmpbuff,"%d",clusterInfo->getStreamFormat());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Format",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPortType());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Type",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPosition(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Position",tmpbuff);

			sprintf(tmpbuff,"%d",clusterInfo->getLocation(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Location",tmpbuff);
			
			sprintf(tmpbuff,"%d",0);
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "DestinationPort",tmpbuff);
			
			// apparently this can fail 
			name=getInputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			if (name=="") { // retry
				name=getInputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			}
			if (name=="") { // give up
				sprintf(tmpbuff,"out_%d_%d",clusterInfo->getPosition(j),clusterInfo->getLocation(j));
				name=tmpbuff;
			}
			debugError( " --> %s\n",name.c_str());

			xmlChar *enc_name=xmlEncodeEntitiesReentrant(connectionSet->doc, BAD_CAST name.c_str());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Name",enc_name);
			xmlFree(enc_name);
		}
	}

    return eFBRC_Success;
}

FBReturnCodes
AvDevice::addCaptureConnectionsToXml( xmlNodePtr connectionSet )    
{
	char tmpbuff[256];
	std::string name;
	
	// At this point only plug 0 is used by the firmware for audio i/o
	int plug=0;
	
	// obtain the music subunit (no beauty contests for this one :)
	AvDeviceSubunit *tmpSubunit=getSubunit(0x0C,0);
	AvDeviceMusicSubunit *musicSubunit;
	AvMusicStatusDescriptor *statusDescriptor;
	AvPlugInfoBlock* plugInfo;
	
	if(tmpSubunit && (tmpSubunit->getType()==0x0C)) {	
		musicSubunit=(AvDeviceMusicSubunit *)tmpSubunit;
	} else {
		debugError( "Could not find music subunit\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	statusDescriptor=musicSubunit->getMusicStatusDescriptor();
	
	if(!statusDescriptor) {
		debugError( "Could not find music status descriptor\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	plugInfo=statusDescriptor->getSourcePlugInfoBlock(plug);
	if(!plugInfo) {
		debugError( "Could not find source plug info block\n" );
		return eFBRC_CreatingXMLDocFailed;
	}
	
	
    xmlNodePtr connection = xmlNewChild( connectionSet, 0, BAD_CAST "Connection", 0 );
    if ( !connection ) {
        debugError( "Couldn't create connection node for direction 0 (capture)\n" );
        return eFBRC_CreatingXMLDocFailed;
    }
    
    sprintf(tmpbuff,"%d",getGuid());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Id",tmpbuff);
    
    sprintf(tmpbuff,"%d",getPort());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Port",tmpbuff);
    
    sprintf(tmpbuff,"%d",getNodeId());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Node",tmpbuff);
   
    sprintf(tmpbuff,"%d",plug); 
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Plug",tmpbuff);
    
    sprintf(tmpbuff,"%d",48000);
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Samplerate",tmpbuff);
 
    sprintf(tmpbuff,"%d",plugInfo->getNbChannels());
    FREEBOB_DIRTY_XML_CHILD_ADD(connection, "Dimension",tmpbuff);

    xmlNodePtr streams=xmlNewChild( connection, 0, BAD_CAST "Streams", 0 );
    if ( !streams ) {
	debugError( "Couldn't create streams node for direction 0 (capture)\n");
	return eFBRC_CreatingXMLDocFailed;
    }
    
	for (unsigned int i=0; i<plugInfo->getNbClusters(); i++) {
		AvClusterInfoBlock *clusterInfo=plugInfo->getCluster(i);

		if (!clusterInfo) {
			debugError( "Could not find destination plug routing cluster %d\n",i );
			return eFBRC_CreatingXMLDocFailed;
		}	
		for (unsigned int j=0;j<clusterInfo->getNbSignals();j++) {
			xmlNodePtr stream = xmlNewChild( streams, 0, BAD_CAST "Stream", 0 );
			if ( !stream ) {
				debugError( "Couldn't create stream node for direction 1 (playback), cluster %d, signal %d\n",i ,j);
				return eFBRC_CreatingXMLDocFailed;
			}
			
			sprintf(tmpbuff,"%d",clusterInfo->getStreamFormat());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Format",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPortType());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Type",tmpbuff);
			
			sprintf(tmpbuff,"%d",clusterInfo->getPosition(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Position",tmpbuff);

			sprintf(tmpbuff,"%d",clusterInfo->getLocation(j));
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Location",tmpbuff);
			
			sprintf(tmpbuff,"%d",0);
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "DestinationPort",tmpbuff);
			
			// apparently this can fail 
			name=getOutputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			if (name=="") { // retry
				name=getOutputPlugChannelName(plug, clusterInfo->getPosition(j)+1);
			}
			if (name=="") { // give up
				sprintf(tmpbuff,"in_%d_%d",clusterInfo->getPosition(j),clusterInfo->getLocation(j));
				name=tmpbuff;
			}
			debugError( " --> %s\n",name.c_str());

			xmlChar *enc_name=xmlEncodeEntitiesReentrant(connectionSet->doc, BAD_CAST name.c_str());
			FREEBOB_DIRTY_XML_CHILD_ADD(stream, "Name",enc_name);
			xmlFree(enc_name);
		}
	}

    return eFBRC_Success;
}
