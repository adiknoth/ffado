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

#include "libffado/ffado.h"

#include "debugtools.h"

int run;

static void sighandler (int sig)
{
	run = 0;
}

int main(int argc, char *argv[])
{

	#define PERIOD_SIZE 256

	int samplesread=0;
// 	int sampleswritten=0;
	int nb_in_channels=0, nb_out_channels=0;
	int retval=0;
	int i=0;
	int start_flag = 0;

	int nb_periods=0;

	float **audiobuffers_in;
	ffado_sample_t **audiobuffers_out;
	ffado_sample_t *nullbuffer;
	
	run=1;

	printf("Ffado streaming test application (3)\n");

	signal (SIGINT, sighandler);
	signal (SIGPIPE, sighandler);

	ffado_device_info_t device_info;
	memset(&device_info,0,sizeof(ffado_device_info_t));

	ffado_options_t dev_options;
	memset(&dev_options,0,sizeof(ffado_options_t));

	dev_options.sample_rate=44100;
	dev_options.period_size=PERIOD_SIZE;

	dev_options.nb_buffers=3;

	dev_options.realtime=1;
	dev_options.packetizer_priority=70;
	
	dev_options.verbose=5;
        
        dev_options.slave_mode=0;
        dev_options.snoop_mode=0;
	
	ffado_device_t *dev=ffado_streaming_init(device_info, dev_options);

	if (!dev) {
		fprintf(stderr,"Could not init Ffado Streaming layer\n");
		exit(-1);
	}

	nb_in_channels=ffado_streaming_get_nb_capture_streams(dev);
	nb_out_channels=ffado_streaming_get_nb_playback_streams(dev);

	/* allocate intermediate buffers */
	audiobuffers_in=calloc(nb_in_channels,sizeof(float *));
	audiobuffers_out=calloc(nb_in_channels,sizeof(ffado_sample_t));
	for (i=0;i<nb_in_channels;i++) {
		audiobuffers_in[i]=calloc(PERIOD_SIZE+1,sizeof(float));
		audiobuffers_out[i]=calloc(PERIOD_SIZE+1,sizeof(ffado_sample_t));
			
		switch (ffado_streaming_get_capture_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				/* assign the audiobuffer to the stream */
				ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
				ffado_streaming_set_capture_buffer_type(dev, i, ffado_buffer_type_float);
				ffado_streaming_playback_stream_onoff(dev, i, 1);
				break;
				// this is done with read/write routines because the nb of bytes can differ.
			case ffado_stream_type_midi:
			default:
				break;
		}
	}
	
	nullbuffer=calloc(PERIOD_SIZE+1,sizeof(ffado_sample_t));
	
	for (i=0;i<nb_out_channels;i++) {
		switch (ffado_streaming_get_playback_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				if (i<nb_in_channels) {
					/* assign the audiobuffer to the stream */
					ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
					ffado_streaming_set_playback_buffer_type(dev, i, ffado_buffer_type_float);
					ffado_streaming_playback_stream_onoff(dev, i, 1);
				} else {
					ffado_streaming_set_playback_stream_buffer(dev, i, (char *)nullbuffer);
					ffado_streaming_set_playback_buffer_type(dev, i, ffado_buffer_type_float);
					ffado_streaming_playback_stream_onoff(dev, i, 1);
				}
				break;
				// this is done with read/write routines because the nb of bytes can differ.
			case ffado_stream_type_midi:
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
		fprintf(fid_in[i], "Channel name: %s\n",name);
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
			
			
			switch (ffado_streaming_get_capture_stream_type(dev,i)) {
			case ffado_stream_type_audio:
				// no need to get the buffers manually, we have set the API internal buffers to the audiobuffer[i]'s
// 				//samplesread=freebob_streaming_read(dev, i, audiobuffer[i], PERIOD_SIZE);
				samplesread=PERIOD_SIZE;
				break;
			case ffado_stream_type_midi:
				samplesread=ffado_streaming_read(dev, i, audiobuffers_out[i], PERIOD_SIZE);
				break;
                        default: break;
			}
	
//   			fprintf(fid_in[i], "---- Period read  (%d samples) ----\n",samplesread);
//   			hexDumpToFile(fid_in[i],(unsigned char*)audiobuffers_in[i],samplesread*sizeof(float)+1);
		}

		for(i=0;i<nb_out_channels;i++) {
			ffado_sample_t *buff;
			int sampleswritten=0;
			if (i<nb_in_channels) {
				buff=audiobuffers_out[i];
			} else {
				buff=nullbuffer;
			}
			
			switch (ffado_streaming_get_playback_stream_type(dev,i)) {
			case ffado_stream_type_audio:
//  				sampleswritten=freebob_streaming_write(dev, i, buff, PERIOD_SIZE);
				sampleswritten=PERIOD_SIZE;
				break;
			case ffado_stream_type_midi:
// 				sampleswritten=freebob_streaming_write(dev, i, buff, PERIOD_SIZE);
				break;
                        default: break;
			}
//   			fprintf(fid_out[i], "---- Period write (%d samples) ----\n",sampleswritten);
//   			hexDumpToFile(fid_out[i],(unsigned char*)buff,sampleswritten*sizeof(ffado_sample_t));
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

  return EXIT_SUCCESS;
}
