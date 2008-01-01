/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
#include <sched.h>

#include "libffado/ffado.h"

#include "debugtools.h"

#include <math.h>

int run;

int set_realtime_priority(unsigned int prio)
{
  if (prio > 0) {
    struct sched_param schp;
    /*
     * set the process to realtime privs
     */
    memset(&schp, 0, sizeof(schp));
    schp.sched_priority = prio;
    
    if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0) {
      perror("sched_setscheduler");
      exit(1);
    }
  } else {
        struct sched_param schp;
        /*
        * set the process to realtime privs
        */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = prio;
        
        if (sched_setscheduler(0, SCHED_OTHER, &schp) != 0) {
        perror("sched_setscheduler");
        exit(1);
        }
    
  }
  return 0;
}

static void sighandler (int sig)
{
    run = 0;
    set_realtime_priority(0);
}

int main(int argc, char *argv[])
{

    #define PERIOD_SIZE 1024
    #define TEST_FREQ 1000.0
    #define do_test_tone 1

    int samplesread=0;
//     int sampleswritten=0;
    int nb_in_channels=0, nb_out_channels=0;
    int retval=0;
    int i=0;
    int start_flag = 0;

    float frame_counter = 0.0;
    float sine_advance = 0.0;
    float amplitude = 0.97;
    
    int nb_periods=0;

    int min_ch_count=0;

    float **audiobuffers_in;
    float **audiobuffers_out;
    float *nullbuffer;
    
    run=1;

    printf("FFADO streaming test application (3)\n");

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
    dev_options.packetizer_priority=60;
    
    dev_options.verbose = 6;
        
    dev_options.slave_mode=0;
    dev_options.snoop_mode=0;
    
    sine_advance = 2.0*M_PI*TEST_FREQ/((float)dev_options.sample_rate);

    ffado_device_t *dev=ffado_streaming_init(device_info, dev_options);

    if (!dev) {
        fprintf(stderr,"Could not init Ffado Streaming layer\n");
        exit(-1);
    }

    nb_in_channels = ffado_streaming_get_nb_capture_streams(dev);
    nb_out_channels = ffado_streaming_get_nb_playback_streams(dev);

    if (nb_in_channels < nb_out_channels) {
        min_ch_count = nb_in_channels;
    } else {
        min_ch_count = nb_out_channels;
    }
    
    /* allocate intermediate buffers */
    audiobuffers_in = (float **)calloc(nb_in_channels, sizeof(float *));
    for (i=0; i < nb_in_channels; i++) {
        audiobuffers_in[i] = (float *)calloc(PERIOD_SIZE+1, sizeof(float));
            
        switch (ffado_streaming_get_capture_stream_type(dev,i)) {
            case ffado_stream_type_audio:
                /* assign the audiobuffer to the stream */
                ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
                ffado_streaming_set_capture_buffer_type(dev, i, ffado_buffer_type_float);
                ffado_streaming_capture_stream_onoff(dev, i, 1);
                break;
                // this is done with read/write routines because the nb of bytes can differ.
            case ffado_stream_type_midi:
            default:
                break;
        }
    }
    
    audiobuffers_out = (float **)calloc(nb_out_channels, sizeof(float));
    for (i=0; i < nb_out_channels; i++) {
        audiobuffers_out[i] = (float *)calloc(PERIOD_SIZE+1, sizeof(float));
            
        switch (ffado_streaming_get_playback_stream_type(dev,i)) {
            case ffado_stream_type_audio:
                /* assign the audiobuffer to the stream */
                ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(audiobuffers_out[i]));
                ffado_streaming_set_playback_buffer_type(dev, i, ffado_buffer_type_float);
                ffado_streaming_playback_stream_onoff(dev, i, 1);
                break;
                // this is done with read/write routines because the nb of bytes can differ.
            case ffado_stream_type_midi:
            default:
                break;
        }
    }
    
    nullbuffer = (float *)calloc(PERIOD_SIZE+1, sizeof(float));
    
    
//     /* open the files to write to*/
//     FILE* fid_in[nb_in_channels];
//     char name[256];
// 
//     for (i=0;i<nb_in_channels;i++) {
//         snprintf(name,sizeof(name),"in_ch_%02d",i);
//         fid_in[i]=fopen(name,"w");
// 
//         ffado_streaming_get_capture_stream_name(dev,i,name,sizeof(name));
//         fprintf(fid_in[i], "Channel name: %s\n",name);
//         switch (ffado_streaming_get_capture_stream_type(dev,i)) {
//         case ffado_stream_type_audio:
//             fprintf(fid_in[i], "Channel type: audio\n");
//             break;
//         case ffado_stream_type_midi:
//             fprintf(fid_in[i], "Channel type: midi\n");
//             break;
//         case ffado_stream_type_unknown:
//             fprintf(fid_in[i],"Channel type: unknown\n");
//             break;
//         default:
//         case ffado_stream_type_invalid:
//             fprintf(fid_in[i],"Channel type: invalid\n");
//             break;
//         }
//     }

    // start the streaming layer
    ffado_streaming_prepare(dev);
    start_flag = ffado_streaming_start(dev);
    
    set_realtime_priority(dev_options.packetizer_priority-1);
    fprintf(stderr,"Entering receive loop (IN: %d, OUT: %d)\n", nb_in_channels, nb_out_channels);
    while(run && start_flag==0) {
        retval = ffado_streaming_wait(dev);
        if (retval < 0) {
            fprintf(stderr,"Xrun\n");
            ffado_streaming_reset(dev);
            continue;
        }
        
        ffado_streaming_transfer_capture_buffers(dev);
        
        if (do_test_tone) {
            // generate the test tone
            for (i=0; i<PERIOD_SIZE; i++) {
                nullbuffer[i] = amplitude * sin(sine_advance * (frame_counter + (float)i));
            }
            
            // copy the test tone to the audio buffers
            for (i=0; i < nb_out_channels; i++) {
                if (ffado_streaming_get_playback_stream_type(dev,i) == ffado_stream_type_audio) {
                    memcpy((char *)(audiobuffers_out[i]), (char *)(nullbuffer), sizeof(float) * PERIOD_SIZE);
                }
            }
        } else {
            for (i=0; i < min_ch_count; i++) {
                switch (ffado_streaming_get_capture_stream_type(dev,i)) {
                    case ffado_stream_type_audio:
                        // if both channels are audio channels, copy the buffers
                        if (ffado_streaming_get_playback_stream_type(dev,i) == ffado_stream_type_audio) {
                            memcpy((char *)(audiobuffers_out[i]), (char *)(audiobuffers_in[i]), sizeof(float) * PERIOD_SIZE);
                        }
                        break;
                        // this is done with read/write routines because the nb of bytes can differ.
                    case ffado_stream_type_midi:
                    default:
                        break;
                }
            }
        }
        
        ffado_streaming_transfer_playback_buffers(dev);
        
        nb_periods++;
        frame_counter += PERIOD_SIZE;

//         if((nb_periods % 32)==0) {
// //             fprintf(stderr,"\r%05d periods",nb_periods);
//         }

//         for(i=0;i<nb_in_channels;i++) {
//             
//             
//             switch (ffado_streaming_get_capture_stream_type(dev,i)) {
//             case ffado_stream_type_audio:
//                 // no need to get the buffers manually, we have set the API internal buffers to the audiobuffer[i]'s
// //                 //samplesread=freebob_streaming_read(dev, i, audiobuffer[i], PERIOD_SIZE);
//                 samplesread=PERIOD_SIZE;
//                 break;
//             case ffado_stream_type_midi:
//                 //samplesread=ffado_streaming_read(dev, i, audiobuffers_out[i], PERIOD_SIZE);
//                 break;
//                         default: break;
//             }
//     
// //               fprintf(fid_in[i], "---- Period read  (%d samples) ----\n",samplesread);
// //               hexDumpToFile(fid_in[i],(unsigned char*)audiobuffers_in[i],samplesread*sizeof(float)+1);
//         }

//         for(i=0;i<nb_out_channels;i++) {
//             float *buff;
//             int sampleswritten=0;
//             if (i<nb_in_channels) {
//                 buff=audiobuffers_out[i];
//             } else {
//                 buff=nullbuffer;
//             }
//             
//             switch (ffado_streaming_get_playback_stream_type(dev,i)) {
//             case ffado_stream_type_audio:
// //                  sampleswritten=freebob_streaming_write(dev, i, buff, PERIOD_SIZE);
//                 sampleswritten=PERIOD_SIZE;
//                 break;
//             case ffado_stream_type_midi:
// //                 sampleswritten=freebob_streaming_write(dev, i, buff, PERIOD_SIZE);
//                 break;
//                         default: break;
//             }
// //               fprintf(fid_out[i], "---- Period write (%d samples) ----\n",sampleswritten);
// //               hexDumpToFile(fid_out[i],(unsigned char*)buff,sampleswritten*sizeof(ffado_sample_t));
//         }
    }

    fprintf(stderr,"\n");

    fprintf(stderr,"Exiting receive loop\n");
    
    ffado_streaming_stop(dev);
    ffado_streaming_finish(dev);

//     for (i=0;i<nb_in_channels;i++) {
//         fclose(fid_in[i]);
//     }
    
    for (i=0;i<nb_in_channels;i++) {
        free(audiobuffers_in[i]);
        free(audiobuffers_out[i]);
    }
    free(nullbuffer);
    free(audiobuffers_in);
    free(audiobuffers_out);

  return EXIT_SUCCESS;
}
