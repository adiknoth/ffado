/*
 * Parts Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

/*
 * based on howdyget.c (unknown source, maybe Maas Digital LLC)
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <argp.h>

#include <inttypes.h>

#include "debugmodule/debugmodule.h"
#include "realtimetools.h"

uint32_t count = 0;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_EXTRA_ARGS 2
// Program documentation.
// Program documentation.
static char doc[] = "FFADO -- simple RT system loader\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";

struct arguments
{
    long int verbose;
    long int rtprio;
    long int period;
    long int countdown;
    long int cpu_pct;
    char* args[MAX_EXTRA_ARGS];
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Verbose level" },
    {"rtprio",  'P', "prio",  0,  "real time priority of the iterator process/thread (0 = no RT)" },
    {"period",  'p', "usecs",  0,  "period duration (in usecs)" },
    {"countdown",  'u', "count",  0,  "number of times to run the load loop" },
    {"cpu_pct",  'c', "count",  0,  "target CPU use (in percent)" },
    { 0 }
};

// Parse a single option.
#define PARSE_ARG_LONG(XXletterXX, XXvarXX, XXdescXX) \
    case XXletterXX: \
        if (arg) { \
            XXvarXX = strtol( arg, &tail, 0 ); \
            if ( errno ) { \
                fprintf( stderr,  "Could not parse '%s' argument\n", XXdescXX ); \
                return ARGP_ERR_UNKNOWN; \
            } \
        } \
        break;

static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;
    char* tail;

    switch (key) {
    PARSE_ARG_LONG('v', arguments->verbose, "verbose");
    PARSE_ARG_LONG('P', arguments->rtprio, "rtprio");
    PARSE_ARG_LONG('p', arguments->period, "period");
    PARSE_ARG_LONG('u', arguments->countdown, "countdown");
    PARSE_ARG_LONG('c', arguments->cpu_pct, "cpu_pct");

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

// the global arguments struct
struct arguments arguments;

// signal handler
int run;
static void sighandler (int sig)
{
    run = 0;
}

// the load function
float global;
void load_function() {
    int cnt = 10;
    while(cnt--) {
        int global_int = (int)global;
        global = global / 7.0;
        global_int++;
        global += (float)global_int;
    }
}

int main(int argc, char **argv)
{
    // register signal handler
    run = 1;
    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    // Default values.
    arguments.verbose = DEBUG_LEVEL_VERBOSE;
    arguments.rtprio = 0;
    arguments.countdown = 1000;
    arguments.period = 1000;
    arguments.cpu_pct = 50;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return -1;
    }

    debugOutput(DEBUG_LEVEL_INFO, "Simple RT system loader\n");
    debugOutput(DEBUG_LEVEL_INFO, " Arguments:\n");
    debugOutput(DEBUG_LEVEL_INFO, "  RT priority : %d\n", arguments.rtprio);
    debugOutput(DEBUG_LEVEL_INFO, "  Countdown   : %d\n", arguments.countdown);
    debugOutput(DEBUG_LEVEL_INFO, "  Period      : %d usec\n", arguments.period);
    debugOutput(DEBUG_LEVEL_INFO, "  CPU load    : %d%%\n", arguments.cpu_pct);

    debugOutput(DEBUG_LEVEL_INFO, "Setting RT priority (%d)...\n", arguments.rtprio);
    set_realtime_priority(arguments.rtprio);

    debugOutput(DEBUG_LEVEL_INFO, "Starting iterate loop...\n");
    flushDebugOutput();
    
    int countdown = arguments.countdown;
    uint64_t sleep_time = rt_gettime_usecs();
    while(countdown-- && run)
    {
        // figure out when to stop calling the load function
        uint64_t run_until = sleep_time + arguments.period * arguments.cpu_pct / 100;
        
        uint64_t tic = rt_gettime_usecs();
        while(rt_gettime_usecs() < run_until) load_function();
        uint64_t toc = rt_gettime_usecs();

        // now wait for the period to end
        sleep_time += arguments.period;
        rt_sleep_absolute_usecs(sleep_time);
        
        // check if we are late
        toc = rt_gettime_usecs();
        int64_t usecs_late = toc - sleep_time;
        if(usecs_late > 1000) {
            debugWarning("late wakeup: %lld usecs\n", usecs_late);
        }

        // try and detect lockup ()
        if(usecs_late > 100000) {
            debugWarning("very late wakeup: %lld usecs\n", usecs_late);
            // force exit, since this is a loop out of control
            run=0;
        }
    }

    if(run) {
        debugOutput(DEBUG_LEVEL_INFO, "Clean exit...\n");
    } else {
        debugOutput(DEBUG_LEVEL_INFO, "Forced exit...\n");
    }
    return 0;
}
