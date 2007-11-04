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

#include <netinet/in.h>

#include "src/libstreaming/cycletimer.h"

#include "src/libstreaming/IsoHandlerManager.h"
#include "SytMonitor.h"

#include "src/libutil/PosixThread.h"
#include "src/libutil/SystemTimeSource.h"

#include <pthread.h>

using namespace Streaming;
using namespace Util;


DECLARE_GLOBAL_DEBUG_MODULE;

int run;
// Program documentation.
static char doc[] = "FFADO -- SYT monitor application\n\n";

// A description of the arguments we accept.
static char args_doc[] = "PORT1,CHANNEL1 [PORT2,CHANNEL2 [... up to 128 combo's]]";

struct port_channel_combo {
    int channel;
    int port;
};

struct arguments
{
    short silent;
    short verbose;
    int   port;
    int   node_id;
    bool   realtime;
    int   rtprio;
    struct port_channel_combo args[128];
    int nb_combos;
};

// The options we understand.
static struct argp_option options[] = {
    {"port",     'p',    "nr",    0,  "IEEE1394 Port to use" },
    {"realtime",     'R',    0,    0,  "Run with realtime scheduling?" },
    {"realtime-priority",     'r',    "nr",    0,  "Realtime scheduling priority" },
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
        case 'p':
            if (arg) {
            arguments->port = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'port' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
            } else {
            if ( errno ) {
                fprintf( stderr, "Could not parse 'port' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
            }
            break;
        case 'R':
        arguments->realtime = true;
            break;
        case 'r':
            if (arg) {
            arguments->rtprio = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'realtime-priority' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
            }
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num >= 128) {
            // Too many arguments.
            argp_usage( state );
            }

            if(sscanf( arg, "%d,%d",
            &arguments->args[state->arg_num].port,
            &arguments->args[state->arg_num].channel) != 2) {
        fprintf( stderr,  "Could not parse port-channel specification ('%s')\n", arg);

            } else {
            printf("Adding Port %d, Channel %d to list...\n",
            arguments->args[state->arg_num].port,
            arguments->args[state->arg_num].channel);
            arguments->nb_combos++;
            }
            break;
        case ARGP_KEY_END:
            if (state->arg_num < 1) {
            // Not enough arguments.
            argp_usage( state );
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
    bool run_realtime=false;
    int realtime_prio=20;
    int nb_iter;
    int i;
    struct sched_param params;
    uint64_t last_print_time=0;

    SystemTimeSource masterTimeSource;

    IsoHandlerManager *m_isoManager=NULL;

    SytMonitor *monitors[128];
    int64_t stream_offset_ticks[128];

    struct arguments arguments;

    // Default values.
    arguments.port        = 0;
    arguments.node_id     = 0;
    arguments.rtprio      = 20;
    arguments.realtime    = false;
    arguments.nb_combos   = 0;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        goto finish;
    }

    memset(&stream_offset_ticks, 0, sizeof(int64_t) * 128);

    run=1;

    run_realtime=arguments.realtime;
    realtime_prio=arguments.rtprio;

    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    debugOutput(DEBUG_LEVEL_NORMAL, "FFADO SYT monitor\n");

    m_isoManager=new IsoHandlerManager();

    if(!m_isoManager) {
        debugOutput(DEBUG_LEVEL_NORMAL, "Could not create IsoHandlerManager\n");
        goto finish;
    }

    m_isoManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

    if(!m_isoManager->init()) {
        debugOutput(DEBUG_LEVEL_NORMAL, "Could not init() IsoHandlerManager\n");
        goto finish;
    }

        // register monitors
        for (i=0;i<arguments.nb_combos;i++) {
            debugOutput(DEBUG_LEVEL_NORMAL, "Registering SytMonitor %d\n",i);

            // add a stream to the manager so that it has something to do
            monitors[i]=new SytMonitor(arguments.args[i].port);

            if (!monitors[i]) {
                debugOutput(DEBUG_LEVEL_NORMAL, "Could not create SytMonitor %d\n", i);
                goto finish;
            }

            monitors[i]->setVerboseLevel(DEBUG_LEVEL_VERBOSE);

            if (!monitors[i]->init()) {
                debugOutput(DEBUG_LEVEL_NORMAL, "Could not init SytMonitor %d\n", i);
                goto finish;
            }

            monitors[i]->setChannel(arguments.args[i].channel);

            if(!m_isoManager->registerStream(monitors[i])) {
                debugOutput(DEBUG_LEVEL_NORMAL, "Could not register SytMonitor %d with isoManager\n", i);
                goto finish;
            }

        }

        debugOutput(DEBUG_LEVEL_NORMAL,   "Preparing IsoHandlerManager...\n");
        if (!m_isoManager->prepare()) {
                debugOutput(DEBUG_LEVEL_NORMAL, "Could not prepare isoManager\n");
                goto finish;
        }

        debugOutput(DEBUG_LEVEL_NORMAL,   "Starting ISO manager sync update thread...\n");


        debugOutput(DEBUG_LEVEL_NORMAL,   "Starting IsoHandlers...\n");
        if (!m_isoManager->startHandlers(0)) {
                debugOutput(DEBUG_LEVEL_NORMAL, "Could not start handlers...\n");
                goto finish;
        }

        if (arguments.realtime) {
            // get rt priority for this thread too.
            params.sched_priority = arguments.rtprio + 1;
            if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &params)) {
                debugWarning("Couldn't set realtime prio for main thread...");
            }
        }

        // do the actual work
        nb_iter=0;

        while(run) {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"--- Iterate ---\n");

//         if(!m_isoManager->iterate()) {
//             debugFatal("Could not iterate the isoManager\n");
//             return false;
//         }

        if(!masterTimeSource.updateTimeSource()) {
            debugFatal("Could not update the masterTimeSource\n");
            return false;
        }

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"--- Process ---\n");
        // process the cycle info's
        if (arguments.nb_combos>0) {
            int master_guard=0;
            struct cycle_info master_cif;

            bool advance_master=true;

            while (advance_master && monitors[0]->readNextCycleInfo(&master_cif)) {
                advance_master=true;

                master_guard++;
                if(master_guard>1000) {
                    debugWarning("Guard passed on master sync!\n");
                    break;
                }
                // we try to match the packets received on equal cycles

                // unwrap the seconds counter
                if (master_cif.seconds==0) master_cif.seconds+=128;

                debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"MASTER: [%2d: %04us %04uc, %04X]\n",
                    0,master_cif.seconds,master_cif.cycle,master_cif.syt);

                for (i=1;i<arguments.nb_combos;i++) {
                    struct cycle_info cif;
                    int slave_guard=0;
                    bool read_slave=monitors[i]->readNextCycleInfo(&cif);

                    while(read_slave) {
                        slave_guard++;
                        if(slave_guard>1000) {
                            debugWarning("Guard passed on slave sync %d!\n",i);
                            break;
                        }
                        // unwrap the seconds counter
                        if (cif.seconds==0) cif.seconds+=128;

                        if(cif.cycle==master_cif.cycle
                        && cif.seconds==master_cif.seconds) { // this is the one
                            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"  GOOD : [%2d: %04us %04uc, %04X]\n",
                                i,cif.seconds, cif.cycle,cif.syt);
                            monitors[i]->consumeNextCycleInfo();

                            // we have a match ;)
                            if ((cif.syt != 0xFFFF) && (master_cif.syt != 0xFFFF)) {
                                // detect seconds wraparound
                                if ((master_cif.pres_seconds==0) && (cif.pres_seconds==127)) {
                                    master_cif.pres_ticks += TICKS_PER_SECOND*128LL;
                                }
                                if ((master_cif.pres_seconds==127) && (cif.pres_seconds==0)) {
                                    cif.pres_ticks += TICKS_PER_SECOND*128LL;
                                }
                            // average out the offset
                                int64_t err=(((uint64_t)master_cif.pres_ticks) - ((uint64_t)cif.pres_ticks));

                                debugOutput(DEBUG_LEVEL_NORMAL,"Diff for %d at cycle %04d: %6lld (MTS: %11llu | STS: %11llu\n",
                                    i,cif.cycle,err, master_cif.pres_ticks, cif.pres_ticks);

                                err = err - stream_offset_ticks[i];

                                if(err>50000 || err < -50000) {
                                    debugOutput(DEBUG_LEVEL_NORMAL,
                                        " Large Diff: %dticks, delta=%d; avg=%d\n",
                                        err,(((int)master_cif.pres_ticks) - ((int)cif.pres_ticks)),stream_offset_ticks[i]);

                                    debugOutput(DEBUG_LEVEL_NORMAL,
                                        "  Master   : %04X -> %10u (%04us %04uc %04ut)\n",
                                        master_cif.syt, master_cif.pres_ticks,
                                        master_cif.pres_seconds, master_cif.pres_cycle, master_cif.pres_offset
                                        );
                                    debugOutput(DEBUG_LEVEL_NORMAL,
                                        "             [%04us %04uc, %04X (%02uc %04ut)]\n",
                                        master_cif.seconds,master_cif.cycle,
                                        master_cif.syt, CYCLE_TIMER_GET_CYCLES(master_cif.syt),
                                        CYCLE_TIMER_GET_OFFSET(master_cif.syt));

                                    debugOutput(DEBUG_LEVEL_NORMAL,
                                        "  Current  : %04X -> %10u (%04us %04uc %04ut)\n",
                                        cif.syt, cif.pres_ticks,
                                        cif.pres_seconds, cif.pres_cycle, cif.pres_offset
                                        );
                                    debugOutput(DEBUG_LEVEL_NORMAL,
                                        "             [%04us %04uc, %04X (%02uc %04ut)]\n",
                                        cif.seconds,cif.cycle,
                                        cif.syt, CYCLE_TIMER_GET_CYCLES(cif.syt),
                                        CYCLE_TIMER_GET_OFFSET(cif.syt));
                                    debugOutput(DEBUG_LEVEL_NORMAL,"\n");
                                }

                                stream_offset_ticks[i] += err/1000;

                                debugOutput(DEBUG_LEVEL_VERY_VERBOSE," Difference: %dticks\n",
                                        (((int)master_cif.pres_ticks) - ((int)cif.pres_ticks)));
                            }

                            break;
                        } else {
                            if ((cif.seconds < master_cif.seconds) ||
                                ((cif.seconds == master_cif.seconds)
                                && (cif.cycle < master_cif.cycle))) {

                                debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"  LAGS : [%2d: %04us %04uc, %04X]\n",
                                    i,cif.seconds, cif.cycle,cif.syt);
                                // the stream lags behind

                                // advance the read pointer
                                // this will always succeed, otherwise we wouldn't be
                                // in this while()
                                monitors[i]->consumeNextCycleInfo();
                            } else {
                                debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"  LEADS: [%2d: %04us %04uc, %04X]\n",
                                    i,cif.seconds, cif.cycle,cif.syt);

                                // the stream is too far ahead,
                                // don't do anything
                                break;
                            }
                        }
                        read_slave=monitors[i]->readNextCycleInfo(&cif);
                    }

                    // detect an empty buffer
                    if (!read_slave) {
                        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"    > No more info's (1)\n");
                        advance_master=false;
                    }

                }
                debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE,"\n");
                if (advance_master) {
                    monitors[0]->consumeNextCycleInfo();
                }
            }
        }

            // show info every x iterations
            if (masterTimeSource.getCurrentTimeAsUsecs()
                - last_print_time > 1000000L) {

                m_isoManager->dumpInfo();
                for (i=0;i<arguments.nb_combos;i++) {
                    monitors[i]->dumpInfo();
                    debugOutputShort(DEBUG_LEVEL_NORMAL,"    ==> Stream offset: %10lld ticks (%6.4f ms)\n",
                    stream_offset_ticks[i], (1.0*((double)stream_offset_ticks[i]))/24576.0);
                }
                masterTimeSource.printTimeSourceInfo();
                last_print_time=masterTimeSource.getCurrentTimeAsUsecs();
            }

            // 125us/packet, so sleep for a while
            usleep(100);
        }

        debugOutput(DEBUG_LEVEL_NORMAL,   "Stopping handlers...\n");
        if(!m_isoManager->stopHandlers()) {
            debugOutput(DEBUG_LEVEL_NORMAL, "Could not stop ISO handlers\n");
            goto finish;
        }

        // unregister monitors
        for (i=0;i<arguments.nb_combos;i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "Unregistering SytMonitor %d\n",i);

        if(!m_isoManager->unregisterStream(monitors[i])) {
            debugOutput(DEBUG_LEVEL_NORMAL, "Could not unregister SytMonitor %d\n",i);
            goto finish;
            }
            delete monitors[i];
    }

    delete m_isoManager;

finish:
        debugOutput(DEBUG_LEVEL_NORMAL, "Bye...\n");

return EXIT_SUCCESS;
}
