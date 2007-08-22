/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
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

#include "devicemanager.h"
#include "ffadodevice.h"
#include "bebob/bebob_avdevice.h"

#include <argp.h>
#include <iostream>


////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "bebob-sync 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "bebob-sync -- select sync mode on a BeBoB device\n\n"
                    "OPERATION:  set <nr>\n";
static char args_doc[] = "NODE_ID OPERATION";
static struct argp_option options[] = {
    {"verbose",   'v', "level",     0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    { 0 }
};

// IMPL_GLOBAL_DEBUG_MODULE( bebob-sync, DebugModule::eDL_Normal );
DECLARE_GLOBAL_DEBUG_MODULE;

struct arguments
{
    arguments()
        : verbose( 0 )
        , port( 0 )
        {
            args[0] = 0;
            args[1] = 0;
            args[2] = 0;
        }

    char* args[3];
    short verbose;
    int   port;
} arguments;

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
            arguments->verbose = strtol( arg, &tail, 0 );
            if ( errno ) {
                fprintf( stderr,  "Could not parse 'verbose' argument\n" );
                return ARGP_ERR_UNKNOWN;
            }
        }
        break;
    case 'p':
        errno = 0;
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 3) {
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 1) {
            argp_usage (state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };


int
main( int argc, char** argv )
{
    using namespace std;

    // arg parsing
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    errno = 0;
    char* tail;
    int node_id = strtol(arguments.args[0], &tail, 0);
    if (errno) {
    perror("argument parsng failed:");
    return -1;
    }

    std::auto_ptr<DeviceManager> pDeviceManager
        = std::auto_ptr<DeviceManager>( new DeviceManager() );
    if ( !pDeviceManager.get() ) {
        debugFatal( "Could not allocate DeviceManager\n" );
        return -1;
    }
    if ( !pDeviceManager->initialize( arguments.port ) ) {
        debugFatal( "Could not initialize device manager\n" );
        return -1;
    }
    if ( arguments.verbose ) {
        pDeviceManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
    }
    if ( !pDeviceManager->discover( ) ) {
        debugError( "Could not discover devices\n" );
        return -1;
    }

    FFADODevice* pAvDevice = pDeviceManager->getAvDevice( node_id );
    if ( !pAvDevice ) {
        printf( "No recognized device found with id %d\n", node_id );
        return 0;
    }

    BeBoB::AvDevice* pBeBoBAvDevice
        = dynamic_cast<BeBoB::AvDevice*>( pAvDevice );
    if ( !pBeBoBAvDevice ) {
        printf( "Not a BeBoB device, cannot set sync\n" );
        return 0;
    }

    int i = 0;
    for ( BeBoB::AvDevice::SyncInfoVector::const_iterator it
              = pBeBoBAvDevice->getSyncInfos().begin();
          it != pBeBoBAvDevice->getSyncInfos().end();
          ++it )
    {
        const BeBoB::AvDevice::SyncInfo* pSyncInfo = &*it;
        printf( "  %2d) %s\n", i, pSyncInfo->m_description.c_str() );
        ++i;
    }

    printf( "Active Sync mode:\n" );
    if ( !pBeBoBAvDevice->getActiveSyncInfo() ) {
        debugError( "Could not retrieve active sync information\n" );
        return 0;
    }
    printf( "  %s\n", pBeBoBAvDevice->getActiveSyncInfo()->m_description.c_str() );

    if ( argc >= 4 ) {
        if ( strcmp( "set", arguments.args[1] ) == 0 ) {
            errno = 0;
            unsigned int syncId = strtol(arguments.args[2], &tail, 0);
            if (errno) {
                perror("argument parsing failed:");
                return -1;
            }

            if (syncId >= pBeBoBAvDevice->getSyncInfos().size() ) {
                printf( "Invalid sync mode id given (%d). "
                        "Valid range is 0 - %d\n",
                        syncId, pBeBoBAvDevice->getSyncInfos().size()-1 );
                return -1;
            }

            const BeBoB::AvDevice::SyncInfo& syncInfo
                = pBeBoBAvDevice->getSyncInfos()[syncId];
            printf( "Setting Sync Mode to \"%s\"... ",
                    syncInfo.m_description.c_str() );

            if ( pBeBoBAvDevice->setActiveSync( syncInfo ) ) {
                printf( "done\n" );
            } else {
                printf( "failed\n" );
            }
        }
    }

    return 0;
}


