/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

/*
 * This version uses the CPP API
 */

#include <config.h>
#include <semaphore.h>

#include "libffado/ffado.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "ffadodevice.h"

#include <dbus-c++/dbus.h>
#include "controlserver.h"
#include "libcontrol/BasicElements.h"

#include "libutil/Functors.h"

#include <signal.h>

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <string>
#include <iostream>
#include <sstream>

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

// DBUS stuff
DBus::BusDispatcher dispatcher;
DBusControl::Container *container = NULL;
DBus::Connection * global_conn;
DeviceManager *m_deviceManager = NULL;

// signal handler
int run=1;
sem_t run_sem;

static void sighandler (int sig)
{
    run = 0;
    dispatcher.leave();
    sem_post(&run_sem);
}

// global's
const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

// Program documentation.
static char doc[] = "ffado-dbus-server -- expose the mixer features of connected FFADO devices through DBus\n\n"
                    ;

// A description of the arguments we accept.
static char args_doc[] = "";

struct arguments
{
    short silent;
    short verbose;
    int   use_cache;
    int   port;
    int   node_id;
    int   node_id_set;
    char* args[2];
};

// The options we understand.
static struct argp_option options[] = {
    {"quiet",    'q',       0,    0,  "Don't produce any output" },
    {"silent",   's',       0,    OPTION_ALIAS },

    {"verbose",  'v', "level",    0,  "Produce verbose output" },
    {"cache",    'c', "enable",   0,  "Use AVC model cache (default=disabled)" },


    {"node",     'n',    "id",    0,  "Only expose mixer of a device on a specific node" },
    {"port",     'p',    "nr",    0,  "IEEE1394 Port to use" },
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
    case 'q': case 's':
        arguments->silent = 1;
        break;
    case 'v':
        if (arg) {
            arguments->verbose = strtol( arg, &tail, 0 );
            if ( errno ) {
                debugError( "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'c':
        if (arg) {
            arguments->use_cache = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'cache' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        if (arg) {
            arguments->port = strtol( arg, &tail, 0 );
            if ( errno ) {
                debugError( "Could not parse 'port' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        } else {
            if ( errno ) {
                debugError("Could not parse 'port' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'n':
        if (arg) {
            arguments->node_id = strtol( arg, &tail, 0 );
            if ( errno ) {
                debugError( "Could not parse 'node' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
            arguments->node_id_set=1;
        } else {
            if ( errno ) {
                debugError("Could not parse 'node' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 1) {
            // Too many arguments.
            argp_usage( state );
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 0) {
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

int exitfunction( int retval ) {
    debugOutput( DEBUG_LEVEL_NORMAL, "Debug output flushed...\n" );
    flushDebugOutput();
    return retval;
}

void
busresetHandler()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "notified of bus reset...\n" );
    dispatcher.leave();

    // delete old container
    delete container;
    container = NULL;

    // build new one
    if(m_deviceManager) {
        container = new DBusControl::Container(*global_conn, "/org/ffado/Control/DeviceManager", *m_deviceManager);
    } else {
        debugError("no device manager, bailing out\n");
        run=0;
    }
    sem_post(&run_sem);
}

int
main( int argc, char **argv )
{
    struct arguments arguments;

    // Default values.
    arguments.silent      = 0;
    arguments.verbose     = DEBUG_LEVEL_NORMAL;
    arguments.use_cache   = 0;
    arguments.port        = 0;
    arguments.node_id     = 0;
    arguments.node_id_set = 0; // if we don't specify a node, discover all
    arguments.args[0]     = "";
    arguments.args[1]     = "";

    setDebugLevel(arguments.verbose);

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return exitfunction(-1);
    }

    debugOutput( DEBUG_LEVEL_NORMAL, "verbose level = %d\n", arguments.verbose);
    debugOutput( DEBUG_LEVEL_NORMAL, "Using ffado library version: %s\n\n", ffado_get_version() );

        m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            debugError("Could not allocate device manager\n" );
            return exitfunction(-1);
        }
        if ( !m_deviceManager->initialize() ) {
            debugError("Could not initialize device manager\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }
        if ( arguments.verbose ) {
            m_deviceManager->setVerboseLevel(arguments.verbose);
        }
        if ( !m_deviceManager->discover(arguments.use_cache) ) {
            debugError("Could not discover devices\n" );
            delete m_deviceManager;
            return exitfunction(-1);
        }

        // add busreset handler
        Util::Functor* tmp_busreset_functor = new Util::CallbackFunctor0< void (*)() >
                    ( &busresetHandler, false );
        if ( !tmp_busreset_functor ) {
            debugFatal( "Could not create busreset handler\n" );
            return false;
        }
        if(!m_deviceManager->registerBusresetNotification(tmp_busreset_functor)) {
            debugError("could not register busreset notifier");
        }

        signal (SIGINT, sighandler);
        
        DBus::_init_threading();
    
        // test DBUS stuff
        DBus::default_dispatcher = &dispatcher;
        DBus::Connection conn = DBus::Connection::SessionBus();
        global_conn = &conn;
        conn.request_name("org.ffado.Control");

        container = new DBusControl::Container(conn, "/org/ffado/Control/DeviceManager", *m_deviceManager);
        
        printMessage("DBUS test service running\n");
        printMessage("press ctrl-c to stop it & exit\n");
        
        while(run) {
            debugOutput( DEBUG_LEVEL_NORMAL, "dispatching...\n");
            dispatcher.enter();
            sem_wait(&run_sem);
        }
        
        if(!m_deviceManager->unregisterBusresetNotification(tmp_busreset_functor)) {
            debugError("could not unregister busreset notifier");
        }
        delete tmp_busreset_functor;
        delete container;

        signal (SIGINT, SIG_DFL);

        printMessage("server stopped\n");
        delete m_deviceManager;
        return exitfunction(0);
}
