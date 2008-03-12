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

#include "libutil/PosixThread.h"
#include "libutil/Watchdog.h"
#include "libutil/SystemTimeSource.h"

#include <pthread.h>

using namespace Util;

SystemTimeSource timesource;
DECLARE_GLOBAL_DEBUG_MODULE;

class HangTask : public Util::RunnableInterface
{
public:
    HangTask(unsigned int time_usecs, unsigned int nb_hangs) 
        : m_time( time_usecs )
        , m_nb_hangs(nb_hangs) {};
    virtual ~HangTask() {};

    bool Init() {return true;};
    bool Execute() {
        debugOutput(DEBUG_LEVEL_VERBOSE, "execute\n");
        ffado_microsecs_t start = timesource.getCurrentTimeAsUsecs();
        ffado_microsecs_t stop_at = start + m_time;
        int cnt;
        int dummyvar = 0;
        while(timesource.getCurrentTimeAsUsecs() < stop_at) {
            cnt=1000;
            while(cnt--) { dummyvar++; }
        }

        // ensure that dummyvar doesn't get optimized away
        bool always_true = (dummyvar + timesource.getCurrentTimeAsUsecs() != 0);

        bool stop = (m_nb_hangs == 0);
        m_nb_hangs--;

        // we sleep for 100ms after a 'hang'
        timesource.SleepUsecRelative(1000*100);

        // we want the thread to exit after m_nb_hangs 'hangs'
        return always_true && !stop;
    };
    unsigned int m_time;
    unsigned int m_nb_hangs;

};

int run;
// Program documentation.
static char doc[] = "FFADO -- Watchdog test\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";


struct arguments
{
    short verbose;
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",     'v',    "n",    0,  "Verbose level" },
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

    Watchdog *w=NULL;

    struct arguments arguments;

    // Default values.
    arguments.verbose        = DEBUG_LEVEL_VERY_VERBOSE;

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

    w=new Watchdog(1000*1000*1, true, 99); // check time is one second
    w->setVerboseLevel(arguments.verbose);

    HangTask *task1=new HangTask(1000*10, 10); // ten millisecond, 10 hangs
    PosixThread *thread1 = new Util::PosixThread(task1, true, 10, PTHREAD_CANCEL_DEFERRED);
    thread1->setVerboseLevel(arguments.verbose);

    HangTask *task2=new HangTask(1000*1000*2, 10); // two seconds, 10 hangs
    PosixThread *thread2 = new Util::PosixThread(task2, true, 10, PTHREAD_CANCEL_DEFERRED);
    thread2->setVerboseLevel(arguments.verbose);

    w->registerThread(thread1);
    w->registerThread(thread2);

    // start the watchdog
    w->start();
    timesource.SleepUsecRelative(1000*1000*1);

    // start the first thread, should be harmless since it's hang time is too low
    thread1->Start();
    timesource.SleepUsecRelative(1000*1000*1);

    // start the second thread, should be rescheduled since it hangs too long
    thread2->Start();

    // wait for a while
    timesource.SleepUsecRelative(1000*1000*5);


    thread1->Stop();
    thread2->Stop();

    w->unregisterThread(thread1);
    w->unregisterThread(thread2);

    delete thread1;
    delete thread2;
    delete task1;
    delete task2;
    delete w;

    return EXIT_SUCCESS;
}


