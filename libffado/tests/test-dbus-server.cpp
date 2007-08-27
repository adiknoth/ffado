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

#include "controlserver.h"

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-dbus-server 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-dbus-server -- test server for the DBUS interface";
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

void leave( int sig )
{
    dispatcher.leave();
}

void start_server() {

    // test DBUS stuff
    DBus::default_dispatcher = &dispatcher;

    DBus::Connection conn = DBus::Connection::SessionBus();
    conn.request_name(SERVER_NAME);

    Control::ControlServer server(conn);

    dispatcher.enter();

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

    debugOutput(DEBUG_LEVEL_NORMAL, "DBUS test server\n");

    DBus::_init_threading();

    start_server();

    debugOutput(DEBUG_LEVEL_NORMAL, "bye...\n");
    
    return 0;
}
