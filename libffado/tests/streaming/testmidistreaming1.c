/*
 * Copyright (C) 2005-2007 by by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * FFADO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */


/**
 * Test application for the direct decode stream API
 * for floating point use
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <libiec61883/iec61883.h>
#include <stdio.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "libffado/ffado.h"

#include "debugtools.h"
#define IEC61883_AM824_LABEL_MIDI_NO_DATA 0x80 
#define IEC61883_AM824_LABEL_MIDI_1X      0x81 
#define IEC61883_AM824_LABEL_MIDI_2X      0x82
#define IEC61883_AM824_LABEL_MIDI_3X      0x83
	
#define ALSA_SEQ_BUFF_SIZE 1024
#define MAX_MIDI_PORTS   20
#define MIDI_TRANSMIT_BUFFER_SIZE 1024

int run;

static void sighandler (int sig)
{
	run = 0;
}

typedef struct {
	int seq_port_nr;
	snd_midi_event_t *parser;
	snd_seq_t *seq_handle;
} ffado_midi_port_t;

typedef struct {
	snd_seq_t *seq_handle;
	int nb_seq_ports;
	ffado_midi_port_t *ports[MAX_MIDI_PORTS];
} ffado_midi_ports_t;

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

void decode_midi_byte (ffado_midi_port_t *port, int byte) {
	snd_seq_event_t ev;
	if ((snd_midi_event_encode_byte(port->parser,byte, &ev)) > 0) {
// 		printf("message ok, sending it to %d\n", port->seq_port_nr);
		// a midi message is complete, send it out to ALSA
		snd_seq_ev_set_subs(&ev);  
		snd_seq_ev_set_direct(&ev);
		snd_seq_ev_set_source(&ev, port->seq_port_nr);
		snd_seq_event_output_direct(port->seq_handle, &ev);						
	}	else {
		
	}
}

int encode_midi_bytes(ffado_midi_port_t *port, unsigned char *byte_buff, int len) {
	return 0;
}

int main(int argc, char *argv[])
{

	#define PERIOD_SIZE 512

	int samplesread=0, sampleswritten=0;
	int nb_in_channels=0, nb_out_channels=0;
	int retval=0;
	int i=0;
	int start_flag = 0;

	int nb_periods=0;

	ffado_sample_t **audiobuffers_in;
	ffado_sample_t **audiobuffers_out;
	ffado_sample_t *nullbuffer;
	
	run=1;

	printf("Ffado MIDI streaming test application (1)\n");

	signal (SIGINT, sighandler);
	signal (SIGPIPE, sighandler);

	ffado_device_info_t device_info;

	ffado_options_t dev_options;

	dev_options.sample_rate=48000; // -1 = detect from discovery
	dev_options.period_size=PERIOD_SIZE;

	dev_options.nb_buffers=3;

	dev_options.port=0;
	dev_options.node_id=-1;
	
	dev_options.realtime=0;
	dev_options.packetizer_priority=60;
	
	dev_options.directions=0;
	
	dev_options.verbose=5;
        
        dev_options.slave_mode=0;
        dev_options.snoop_mode=0;

	ffado_device_t *dev=ffado_streaming_init(&device_info, dev_options);
	if (!dev) {
		fprintf(stderr,"Could not init Ffado Streaming layer\n");
		exit(-1);
	}

	nb_in_channels=ffado_streaming_get_nb_capture_streams(dev);
	nb_out_channels=ffado_streaming_get_nb_playback_streams(dev);

	int midi_in_nbchannels=0;
	int midi_out_nbchannels=0;
	
	/* allocate intermediate buffers */
	audiobuffers_in=calloc(nb_in_channels,sizeof(ffado_sample_t *));
	audiobuffers_out=calloc(nb_in_channels,sizeof(ffado_sample_t));
	for (i=0;i<nb_in_channels;i++) {
		audiobuffers_in[i]=calloc(PERIOD_SIZE+1,sizeof(ffado_sample_t));
		audiobuffers_out[i]=calloc(PERIOD_SIZE+1,sizeof(ffado_sample_t));
			
		switch (ffado_streaming_get_capture_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				/* assign the audiobuffer to the stream */
				ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
				ffado_streaming_set_capture_buffer_type(dev, i, ffado_buffer_type_float);
				break;
				
			// this is done with read/write routines because the nb of bytes can differ.
			case ffado_stream_type_midi:
				midi_in_nbchannels++;
			default:
				break;
		}
	}
	
	nullbuffer=calloc(PERIOD_SIZE+1,sizeof(ffado_sample_t));
	
	for (i=0;i<nb_out_channels;i++) {
		switch (ffado_streaming_get_capture_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				if (i<nb_in_channels) {
					/* assign the audiobuffer to the stream */
					ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
					ffado_streaming_set_playback_buffer_type(dev, i, ffado_buffer_type_float);
				} else {
					ffado_streaming_set_playback_stream_buffer(dev, i, (char *)nullbuffer);
					ffado_streaming_set_playback_buffer_type(dev, i, ffado_buffer_type_int24);	
				}
				break;
				// this is done with read/write routines because the nb of bytes can differ.
			case ffado_stream_type_midi:
				midi_out_nbchannels++;
			default:
				break;
		}
	}
	
	/* open the files to write to*/
	FILE* fid_out[nb_out_channels];
	FILE* fid_in[nb_in_channels];
	char name[256];

	for (i=0;i<nb_out_channels;i++) {
		snprintf(name,sizeof(name),"out_ch_%02d",i);

		fid_out[i]=fopen(name,"w");

		ffado_streaming_get_playback_stream_name(dev,i,name,sizeof(name));
		fprintf(fid_out[i],"Channel name: %s\n",name);
		switch (ffado_streaming_get_playback_stream_type(dev,i)) {
		case ffado_stream_type_audio:
			fprintf(fid_out[i],"Channel type: audio\n");
			break;
		case ffado_stream_type_midi:
			fprintf(fid_out[i],"Channel type: midi\n");
			break;
		case ffado_stream_type_unknown:
			fprintf(fid_out[i],"Channel type: unknown\n");
			break;
		default:
		case ffado_stream_type_invalid:
			fprintf(fid_out[i],"Channel type: invalid\n");
			break;
		}

	}
	for (i=0;i<nb_in_channels;i++) {
		snprintf(name,sizeof(name),"in_ch_%02d",i);
		fid_in[i]=fopen(name,"w");

		ffado_streaming_get_capture_stream_name(dev,i,name,sizeof(name));
		fprintf(fid_in[i], "Channel name: %s\n", name);
		switch (ffado_streaming_get_capture_stream_type(dev,i)) {
		case ffado_stream_type_audio:
			fprintf(fid_in[i], "Channel type: audio\n");
			break;
		case ffado_stream_type_midi:
			fprintf(fid_in[i], "Channel type: midi\n");
			break;
		case ffado_stream_type_unknown:
			fprintf(fid_in[i],"Channel type: unknown\n");
			break;
		default:
		case ffado_stream_type_invalid:
			fprintf(fid_in[i],"Channel type: invalid\n");
			break;
		}
	}

	// setup the ALSA midi seq clients
	snd_seq_t *seq_handle;
	int in_ports[midi_in_nbchannels], out_ports[midi_out_nbchannels];
	
    // open sequencer
	// the number of midi ports is statically set to 2
	if (open_seq(&seq_handle, in_ports, out_ports, midi_in_nbchannels, midi_out_nbchannels) < 0) {
		fprintf(stderr, "ALSA Error.\n");
		exit(1);
	}

	ffado_midi_port_t* midi_out_portmap[nb_out_channels];
	ffado_midi_port_t* midi_in_portmap[nb_in_channels];
	
	int cnt=0;
	
	for (i=0;i<nb_out_channels;i++) {
		ffado_midi_port_t *midi_out_port;
		switch (ffado_streaming_get_playback_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				midi_out_portmap[i]=NULL;
				break;
			case ffado_stream_type_midi:
				midi_out_port=malloc(sizeof(ffado_midi_port_t));
				if(!midi_out_port) {
					fprintf(stderr, "Could not allocate memory for MIDI OUT port %d\n",i);
					exit(1); // this could be/is a memory leak, I know...
				} else {
					midi_out_port->seq_port_nr=in_ports[cnt++];
					midi_out_port->seq_handle=seq_handle;
					if (snd_midi_event_new  ( ALSA_SEQ_BUFF_SIZE, &(midi_out_port->parser)) < 0) {
						fprintf(stderr, "ALSA Error: could not init parser for MIDI OUT port %d\n",i);
						exit(1); // this too
					}   
					midi_out_portmap[i]=midi_out_port;
				}
				break;
                        default: break;
		}
	}
	
	cnt=0;
	for (i=0;i<nb_in_channels;i++) {
		ffado_midi_port_t *midi_in_port;
		switch (ffado_streaming_get_capture_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				midi_in_portmap[i]=NULL;
				break;
			case ffado_stream_type_midi:
		
				midi_in_port=malloc(sizeof(ffado_midi_port_t));
				if(!midi_in_port) {
					fprintf(stderr, "Could not allocate memory for MIDI IN port %d\n",i);
					exit(1); // this could be/is a memory leak, I know...
				} else {
					midi_in_port->seq_port_nr=out_ports[cnt++];
					midi_in_port->seq_handle=seq_handle;
			
					if (snd_midi_event_new  ( ALSA_SEQ_BUFF_SIZE, &(midi_in_port->parser)) < 0) {
						fprintf(stderr, "ALSA Error: could not init parser for MIDI IN port %d\n",i);
						exit(1); // this too
					}   
					midi_in_portmap[i]=midi_in_port;
				}
				break;
                        default: break;
		}
	}	
	
	// start the streaming layer
	ffado_streaming_prepare(dev);
	start_flag = ffado_streaming_start(dev);

	fprintf(stderr,"Entering receive loop (%d,%d)\n",nb_in_channels,nb_out_channels);
	while(run && start_flag==0) {
		retval = ffado_streaming_wait(dev);
		if (retval < 0) {
			fprintf(stderr,"Xrun\n");
			ffado_streaming_reset(dev);
			continue;
		}
		
// 		ffado_streaming_transfer_buffers(dev);
		ffado_streaming_transfer_capture_buffers(dev);
		ffado_streaming_transfer_playback_buffers(dev);
		
		nb_periods++;

		if((nb_periods % 32)==0) {
// 			fprintf(stderr,"\r%05d periods",nb_periods);
		}

		for(i=0;i<nb_in_channels;i++) {
			int s;
			
			switch (ffado_streaming_get_capture_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				// no need to get the buffers manually, we have set the API internal buffers to the audiobuffer[i]'s
// 				//samplesread=ffado_streaming_read(dev, i, audiobuffer[i], PERIOD_SIZE);
				samplesread=PERIOD_SIZE;
				break;
			case ffado_stream_type_midi:
				samplesread=ffado_streaming_read(dev, i, audiobuffers_in[i], PERIOD_SIZE);
				quadlet_t *buff=(quadlet_t *)audiobuffers_in[i];
				for (s=0;s<samplesread;s++) {
					quadlet_t *byte=(buff+s) ;
//  					printf("%08X ",*byte);
					decode_midi_byte (midi_in_portmap[i], (*byte) & 0xFF);
				}
				if(samplesread>0) {
	   				fprintf(fid_in[i], "---- Period read (%d samples) ----\n",samplesread);
					hexDumpToFile(fid_in[i],(unsigned char*)audiobuffers_in[i],samplesread*sizeof(ffado_sample_t));
				}
				break;
                        default: break;
			}
//   			fprintf(fid_in[i], "---- Period read (%d samples) ----\n",samplesread);
//   			hexDumpToFile(fid_in[i],(unsigned char*)buff,samplesread*sizeof(ffado_sample_t));
	
		}

		for(i=0;i<nb_out_channels;i++) {
			ffado_sample_t *buff;
			int b=0;
			unsigned char* byte_buff;
			
			if (i<nb_in_channels) {
				buff=audiobuffers_out[i];
			} else {
				buff=nullbuffer;
			}
			
			switch (ffado_streaming_get_playback_stream_type(dev,i)) {
			case ffado_stream_type_audio:
//  				sampleswritten=ffado_streaming_write(dev, i, buff, PERIOD_SIZE);
//				sampleswritten=PERIOD_SIZE;
				break;
			case ffado_stream_type_midi:
				
				#define max_midi_bytes_to_write PERIOD_SIZE/8
				byte_buff=(unsigned char*)buff;
				
				sampleswritten=encode_midi_bytes(midi_out_portmap[i], byte_buff, max_midi_bytes_to_write);
				
				for(b=0;b<sampleswritten;b++) {
					ffado_sample_t tmp_event=*(byte_buff+b);
					ffado_streaming_write(dev, i, &tmp_event, 1);
				}
				
				
				fprintf(fid_out[i], "---- Period write (%d samples) ----\n",sampleswritten);
				hexDumpToFile(fid_out[i],(unsigned char*)buff,sampleswritten*sizeof(ffado_sample_t));
				break;
                        default: break;
			}
		}

	}

	fprintf(stderr,"\n");

	fprintf(stderr,"Exiting receive loop\n");
	
	ffado_streaming_stop(dev);

	ffado_streaming_finish(dev);

	for (i=0;i<nb_out_channels;i++) {
		fclose(fid_out[i]);

	}
	for (i=0;i<nb_in_channels;i++) {
		fclose(fid_in[i]);
	}
	
	for (i=0;i<nb_in_channels;i++) {
		free(audiobuffers_in[i]);
		free(audiobuffers_out[i]);
	}
	free(nullbuffer);
	free(audiobuffers_in);
	free(audiobuffers_out);
	
	// free the MIDI to seq parsers and port structures
	for(i=0;i<midi_in_nbchannels;i++) {
		ffado_midi_port_t *midi_in_port=midi_in_portmap[i];
		
		if(midi_in_port) {
			snd_midi_event_free  (  midi_in_port->parser );
			free(midi_in_port);
		}
	}
	// free the MIDI to seq parsers and port structures
	for(i=0;i<midi_out_nbchannels;i++) {
		ffado_midi_port_t *midi_out_port=midi_out_portmap[i];

		if(midi_out_port) {
			snd_midi_event_free  (  midi_out_port->parser );
			free(midi_out_port);
		}
	}

  return EXIT_SUCCESS;
}
