/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include <argp.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-fw410 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-fw410 -- test program to get the fw410 streaming";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
   { 0 }
};

struct arguments
{
    arguments()
        : verbose( false )
        , test( false )
        , port( 0 )
        {
            args[0] = 0;
        }

    char* args[1];
    bool  verbose;
    bool  test;
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
        arguments->verbose = true;
        break;
    case 't':
        arguments->test = true;
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
        if (state->arg_num >= 1) {
            // Too many arguments.
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 1) {
            // Not enough arguments.
            argp_usage (state);
        }
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
int
main(int argc, char **argv)
{
    // arg parsing
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    errno = 0;
    char* tail;
    int iNodeId = strtol(arguments.args[0], &tail, 0);
    if (errno) {
    perror("argument parsing failed:");
    return -1;
    }

    raw1394handle_t pHandle = raw1394_new_handle_on_port( arguments.port );
    if ( !pHandle ) {
        if ( !errno ) {
            cerr << "libraw1394 not compatible" << endl;
        } else {
            perror( "Could not get 1394 handle" );
            cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
        }
        return -1;
    }

    struct Connection {
        int m_output;
        int m_oplug;
        int m_input;
        int m_iplug;
        int m_iBandwith;
        int m_iIsoChannel;
    };


    int iLocalId  = raw1394_get_local_id( pHandle );
    int iRemoteId = iNodeId | 0xffc0;
    Connection cons[] = {
        //   output,  oplug,     input, iplug, bandwith, iso channel
        { iRemoteId,      0,  iLocalId,    -1,    0x148,          -1 },     // oPCR[0]
        { iRemoteId,      1,  iLocalId,    -1,    0x148,          -1 },     // oPCR[1]
        //        { iRemoteId,      2,  iLocalId,    -1,        0,          -1 },     // oPCR[2]: cmp not supported
        {  iLocalId,     -1, iRemoteId,     0,    0x148,          -1 },     // iPCR[0]
        {  iLocalId,     -1, iRemoteId,     1,    0x148,          -1 },     // iPCR[1]
        //        {  iLocalId,     -1, iRemoteId,     2,        0,          -1 },     // iPCR[2]: cmp not supported
    };

    printf( "local node id %d\n", iLocalId  & ~0xffc0);
    printf( "remote node id %d\n", iRemoteId & ~0xffc0);

    for ( unsigned int i = 0; i < sizeof( cons ) / sizeof( cons[0] ); ++i ) {
        Connection* pCons = &cons[i];

        // the bandwith calculation fails, so its better to use
        // some default values.
        pCons->m_iBandwith = iec61883_cmp_calc_bandwidth ( pHandle,
                                                           pCons->m_output,
                                                           pCons->m_oplug,
                                                           IEC61883_DATARATE_400 );
        sleep(1);
        pCons->m_iIsoChannel = iec61883_cmp_connect( pHandle,
                                                     pCons->m_output,
                                                     &pCons->m_oplug,
                                                     pCons->m_input,
                                                     &pCons->m_iplug,
                                                     &pCons->m_iBandwith );
        printf( "%2d -> %2d %cPCR[%2d]: bw = %4d, ch = %2d\n",
                pCons->m_output & ~0xffc0,
                pCons->m_input & ~0xffc0,
                pCons->m_oplug == -1? 'i' : 'o',
                pCons->m_oplug == -1? pCons->m_iplug: pCons->m_oplug,
                pCons->m_iBandwith,
                pCons->m_iIsoChannel );
        sleep(1);
    }

    sleep( 4 );

    for ( unsigned int i = 0; i < sizeof( cons ) / sizeof( cons[0] ); ++i ) {
        Connection* pCons = &cons[i];

        if ( pCons->m_iIsoChannel != -1 ) {
            iec61883_cmp_disconnect( pHandle,
                                     pCons->m_output,
                                     pCons->m_oplug,
                                     pCons->m_input,
                                     pCons->m_iplug,
                                     pCons->m_iIsoChannel,
                                     pCons->m_iBandwith );


        }
    }


    raw1394_destroy_handle( pHandle );
    return 0;
}
