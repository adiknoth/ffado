/* ieee1394service.cpp
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
#include "ieee1394service.h"
#include "debugmodule.h"

#include "avdevice.h"
#include "avdescriptor.h"
#include "avmusicidentifierdescriptor.h"
#include "avmusicstatusdescriptor.h"
#include "avinfoblock.h"
#include "avgeneralmusicstatusinfoblock.h"
#include "avnameinfoblock.h"
#include "avaudioinfoblock.h"
#include "avmidiinfoblock.h"
#include "avaudiosyncinfoblock.h"
#include "avsourcepluginfoblock.h"
#include "avoutputplugstatusinfoblock.h"

Ieee1394Service* Ieee1394Service::m_pInstance = 0;

Ieee1394Service::Ieee1394Service()
    : m_iPort( 0 )
    , m_bInitialised( false )
    , m_bRHThreadRunning( false )
    , m_iGenerationCount( 0 )
{
    pthread_mutex_init( &m_mutex, NULL );
}

Ieee1394Service::~Ieee1394Service()
{
    stopRHThread();

    if ( m_rhHandle ) {
        raw1394_destroy_handle( m_rhHandle );
        m_rhHandle = 0;
    }

    if ( m_handle ) {
        raw1394_destroy_handle( m_handle );
        m_handle = 0;
    }

    m_pInstance = 0;
}

FBReturnCodes
Ieee1394Service::initialize()
{
    if ( !m_bInitialised ) {
        m_rhHandle = raw1394_new_handle();
        m_handle = raw1394_new_handle();
        if ( !m_rhHandle || !m_handle ) {
            if ( !errno ) {
                fprintf( stderr,  "libraw1394 not compatible.\n" );
            } else {
                perror ("Could not get 1394 handle");
                fprintf (stderr, "Is ieee1394 and raw1394 driver loaded?\n");
            }
            return eFBRC_Creating1394HandleFailed;
        }

        // Store this instance in the user data pointer, in order
        // to be able to retrieve the instance in the pure C bus reset
        // call back function.
        raw1394_set_userdata( m_rhHandle,  this );

        if ( raw1394_set_port( m_rhHandle,  m_iPort ) < 0 ) {
            perror( "Could not set port" );
            return eFBRC_Setting1394PortFailed;
        }

        if ( raw1394_set_port( m_handle,  m_iPort ) < 0 ) {
            perror( "Could not set port" );
            return eFBRC_Setting1394PortFailed;
        }

        raw1394_set_bus_reset_handler( m_rhHandle,  this->resetHandler );

        startRHThread();

        discoveryDevices();
        m_bInitialised = true;
    }
    return eFBRC_Success;
}

void
Ieee1394Service::shutdown()
{
    delete this;
}

Ieee1394Service*
Ieee1394Service::instance()
{
    if ( !m_pInstance ) {
        m_pInstance = new Ieee1394Service;
    }
    return m_pInstance;
}

FBReturnCodes
Ieee1394Service::discoveryDevices()
{
    //scan bus
    int iNodeCount = raw1394_get_nodecount( m_handle );
    for ( int iNodeId = 0; iNodeId < iNodeCount; ++iNodeId ) {
        rom1394_directory romDir;
        rom1394_get_directory( m_handle, iNodeId, &romDir );
        printRomDirectory( iNodeId, &romDir );

        switch (rom1394_get_node_type( &romDir )) {
	case ROM1394_NODE_TYPE_UNKNOWN:
            debugPrint (DEBUG_LEVEL_INFO,
                        "Node %d has node type UNKNOWN\n", iNodeId);
            break;
	case ROM1394_NODE_TYPE_DC:
            debugPrint (DEBUG_LEVEL_INFO,
                        "Node %d has node type DC\n", iNodeId);
            break;
	case ROM1394_NODE_TYPE_AVC:
            debugPrint (DEBUG_LEVEL_INFO,
                        "Node %d has node type AVC\n", iNodeId);
            printAvcUnitInfo( iNodeId );

            if ( avc1394_check_subunit_type( m_handle, iNodeId,
                                             AVC1394_SUBUNIT_TYPE_AUDIO ) ) {

                // XXX
                // create avcDevice which discovers itself :)

		// PP: just a static try, don't want to mess with the device manager yet...
		// Remark: the AvDevice and AvDescriptor aren't debugged thouroughly yet!
		//         the following code is the only debug I had time for... to be continued! (later this week)
            	debugPrint (DEBUG_LEVEL_INFO, "  Trying to create an AvDevice...\n");

		AvDevice *test=new AvDevice(m_iPort, iNodeId);
      		debugPrint (DEBUG_LEVEL_INFO, "   Created...\n");
		test->Initialize();
		if (test->isInitialised()) {
            		debugPrint (DEBUG_LEVEL_INFO, "   Init successfull...\n");
            		debugPrint (DEBUG_LEVEL_INFO, "   Trying to create an AvDescriptor...\n");
			AvDescriptor *testdesc=new AvDescriptor(test,AVC1394_SUBUNIT_TYPE_MUSIC | AVC1394_SUBUNIT_ID_0,0x00);
            		debugPrint (DEBUG_LEVEL_INFO, "    Created...\n");
            		debugPrint (DEBUG_LEVEL_INFO, "    Opening...\n");
			testdesc->OpenReadOnly();
            		
			debugPrint (DEBUG_LEVEL_INFO, "    Loading...\n");

			testdesc->Load();
			            		
           		debugPrint (DEBUG_LEVEL_INFO, "   Trying to create another AvDescriptor...\n");
			AvDescriptor *testdesc2=new AvDescriptor(test,AVC1394_SUBUNIT_TYPE_MUSIC | AVC1394_SUBUNIT_ID_0,0x80);
            		debugPrint (DEBUG_LEVEL_INFO, "    Created...\n");
            		debugPrint (DEBUG_LEVEL_INFO, "    Opening...\n");
			testdesc2->OpenReadOnly();
            		
			debugPrint (DEBUG_LEVEL_INFO, "    Loading...\n");

			testdesc2->Load();
			
			unsigned char *buff=new unsigned char[testdesc->getLength()];
			
			testdesc->readBuffer(0,testdesc->getLength(),buff);
			debugPrint (DEBUG_LEVEL_INFO, "    AvDescriptor 1 Contents:\n");
			
			hexDump(buff,testdesc->getLength());
			
			delete buff;
			
			buff=new unsigned char[testdesc2->getLength()];
			
			testdesc2->readBuffer(0,testdesc2->getLength(),buff);
			
			debugPrint (DEBUG_LEVEL_INFO, "    AvDescriptor 2 Contents:\n");
			hexDump(buff,testdesc2->getLength());
			delete buff;
			
			
			debugPrint (DEBUG_LEVEL_INFO, "    Closing AvDescriptors...\n");

			testdesc->Close();
			testdesc2->Close();
			
	      		debugPrint (DEBUG_LEVEL_INFO, "    Deleting AvDescriptors...\n");

			delete testdesc;
			delete testdesc2;
			
			// test the AvMusicIdentifierDescriptor
           		debugPrint (DEBUG_LEVEL_INFO, "   Trying to create an AvMusicIdentifierDescriptor...\n");
			AvMusicIdentifierDescriptor *testdesc_mid=new AvMusicIdentifierDescriptor(test);
            		debugPrint (DEBUG_LEVEL_INFO, "    Created...\n");
			testdesc_mid->printCapabilities();
		      	debugPrint (DEBUG_LEVEL_INFO, "    Deleting AvMusicIdentifierDescriptor...\n");
			delete testdesc_mid;
			
			// test the AvMusicStatusDescriptor
           		debugPrint (DEBUG_LEVEL_INFO, "   Trying to create an AvMusicStatusDescriptor...\n");
			AvMusicStatusDescriptor *testdesc_mid2=new AvMusicStatusDescriptor(test);
            		debugPrint (DEBUG_LEVEL_INFO, "    Created...\n");
			testdesc_mid2->printCapabilities();
			
			// test the AvInfoBlock
           		debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvInfoBlock...\n");
			
			AvInfoBlock *testblock1=new AvInfoBlock(testdesc_mid2,0);
           		debugPrint (DEBUG_LEVEL_INFO, "      Length: 0x%04X (%d)  Type: 0x%04X\n",testblock1->getLength(),testblock1->getLength(),testblock1->getType());
           		
			debugPrint (DEBUG_LEVEL_INFO, "    Trying to fetch next block...\n");
			// PP: might be better to have something like AvInfoBlock::moveToNextBlock();
			AvInfoBlock *testblock2=new AvInfoBlock(testdesc_mid2,2+testblock1->getLength());
			
          		debugPrint (DEBUG_LEVEL_INFO, "      Length: 0x%04X (%d)  Type: 0x%04X\n",testblock2->getLength(),testblock2->getLength(),testblock2->getType());
		
			// test the general status info block
           		debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvGeneralMusicStatusInfoBlock...\n");
			AvGeneralMusicInfoBlock *testblock3=new AvGeneralMusicInfoBlock(testdesc_mid2,0);
			
			// PP: the next tests could fail because of the difference in hardware.
			// these classes are intended to be used in the parser. I use hardcoded addresses in the test code,
			// instead of derived addresses based on the parent descriptors.
			// this is only intended to debug the base classes before using them in the parser.
			
			
			// this one should be valid (on my config)
           		debugPrint (DEBUG_LEVEL_INFO, "     isValid? %s\n",(testblock3->isValid()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canTransmitBlocking? %s\n",(testblock3->canTransmitBlocking()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canTransmitNonblocking? %s\n",(testblock3->canTransmitNonblocking()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canReceiveBlocking? %s\n",(testblock3->canReceiveBlocking()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canReceiveNonblocking? %s\n",(testblock3->canReceiveNonblocking()?"yes":"no"));
			
			delete testblock3;
			// this one shouldn't be valid
			testblock3=new AvGeneralMusicInfoBlock(testdesc_mid2,2+testblock1->getLength());
           		debugPrint (DEBUG_LEVEL_INFO, "     isValid? %s\n",(testblock3->isValid()?"yes":"no"));
          		debugPrint (DEBUG_LEVEL_INFO, "      canTransmitBlocking? %s\n",(testblock3->canTransmitBlocking()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canTransmitNonblocking? %s\n",(testblock3->canTransmitNonblocking()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canReceiveBlocking? %s\n",(testblock3->canReceiveBlocking()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canReceiveNonblocking? %s\n",(testblock3->canReceiveNonblocking()?"yes":"no"));
           		
			debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvAudioInfoBlock...\n");
			
			AvAudioInfoBlock *testblock4=new AvAudioInfoBlock(testdesc_mid2,0x01A);
           		debugPrint (DEBUG_LEVEL_INFO, "     isValid? %s\n",(testblock4->isValid()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      Length? 0x%04X (%d)\n",testblock4->getLength(),testblock4->getLength());
           		debugPrint (DEBUG_LEVEL_INFO, "      streams: %d\n",testblock4->getNbStreams());
           		debugPrint (DEBUG_LEVEL_INFO, "      Name: %s\n",testblock4->getName());
			
			debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvMidiInfoBlock...\n");

			AvMidiInfoBlock *testblock5=new AvMidiInfoBlock(testdesc_mid2,0x099);
           		debugPrint (DEBUG_LEVEL_INFO, "     isValid? %s\n",(testblock5->isValid()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      Length? 0x%04X (%d)\n",testblock5->getLength(),testblock5->getLength());
           		unsigned int nb_midi_streams=testblock5->getNbStreams();
			debugPrint (DEBUG_LEVEL_INFO, "      streams: %d\n",nb_midi_streams);
			for (unsigned int i=0;i<nb_midi_streams;i++) {
           			debugPrint (DEBUG_LEVEL_INFO, "       stream %d name: %s\n",i,testblock5->getName(i));
			}
			
			debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvAudioSyncInfoBlock...\n");
			AvAudioSyncInfoBlock *testblock6=new AvAudioSyncInfoBlock(testdesc_mid2,0x0262);
           		debugPrint (DEBUG_LEVEL_INFO, "     isValid? %s\n",(testblock6->isValid()?"yes":"no"));
          		debugPrint (DEBUG_LEVEL_INFO, "      canSyncBus? %s\n",(testblock6->canSyncBus()?"yes":"no"));
           		debugPrint (DEBUG_LEVEL_INFO, "      canSyncExternal? %s\n",(testblock6->canSyncExternal()?"yes":"no"));
           		
			
			debugPrint (DEBUG_LEVEL_INFO, "    Deleting AvInfoBlocks...\n");
			
			delete testblock1;
			delete testblock2;
			delete testblock3;
			delete testblock4;
			delete testblock5;
			delete testblock6;
			
			// now try to parse a full source plug entry
			debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvSourcePlugInfoBlock...\n");
			AvSourcePlugInfoBlock *testblock7=new AvSourcePlugInfoBlock(testdesc_mid2,0x13);
			
			debugPrint (DEBUG_LEVEL_INFO, "    Deleting AvSourcePlugInfoBlock...\n");
			
			delete testblock7;
			
			// now try to parse the full music output plug status infoblock
			debugPrint (DEBUG_LEVEL_INFO, "    Trying to create an AvOutputPlugStatusInfoBlock...\n");
			AvOutputPlugStatusInfoBlock *testblock8=new AvOutputPlugStatusInfoBlock(testdesc_mid2,0x0C);
			
			debugPrint (DEBUG_LEVEL_INFO, "    Deleting AvOutputPlugStatusInfoBlock...\n");
			
			delete testblock8;
			
		      	debugPrint (DEBUG_LEVEL_INFO, "    Deleting AvMusicStatusDescriptor...\n");
			delete testdesc_mid2;			
		}
      		debugPrint (DEBUG_LEVEL_INFO, "   Deleting AvDevice...\n");
		delete test;

	    }
            break;
	case ROM1394_NODE_TYPE_SBP2:
            debugPrint( DEBUG_LEVEL_INFO,
                        "Node %d has node type SBP2\n", iNodeId);
            break;
	case ROM1394_NODE_TYPE_CPU:
            debugPrint( DEBUG_LEVEL_INFO,
                        "Node %d has node type CPU\n", iNodeId);
            break;
	default:
            debugPrint( DEBUG_LEVEL_INFO,
                        "No matching node type found for node %d\n", iNodeId);
	}
    }
    return eFBRC_Success;
}

void
Ieee1394Service::printAvcUnitInfo( int iNodeId )
{
    printf( "AVC: video monitor?......%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_VIDEO_MONITOR ) ?
            "yes":"no" );
    printf( "AVC: audio?..............%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_AUDIO ) ?
            "yes":"no" );
    printf( "AVC; printer?............%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_PRINTER ) ?
            "yes":"no" );
    printf( "AVC: disk recorder?......%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_DISC_RECORDER ) ?
            "yes":"no" );
    printf( "AVC: video recorder?.....%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_TAPE_RECORDER ) ?
            "yes":"no" );
    printf( "AVC: vcr?................%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_VCR ) ?
            "yes":"no" );
    printf( "AVC: tuner?..............%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_TUNER ) ?
            "yes":"no" );
    printf( "AVC: CA?.................%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_CA ) ?
            "yes":"no" );
    printf( "AVC: video camera?.......%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_VIDEO_CAMERA ) ?
            "yes":"no" );
    printf( "AVC: panel?..............%s\n",
            avc1394_check_subunit_type(m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_PANEL ) ?
            "yes":"no" );
    printf( "AVC: camera storage?.....%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_CAMERA_STORAGE ) ?
            "yes":"no" );
    printf( "AVC: bulletin board?.....%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_BULLETIN_BOARD ) ?
            "yes":"no" );
    printf( "AVC: vendor specific?....%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_VENDOR_UNIQUE ) ?
            "yes":"no" );
    printf( "AVC: extended?...........%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_EXTENDED ) ?
            "yes":"no" );
    printf( "AVC: unit?...............%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_UNIT ) ?
            "yes":"no" );
    printf( "AVC: music?..............%s\n",
            avc1394_check_subunit_type( m_handle, iNodeId,
                                        AVC1394_SUBUNIT_TYPE_MUSIC ) ?
            "yes":"no" );
}

void
Ieee1394Service::printRomDirectory( int iNodeId,  rom1394_directory* pRomDir )
{
    int iBusInfoBlockLength
        = rom1394_get_bus_info_block_length( m_handle,  iNodeId );
    int iBusId = rom1394_get_bus_id( m_handle,  iNodeId );
    octlet_t oGuid = rom1394_get_guid( m_handle, iNodeId );
    rom1394_bus_options busOptions;
    rom1394_get_bus_options( m_handle, iNodeId, &busOptions );

    printf( "\nNode %d: \n", iNodeId );
    printf( "-------------------------------------------------\n" );
    printf( "bus info block length = %d\n", iBusInfoBlockLength);
    printf( "bus id = 0x%08x\n", iBusId );
    printf( "bus options:\n" );
    printf( "    isochronous resource manager capable: %d\n", busOptions.irmc );
    printf ("    cycle master capable                : %d\n", busOptions.cmc );
    printf ("    isochronous capable                 : %d\n", busOptions.isc );
    printf ("    bus manager capable                 : %d\n", busOptions.bmc );
    printf ("    cycle master clock accuracy         : %d ppm\n", busOptions.cyc_clk_acc );
    printf( "    maximum asynchronous record size    : %d bytes\n", busOptions.max_rec );
    printf("GUID: 0x%08x%08x\n", (quadlet_t) (oGuid>>32),
           (quadlet_t) (oGuid & 0xffffffff) );
    printf( "directory:\n");
    printf( "    node capabilities    : 0x%08x\n", pRomDir->node_capabilities );
    printf( "    vendor id            : 0x%08x\n", pRomDir->vendor_id );
    printf( "    unit spec id         : 0x%08x\n", pRomDir->unit_spec_id );
    printf( "    unit software version: 0x%08x\n", pRomDir->unit_sw_version );
    printf( "    model id             : 0x%08x\n", pRomDir->model_id );
    printf( "    textual leaves       : %s\n",     pRomDir->label );
}

int
Ieee1394Service::resetHandler( raw1394handle_t handle,
                               unsigned int iGeneration )
{
    debugPrint( DEBUG_LEVEL_INFO,
                "Bus reset has occurred (generation = %d).\n", iGeneration );
    raw1394_update_generation (handle, iGeneration);
    Ieee1394Service* pInstance
        = (Ieee1394Service*) raw1394_get_userdata (handle);
    pInstance->setGenerationCount( iGeneration );
    return 0;
}

bool
Ieee1394Service::startRHThread()
{
    if ( m_bRHThreadRunning ) {
        return true;
    }
    debugPrint( DEBUG_LEVEL_INFO,
                "Starting bus reset handler thread.\n" );
    pthread_mutex_lock( &m_mutex );
    pthread_create( &m_thread, NULL, rHThread, this );
    pthread_mutex_unlock( &m_mutex );
    m_bRHThreadRunning = true;
    return true;
}

void
Ieee1394Service::stopRHThread()
{
    if ( m_bRHThreadRunning ) {
        debugPrint( DEBUG_LEVEL_INFO,
                    "Stopping bus reset handler thread.\n" );
        pthread_mutex_lock (&m_mutex);
        pthread_cancel (m_thread);
        pthread_join (m_thread, NULL);
        pthread_mutex_unlock (&m_mutex);
    }
    m_bRHThreadRunning = false;
}

void*
Ieee1394Service::rHThread( void* arg )
{
    Ieee1394Service* pIeee1394Service = (Ieee1394Service*) arg;

    while (true) {
        raw1394_loop_iterate (pIeee1394Service->m_rhHandle);
        pthread_testcancel ();
    }

    return NULL;
}

void
Ieee1394Service::setGenerationCount( unsigned int iGeneration )
{
    m_iGenerationCount = iGeneration;
}

unsigned int
Ieee1394Service::getGenerationCount()
{
    return m_iGenerationCount;
}
