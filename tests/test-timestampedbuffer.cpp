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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>

#include <signal.h>
#include "src/debugmodule/debugmodule.h"

#include "libutil/ByteSwap.h"

#include "src/libieee1394/cycletimer.h"

#include "src/libutil/TimestampedBuffer.h"
#include "libutil/Time.h"

#include <pthread.h>

using namespace Util;

class TimestampedBufferTestClient
    : public TimestampedBufferClient {
public:
    bool processReadBlock(char *data, unsigned int nevents, unsigned int offset) {return true;};
    bool processWriteBlock(char *data, unsigned int nevents, unsigned int offset) {return true;};

    void setVerboseLevel(int l) {setDebugLevel(l);};
private:
    DECLARE_DEBUG_MODULE;
};

IMPL_DEBUG_MODULE( TimestampedBufferTestClient, TimestampedBufferTestClient, DEBUG_LEVEL_VERBOSE );

DECLARE_GLOBAL_DEBUG_MODULE;

int run;
// Program documentation.
static char doc[] = "FFADO -- Timestamped buffer test\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";


struct arguments
{
    short verbose;
    uint64_t wrap_at;
    uint64_t frames_per_packet;
    uint64_t events_per_frame;
    float rate;
    uint64_t total_cycles;
    uint64_t buffersize;
    uint64_t start_at_cycle;
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",     'v',    "n",    0,  "Verbose level" },
    {"wrap",        'w',    "n",    0,  "Wrap at (ticks) (3072000)" },
    {"fpp",        'f',    "n",    0,  "Frames per packet (8)" },
    {"epf",        'e',    "n",    0,  "Events per frame (10)" },
    {"rate",        'r',    "n",    0,  "Rate (ticks/frame) (512.0)" },
    {"cycles",        'c',    "n",    0,  "Total cycles to run (2000)" },
    {"buffersize",        'b',    "n",    0,  "Buffer size (in frames) (1024)" },
    {"startcycle",        's',    "n",    0,  "Start at cycle (0)" },
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

    switch (key) {
        case 'v':
            if (arg) {
                arguments->verbose = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'verbose' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'verbose' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 'w':
            if (arg) {
                arguments->wrap_at = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'wrap' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'wrap' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 'f':
            if (arg) {
                arguments->frames_per_packet = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'fpp' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'fpp' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 'e':
            if (arg) {
                arguments->events_per_frame = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'epf' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'epf' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 'c':
            if (arg) {
                arguments->total_cycles = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'cycles' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'cycles' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 's':
            if (arg) {
                arguments->start_at_cycle = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'startcycle' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'startcycle' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 'b':
            if (arg) {
                arguments->buffersize = strtoll( arg, &tail, 0 );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'buffersize' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'buffersize' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
        case 'r':
            if (arg) {
                arguments->rate = strtof( arg, &tail );
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'rate' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            } else {
                if ( errno ) {
                    fprintf( stderr, "Could not parse 'rate' argument\n" );
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;
       default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };


static void sighandler (int sig)
{
        run = 0;
}

int main(int argc, char *argv[])
{

    TimestampedBuffer *t=NULL;
    TimestampedBufferTestClient *c=NULL;

    struct arguments arguments;

    // Default values.
    arguments.verbose        = 0;
    arguments.wrap_at = 3072000LLU; // 1000 cycles
    arguments.frames_per_packet = 8;
    arguments.events_per_frame = 10;
    arguments.rate = 512.0;
    arguments.total_cycles = 2000;
    arguments.buffersize = 1024;
    arguments.start_at_cycle = 0;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(1);
    }

    setDebugLevel(arguments.verbose);

    run=1;

    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    c=new TimestampedBufferTestClient();

    if(!c) {
        debugOutput(DEBUG_LEVEL_NORMAL, "Could not create TimestampedBufferTestClient\n");
        exit(1);
    }
    c->setVerboseLevel(arguments.verbose);

    t=new TimestampedBuffer(c);

    if(!t) {
        debugOutput(DEBUG_LEVEL_NORMAL, "Could not create TimestampedBuffer\n");
        delete c;
        exit(1);
    }
    t->setVerboseLevel(arguments.verbose);

    // Setup the buffer
    t->setBufferSize(arguments.buffersize);
    t->setEventSize(sizeof(int));
    t->setEventsPerFrame(arguments.events_per_frame);

    t->setUpdatePeriod(arguments.frames_per_packet);
    t->setNominalRate(arguments.rate);

    t->setWrapValue(arguments.wrap_at);

    t->prepare();

    SleepRelativeUsec(1000);

    debugOutput(DEBUG_LEVEL_NORMAL, "Start setBufferHeadTimestamp test...\n");
    {
        bool pass=true;
        uint64_t time=arguments.start_at_cycle*3072;
        int dummyframe_in[arguments.events_per_frame*arguments.frames_per_packet];

        // initialize the timestamp
        uint64_t timestamp=time;
        if (timestamp >= arguments.wrap_at) {
            // here we need a modulo because start_at_cycle can be large
            timestamp %= arguments.wrap_at;
        }

        // account for the fact that there is offset,
        // and that setBufferHeadTimestamp doesn't take offset
        // into account
        uint64_t timestamp2=timestamp;
        if (timestamp2>=arguments.wrap_at) {
            timestamp2-=arguments.wrap_at;
        }

        t->setBufferHeadTimestamp(timestamp2);

        timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
        if (timestamp >= arguments.wrap_at) {
            timestamp -= arguments.wrap_at;
        }

        // write some packets
        for (unsigned int i=0;i<20;i++) {
            t->writeFrames(arguments.frames_per_packet, (char *)&dummyframe_in, timestamp);
            timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
            if (timestamp >= arguments.wrap_at) {
                timestamp -= arguments.wrap_at;
            }
        }

        for(unsigned int cycle=arguments.start_at_cycle;
            cycle < arguments.start_at_cycle+arguments.total_cycles;
            cycle++) {
                ffado_timestamp_t ts_head_tmp;
                uint64_t ts_head;
                signed int fc_head;

                t->setBufferHeadTimestamp(timestamp);
                t->getBufferHeadTimestamp(&ts_head_tmp, &fc_head);
                ts_head=(uint64_t)ts_head_tmp;

                if (timestamp != ts_head) {
                    debugError(" cycle %4u error: %011llu != %011llu\n",
                        timestamp, ts_head);
                        pass=false;
                }

                timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
                if (timestamp >= arguments.wrap_at) {
                    timestamp -= arguments.wrap_at;
                }

            // simulate the cycle timer clock in ticks
            time += 3072;
            if (time >= arguments.wrap_at) {
                time -= arguments.wrap_at;
            }

            // allow for the messagebuffer thread to catch up
            SleepRelativeUsec(200);

            if(!run) break;
        }

        if(!pass) {
            debugError("Test failed, exiting...\n");

            delete t;
            delete c;

            return -1;

        }
    }



    debugOutput(DEBUG_LEVEL_NORMAL, "Start read/write test...\n");
    {
        int dummyframe_in[arguments.events_per_frame*arguments.frames_per_packet];
        int dummyframe_out[arguments.events_per_frame*arguments.frames_per_packet];

        for (unsigned int i=0;i<arguments.events_per_frame*arguments.frames_per_packet;i++) {
            dummyframe_in[i]=i;
        }

        uint64_t time=arguments.start_at_cycle*3072;

        // initialize the timestamp
        uint64_t timestamp=time;
        if (timestamp >= arguments.wrap_at) {
            // here we need a modulo because start_at_cycle can be large
            timestamp %= arguments.wrap_at;
        }
        t->setBufferTailTimestamp(timestamp);

        timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
        if (timestamp >= arguments.wrap_at) {
            timestamp -= arguments.wrap_at;
        }

        for(unsigned int cycle=arguments.start_at_cycle;
            cycle < arguments.start_at_cycle+arguments.total_cycles;
            cycle++) {

            // simulate the rate adaptation
            int64_t diff=(time%arguments.wrap_at)-timestamp;

            if (diff>(int64_t)arguments.wrap_at/2) {
                diff -= arguments.wrap_at;
            } else if (diff<(-(int64_t)arguments.wrap_at)/2){
                diff += arguments.wrap_at;
            }

            debugOutput(DEBUG_LEVEL_NORMAL, "Simulating cycle %d @ time=%011llu, diff=%lld\n",cycle,time,diff);

            if(diff>0) {
                ffado_timestamp_t ts_head_tmp, ts_tail_tmp;
                uint64_t ts_head, ts_tail;
                signed int fc_head, fc_tail;

                // write one packet
                t->writeFrames(arguments.frames_per_packet, (char *)&dummyframe_in, timestamp);

                // read the buffer head timestamp
                t->getBufferHeadTimestamp(&ts_head_tmp, &fc_head);
                t->getBufferTailTimestamp(&ts_tail_tmp, &fc_tail);
                ts_head=(uint64_t)ts_head_tmp;
                ts_tail=(uint64_t)ts_tail_tmp;
                debugOutput(DEBUG_LEVEL_NORMAL,
                        " TS after write: HEAD: %011llu, FC=%04u\n",
                        ts_head,fc_head);
                debugOutput(DEBUG_LEVEL_NORMAL,
                        "                 TAIL: %011llu, FC=%04u\n",
                        ts_tail,fc_tail);

                // read one packet
                t->readFrames(arguments.frames_per_packet, (char *)&dummyframe_out);

                // read the buffer head timestamp
                t->getBufferHeadTimestamp(&ts_head_tmp, &fc_head);
                t->getBufferTailTimestamp(&ts_tail_tmp, &fc_tail);
                ts_head=(uint64_t)ts_head_tmp;
                ts_tail=(uint64_t)ts_tail_tmp;
                debugOutput(DEBUG_LEVEL_NORMAL,
                        " TS after write: HEAD: %011llu, FC=%04u\n",
                        ts_head,fc_head);
                debugOutput(DEBUG_LEVEL_NORMAL,
                        "                 TAIL: %011llu, FC=%04u\n",
                        ts_tail,fc_tail);

                // check
                bool pass=true;
                for (unsigned int i=0;i<arguments.events_per_frame*arguments.frames_per_packet;i++) {
                    pass = pass && (dummyframe_in[i]==dummyframe_out[i]);
                }
                if (!pass) {
                    debugOutput(DEBUG_LEVEL_NORMAL, "write/read check for cycle %d failed\n",cycle);
                }

                // update the timestamp
                timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
                if (timestamp >= arguments.wrap_at) {
                    timestamp -= arguments.wrap_at;
                }
            }

            // simulate the cycle timer clock in ticks
            time += 3072;
            if (time >= arguments.wrap_at) {
                time -= arguments.wrap_at;
            }

            // allow for the messagebuffer thread to catch up
            SleepRelativeUsec(200);

            if(!run) break;
        }
    }

    // second run, now do block processing
    debugOutput(DEBUG_LEVEL_NORMAL, "Start block read test...\n");
    {
        unsigned int blocksize=32;
        int dummyframe_out_block[arguments.events_per_frame*arguments.frames_per_packet*blocksize];
        int dummyframe_in[arguments.events_per_frame*arguments.frames_per_packet];

        for (unsigned int i=0;i<arguments.events_per_frame*arguments.frames_per_packet;i++) {
            dummyframe_in[i]=i;
        }

        uint64_t time=arguments.start_at_cycle*3072;

        // initialize the timestamp
        uint64_t timestamp=time;
        if (timestamp >= arguments.wrap_at) {
            // here we need a modulo because start_at_cycle can be large
            timestamp %= arguments.wrap_at;
        }
        t->setBufferTailTimestamp(timestamp);

        timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
        if (timestamp >= arguments.wrap_at) {
            timestamp -= arguments.wrap_at;
        }

        for(unsigned int cycle=arguments.start_at_cycle;
            cycle < arguments.start_at_cycle+arguments.total_cycles;
            cycle++) {

            // simulate the rate adaptation
            int64_t diff=(time%arguments.wrap_at)-timestamp;

            if (diff>(int64_t)arguments.wrap_at/2) {
                diff -= arguments.wrap_at;
            } else if (diff<(-(int64_t)arguments.wrap_at)/2){
                diff += arguments.wrap_at;
            }

            debugOutput(DEBUG_LEVEL_NORMAL, "Simulating cycle %d @ time=%011llu, diff=%lld\n",cycle,time,diff);

            if(diff>0) {
                ffado_timestamp_t ts_head_tmp, ts_tail_tmp;
                uint64_t ts_head, ts_tail;
                signed int fc_head, fc_tail;

                // write one packet
                t->writeFrames(arguments.frames_per_packet, (char *)&dummyframe_in, timestamp);

                // read the buffer head timestamp
                t->getBufferHeadTimestamp(&ts_head_tmp, &fc_head);
                t->getBufferTailTimestamp(&ts_tail_tmp, &fc_tail);
                ts_head=(uint64_t)ts_head_tmp;
                ts_tail=(uint64_t)ts_tail_tmp;
                debugOutput(DEBUG_LEVEL_NORMAL,
                        " TS after write: HEAD: %011llu, FC=%04u\n",
                        ts_head,fc_head);
                debugOutput(DEBUG_LEVEL_NORMAL,
                        "                 TAIL: %011llu, FC=%04u\n",
                        ts_tail,fc_tail);

                if (fc_head > (signed int)blocksize) {
                    debugOutput(DEBUG_LEVEL_NORMAL,"Reading one block (%u frames)\n",blocksize);

                    // read one block
                    t->readFrames(blocksize, (char *)&dummyframe_out_block);

                    // read the buffer head timestamp
                    t->getBufferHeadTimestamp(&ts_head_tmp, &fc_head);
                    t->getBufferTailTimestamp(&ts_tail_tmp, &fc_tail);
                    ts_head=(uint64_t)ts_head_tmp;
                    ts_tail=(uint64_t)ts_tail_tmp;
                    debugOutput(DEBUG_LEVEL_NORMAL,
                            " TS after read: HEAD: %011llu, FC=%04u\n",
                            ts_head,fc_head);
                    debugOutput(DEBUG_LEVEL_NORMAL,
                            "                TAIL: %011llu, FC=%04u\n",
                            ts_tail,fc_tail);
                }

                // update the timestamp
                timestamp += (uint64_t)(arguments.rate * arguments.frames_per_packet);
                if (timestamp >= arguments.wrap_at) {
                    timestamp -= arguments.wrap_at;
                }
            }

            // simulate the cycle timer clock in ticks
            time += 3072;
            if (time >= arguments.wrap_at) {
                time -= arguments.wrap_at;
            }

            // allow for the messagebuffer thread to catch up
            SleepRelativeUsec(200);

            if(!run) break;
        }
    }

    delete t;
    delete c;

    return EXIT_SUCCESS;
}


