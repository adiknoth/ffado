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
 * Test application for the IPC audio server
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sched.h>

#include "libffado/ffado.h"

#include "debugmodule/debugmodule.h"

#include "libutil/IpcRingBuffer.h"
#include "libutil/SystemTimeSource.h"

#include <math.h>
#include <argp.h>

int run;

DECLARE_GLOBAL_DEBUG_MODULE;

using namespace Util;

// Program documentation.
// Program documentation.
static char doc[] = "FFADO -- a driver for Firewire Audio devices (IPC streaming test application)\n\n"
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
    long int playback;
    long int capture;
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
    {"playback",  'o', "",  0,  "Number of playback channels" },
    {"capture",  'i', "",  0,  "Number of capture channels" },
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
    case 'i':
        if (arg) {
            arguments->capture = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'capture' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'o':
        if (arg) {
            arguments->playback = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'playback' argument\n" );
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
    arguments.verbose           = 6;
    arguments.period            = 1024;
    arguments.slave_mode        = 0;
    arguments.snoop_mode        = 0;
    arguments.nb_buffers        = 3;
    arguments.sample_rate       = 44100;
    arguments.rtprio            = 0;
    arguments.audio_buffer_type = 1;
    arguments.playback          = 0;
    arguments.capture           = 0;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return -1;
    }

    debugOutput(DEBUG_LEVEL_NORMAL, "verbose level = %d\n", arguments.verbose);
    setDebugLevel(arguments.verbose);

    if(arguments.playback == 0 && arguments.capture == 0) {
        debugError("No playback nor capture channels requested\n");
        return -1;
    }

    int nb_in_channels=0, nb_out_channels=0;
    int i=0;
    int start_flag = 0;

    int nb_periods=0;

    uint32_t **audiobuffers_in = NULL;
    uint32_t **audiobuffers_out = NULL;
    float *nullbuffer = NULL;

    run=1;

    debugOutput(DEBUG_LEVEL_NORMAL, "FFADO streaming test application (3)\n");
    printMessage(" period %d, nb_buffers %d, playback %d, capture %d\n",
                 arguments.period, arguments.nb_buffers,
                 arguments.playback,
                 arguments.capture );

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

    int nb_in_channels_all = ffado_streaming_get_nb_capture_streams(dev);
    int nb_out_channels_all = ffado_streaming_get_nb_playback_streams(dev);

    // only do audio for now
    for (i=0; i < nb_in_channels_all; i++) {
        switch (ffado_streaming_get_capture_stream_type(dev,i)) {
            case ffado_stream_type_audio:
                nb_in_channels++;
                break;
            case ffado_stream_type_midi:
            default:
                break;
        }
    }
    for (i=0; i < nb_out_channels_all; i++) {
        switch (ffado_streaming_get_playback_stream_type(dev,i)) {
            case ffado_stream_type_audio:
                nb_out_channels++;
                break;
            case ffado_stream_type_midi:
            default:
                break;
        }
    }

    printMessage("Device channel count: %d capture, %d playback\n",
                 nb_in_channels, nb_out_channels);
    printMessage("Requested channel count: %d capture, %d playback\n",
                 arguments.capture, arguments.playback);

    if(arguments.playback > nb_out_channels) {
        debugError("Too many playback channels requested (want: %d, have:%d)\n", 
                   arguments.playback, nb_out_channels);
        return -1;
    }
    if(arguments.capture > nb_in_channels) {
        debugError("Too many capture channels requested (want: %d, have:%d)\n", 
                   arguments.capture, nb_in_channels);
        return -1;
    }

    printMessage("Buffer size: %d capture, %d playback\n", 
                  nb_in_channels*dev_options.period_size * 4,
                  nb_out_channels*dev_options.period_size * 4);

    // allocate the IPC structures
    IpcRingBuffer* capturebuffer = NULL;
    IpcRingBuffer* playbackbuffer = NULL;
    if(arguments.capture) {
        // 4 bytes per channel per sample
        capturebuffer = new IpcRingBuffer("capturebuffer",
                                IpcRingBuffer::eBT_Master,
                                IpcRingBuffer::eD_Outward,
                                IpcRingBuffer::eB_NonBlocking,
                                arguments.nb_buffers,
                                dev_options.period_size * arguments.capture * 4);
        if(capturebuffer == NULL) {
            debugError("Could not create capture IPC buffer\n");
            exit(-1);
        }
    
        capturebuffer->setVerboseLevel(arguments.verbose);
    
        if(!capturebuffer->init()) {
            debugError("Could not init capture buffer\n");
            delete capturebuffer;
            exit(-1);
        }
        // indexes to the memory locations where the frames should be put
        audiobuffers_in = (uint32_t **)calloc(arguments.capture, sizeof(uint32_t *));
    }

    if(arguments.playback) {
        // 4 bytes per channel per sample
        playbackbuffer = new IpcRingBuffer("playbackbuffer",
                                IpcRingBuffer::eBT_Master,
                                IpcRingBuffer::eD_Inward,
                                IpcRingBuffer::eB_NonBlocking,
                                arguments.nb_buffers,
                                dev_options.period_size * arguments.playback * 4);
        if(playbackbuffer == NULL) {
            debugError("Could not create playback IPC buffer\n");
            exit(-1);
        }
    
        playbackbuffer->setVerboseLevel(arguments.verbose);
    
        if(!playbackbuffer->init()) {
            debugError("Could not init playback buffer\n");
            delete capturebuffer;
            delete playbackbuffer;
            exit(-1);
        }
        // indexes to the memory locations where the frames should be get
        audiobuffers_out = (uint32_t **)calloc(arguments.playback, sizeof(uint32_t *));
    }

    // this serves in case we miss a cycle, or a channel is disabled
    nullbuffer = (float *)calloc(arguments.period, sizeof(float));

    // give us RT prio
    set_realtime_priority(arguments.rtprio);

    // start the streaming layer
    if (ffado_streaming_prepare(dev)) {
        debugFatal("Could not prepare streaming system\n");
        ffado_streaming_finish(dev);
        return -1;
    }
    start_flag = ffado_streaming_start(dev);

    // enter the loop
    debugOutput(DEBUG_LEVEL_NORMAL,
                "Entering receive loop (IN: %d, OUT: %d)\n",
                arguments.capture, arguments.playback);

    while(run && start_flag==0) {
        bool need_silent;
        enum IpcRingBuffer::eResult msg_res;

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

        // get a block pointer from the IPC buffer to write
        if(arguments.capture) {
            uint32_t *audiobuffers_raw;
            msg_res = capturebuffer->requestBlockForWrite((void**) &audiobuffers_raw); // pointer voodoo
            if(msg_res == IpcRingBuffer::eR_OK) {
                // if we got a valid pointer, setup the stream pointers
                for (i=0; i < nb_in_channels; i++) {
                    if(i < arguments.capture) {
                        audiobuffers_in[i] = audiobuffers_raw + i*dev_options.period_size;
                        switch (ffado_streaming_get_capture_stream_type(dev,i)) {
                            case ffado_stream_type_audio:
                                /* assign the audiobuffer to the stream */
                                ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
                                ffado_streaming_capture_stream_onoff(dev, i, 1);
                                break;
                                // this is done with read/write routines because the nb of bytes can differ.
                            case ffado_stream_type_midi:
                                ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(audiobuffers_in[i]));
                                ffado_streaming_capture_stream_onoff(dev, i, 1);
                            default:
                                break;
                        }
                    } else {
                        ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(nullbuffer));
                        ffado_streaming_capture_stream_onoff(dev, i, 0);
                    }
                }
                need_silent = false;
            } else  {
                need_silent = true;
                debugOutput(DEBUG_LEVEL_NORMAL, "CAP: missed period %d\n", nb_periods);
            }
        } else {
            need_silent=true;
        }
        if(need_silent) {
            // if not, use the null buffer
            for (i=0; i < nb_in_channels; i++) {
                switch (ffado_streaming_get_capture_stream_type(dev,i)) {
                    case ffado_stream_type_audio:
                        /* assign the audiobuffer to the stream */
                        ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(nullbuffer));
                        ffado_streaming_capture_stream_onoff(dev, i, 0);
                        break;
                        // this is done with read/write routines because the nb of bytes can differ.
                    case ffado_stream_type_midi:
                        ffado_streaming_set_capture_stream_buffer(dev, i, (char *)(nullbuffer));
                        ffado_streaming_capture_stream_onoff(dev, i, 0);
                    default:
                        break;
                }
            }
        }

        // transfer
        ffado_streaming_transfer_capture_buffers(dev);

        if(capturebuffer && !need_silent && msg_res == IpcRingBuffer::eR_OK) {
            // if we had a good block, release it
            // FIXME: we should check for errors here
            capturebuffer->releaseBlockForWrite();
        }

        if(arguments.playback) {
            uint32_t *audiobuffers_raw;
            // get a block pointer from the IPC buffer to read
            msg_res = playbackbuffer->requestBlockForRead((void**) &audiobuffers_raw); // pointer voodoo
            if(msg_res == IpcRingBuffer::eR_OK) {
                // if we got a valid pointer, setup the stream pointers
                for (i=0; i < nb_out_channels; i++) {
                    if(i < arguments.playback) {
                        audiobuffers_out[i] = audiobuffers_raw + i*dev_options.period_size;
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
                    } else {
                        ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(nullbuffer));
                        ffado_streaming_playback_stream_onoff(dev, i, 0);
                    }
                }
                need_silent=false;
            } else {
                debugOutput(DEBUG_LEVEL_NORMAL, "PBK: missed period %d\n", nb_periods);
                need_silent=true;
            }
        } else {
            need_silent=true;
        }
        if(need_silent) {
            // if not, use the null buffer
            memset(nullbuffer, 0, arguments.period * sizeof(float)); // clean it first
            for (i=0; i < nb_out_channels; i++) {
                switch (ffado_streaming_get_playback_stream_type(dev,i)) {
                    case ffado_stream_type_audio:
                        /* assign the audiobuffer to the stream */
                        ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(nullbuffer));
                        ffado_streaming_playback_stream_onoff(dev, i, 0);
                        break;
                        // this is done with read/write routines because the nb of bytes can differ.
                    case ffado_stream_type_midi:
                        ffado_streaming_set_playback_stream_buffer(dev, i, (char *)(nullbuffer));
                        ffado_streaming_playback_stream_onoff(dev, i, 0);
                    default:
                        break;
                }
            }
        }

        // transfer playback buffers
        ffado_streaming_transfer_playback_buffers(dev);

        if(playbackbuffer && !need_silent && msg_res == IpcRingBuffer::eR_OK) {
            // if we had a good block, release it
            // FIXME: we should check for errors here
            playbackbuffer->releaseBlockForRead();
        }

        nb_periods++;
    }

    debugOutput(DEBUG_LEVEL_NORMAL, "Exiting receive loop\n");

    ffado_streaming_stop(dev);
    ffado_streaming_finish(dev);

    delete capturebuffer;
    delete playbackbuffer;

    free(nullbuffer);
    free(audiobuffers_in);
    free(audiobuffers_out);

  return EXIT_SUCCESS;
}
