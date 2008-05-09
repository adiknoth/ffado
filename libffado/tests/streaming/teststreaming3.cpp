/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sched.h>

#include "libffado/ffado.h"

#include "debugmodule/debugmodule.h"

#include <math.h>
#include <argp.h>

int run;

DECLARE_GLOBAL_DEBUG_MODULE;

// Program documentation.
// Program documentation.
static char doc[] = "FFADO -- a driver for Firewire Audio devices (streaming test application)\n\n"
                    ;

// A description of the arguments we accept.
static char args_doc[] = "";

struct arguments
{
    long int verbose;
    long int test_tone;
    long int test_tone_freq;
    long int period;
    long int slave_mode;
    long int snoop_mode;
    long int nb_buffers;
    long int sample_rate;
    long int rtprio;
    long int audio_buffer_type;
    char* args[2];
    
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Verbose level" },
    {"rtprio",  'P', "prio",  0,  "Realtime priority (0 = no RT scheduling)" },
    {"test-tone",  't', "bool",  0,  "Output test sine" },
    {"test-tone-freq",  'f', "hz",  0,  "Test sine frequency" },
    {"samplerate",  'r', "hz",  0,  "Sample rate" },
    {"period",  'p', "frames",  0,  "Period (buffer) size" },
    {"nb_buffers",  'n', "nb",  0,  "Nb buffers (periods)" },
    {"slave_mode",  's', "bool",  0,  "Run in slave mode" },
    {"snoop_mode",  'S', "bool",  0,  "Run in snoop mode" },
    {"audio_buffer_type",  'b', "",  0,  "Datatype of audio buffers (0=float, 1=int24)" },
    { 0 }
};

//-------------------------------------------------------------

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;
    char* tail;

    errno = 0;
    switch (key) {
    case 'v':
        if (arg) {
            arguments->verbose = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'P':
        if (arg) {
            arguments->rtprio = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'rtprio' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        if (arg) {
            arguments->period = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'period' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'n':
        if (arg) {
            arguments->nb_buffers = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'nb_buffers' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'r':
        if (arg) {
            arguments->sample_rate = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'samplerate' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 't':
        if (arg) {
            arguments->test_tone = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'test-tone' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'f':
        if (arg) {
            arguments->test_tone_freq = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'test-tone-freq' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'b':
        if (arg) {
            arguments->audio_buffer_type = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'audio-buffer-type' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 's':
        if (arg) {
            arguments->slave_mode = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'slave_mode' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'S':
        if (arg) {
            arguments->snoop_mode = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'snoop_mode' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case ARGP_KEY_ARG:
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };

int set_realtime_priority(unsigned int prio)
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Setting thread prio to %u\n", prio);
    if (prio > 0) {
        struct sched_param schp;
        /*
        * set the process to realtime privs
        */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = prio;
        
        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0) {
            perror("sched_setscheduler");
            return -1;
        }
  } else {
        struct sched_param schp;
        /*
        * set the process to realtime privs
        */
        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = 0;
        
        if (sched_setscheduler(0, SCHED_OTHER, &schp) != 0) {
            perror("sched_setscheduler");
            return -1;
        }
  }
  return 0;
}

static void sighandler (int sig)
{
    run = 0;
}

int main(int argc, char *argv[])
{

    struct arguments arguments;

    // Default values.
    arguments.test_tone         = 0;
    arguments.test_tone_freq    = 1000;
    arguments.verbose           = 6;
    arguments.period            = 1024;
    arguments.slave_mode        = 0;
    arguments.snoop_mode        = 0;
    arguments.nb_buffers        = 3;
    arguments.sample_rate       = 44100;
    arguments.rtprio            = 0;
    arguments.audio_buffer_type = 0;
    
    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return -1;
    }

    debugOutput(DEBUG_LEVEL_NORMAL, "verbose level = %d\n", arguments.verbose);
    setDebugLevel(arguments.verbose);

    int nb_in_channels=0, nb_out_channels=0;
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

    debugOutput(DEBUG_LEVEL_NORMAL, "FFADO streaming test application (3)\n");

    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    ffado_device_info_t device_info;
    memset(&device_info,0,sizeof(ffado_device_info_t));

    ffado_options_t dev_options;
    memset(&dev_options,0,sizeof(ffado_options_t));

    dev_options.sample_rate = arguments.sample_rate;
    dev_options.period_size = arguments.period;

    dev_options.nb_buffers = arguments.nb_buffers;

    dev_options.realtime = (arguments.rtprio != 0);
    dev_options.packetizer_priority = arguments.rtprio;
    
    dev_options.verbose = arguments.verbose;
        
    dev_options.slave_mode = arguments.slave_mode;
    dev_options.snoop_mode = arguments.snoop_mode;
    
    sine_advance = 2.0*M_PI*arguments.test_tone_freq/((float)dev_options.sample_rate);

    ffado_device_t *dev=ffado_streaming_init(device_info, dev_options);

    if (!dev) {
        debugError("Could not init Ffado Streaming layer\n");
        exit(-1);
    }
    if (arguments.audio_buffer_type == 0) {
        ffado_streaming_set_audio_datatype(dev, ffado_audio_datatype_float);
    } else {
        ffado_streaming_set_audio_datatype(dev, ffado_audio_datatype_int24);
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
        audiobuffers_in[i] = (float *)calloc(arguments.period+1, sizeof(float));

        switch (ffado_streaming_get_capture_stream_type(dev,i)) {
            case ffado_stream_type_audio:
                /* assign the audiobuffer to the stream */
                ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
                ffado_streaming_capture_stream_onoff(dev, i, 1);
                break;
                // this is done with read/write routines because the nb of bytes can differ.
            case ffado_stream_type_midi:
                // note that using a float * buffer for midievents is a HACK
                ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
                ffado_streaming_capture_stream_onoff(dev, i, 1);
            default:
                break;
        }
    }

    audiobuffers_out = (float **)calloc(nb_out_channels, sizeof(float));
    for (i=0; i < nb_out_channels; i++) {
        audiobuffers_out[i] = (float *)calloc(arguments.period+1, sizeof(float));

        switch (ffado_streaming_get_playback_stream_type(dev,i)) {
            case ffado_stream_type_audio:
                /* assign the audiobuffer to the stream */
                ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(audiobuffers_out[i]));
                ffado_streaming_playback_stream_onoff(dev, i, 1);
                break;
                // this is done with read/write routines because the nb of bytes can differ.
            case ffado_stream_type_midi:
                ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(audiobuffers_out[i]));
                ffado_streaming_playback_stream_onoff(dev, i, 1);
            default:
                break;
        }
    }
    
    nullbuffer = (float *)calloc(arguments.period+1, sizeof(float));
    
    
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
    if (ffado_streaming_prepare(dev)) {
        debugFatal("Could not prepare streaming system\n");
        ffado_streaming_finish(dev);
        return -1;
    }
    start_flag = ffado_streaming_start(dev);
    
    set_realtime_priority(arguments.rtprio);
    debugOutput(DEBUG_LEVEL_NORMAL, "Entering receive loop (IN: %d, OUT: %d)\n", nb_in_channels, nb_out_channels);
    while(run && start_flag==0) {
        ffado_wait_response response;
        response = ffado_streaming_wait(dev);
        if (response == ffado_wait_xrun) {
            debugOutput(DEBUG_LEVEL_NORMAL, "Xrun\n");
            ffado_streaming_reset(dev);
            continue;
        } else if (response == ffado_wait_error) {
            debugError("fatal xrun\n");
            break;
        }
        ffado_streaming_transfer_capture_buffers(dev);

        if (arguments.test_tone) {
            // generate the test tone
            for (i=0; i<arguments.period; i++) {
                float v = amplitude * sin(sine_advance * (frame_counter + (float)i));
                if (arguments.audio_buffer_type == 0) {
                    nullbuffer[i] = v;
                } else {
                    v = (v * 2147483392.0);
                    int32_t tmp = ((int) v);
                    tmp = tmp >> 8;
                    memcpy(&nullbuffer[i], &tmp, sizeof(float));
                }
            }
            
            // copy the test tone to the audio buffers
            for (i=0; i < nb_out_channels; i++) {
                if (ffado_streaming_get_playback_stream_type(dev,i) == ffado_stream_type_audio) {
                    memcpy((char *)(audiobuffers_out[i]), (char *)(nullbuffer), sizeof(float) * arguments.period);
                }
            }
        } else {
            uint32_t *midibuffer;
            int idx;
            for (i=0; i < min_ch_count; i++) {
                switch (ffado_streaming_get_capture_stream_type(dev,i)) {
                    case ffado_stream_type_audio:
                        // if both channels are audio channels, copy the buffers
                        if (ffado_streaming_get_playback_stream_type(dev,i) == ffado_stream_type_audio) {
                            memcpy((char *)(audiobuffers_out[i]), (char *)(audiobuffers_in[i]), sizeof(float) * arguments.period);
                        }
                        break;
                        // this is done with read/write routines because the nb of bytes can differ.
                    case ffado_stream_type_midi:
                        midibuffer=(uint32_t *)audiobuffers_in[i];
                        for(idx=0; idx < arguments.period; idx++) {
                            uint32_t midievent = *(midibuffer + idx);
                            if(midievent & 0xFF000000) {
                                debugOutput(DEBUG_LEVEL_NORMAL, " Received midi event %08X at idx %d of period %d on port %d\n", 
                                            midievent, idx, nb_periods, i);
                            }
                        }
                    default:
                        break;
                }
            }
            for (i=0; i < nb_out_channels; i++) {
                if (ffado_streaming_get_playback_stream_type(dev,i) == ffado_stream_type_midi) {
                    uint32_t *midievent = (uint32_t *)audiobuffers_out[i];
                    *midievent = 0x010000FF;
                    break;
                }
            }

        }

        ffado_streaming_transfer_playback_buffers(dev);

        nb_periods++;
        frame_counter += arguments.period;

//         if((nb_periods % 32)==0) {
// //             debugOutput(DEBUG_LEVEL_NORMAL, "\r%05d periods",nb_periods);
//         }

//         for(i=0;i<nb_in_channels;i++) {
//             
//             
//             switch (ffado_streaming_get_capture_stream_type(dev,i)) {
//             case ffado_stream_type_audio:
//                 // no need to get the buffers manually, we have set the API internal buffers to the audiobuffer[i]'s
// //                 //samplesread=freebob_streaming_read(dev, i, audiobuffer[i], arguments.period);
//                 samplesread=arguments.period;
//                 break;
//             case ffado_stream_type_midi:
//                 //samplesread=ffado_streaming_read(dev, i, audiobuffers_out[i], arguments.period);
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
// //                  sampleswritten=freebob_streaming_write(dev, i, buff, arguments.period);
//                 sampleswritten=arguments.period;
//                 break;
//             case ffado_stream_type_midi:
// //                 sampleswritten=freebob_streaming_write(dev, i, buff, arguments.period);
//                 break;
//                         default: break;
//             }
// //               fprintf(fid_out[i], "---- Period write (%d samples) ----\n",sampleswritten);
// //               hexDumpToFile(fid_out[i],(unsigned char*)buff,sampleswritten*sizeof(ffado_sample_t));
//         }
    }

    debugOutput(DEBUG_LEVEL_NORMAL, "Exiting receive loop\n");
    
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
