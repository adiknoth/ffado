/* test-freebob.c
 * Copyright (C) 2005 by Daniel Wagner
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

/*
 * This version uses the CPP API
 */

#include <config.h>

#include "libfreebob/freebob.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "iavdevice.h"

#include <signal.h>

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#include <lo/lo.h>

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

// prototypes & stuff for listing the OSC space
void list_osc_paths(lo_server s, lo_address t, const char *path);
void list_osc_params(lo_server s, lo_address t, const char *path);
string osc_param_get_value(lo_server s, lo_address t, const char *path, const char *param);

vector<string> osc_paths;
vector<string> osc_params;
string osc_value;

int osc_path_response_handler(const char *path, const char *types, lo_arg **argv, int argc, 
    void *data, void *user_data);
    
int osc_param_response_handler(const char *path, const char *types, lo_arg **argv, int argc, 
    void *data, void *user_data);
    
int osc_data_response_handler(const char *path, const char *types, lo_arg **argv, int argc, 
    void *data, void *user_data);
    
void osc_error_handler(int num, const char *msg, const char *path);

// signal handler
int run=1;
static void sighandler (int sig)
{
    run = 0;
}

// global's
const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

// Program documentation.
static char doc[] = "FreeBoB -- a driver for Firewire Audio devices (test application)\n\n"
                    "OPERATION: Discover\n"
                    "           SetSamplerate\n"
                    "           ListOscSpace\n"
                    "           OscServer\n";

// A description of the arguments we accept.
static char args_doc[] = "OPERATION";

struct arguments
{
    short silent;
    short verbose;
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


    {"node",     'n',    "id",    0,  "Node to use" },
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
                debugError(  "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        if (arg) {
            arguments->port = strtol( arg, &tail, 0 );
            if ( errno ) {
                debugError(  "Could not parse 'port' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        } else {
            if ( errno ) {
                debugError( "Could not parse 'port' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'n':
        if (arg) {
            arguments->node_id = strtol( arg, &tail, 0 );
            if ( errno ) {
                debugError(  "Could not parse 'node' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
            arguments->node_id_set=1;
        } else {
            if ( errno ) {
                debugError( "Could not parse 'node' argumen\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 2) {
            // Too many arguments.
            argp_usage( state );
        }
        arguments->args[state->arg_num] = arg;
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

int
main( int argc, char **argv )
{
    struct arguments arguments;

    // Default values.
    arguments.silent      = 0;
    arguments.verbose     = DEBUG_LEVEL_NORMAL;
    arguments.port        = 0;
    arguments.node_id     = 0;
    arguments.node_id_set = 0; // if we don't specify a node, discover all
    arguments.args[0]     = "";
    arguments.args[1]     = "";

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError( "Could not parse command line\n" );
        return -1;
    }

    setDebugLevel(arguments.verbose);

    debugOutput(DEBUG_LEVEL_NORMAL, "verbose level = %d\n", arguments.verbose);

    debugOutput(DEBUG_LEVEL_NORMAL,  "Using freebob library version: %s\n\n", freebob_get_version() );
    
    if ( strcmp( arguments.args[0], "Discover" ) == 0 ) {
        DeviceManager *m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            debugError( "Could not allocate device manager\n" );
            return -1;
        }
        if ( !m_deviceManager->initialize( arguments.port ) ) {
            debugError( "Could not initialize device manager\n" );
            delete m_deviceManager;
            return -1;
        }

        m_deviceManager->setVerboseLevel(arguments.verbose);
        if ( !m_deviceManager->discover() ) {
            debugError( "Could not discover devices\n" );
            delete m_deviceManager;
            return -1;
        }
        
        int devices_on_bus = m_deviceManager->getNbDevices();
        debugOutput(DEBUG_LEVEL_NORMAL, "port = %d, devices_on_bus = %d\n", arguments.port, devices_on_bus);
        
        for(int i=0;i<devices_on_bus;i++) {
            int node_id=m_deviceManager->getDeviceNodeId(i);
            debugOutput(DEBUG_LEVEL_NORMAL, " device = %d, node = %d\n", i, node_id);
            IAvDevice* avDevice = m_deviceManager->getAvDevice( node_id );
            if ( avDevice ) {
                avDevice->showDevice();
            }
        }
        
        delete m_deviceManager;
        return 0;
    } else if ( strcmp( arguments.args[0], "SetSamplerate" ) == 0 ) {
        char* tail;
        int samplerate = strtol( arguments.args[1], &tail, 0 );
        if ( errno ) {
            debugError(  "Could not parse samplerate argument\n" );
            return -1;
        }
        
        DeviceManager *m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            debugError( "Could not allocate device manager\n" );
            return -1;
        }
        if ( !m_deviceManager->initialize( arguments.port ) ) {
            debugError( "Could not initialize device manager\n" );
            delete m_deviceManager;
            return -1;
        }
        
        m_deviceManager->setVerboseLevel(arguments.verbose);
        if ( !m_deviceManager->discover() ) {
            debugError( "Could not discover devices\n" );
            delete m_deviceManager;
            return -1;
        }
    
        if(arguments.node_id_set) {
            IAvDevice* avDevice = m_deviceManager->getAvDevice( arguments.node_id );
            if ( avDevice ) {
                if ( avDevice->setSampleRate( parseSampleRate( samplerate ) ) ) {
                    m_deviceManager->discover();
                } else {
                    debugError( "Could not set samplerate\n" );
                }
            }
        } else {
            int i=0;
                
            int devices_on_bus = m_deviceManager->getNbDevices();
            debugOutput(DEBUG_LEVEL_NORMAL, "  port = %d, devices_on_bus = %d\n", arguments.port, devices_on_bus);
    
            for(i=0;i<devices_on_bus;i++) {
                int node_id=m_deviceManager->getDeviceNodeId(i);
                debugOutput(DEBUG_LEVEL_NORMAL, "  set samplerate for device = %d, node = %d\n", i, node_id);
                IAvDevice* avDevice = m_deviceManager->getAvDevice( node_id );
                if ( avDevice ) {
                    if ( !avDevice->setSampleRate( parseSampleRate( samplerate ) ) ) {
                        debugError( "Could not set samplerate\n" );
                    }
                }
            }
        }
        delete m_deviceManager;
        return 0;
    } else if ( strcmp( arguments.args[0], "ListOscSpace" ) == 0 ) {
        // list osc space by using OSC messages
        // a server is assumed to be present
        
        /* start a new server.
        when sending a message from this context, the response
        address will be set to this server's address.
        */
        lo_server s = lo_server_new(NULL, osc_error_handler);
        lo_address t = lo_address_new(NULL, "17820");
        
        list_osc_paths(s, t, "/");
        
        lo_address_free(t);
        lo_server_free(s);
        
    } else if ( strcmp( arguments.args[0], "OscServer" ) == 0 ) {
        DeviceManager *m_deviceManager = new DeviceManager();
        if ( !m_deviceManager ) {
            debugError( "Could not allocate device manager\n" );
            return -1;
        }
        if ( !m_deviceManager->initialize( arguments.port ) ) {
            debugError( "Could not initialize device manager\n" );
            delete m_deviceManager;
            return -1;
        }
        
        m_deviceManager->setVerboseLevel(arguments.verbose);
        if ( !m_deviceManager->discover() ) {
            debugError( "Could not discover devices\n" );
            delete m_deviceManager;
            return -1;
        }

        debugOutput(DEBUG_LEVEL_NORMAL, "server started\n");
        debugOutput(DEBUG_LEVEL_NORMAL, "press ctrl-c to stop it & continue\n");
        
        signal (SIGINT, sighandler);
        
        run=1;
        while(run) {
            sleep(1);
            fflush(stdout);
            fflush(stderr);
        }
        signal (SIGINT, SIG_DFL);
        
        debugOutput(DEBUG_LEVEL_NORMAL, "server stopped\n");
        delete m_deviceManager;
        return 0;
        
    } else {
        debugOutput(DEBUG_LEVEL_NORMAL,  "unknown operation\n" );
    }

}

void list_osc_paths(lo_server s, lo_address t, const char *path) {
    vector<string> my_paths;
    
    debugOutput(DEBUG_LEVEL_NORMAL, "listing path: %s\n", path);
    
    osc_paths.clear();
    lo_server_add_method(s, "/response", NULL, osc_path_response_handler, NULL);

    if (lo_send(t, path, "s", "list") == -1) {
        debugOutput(DEBUG_LEVEL_NORMAL, " OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t));
    }

    if (lo_server_recv_noblock(s, 1000) == 0) {
        debugOutput(DEBUG_LEVEL_NORMAL, "timeout\n");
        return;
    }
    
    lo_server_del_method(s, "/response", NULL);
    
    list_osc_params(s, t, path);
    
    my_paths=osc_paths;
    for ( vector<string>::iterator it = my_paths.begin();
            it != my_paths.end();
            ++it )
    {
        string new_path=string(path) + *it;
        new_path += string("/");
        list_osc_paths(s, t, new_path.c_str());
    }

}

void list_osc_params(lo_server s, lo_address t, const char *path) {
    vector<string> my_paths;
    debugOutput(DEBUG_LEVEL_NORMAL, "params for: %s\n", path);
    
    osc_params.clear();
    lo_server_add_method(s, "/response", NULL, osc_param_response_handler, NULL);

    if (lo_send(t, path, "s", "params") == -1) {
        debugOutput(DEBUG_LEVEL_NORMAL, " OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t));
    }
    
    if (lo_server_recv_noblock(s, 1000) == 0) {
        debugOutput(DEBUG_LEVEL_NORMAL, "timeout\n");
        return;
    }
    
    lo_server_del_method(s, "/response", NULL);
    
    vector<string> my_params=osc_params;
    
    for ( vector<string>::iterator it = my_params.begin();
            it != my_params.end();
            ++it )
    {
        string value=osc_param_get_value(s, t, path, (*it).c_str());
        debugOutput(DEBUG_LEVEL_NORMAL, "  %20s = %s\n", (*it).c_str(), value.c_str());
    }

}

string osc_param_get_value(lo_server s, lo_address t, const char *path, const char *param) {
    lo_server_add_method(s, "/response", NULL, osc_data_response_handler, NULL);

    if (lo_send(t, path, "ss", "get", param) == -1) {
        debugOutput(DEBUG_LEVEL_NORMAL, " OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t));
    }
    
    if (lo_server_recv_noblock(s, 1000) == 0) {
        return string("timeout");
    }
    
    lo_server_del_method(s, "/response", NULL);
    return osc_value;
}

void osc_error_handler(int num, const char *msg, const char *path)
{
    debugOutput(DEBUG_LEVEL_NORMAL, "liblo server error %d in path %s: %s\n", num, path, msg);
}

int osc_path_response_handler(const char *path, const char *types, lo_arg **argv, int argc,
        void *data, void *user_data)
{
    for (int i=0; i< argc;i++) {
        switch (lo_type(types[i])) {
            /** Standard C, NULL terminated string. */
            case LO_STRING:
                osc_paths.push_back(string(&(argv[i]->s)));
                break;
            default:
                debugOutput(DEBUG_LEVEL_NORMAL, "unexpected data type in response message\n");
        }
    }
    return 1;
}

int osc_param_response_handler(const char *path, const char *types, lo_arg **argv, int argc,
        void *data, void *user_data)
{
    for (int i=0; i< argc;i++) {
        switch (lo_type(types[i])) {
            /** Standard C, NULL terminated string. */
            case LO_STRING:
                osc_params.push_back(string(&(argv[i]->s)));
                break;
            default:
                debugOutput(DEBUG_LEVEL_NORMAL, "unexpected data type in response message\n");
        }
    }
    return 1;
}

int osc_data_response_handler(const char *path, const char *types, lo_arg **argv, int argc,
        void *data, void *user_data)
{
    std::ostringstream str;
    
    if(argc==1) {
        switch (lo_type(types[0])) {
            /* basic OSC types */
            /** 32 bit signed integer. */
            case LO_INT32:
                str << "0x" << std::hex << argv[0]->i;
                osc_value=str.str();
                break;
            case LO_INT64:
                str << "0x" << std::hex << argv[0]->h;
                osc_value=str.str();
                break;
            /** 32 bit IEEE-754 float. */
            case LO_FLOAT:
                str << argv[0]->f;
                osc_value=str.str();
                break;
            /** Standard C, NULL terminated string. */
            case LO_STRING:
                osc_value=string(&argv[0]->s);
                break;
            default:
                osc_value="unsupported response datatype";
        }
    } else {
        osc_value="invalid response";
    }
    return 1;
}
