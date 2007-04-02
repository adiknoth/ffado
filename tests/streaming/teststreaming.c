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
 * Test application for the per-stream decode API
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include "libfreebob/freebob_streaming.h"

#include "debugtools.h"

int run;

static void sighandler (int sig)
{
	run = 0;
}

int main(int argc, char *argv[])
{

	#define PERIOD_SIZE 1024

	int samplesread=0, sampleswritten=0;
	int nb_in_channels=0, nb_out_channels=0;
	int retval=0;
	int i=0;

	int nb_periods=0;

	freebob_sample_t **audiobuffer;
	freebob_sample_t *nullbuffer;
	
	run=1;

	printf("Freebob streaming test application\n");

	signal (SIGINT, sighandler);
	signal (SIGPIPE, sighandler);

	freebob_device_info_t device_info;

	freebob_options_t dev_options;

	dev_options.sample_rate=44100;
	dev_options.period_size=PERIOD_SIZE;

	dev_options.nb_buffers=3;

	dev_options.port=0;
	dev_options.node_id=-1;
	
	dev_options.realtime=0;
	dev_options.packetizer_priority=60;

	freebob_device_t *dev=freebob_streaming_init(&device_info, dev_options);
	if (!dev) {
		fprintf(stderr,"Could not init Freebob Streaming layer\n");
		exit(-1);
	}

	nb_in_channels=freebob_streaming_get_nb_capture_streams(dev);
	nb_out_channels=freebob_streaming_get_nb_playback_streams(dev);

	/* allocate intermediate buffers */
	audiobuffer=calloc(nb_in_channels,sizeof(freebob_sample_t *));
	for (i=0;i<nb_in_channels;i++) {
		audiobuffer[i]=calloc(PERIOD_SIZE+1,sizeof(freebob_sample_t));
	}
	
	nullbuffer=calloc(PERIOD_SIZE+1,sizeof(freebob_sample_t));
	
	/* open the files to write to*/
	FILE* fid_out[nb_out_channels];
	FILE* fid_in[nb_in_channels];
	char name[256];

	for (i=0;i<nb_out_channels;i++) {
		snprintf(name,sizeof(name),"out_ch_%02d",i);

		fid_out[i]=fopen(name,"w");

		freebob_streaming_get_playback_stream_name(dev,i,name,sizeof(name));
		fprintf(fid_out[i],"Channel name: %s\n",name);
		switch (freebob_streaming_get_playback_stream_type(dev,i)) {
		case freebob_stream_type_audio:
			fprintf(fid_out[i],"Channel type: audio\n");
			break;
		case freebob_stream_type_midi:
			fprintf(fid_out[i],"Channel type: midi\n");
			break;
		case freebob_stream_type_unknown:
			fprintf(fid_out[i],"Channel type: unknown\n");
			break;
		default:
		case freebob_stream_type_invalid:
			fprintf(fid_out[i],"Channel type: invalid\n");
			break;
		}

	}
	for (i=0;i<nb_in_channels;i++) {
		snprintf(name,sizeof(name),"in_ch_%02d",i);
		fid_in[i]=fopen(name,"w");

		freebob_streaming_get_capture_stream_name(dev,i,name,sizeof(name));
		fprintf(fid_in[i], "Channel name: %s\n",name);
		switch (freebob_streaming_get_capture_stream_type(dev,i)) {
		case freebob_stream_type_audio:
			fprintf(fid_in[i], "Channel type: audio\n");
			break;
		case freebob_stream_type_midi:
			fprintf(fid_in[i], "Channel type: midi\n");
			break;
		case freebob_stream_type_unknown:
			fprintf(fid_in[i],"Channel type: unknown\n");
			break;
		default:
		case freebob_stream_type_invalid:
			fprintf(fid_in[i],"Channel type: invalid\n");
			break;
		}
	}

	freebob_streaming_prepare(dev);
	freebob_streaming_start(dev);

	fprintf(stderr,"Entering receive loop (%d,%d)\n",nb_in_channels,nb_out_channels);
	while(run) {
		retval = freebob_streaming_wait(dev);
		if (retval < 0) {
			fprintf(stderr,"Xrun\n");
			freebob_streaming_reset(dev);
			continue;
		}

// 		freebob_streaming_transfer_buffers(dev);
		freebob_streaming_transfer_capture_buffers(dev);
		freebob_streaming_transfer_playback_buffers(dev);
		
		nb_periods++;

		if((nb_periods % 32)==0) {
			fprintf(stderr,"\r%05d periods",nb_periods);
		}

		for(i=0;i<nb_in_channels;i++) {
			switch (freebob_streaming_get_capture_stream_type(dev,i)) {
			case freebob_stream_type_audio:
				samplesread=freebob_streaming_read(dev, i, audiobuffer[i], PERIOD_SIZE);
				break;
			case freebob_stream_type_midi:
				samplesread=freebob_streaming_read(dev, i, audiobuffer[i], PERIOD_SIZE);
				break;
			default:
				;
			}
//   			fprintf(fid_in[i], "---- Period read  (%d samples) ----\n",samplesread);
//   			hexDumpToFile(fid_in[i],(unsigned char*)audiobuffer[i],samplesread*sizeof(freebob_sample_t));
		}

		for(i=0;i<nb_out_channels;i++) {
			freebob_sample_t *buff;
			if (i<nb_in_channels) {
				buff=audiobuffer[i];
			} else {
				buff=nullbuffer;
			}
			
			switch (freebob_streaming_get_playback_stream_type(dev,i)) {
			case freebob_stream_type_audio:
				sampleswritten=freebob_streaming_write(dev, i, buff, PERIOD_SIZE);
				break;
			case freebob_stream_type_midi:
				sampleswritten=freebob_streaming_write(dev, i, buff, PERIOD_SIZE);
				break;
			default:
				;
			}
//  			fprintf(fid_out[i], "---- Period write (%d samples) ----\n",sampleswritten);
//  			hexDumpToFile(fid_out[i],(unsigned char*)buff,sampleswritten*sizeof(freebob_sample_t));
		}

	}

	fprintf(stderr,"\n");

	fprintf(stderr,"Exiting receive loop\n");
	
	freebob_streaming_stop(dev);

	freebob_streaming_finish(dev);

	for (i=0;i<nb_out_channels;i++) {
		fclose(fid_out[i]);

	}
	for (i=0;i<nb_in_channels;i++) {
		fclose(fid_in[i]);
	}
	
	for (i=0;i<nb_in_channels;i++) {
		free(audiobuffer[i]);
	}
	free(nullbuffer);
	free(audiobuffer);

  return EXIT_SUCCESS;
}
