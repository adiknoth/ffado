/* 
 * FreeBob Test Program
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
 * ./test-freebob-iso -b 100 -i 10 -p 1024
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
	
	#define QUATAFIRE_DEFAULT_SAMPLE_RATE 48000
	#define QUATAFIRE_OUT_DIMENSION 11
	#define QUATAFIRE_IN_DIMENSION 7
	#define QUATAFIRE_INPUT_TARGET_CHANNEL 1
	#define QUATAFIRE_MIDI_OUT_STREAM_POSITION 10 
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

#else

#error Unknown bitfield type

#endif

inline unsigned long currentTime2() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_sec*1000+now.tv_usec/1000;
}

static int g_done = 0;

static void sighandler (int sig)
{
	g_done = 1;
}

static enum raw1394_iso_disposition 
iso_handler(raw1394handle_t handle, unsigned char *data, 
        unsigned int length, unsigned char channel,
        unsigned char tag, unsigned char sy, unsigned int cycle, 
        unsigned int dropped)
{
        int ret;
        static unsigned int counter = 0;

	struct iec61883_packet *packet = (struct iec61883_packet *) data;
	
	if (counter++>16) {
		printf("FDF code = %X. SYT = %10d, DBS = %10d, DBC = %10d\n", packet->fdf,packet->syt,packet->dbs,packet->dbc);
		counter=0;
	}
        /* write header */
 /*       write(file, &length, sizeof(length));
        write(file, &channel, sizeof(channel));
        write(file, &tag, sizeof(tag));
        write(file, &sy, sizeof(sy));
        sy = 0;
        write(file, &sy, sizeof(sy));

        while (length) {
                ret = write(file, data, length);
                if (ret < 0) {
                        perror("data write");
                        return RAW1394_ISO_ERROR;
                }

                length -= ret;
                data += ret;
        }
*/
        return RAW1394_ISO_OK;
}

//enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_DEFAULT;
enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_BUFFERFILL;
//enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_PACKET_PER_BUFFER;
//#define BUFFER 1000
//#define IRQ 20
//#define PACKET_MAX 8192

int main (int argc, char *argv[])
{
	raw1394handle_t handle = raw1394_new_handle_on_port (0);
	nodeid_t node = 0xffc0;
	FILE *f = NULL;
	int is_transmit = 0;
	int node_specified = 0;
	int i;
	int channel;
	int bandwidth = -1;
	int heartbeat=0;
	int iplug=-1;
	int oplug=-1;	
	int irq=100;
	int BUFFER=1000;
	int PACKET_MAX=1024;
	int retval;


	int fd=raw1394_get_fd(handle);
        struct pollfd pfd;

	u_int64_t listen_channels=0xFFFFFFFF;
	unsigned long which_port;

	signal (SIGINT, sighandler);
	signal (SIGPIPE, sighandler);
	
	for (i = 1; i < argc; i++) {
		
		if (strncmp (argv[i], "-h", 2) == 0 || 
			strncmp (argv[i], "--h", 3) == 0)
		{
			fprintf (stderr, 
			"usage: %s [[-r | -t] node-id]", argv[0]);
			raw1394_destroy_handle (handle);
			return 1;
		} else if (strncmp (argv[i], "-t", 2) == 0) {
			node |= atoi (argv[++i]);
			is_transmit = 1;
			node_specified = 1;
		} else if (strncmp (argv[i], "-r", 2) == 0) {
			node |= atoi (argv[++i]);
			is_transmit = 0;
			node_specified = 1;
		} else if (strncmp (argv[i], "-b", 2) == 0) {
			BUFFER = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-p", 2) == 0) {
			PACKET_MAX = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-i", 2) == 0) {
			irq = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-j", 2) == 0) {
			iplug = atoi (argv[++i]);
		} else if (strncmp (argv[i], "-k", 2) == 0) {
			oplug = atoi (argv[++i]);
		} else if (!node_specified) {
			is_transmit = 1;
		}
	}
		
	if (handle) {
		fprintf (stderr, "Init ISO receive handler...\n");

		if (mode == RAW1394_DMA_DEFAULT) {
			fprintf (stderr, "   RAW1394_DMA_DEFAULT (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",BUFFER,PACKET_MAX, irq);
			
			raw1394_iso_multichannel_recv_init(handle, iso_handler,
				BUFFER, PACKET_MAX, irq); /* >2048 makes rawiso stall! */
			raw1394_iso_recv_set_channel_mask(handle, listen_channels);
	
		} else {
			fprintf (stderr, "   other mode (BUFFER=%d,PACKET_MAX=%d,IRQ=%d)...\n",BUFFER,PACKET_MAX, irq);
			for (i = 0; i < 64; i++) {
				if (!(listen_channels & 1ULL << i))
					continue;
				raw1394_iso_recv_init(handle, iso_handler, BUFFER, PACKET_MAX,
					i, mode, irq);
			}
		}

		fprintf (stderr, "Start ISO receive...\n");
		raw1394_iso_recv_start(handle, -1, -1, 0);

		fprintf (stderr, "Create CMP connection...\n");
		//channel = iec61883_cmp_connect (handle, raw1394_get_local_id (handle),  &oplug, node, &iplug, &bandwidth);
		channel = iec61883_cmp_connect (handle, node ,  &oplug, raw1394_get_local_id (handle), &iplug, &bandwidth);
		if (channel > -1) {
			//amdtp_receive (handle, f, channel);
		} else {
			fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
		}
	
		fprintf (stderr, "Running raw1394_loop_iterate()...\n");
		
		int packetcount=0;

// polled receive loop
		while (!g_done) {
			int i=0;
			while ((raw1394_loop_iterate(handle) == 0) && (i<packetcount)) {

				i++;
			}
			//fprintf(stderr, "  %d packets received",i);
			
			pfd.fd = raw1394_get_fd(handle);
			pfd.events = POLLIN;
			pfd.revents = 0;
			/*retval = poll(&pfd, 1, 10);
			if (retval < 1) {
				fprintf(stderr, " - poll() timeout");

			} else {
				//packetcount=raw1394_iso_recv_packetcount(handle);
				fprintf(stderr, " - %d packets available after poll()",packetcount);
			}
			fprintf(stderr, "\r");*/
		}

/*
// simple receive loop
			while ((raw1394_loop_iterate(handle) == 0) && !g_done) {
				if(heartbeat) {
					fprintf (stderr, "\r°");
					heartbeat=0;
				} else {
					fprintf (stderr, "\r*");
					heartbeat=1;
				}
	
			}
*/

		fprintf (stderr, "Closing CMP connection...\n");
		iec61883_cmp_disconnect (handle, raw1394_get_local_id (handle), oplug, node, iplug, channel, bandwidth);

		fprintf (stderr, "Shutdown...\n");
	
		fprintf(stderr, "\n");
		raw1394_iso_shutdown(handle);
		raw1394_destroy_handle(handle);
	} else {
		fprintf (stderr, "Failed to get libraw1394 handle\n %d: %s\n",errno,strerror(errno));
		return -1;
	}
	
	return 0;
}
