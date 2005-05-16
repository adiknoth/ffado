/* 
 * FreeBob Test Program
 * Bounces all input back to the sender
 * Copyright (C) 2005 Pieter Palmers
 *
 * Timing code shamelessly copied from Mixxx source code
 *   (C) 2002 by Tue and Ken Haste Andersen
 */

/* Based upon code from: 
 * libiec61883 - Linux IEEE 1394 streaming media library.
 * Copyright (C) 2004 Kristian Hogsberg and Dan Dennedy
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * ./test-freebob-bounce
 */

  
#include <libiec61883/iec61883.h>
#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <errno.h>
#include <time.h>


#include <jack/ringbuffer.h>
	
	#define QUATAFIRE_DEFAULT_SAMPLE_RATE 48000
	#define QUATAFIRE_OUT_DIMENSION 11
	#define QUATAFIRE_IN_DIMENSION 7
	#define QUATAFIRE_INPUT_TARGET_CHANNEL 1
	#define QUATAFIRE_MIDI_OUT_STREAM_POSITION 10 

//enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_DEFAULT;
enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_BUFFERFILL;
//enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_PACKET_PER_BUFFER;
//#define BUFFER 1000
//#define IRQ 20
//#define PACKET_MAX 8192

#if __BYTE_ORDER == __BIG_ENDIAN

struct iso_packet_header {
	unsigned int data_length : 16;
	unsigned int tag         : 2;
	unsigned int channel     : 6;
	unsigned int tcode       : 4;
	unsigned int sy          : 4;
};

struct iec61883_packet {
	/* First quadlet */
	unsigned int dbs      : 8;
	unsigned int eoh0     : 2;
	unsigned int sid      : 6;

	unsigned int dbc      : 8;
	unsigned int fn       : 2;
	unsigned int qpc      : 3;
	unsigned int sph      : 1;
	unsigned int reserved : 2;

	/* Second quadlet */
	unsigned int fdf      : 8;
	unsigned int eoh1     : 2;
	unsigned int fmt      : 6;

	unsigned int syt      : 16;

	unsigned char data[0];
};

typedef struct packet_info packet_info_t;
struct packet_info {
	/* The first part of this structure is a copy of the CIP header 
	 * This can be memcpy'd from the packet itself.
	 */
	/* First quadlet */
	unsigned int dbs      : 8;
	unsigned int eoh0     : 2;
	unsigned int sid      : 6;

	unsigned int dbc      : 8;
	unsigned int fn       : 2;
	unsigned int qpc      : 3;
	unsigned int sph      : 1;
	unsigned int reserved : 2;

	/* Second quadlet */
	unsigned int fdf      : 8;
	unsigned int eoh1     : 2;
	unsigned int fmt      : 6;

	unsigned int syt      : 16;

	/* the second part is some extra info */
	unsigned int nevents;
	unsigned int length;
};

#elif __BYTE_ORDER == __LITTLE_ENDIAN

struct iso_packet_header {
	unsigned int data_length : 16;
	unsigned int channel     : 6;
	unsigned int tag         : 2;
	unsigned int sy          : 4;
	unsigned int tcode       : 4;
};

struct iec61883_packet {
	/* First quadlet */
	unsigned int sid      : 6;
	unsigned int eoh0     : 2;
	unsigned int dbs      : 8;

	unsigned int reserved : 2;
	unsigned int sph      : 1;
	unsigned int qpc      : 3;
	unsigned int fn       : 2;
	unsigned int dbc      : 8;

	/* Second quadlet */
	unsigned int fmt      : 6;
	unsigned int eoh1     : 2;
	unsigned int fdf      : 8;

	unsigned int syt      : 16;

	unsigned char data[0];
};

typedef struct packet_info packet_info_t;
struct packet_info {
	/* The first part of this structure is a copy of the CIP header 
	 * This can be memcpy'd from the packet itself.
	 */
	/* First quadlet */
	unsigned int sid      : 6;
	unsigned int eoh0     : 2;
	unsigned int dbs      : 8;

	unsigned int reserved : 2;
	unsigned int sph      : 1;
	unsigned int qpc      : 3;
	unsigned int fn       : 2;
	unsigned int dbc      : 8;

	/* Second quadlet */
	unsigned int fmt      : 6;
	unsigned int eoh1     : 2;
	unsigned int fdf      : 8;

	unsigned int syt      : 16;

	/* the second part is some extra info */
	unsigned int nevents;
	unsigned int length;
};


#else

#error Unknown bitfield type

#endif

typedef struct connection_info connection_info_t;
struct connection_info {
	int packets;
	int events;
	int total_packets;
	int total_events;
	
	int dropped;
	
	unsigned long handler_time;
	
	int packet_info_table_size;
	packet_info_t *packet_info_table;
	
	connection_info_t *master;
	
	jack_ringbuffer_t *buffer;
	
};

inline unsigned long getCurrentUTime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec*1000000+now.tv_usec;
}

static int g_done = 0;

static void sighandler (int sig)
{
	g_done = 1;
}

static enum raw1394_iso_disposition 
iso_master_receive_handler(raw1394handle_t handle, unsigned char *data, 
        unsigned int length, unsigned char channel,
        unsigned char tag, unsigned char sy, unsigned int cycle, 
        unsigned int dropped)
{
        enum raw1394_iso_disposition retval;
        
        static unsigned int counter = 0;
		int writelen;
		unsigned long timestamp_enter=getCurrentUTime();
		
        
	connection_info_t *info = (connection_info_t *)raw1394_get_userdata(handle);
	
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	
	if(!info) {
		fprintf(stderr,"invalid connection info handle");
		return RAW1394_ISO_ERROR;
	}
	
	info->dropped+=dropped;
	
	if(info->packets< (info->packet_info_table_size)) {
		// copy the packet_info from the packet to the packet_info_table
		memcpy((void *)(&info->packet_info_table[info->packets]),packet,8); // the CIP header is 8 bytes long
		
		info->packet_info_table[info->packets].nevents=((length / sizeof (quadlet_t)) - 2)/packet->dbs;
		info->packet_info_table[info->packets].length=length;
		
		// add the data payload to the ringbuffer
		// rb_add(packet->data);
		writelen= jack_ringbuffer_write(info->buffer, (const char *)(data+8), length - 8 );
		if(writelen < length-8) {
			fprintf(stderr,"ringbuffer overrun\n");
		}
		
		// keep track of the total amount of events received
		info->events+=info->packet_info_table[info->packets].nevents;
		
		// one packet received
		info->packets++;
		
		//printf("MASTER RCV: FDF code = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %6d\n", packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, length,((length / sizeof (quadlet_t)) - 2)/packet->dbs, dropped);
		retval=RAW1394_ISO_OK;
		
	} else {
		// this shouldn't occur, because exiting raw_loop_iterate() should have freed up some space.
		printf("MASTER RCV: Buffer overrun!\n");
		retval=RAW1394_ISO_ERROR;
	}

	/* Check if the packet table is full. if so instruct libraw to leave raw1394_loop_iterate()
	 */	
	if(info->packets==info->packet_info_table_size) {
		// we processed this packet, but it is the last one we can queue in the packet_info_table.
		// make sure we leave raw1394_loop_iterate()
		retval=RAW1394_ISO_DEFER;
	}
   
    info->handler_time+=(getCurrentUTime()-timestamp_enter);
    
    return retval;
}

static enum raw1394_iso_disposition 
iso_slave_transmit_handler(raw1394handle_t handle,
		unsigned char *data, unsigned int *length,
		unsigned char *tag, unsigned char *sy,
		int cycle, unsigned int dropped)
{
        enum raw1394_iso_disposition retval;
        
        static unsigned int counter = 0;
	
	int readlen;
	unsigned long timestamp_enter=getCurrentUTime();
	
	connection_info_t *info = (connection_info_t *)raw1394_get_userdata(handle);
	connection_info_t *master_info;
	packet_info_t *master_packet_info;
		
	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	
	if(!info) {
		fprintf(stderr,"invalid connection info handle");
		return RAW1394_ISO_ERROR;
	}
	
	master_info=info->master;
	if(!master_info) {
		fprintf(stderr,"invalid master connection info handle");
		return RAW1394_ISO_ERROR;
	}
	
	info->dropped+=dropped;
	
	if(info->packets< (master_info->packet_info_table_size)) {
		master_packet_info=&master_info->packet_info_table[info->packets];
		
		// copy the packet_info from the packet_info_table to the packet
		memcpy(packet,(void *)(master_packet_info),8); // the CIP header is 8 bytes long
		
		*tag = 1;
		*sy = 0;
		*length=master_packet_info->length;
		
		// read the data payload from the ringbuffer
		readlen=jack_ringbuffer_read(info->buffer, (const char *)(data+8), master_packet_info->length - 8 );
		if(readlen < master_packet_info->length - 8) {
			fprintf(stderr,"ringbuffer underrun\n");
		}
		
		// one packet received
		info->packets++;
		
		// keep track of the total amount of events received
		info->events+=master_packet_info->nevents;
		//printf("SLAVE XMT:  FDF code = %X. SYT = %6d, DBS = %3d, DBC = %3d, FMT = %3d, LEN = %4d (%2d), DROPPED = %6d\n", packet->fdf,packet->syt,packet->dbs,packet->dbc,packet->fmt, *length,master_packet_info->nevents, dropped);
		retval=RAW1394_ISO_OK;
		
	} else {
		// this shouldn't occur, because exiting raw_loop_iterate() should have freed up some space.
		printf("SLAVE XMT:  Buffer overrun!\n");
		retval=RAW1394_ISO_ERROR;
	}
	
    if(info->packets==(master_info->packet_info_table_size)) {
		retval=RAW1394_ISO_DEFER;
	} 
	
    info->handler_time+=(getCurrentUTime()-timestamp_enter);

    return retval;
}

int main (int argc, char *argv[])
{
	raw1394handle_t master_receive_handle = raw1394_new_handle();
	raw1394handle_t slave_transmit_handle = raw1394_new_handle();
	raw1394handle_t plug_handle = raw1394_new_handle();
	
	unsigned long timestamp_start;
	unsigned long timestamp_wait_receive;
	unsigned long timestamp_end_transmit;
	
	int irq=100;
	int BUFFER=1000;
	int PACKET_MAX=1024;
	int TABLE_SIZE=8;
	int PREBUFFER=0;
	int port=0;
	
	int i;
	
	int master_receive_iso_channel;
	int master_receive_bandwidth = -1;
	int master_receive_iplug=-1;
	int master_receive_oplug=0;	
	nodeid_t master_receive_node = 0xffc0;
	
	int slave_transmit_iso_channel;
	int slave_transmit_bandwidth = -1;
	int slave_transmit_iplug=0;
	int slave_transmit_oplug=-1;	
	nodeid_t slave_transmit_node = 0xffc0;
		
	jack_ringbuffer_t *ringbuffer;

	signal (SIGINT, sighandler);
	signal (SIGPIPE, sighandler);

	for (i = 1; i < argc; i++) {
		
		if (strncmp (argv[i], "-h", 2) == 0 || 
			strncmp (argv[i], "--h", 3) == 0)
		{
			fprintf (stderr, 
			"usage: %s", argv[0]);
			raw1394_destroy_handle (master_receive_handle);
			raw1394_destroy_handle (slave_transmit_handle);
			raw1394_destroy_handle (plug_handle);
			return 1;
		} else if (strncmp (argv[i], "-b", 2) == 0) {
			BUFFER = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-l", 2) == 0) {
			PREBUFFER = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-p", 2) == 0) {
			PACKET_MAX = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-t", 2) == 0) {
			TABLE_SIZE = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-i", 2) == 0) {
			irq = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-o", 2) == 0) {
			port = atoi (argv[++i]);
		}
	}

	if ((raw1394_set_port(master_receive_handle, port) < 0) || (raw1394_set_port(slave_transmit_handle, port) < 0) || (raw1394_set_port(plug_handle, port) < 0)) {
		perror("couldn't set port");
		raw1394_destroy_handle (master_receive_handle);
		raw1394_destroy_handle (slave_transmit_handle);
		raw1394_destroy_handle (plug_handle);
		return 1;
	}
		
	fprintf (stderr, "Init Plugs...\n");
	
	iec61883_plug_impr_init (plug_handle, IEC61883_DATARATE_400);
	iec61883_plug_ompr_init (plug_handle, IEC61883_DATARATE_400, 63);

	iec61883_plug_ipcr_add (plug_handle, 1);
	iec61883_plug_ipcr_add (plug_handle, 1);
	iec61883_plug_opcr_add (plug_handle, 1,IEC61883_OVERHEAD_512,2048);
	iec61883_plug_opcr_add (plug_handle, 1,IEC61883_OVERHEAD_512,2048);
	
	fprintf (stderr, "Wait for plug connections...\n");
	while ((raw1394_loop_iterate(plug_handle) == 0)) {
		fprintf (stderr, "something happened at the plugs...\n");
	}	

//#define TABLE_SIZE 4
//#define PREBUFFER 0
#define RINGBUFFER_SIZE_EVENTS     TABLE_SIZE * 11 * 8 // nb packets * frames/event * events/packet

	connection_info_t master_receive_connection_info;
	memset(&master_receive_connection_info,'\0',sizeof(connection_info_t));
	master_receive_connection_info.packet_info_table_size=TABLE_SIZE;
	
	master_receive_connection_info.packet_info_table=calloc(master_receive_connection_info.packet_info_table_size,sizeof(packet_info_t));

	if (!master_receive_connection_info.packet_info_table) {
		fprintf (stderr, "Could not allocate memory for master packet info table\n");
		return -ENOMEM;
	}
	
	connection_info_t slave_transmit_connection_info;
	memset(&slave_transmit_connection_info,'\0',sizeof(connection_info_t));
	slave_transmit_connection_info.master=&master_receive_connection_info;

	if (master_receive_handle) {
		
		ringbuffer = jack_ringbuffer_create (RINGBUFFER_SIZE_EVENTS * sizeof (quadlet_t));
		master_receive_connection_info.buffer=ringbuffer;
		slave_transmit_connection_info.buffer=ringbuffer;
				
		raw1394_set_userdata(master_receive_handle,&master_receive_connection_info);
		
		fprintf (stderr, "** Master receive...\n");
		fprintf (stderr, "Create CMP connection...\n");
		//channel = iec61883_cmp_connect (handle, raw1394_get_local_id (handle),  &oplug, node, &iplug, &bandwidth);
		//master_receive_iso_channel = iec61883_cmp_connect (master_receive_handle, master_receive_node ,  &master_receive_oplug, raw1394_get_local_id (master_receive_handle), &master_receive_iplug, &master_receive_bandwidth);
		
		if (master_receive_iso_channel > -1) {
			fprintf (stderr, "Init ISO master receive handler on channel %d...\n",master_receive_iso_channel);
			fprintf (stderr, "   other mode (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",BUFFER,PACKET_MAX, irq);
			raw1394_iso_recv_init(master_receive_handle, iso_master_receive_handler, BUFFER, PACKET_MAX, master_receive_iso_channel, mode, irq);
	
			fprintf (stderr, "Start ISO master receive...\n");
			raw1394_iso_recv_start(master_receive_handle, -1, -1, 0);
		} else {
			fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
		}
		
		if (slave_transmit_handle) {
			fprintf (stderr, "** Slave transmit...\n");
			raw1394_set_userdata(slave_transmit_handle,&slave_transmit_connection_info);
			fprintf (stderr, "Create CMP connection...\n");
			
			//slave_transmit_iso_channel = iec61883_cmp_connect (slave_transmit_handle, raw1394_get_local_id (slave_transmit_handle),  &slave_transmit_oplug, slave_transmit_node, &slave_transmit_iplug, &slave_transmit_bandwidth);
		
			if (slave_transmit_iso_channel > -1) {
				fprintf (stderr, "Init ISO slave transmit handler on channel %d...\n",slave_transmit_iso_channel);
				fprintf (stderr, "   other mode (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",BUFFER,PACKET_MAX, irq);
				raw1394_iso_xmit_init(slave_transmit_handle, iso_slave_transmit_handler, BUFFER, PACKET_MAX, slave_transmit_iso_channel, RAW1394_ISO_SPEED_400, irq);
		
				fprintf (stderr, "Start ISO slave transmit... PREBUFFER=%d\n",PREBUFFER);
				raw1394_iso_xmit_start(slave_transmit_handle, -1, PREBUFFER);
			} else {
				fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
			}
		} else {
			fprintf (stderr, "Failed to get transmit libraw1394 handle\n %d: %s\nContinuing without transmitting\n",errno,strerror(errno));
		}
		
		fprintf (stderr, "Running raw1394_loop_iterate()...\n");
		
		int packetcount=0;

// polled receive loop
		while (!g_done) {
			int i=0,j=0;
			// wait for enough packets
			timestamp_start=getCurrentUTime();
			while ((raw1394_loop_iterate(master_receive_handle) == 0) && (master_receive_connection_info.packets < TABLE_SIZE)) {

				i++;
			}
			timestamp_wait_receive=getCurrentUTime();
			
			/*
			printf("LOOP:   i = %4d. packets = %4d, events = %4d, total_packets = %6d, total_events = %6d, dropped = %6d\n",
				i,
				master_receive_connection_info.packets,
				master_receive_connection_info.events,
				master_receive_connection_info.total_packets,
				master_receive_connection_info.total_events,
				master_receive_connection_info.dropped);
			*/
			//printf("        rb_readspace = %6d, rb_writespace = %6d\n",jack_ringbuffer_read_space(ringbuffer),jack_ringbuffer_write_space(ringbuffer));
				
			if(slave_transmit_handle) {
				j=0;
				while ((raw1394_loop_iterate(slave_transmit_handle) == 0) && (slave_transmit_connection_info.packets < master_receive_connection_info.packets )) {
	
					j++;
				}
				
				/*
				printf("        j = %4d. packets = %4d, events = %4d, total_packets = %6d, total_events = %6d, dropped = %6d\n",
					j,
					slave_transmit_connection_info.packets,
					slave_transmit_connection_info.events,
					slave_transmit_connection_info.total_packets,
					slave_transmit_connection_info.total_events,
					slave_transmit_connection_info.dropped);
				*/
				// consume
				slave_transmit_connection_info.total_packets+=slave_transmit_connection_info.packets;
				slave_transmit_connection_info.total_events+=slave_transmit_connection_info.events;
				slave_transmit_connection_info.packets=0;
				slave_transmit_connection_info.events=0;
					
			}
			timestamp_end_transmit=getCurrentUTime();
						
			// consume
			master_receive_connection_info.total_packets+=master_receive_connection_info.packets;
			master_receive_connection_info.total_events+=master_receive_connection_info.events;
			master_receive_connection_info.packets=0;
			master_receive_connection_info.events=0;
			
			fprintf(stderr,"%06.03f %06.03f %06.03f %06.03f\r",
				(float)(master_receive_connection_info.handler_time/master_receive_connection_info.total_packets/1000.0),
				(float)(slave_transmit_connection_info.handler_time/slave_transmit_connection_info.total_packets/1000.0),
				(float)(timestamp_wait_receive/1000.0-timestamp_start/1000.0),
				(float)(timestamp_end_transmit/1000.0-timestamp_wait_receive/1000.0)
				);
			//printf("        rb_readspace = %6d, rb_writespace = %6d\n",jack_ringbuffer_read_space(ringbuffer),jack_ringbuffer_write_space(ringbuffer));
		}


		fprintf (stderr, "Shutdown...\n");
		raw1394_iso_stop(master_receive_handle);
		raw1394_iso_shutdown(master_receive_handle);
		
		if (slave_transmit_handle) {		
			raw1394_iso_stop(slave_transmit_handle);
			raw1394_iso_shutdown(slave_transmit_handle);
		}

		fprintf (stderr, "Closing CMP connection...\n");
		
		
		
		//iec61883_cmp_disconnect (master_receive_handle, master_receive_node, master_receive_oplug, raw1394_get_local_id (master_receive_handle), master_receive_iplug, master_receive_iso_channel, master_receive_bandwidth);
		
		iec61883_plug_impr_close (plug_handle);
		iec61883_plug_ompr_close (plug_handle);

		raw1394_destroy_handle(master_receive_handle);
	
		
		if (slave_transmit_handle) {		
			raw1394_destroy_handle(slave_transmit_handle);
		}
		if (plug_handle) {		
				raw1394_destroy_handle (plug_handle);
		}

		jack_ringbuffer_free(ringbuffer);
		
	} else {
		fprintf (stderr, "Failed to get libraw1394 handle\n %d: %s\n",errno,strerror(errno));
		return -1;
	}
	
	return 0;
}
