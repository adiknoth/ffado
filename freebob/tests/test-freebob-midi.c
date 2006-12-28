/* 
 * FreeBob Test Program
 * Copyright (C) 2005 Pieter Palmers
 * Parts of this code (C) M. Nagorni
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

#include <libiec61883/iec61883.h>
#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

static int g_done = 0;

	// QUATAFIRE specific:
	#define QUATAFIRE_MIDI_IN_STREAM_POSITION 6 
	#define QUATAFIRE_MIDI_OUT_STREAM_POSITION 10 
	#define QUATAFIRE_DEFAULT_SAMPLE_RATE 48000
	#define QUATAFIRE_OUT_DIMENSION 11
	
	#define IEC61883_AM824_LABEL_MIDI_NO_DATA 0x80 
	#define IEC61883_AM824_LABEL_MIDI_1X      0x81 
	#define IEC61883_AM824_LABEL_MIDI_2X      0x82
	#define IEC61883_AM824_LABEL_MIDI_3X      0x83
	
	#define ALSA_SEQ_BUFF_SIZE 1024
	#define MAX_MIDI_PORTS   2
	#define MIDI_TRANSMIT_BUFFER_SIZE 1024

typedef struct {
	int seq_port_nr;
	snd_midi_event_t *parser;
} freebob_midi_port_t;

typedef struct {
	snd_seq_t *seq_handle;
	int nb_seq_ports;
	freebob_midi_port_t *ports[MAX_MIDI_PORTS];
} freebob_midi_ports_t;

int open_seq(snd_seq_t **seq_handle, int in_ports[], int out_ports[], int num_in, int num_out);

/* Open ALSA sequencer with num_in writeable ports and num_out readable ports. */
/* The sequencer handle and the port IDs are returned.                        */  
int open_seq(snd_seq_t **seq_handle, int in_ports[], int out_ports[], int num_in, int num_out) {

  int l1;
  char portname[64];

  if (snd_seq_open(seq_handle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
    fprintf(stderr, "Error opening ALSA sequencer.\n");
    return(-1);
  }
  snd_seq_set_client_name(*seq_handle, "FreeBob MIDI I/O test");
  for (l1 = 0; l1 < num_in; l1++) {
    sprintf(portname, "MIDI OUT %d", l1);
    if ((in_ports[l1] = snd_seq_create_simple_port(*seq_handle, portname,
              SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
              SND_SEQ_PORT_TYPE_MIDI_GENERIC)) < 0) {
      fprintf(stderr, "Error creating sequencer port.\n");
      return(-1);
    }
  }
  for (l1 = 0; l1 < num_out; l1++) {
    sprintf(portname, "MIDI IN %d", l1);
    if ((out_ports[l1] = snd_seq_create_simple_port(*seq_handle, portname,
              SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
              SND_SEQ_PORT_TYPE_MIDI_GENERIC)) < 0) {
      fprintf(stderr, "Error creating sequencer port.\n");
      return(-1);
    }
  }
  return(0);
}

static void sighandler (int sig)
{
	g_done = 1;
}

int fill_packet(iec61883_amdtp_t amdtp, 
		char *data, 
		int nevents, 
		unsigned int dbc,
		unsigned int dropped, 
		void *callback_data)
{

	snd_seq_event_t *ev;
	
	freebob_midi_ports_t *f = (freebob_midi_ports_t*) callback_data;
	
	static int total_packets = 0;
	int i;
	int j;
	char *current_pos_in_buffer=data;
	quadlet_t test=0;
	
	static int bytes_to_send=0;
	static int next_byte_to_send;
	static unsigned char work_buffer[MIDI_TRANSMIT_BUFFER_SIZE];
	
	int current_midi_port;
	
	for (i=0; i < nevents; ++i) { // cycle through all events requested
		
		current_midi_port=(dbc+i)%8; // calculate the current MIDI port
		
		current_pos_in_buffer=data+i*QUATAFIRE_OUT_DIMENSION*sizeof(quadlet_t);
			
		test = 0x40 << 24; // empty MBLA value	
		
		// fill up all streams with the empty value
		for (j=0; j<QUATAFIRE_OUT_DIMENSION;++j) {
			memcpy(current_pos_in_buffer+j*sizeof(quadlet_t),&test,sizeof(quadlet_t));
		}
		
		// position the pointer to the beginning of the midi data in this event
		current_pos_in_buffer=data+i*QUATAFIRE_OUT_DIMENSION*sizeof(quadlet_t)+QUATAFIRE_MIDI_OUT_STREAM_POSITION*sizeof(quadlet_t);
		
		// now fill in the the midi stream value
		/* this should be something like this: */
		
		// note: this is hacked to only send through MIDI OUT 1 because I don't know how to 
		//       make the ALSA sequencer deliver only the events of a specific port. therefore
		//       the port info is ignored, and all is sent through port 1
		//       I tried to make the code generic anyway.
		
		if ((current_midi_port==0)
		    && (current_midi_port < MAX_MIDI_PORTS) 
		    && f && f->ports[current_midi_port]
		    && f->ports[current_midi_port]->parser) {
		    
			// are we still sending a previously decoded event?
			if (bytes_to_send==0) { // get the next event, if present
				
				// get next event, if one is present
				if ((snd_seq_event_input(f->seq_handle, &ev) > 0)) {
					// decode it to the work buffer
					if((bytes_to_send = snd_midi_event_decode ( f->ports[current_midi_port]->parser, 
						work_buffer,
						MIDI_TRANSMIT_BUFFER_SIZE, 
						ev))<0) 
					{ // failed
    						fprintf (stderr, "Error decoding event for port %d (errcode=%d)\n", current_midi_port,bytes_to_send);
						bytes_to_send=0;
						//return -1;
					} 
					// decoding succeeded
					next_byte_to_send=0;
				} else {
					// do nothing
				}
			}
			// check if we need to send a byte
			if (bytes_to_send>0) {
				test = 0x81 << 24 | (work_buffer[next_byte_to_send] << 16);
				//fprintf (stderr, "sending 0x%08X\n", test);
				
				bytes_to_send--;
				next_byte_to_send++;
			
			} else {
				// send empty data
				test = 0x80 << 24;
			}
			
		
		} else if (current_midi_port==1) {
			//test= 0x81 << 24 | 0xFE0000;  // send out a 0xFE message
			test = 0x80 << 24;
		} else {
			test = 0x80 << 24;
		}
		
		/*
		but for now this will have to do:
		*/
		/*if (current_midi_port == 1) {
			test= 0x81 << 24 | 0xFE0000; // compose the event
		} else {
			test= 0x80 << 24; // compose the event
		}*/
		
		memcpy(current_pos_in_buffer,&test,sizeof(quadlet_t));
		
	}

  total_packets++;
  
  if ((total_packets & 0xfff) == 0) {
    fprintf (stderr, "\r%10d packets", total_packets);
    fflush (stderr);
  }

  return 0;
}

static int read_packet(iec61883_amdtp_t amdtp, 
		       char *data, 
		       int nsamples, 
		       unsigned int dbc,
		       unsigned int dropped,
		       void *callback_data)
{
	freebob_midi_ports_t *f = (freebob_midi_ports_t*) callback_data;
	snd_seq_event_t ev;
	
	quadlet_t *events=(quadlet_t*) data;
	quadlet_t this_event;
	
	static int total_packets = 0;
	
	int i, dimension;
	
	dimension = iec61883_amdtp_get_dimension(amdtp);

	// as long as the device sends clusters of 8 events, the dbc=0 is ok
	// DBC should be available though
	
	if (total_packets == 0)
		fprintf (stderr, "channels=%d",
			dimension);
			
	if (f) { // if the callback data pointer is valid
		for (i=0; i<nsamples;i += dimension) { // cycle through the events
		
			// select the midi stream data from the current event
			this_event=events[i + QUATAFIRE_MIDI_IN_STREAM_POSITION];
			
			// calculate the midi port for this data
			// XXX: I'm not sure if this should be (DBC+i)%8 or DBC%8 according to spec
			//      experiments show that it should be (DBC+i)%8 for freebob
			int midi_port=(dbc+i)%8;
			
			if ((this_event & 0xFF000000) == (IEC61883_AM824_LABEL_MIDI_NO_DATA<<24)) {
				continue;  // (MIDI NO-DATA is ignored)
			} else if(
				((this_event & 0xFF000000) == (IEC61883_AM824_LABEL_MIDI_1X<<24)) ||
				((this_event & 0xFF000000) == (IEC61883_AM824_LABEL_MIDI_2X<<24)) ||
				((this_event & 0xFF000000) == (IEC61883_AM824_LABEL_MIDI_3X<<24))
				) { // The event is a MIDI event
				
				// get the number of valid bytes in the MIDI event
				// one MIDI data event can contain up to 3 MIDI data bytes
				// remark: this supports MIDI x3 speed mode, which isn't done by freebob (yet)
				int nbbytes=(this_event & 0x0F000000) >>24;
				
				if (nbbytes && midi_port < MAX_MIDI_PORTS && f->ports[midi_port]) {
					// read the bytes
					while(nbbytes--) {
						/* the next line replaces:
							int byte1=(this_event & 0x00FF0000) >>16;
							int byte2=(this_event & 0x0000FF00) >>8;
							int byte3=(this_event & 0x000000FF);
							
							remark: it isn't possible to have e.g. byte2 valid without byte1 valid
						*/
						int byte=(this_event >> (8 * (2-nbbytes))) & 0xFF;
						if ((snd_midi_event_encode_byte(f->ports[midi_port]->parser,byte, &ev)) > 0) {
							// a midi message is complete, send it out to ALSA
							snd_seq_ev_set_subs(&ev);  
							snd_seq_ev_set_direct(&ev);
							snd_seq_ev_set_source(&ev, f->ports[midi_port]->seq_port_nr);
							snd_seq_event_output_direct(f->seq_handle, &ev);						
						}
					}
				}
			} else {
				fprintf (stderr, "Receive error: not a MIDI event 0x%08X\n", this_event);
			}
		}
	}
	
	total_packets++;
	
	if ((total_packets & 0xfff) == 0) {
		fprintf (stderr, "\r%10d packets, %d samples/packet (last packet)", total_packets,nsamples);
		fflush (stderr);
	}
	return 0;
}

static void amdtp_receive( raw1394handle_t handle, freebob_midi_ports_t *f, int channel)
{	
	iec61883_amdtp_t amdtp = iec61883_amdtp_recv_init (handle, read_packet, (void *)f );
	
	// lowering the buffers is nescessary in order to get rid of the lag.
	// should be as low as possible i guess, but i can't get much lower than 300
	// maybe RT priority will help at some point?
	iec61883_amdtp_set_buffers(amdtp,300);
		
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

static void amdtp_transmit( raw1394handle_t handle, freebob_midi_ports_t *f, int channel)
{	
	iec61883_amdtp_t amdtp;
		
	amdtp = iec61883_amdtp_xmit_init (handle, QUATAFIRE_DEFAULT_SAMPLE_RATE, 
					  IEC61883_AMDTP_FORMAT_RAW, 
					  IEC61883_AMDTP_INPUT_LE24,
					  IEC61883_MODE_BLOCKING_EMPTY, 
					  QUATAFIRE_OUT_DIMENSION, 
					  fill_packet, (void *)f );
	
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

	int iplug=-1;
	int oplug=-1;	

	int is_transmit = 0;
	int node_specified = 0;
	int i;
	int channel;
	int bandwidth = -1;
	
	// setup the ALSA midi seq clients
	snd_seq_t *seq_handle;
	int in_ports[MAX_MIDI_PORTS], out_ports[MAX_MIDI_PORTS];
	
	freebob_midi_ports_t midi_in_ports;
	freebob_midi_ports_t midi_out_ports;
	
	for (i = 1; i < argc; i++) {
		
		if (strncmp (argv[i], "-h", 2) == 0 || 
			strncmp (argv[i], "--h", 3) == 0)
		{
			fprintf (stderr, 
			"usage: %s [[-r | -t] node-id]\n", argv[0]);
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
		} else if (!node_specified) {
			is_transmit = 1;
		}
	}
  
    // open sequencer
	// the number of midi ports is statically set to 2
	if (open_seq(&seq_handle, in_ports, out_ports, 2, 2) < 0) {
		fprintf(stderr, "ALSA Error.\n");
		raw1394_destroy_handle (handle);
		exit(1);
	}
	
	for(i=0;i<MAX_MIDI_PORTS;i++) {
		freebob_midi_port_t *midi_in_port;
		freebob_midi_port_t *midi_out_port;
		
		midi_in_port=malloc(sizeof(freebob_midi_port_t));
		if(!midi_in_port) {
			fprintf(stderr, "Could not allocate memory for MIDI IN port %d\n",i);
			exit(1); // this could be/is a memory leak, I know...
		} else {
			midi_in_port->seq_port_nr=out_ports[i];
			midi_in_ports.ports[i]=midi_in_port;
			if (snd_midi_event_new  ( ALSA_SEQ_BUFF_SIZE, &(midi_in_port->parser)) < 0) {
				fprintf(stderr, "ALSA Error: could not init parser for MIDI IN port %d\n",i);
				exit(1); // this too
			}   
		}
		
		midi_out_port=malloc(sizeof(freebob_midi_port_t));
		if(!midi_out_port) {
			fprintf(stderr, "Could not allocate memory for MIDI OUT port %d\n",i);
			exit(1); // this could be/is a memory leak, I know...
		} else {
			midi_out_port->seq_port_nr=in_ports[i];
			midi_out_ports.ports[i]=midi_out_port;
			if (snd_midi_event_new  ( ALSA_SEQ_BUFF_SIZE, &(midi_out_port->parser)) < 0) {
				fprintf(stderr, "ALSA Error: could not init parser for MIDI OUT port %d\n",i);
				exit(1); // this too
			}   
		}
	}
	
	midi_in_ports.nb_seq_ports=MAX_MIDI_PORTS;
	midi_in_ports.seq_handle=seq_handle;
	
	midi_out_ports.nb_seq_ports=MAX_MIDI_PORTS;
	midi_out_ports.seq_handle=seq_handle;

	if (handle) {
		if (is_transmit) {
			if (node_specified) {
				channel = iec61883_cmp_connect (handle,
					raw1394_get_local_id (handle), &oplug, node, &iplug, &bandwidth);
				if (channel > -1) {
					amdtp_transmit (handle, &midi_out_ports, channel);
					iec61883_cmp_disconnect (handle,
						raw1394_get_local_id (handle), oplug, node, iplug, channel, bandwidth);
				} else {
					fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
					amdtp_transmit (handle, &midi_out_ports, 63);
				}
			} else {
				amdtp_transmit (handle, &midi_out_ports, 63);
			}
		} else {
			if (node_specified) {
				channel = iec61883_cmp_connect (handle, node, &oplug, 
					raw1394_get_local_id (handle), &iplug, &bandwidth);
				if (channel > -1) {
					amdtp_receive (handle, &midi_in_ports, channel);
					iec61883_cmp_disconnect (handle, node, oplug,
						raw1394_get_local_id (handle), iplug, channel, bandwidth);
				} else {
					fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
					amdtp_receive (handle, &midi_in_ports, 63);
				}
			} else {
				amdtp_receive (handle, &midi_in_ports, 63);
			}
		}
		raw1394_destroy_handle (handle);
	} else {
		fprintf (stderr, "Failed to get libraw1394 handle\n");
		return -1;
	}
	
	// free the MIDI to seq parsers and port structures
	for(i=0;i<MAX_MIDI_PORTS;i++) {
		freebob_midi_port_t *midi_in_port=midi_in_ports.ports[i];
		freebob_midi_port_t *midi_out_port=midi_out_ports.ports[i];
		
		if(midi_in_port) {
			snd_midi_event_free  (  midi_in_port->parser );
			free(midi_in_port);
		}
		if(midi_out_port) {
			snd_midi_event_free  (  midi_out_port->parser );
			free(midi_out_port);
		}
	}
	
	return 0;
}
