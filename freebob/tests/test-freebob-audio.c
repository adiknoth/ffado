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
 * To receive a stream (number is hardcoded by QUATAFIRE_INPUT_TARGET_CHANNEL) from the device at node 0
 * and play it out through /dev/dsp using sox play
 *
 * ./test-freebob-audio -r 0 - | play -r 48000 -t raw -s w -f s -c 1 -   
 *
 */

/* To play a file on all outputs of the device at node 0
 *
 * sox test.wav -r 48000 -t raw -s -w -c 1 - | ./test-freebob-audio -t 0 - 
 *
 */
  
#include <libiec61883/iec61883.h>
#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <time.h>
	
	#define QUATAFIRE_DEFAULT_SAMPLE_RATE 48000
	#define QUATAFIRE_OUT_DIMENSION 11
	#define QUATAFIRE_IN_DIMENSION 7
	#define QUATAFIRE_INPUT_TARGET_CHANNEL 1
	#define QUATAFIRE_MIDI_OUT_STREAM_POSITION 10 

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

int fill_packet(char *data, int nevents, unsigned int dropped, void *callback_data)
{
  FILE *fp = (FILE *) callback_data;
  
  static int total_packets = 0;
	
	static int prev_total_packets=0;
	static unsigned long prevtime=0;
	float packets_per_second;
	unsigned long thistime=currentTime2();
	
	int i;
	int j;
	char *current_pos=data;
	quadlet_t test=0;
	short int sample;
	
	for (i=0; i < nevents; ++i) {
		current_pos=data+i*QUATAFIRE_OUT_DIMENSION*4;
		
		// read in the sample from the file
		if (fread((char *)(&sample), 2, 1, fp) != 1)
			return -1;	
		
		// make it 24 bit
		test = 0x40 << 24 | sample<<8;	
		
		// copy to all channels
		for (j=0; j<QUATAFIRE_OUT_DIMENSION;++j) {
			memcpy(current_pos+j*4,&test,4);
		}
		
		// make sure we don't start sending random midi values
		current_pos=data+i*QUATAFIRE_OUT_DIMENSION*4+QUATAFIRE_MIDI_OUT_STREAM_POSITION*4;
		test= 0x80 << 24 | 0x000000;
		memcpy(current_pos,&test,4);
		
	}

	total_packets++;
	if ((total_packets & 0xfff) == 0) {
		packets_per_second = (total_packets-prev_total_packets)/((float)(thistime-prevtime))*1000.0;
		fprintf (stderr, "\r%10d packets (%5.2f packets/sec, %5d dropped, %d events in last packet, packet dimension=%d)", total_packets, packets_per_second, dropped, nevents, QUATAFIRE_OUT_DIMENSION);
		fflush (stderr);
		prevtime=thistime;
		prev_total_packets=total_packets;
	}

  return 0;
}

static int read_packet(char *data, int nevents, int dimension, int rate,
	enum iec61883_amdtp_format format, 
	enum iec61883_amdtp_sample_format sample_format,
	unsigned int dropped,
	void *callback_data)
{
	FILE *f = (FILE*) callback_data;
	
	static int total_packets = 0;
	static int prev_total_packets=0;
	static unsigned long prevtime=0;
	float packets_per_second;
	int i;
		
	short int value;
	unsigned long thistime=currentTime2();
	
	quadlet_t this_event;
		
	quadlet_t *events=(quadlet_t*) data;
		
	if (total_packets == 0)
		fprintf (stderr, "format=0x%x sample_format=0x%x channels=%d rate=%d\n",
			format, sample_format, dimension, rate);
	
	for (i=0; i<nevents;i++) {
		// no label check is done, so be carefull for the speakers!
		this_event=events[i*dimension+QUATAFIRE_INPUT_TARGET_CHANNEL];
		// truncate to 16 bits
		value=(this_event & 0x00FFFFFF)>>8;
		
		if (fwrite(&value, 2, 1, f) != 1) {
			return -1;
		}
	}
	
	total_packets++;
	if ((total_packets & 0xfff) == 0) {
		packets_per_second = (total_packets-prev_total_packets)/((float)(thistime-prevtime))*1000.0;
		fprintf (stderr, "\r%10d packets (%5.2f packets/sec, %5d dropped, %d events in last packet, packet dimension=%d)", total_packets, packets_per_second, dropped, nevents, dimension);
		fflush (stderr);
		prevtime=thistime;
		prev_total_packets=total_packets;
	}
	return 0;
}

static void amdtp_receive( raw1394handle_t handle, FILE *f, int channel)
{	
	iec61883_amdtp_t amdtp = iec61883_amdtp_recv_init (handle, read_packet, (void *)f );
		
	if ( amdtp && iec61883_amdtp_recv_start (amdtp, channel) == 0)
	{
		int fd = raw1394_get_fd (handle);
		struct timeval tv;
		fd_set rfds;
		int result = 0;
		
		signal (SIGINT, sighandler);
		signal (SIGPIPE, sighandler);
		fprintf (stderr, "Starting to receive\n");

		do {
			FD_ZERO (&rfds);
			FD_SET (fd, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 20000;
			
			if (select (fd + 1, &rfds, NULL, NULL, &tv) > 0)
				result = raw1394_loop_iterate (handle);
			
		} while (g_done == 0 && result == 0);
		
		fprintf (stderr, "\ndone.\n");
	}
	iec61883_amdtp_close (amdtp);
}

static void amdtp_transmit( raw1394handle_t handle, FILE *f, int channel)
{	
	iec61883_amdtp_t amdtp;
	
	amdtp = iec61883_amdtp_xmit_init (handle, 48000, IEC61883_AMDTP_FORMAT_RAW,
		IEC61883_MODE_BLOCKING, QUATAFIRE_OUT_DIMENSION, fill_packet, (void *)f );
	
	if (amdtp && iec61883_amdtp_xmit_start (amdtp, channel) == 0)
	{
		int fd = raw1394_get_fd (handle);
		struct timeval tv;
		fd_set rfds;
		int result = 0;
		
		signal (SIGINT, sighandler);
		signal (SIGPIPE, sighandler);
		fprintf (stderr, "Starting to transmit\n");

		do {
			FD_ZERO (&rfds);
			FD_SET (fd, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 20000;
			
			if (select (fd + 1, &rfds, NULL, NULL, &tv) > 0)
				result = raw1394_loop_iterate (handle);
			
		} while (g_done == 0 && result == 0);
		
		fprintf (stderr, "\ndone.\n");
	}
	iec61883_amdtp_close (amdtp);
}

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
	
	for (i = 1; i < argc; i++) {
		
		if (strncmp (argv[i], "-h", 2) == 0 || 
			strncmp (argv[i], "--h", 3) == 0)
		{
			fprintf (stderr, 
			"usage: %s [[-r | -t] node-id] [- | file]\n"
			"       All audio data must be signed 16bit 48KHz stereo PCM\n"
			"       Use - to transmit raw PCM from stdin, or\n"
			"       supply a filename to to transmit from a raw PCM file.\n"
			"       Otherwise, capture raw PCM to stdout.\n", argv[0]);
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
		} else if (strcmp (argv[i], "-") != 0) {
			if (node_specified && !is_transmit)
				f = fopen (argv[i], "wb");
			else {
				f = fopen (argv[i], "rb");
				is_transmit = 1;
			}
		} else if (!node_specified) {
			is_transmit = 1;
		}
	}
		
	if (handle) {
		if (is_transmit) {
			if (f == NULL)
				f = stdin;
			if (node_specified) {
				channel = iec61883_cmp_connect (handle,
					raw1394_get_local_id (handle), node, &bandwidth);
				if (channel > -1) {
					amdtp_transmit (handle, f, channel);
					iec61883_cmp_disconnect (handle,
						raw1394_get_local_id (handle), node, channel, bandwidth);
				} else {
					fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
					amdtp_transmit (handle, f, 63);
				}
			} else {
				amdtp_transmit (handle, f, 63);
			}
			if (f != stdin)
				fclose (f);
		} else {
			if (f == NULL)
				f = stdout;
			if (node_specified) {
				channel = iec61883_cmp_connect (handle, node, 
					raw1394_get_local_id (handle), &bandwidth);
				if (channel > -1) {
					amdtp_receive (handle, f, channel);
					iec61883_cmp_disconnect (handle, node, 
						raw1394_get_local_id (handle), channel, bandwidth);
				} else {
					fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
					amdtp_receive (handle, f, 63);
				}
			} else {
				amdtp_receive (handle, f, 63);
			}
			if (f != stdout)
				fclose (f);
		}
		raw1394_destroy_handle (handle);
	} else {
		fprintf (stderr, "Failed to get libraw1394 handle\n");
		return -1;
	}
	
	return 0;
}
