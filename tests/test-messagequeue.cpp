/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#include "debugmodule/debugmodule.h"

#include "libutil/PosixMessageQueue.h"
#include "libutil/Functors.h"

#include <argp.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>

#include <semaphore.h>

using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 2

int run=1;
int lastsig=-1;
static void sighandler (int sig)
{
    run = 0;
}

sem_t peep_sem;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-messagequeue 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to test the message queues.";
static char args_doc[] = "DIRECTION";
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Produce verbose output" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( false )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    long int verbose;
} arguments;

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
    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        arguments->nargs++;
        break;
    case ARGP_KEY_END:
        if(arguments->nargs <= 0) {
            printMessage("not enough arguments\n");
            return -1;
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

class TestMessage : public Util::PosixMessageQueue::Message
{
public:
    TestMessage()
    : Message()
    , m_prio( 0 )
    , m_length( 64 )
    , m_cnt( 0 )
    {};
    virtual ~TestMessage()
    {};

    virtual unsigned int getPriority()
        {return m_prio;};
    virtual unsigned int getLength() {
        return m_length;
    };

    virtual bool serialize(char *buff) {
        snprintf(buff, m_length, "cnt: %d", m_cnt++);
        return true;
    }

    virtual bool deserialize(const char *buff, unsigned int length, unsigned prio) {
        char tmp[length+1];
        snprintf(tmp, length, "%s", buff);
        tmp[length]=0;
        printMessage("got message: '%s', prio %u\n", tmp, prio);
        m_prio = prio;
        return true;
    };

private:
    unsigned m_prio;
    unsigned int m_length;
    int m_cnt;
};

void peep() {
     printMessage("peep...\n");
     sem_post(&peep_sem);
}

///////////////////////////
// main
//////////////////////////
int
main(int argc, char **argv)
{
    int rv=0;
    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }

    setDebugLevel(arguments.verbose);

    errno = 0;
    char* tail;
    long int direction = strtol( arguments.args[0], &tail, 0 );
    if ( errno ) {
        fprintf( stderr,  "Could not parse direction argument\n" );
        exit(-1);
    }

    if(sem_init(&peep_sem, 0, 0)) {
        debugError("Could not init wait sem\n");
        exit(-1);
    }

    printMessage("Testing message queue direction %ld\n", direction);

    Util::Functor* peepfunc = NULL;

    PosixMessageQueue p = PosixMessageQueue("testqueue1");
    p.setVerboseLevel(arguments.verbose);
    if(direction == 0) {
        if(!p.Create(PosixMessageQueue::eD_WriteOnly)) {
            debugError("Could not create queue\n");
            exit(-1);
        }
    } else {
        if(!p.Open(PosixMessageQueue::eD_ReadOnly, PosixMessageQueue::eB_NonBlocking)) {
            debugError("Could not open queue\n");
            exit(-1);
        }
        peepfunc = new Util::CallbackFunctor0< void (*)() >
                    ( &peep, false );
        if ( !peepfunc ) {
            debugError( "Could not create peep handler\n" );
            exit(-1);
        }
        if(!p.setNotificationHandler(peepfunc)) {
            debugError("Could not set Notification Handler\n");
             exit(-1);
        }
//         if(!p.enableNotification()) {
//             debugError("Could not enable Notification\n");
//             exit(-1);
//         }
    }

    #define TIME_TO_SLEEP 1000*100
    TestMessage m = TestMessage();
    //m.setVerboseLevel(arguments.verbose);
    run=1;
    while(run) {
        if(direction == 0) {
            printMessage("sending...\n");
            if(p.Send(m) != PosixMessageQueue::eR_OK) {
                debugError("Could not send to queue\n");
                goto out_err;
            }
            usleep(TIME_TO_SLEEP);
        } else {
            printMessage("receiving...\n");
            printMessage(" enable notification...\n");
            // first enable notification
            if(!p.enableNotification()) {
                debugError("Could not enable Notification\n");
                goto out_err;
            }
            // then read all there is to read
            printMessage(" reading...\n");
            enum PosixMessageQueue::eResult ret;
            while((ret = p.Receive(m)) == PosixMessageQueue::eR_OK) {
                // nothing
            }
            if (ret != PosixMessageQueue::eR_OK && ret != PosixMessageQueue::eR_Again) {
                debugError("Could not receive from queue\n");
                goto out_err;
            }
            // wait for a notification
            printMessage(" waiting...\n");
            if(sem_wait(&peep_sem)) {
                printMessage(" error: %s\n", strerror(errno));
                goto out_err;
            }
        }
    }

cleanup:
    if(peepfunc) {
        if(!p.disableNotification()) {
            debugError("Could not disable Notification\n");
        }
        if(!p.unsetNotificationHandler()) {
            debugError("Could not unset Notification Handler\n");
             exit(-1);
        }
        delete peepfunc;
    }
    
    sem_destroy(&peep_sem);
    return rv;

out_err:
    rv=-1;
    goto cleanup;

}

