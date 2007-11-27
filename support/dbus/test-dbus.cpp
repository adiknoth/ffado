/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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
 *
 */

#include <argp.h>
#include <stdlib.h>
#include <iostream>
#include <signal.h>

#include "controlclient.h"
#include <dbus-c++/dbus.h>

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-dbus 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-dbus -- test client for the DBUS interface";
static char args_doc[] = "";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,  "Produce verbose output" },
   { 0 }
};

struct arguments
{
    arguments()
        : verbose( false )
        {
            args[0] = 0;
        }

    char* args[50];
    bool  verbose;
} arguments;

// Parse a single option.
static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;

//     char* tail;
    switch (key) {
    case 'v':
        arguments->verbose = true;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 50) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

///////////////////////////
// main
//////////////////////////

DBus::BusDispatcher dispatcher;
static const int THREADS = 1;
static bool spin = true;

void leave( int sig )
{
    spin = false;
    dispatcher.leave();
}

void* worker_thread( void* )
{
    DBus::Connection conn = DBus::Connection::SessionBus();

    DBusControl::ContinuousClient client(conn, SERVER_PATH, SERVER_NAME);

    int i=0;
    while(spin)
    {
        try {
            client.setValue(i++);
        } catch(...) {
            cout << "error on setValue()\n";
        };
//         try {
//             std::map< DBus::String, DBus::String > info = client.Info();
//             cout << info["testset1"] << " - " << info["testset2"] << std::endl;
//         } catch(...) {
//             cout << "error on Info()\n";
//         };

        cout << "* " << i << "*\n";
        sleep(1);
    }

    return NULL;
}

void run_client_tests() {
    DBus::default_dispatcher = &dispatcher;

    pthread_t thread;

    pthread_create(&thread, NULL, worker_thread, NULL);

    dispatcher.enter();

    pthread_join(thread, NULL);
}

int
main(int argc, char **argv)
{
    signal(SIGTERM, leave);
    signal(SIGINT, leave);
    
    setDebugLevel(DEBUG_LEVEL_VERBOSE);
    
    // arg parsing
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    errno = 0;
//     char* tail;
    
    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }

    debugOutput(DEBUG_LEVEL_NORMAL, "DBUS test application\n");

    DBus::_init_threading();

    run_client_tests();

    debugOutput(DEBUG_LEVEL_NORMAL, "bye...\n");
    
    return 0;
}
