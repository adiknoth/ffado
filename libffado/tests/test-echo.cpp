/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>
#include <libavc1394/avc1394.h>
#include "libutil/Time.h"

#include <argp.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-echo 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-echo -- test program for the ECHO AUDIOFIRE devices";
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
    quadlet_t cmd[6];
    unsigned int response_len;
    
//     cerr << "Opening descriptor" << endl;
//     0h 05m 21.760442s - GetDescriptorOpen:  cmd[0]= < 0x00 0x60 0x08 0x80 0x01 0xFF>
//     0h 05m 21.760687s - GetDescriptorOpen: resp[0]= < 0x09 0x60 0x08 0x80 0x01 0xFF > ACCEPTED


//     cmd[0] = 0x00600880;
//     cmd[1] = 0x01FF0000;
//     avc1394_transaction_block2(pHandle, 0xffc0 | iNodeId, cmd, 2, &response_len, 10);
//     SleepRelativeUsec(100000); 
//     
//     cerr << "Reading descriptor" << endl;
// //     0h 05m 21.760700s - GetDescriptorRead:  cmd[0]= < 0x00 0x60 0x09 0x80 0xFF 0xFF 0x00 0x00 0x00 0x00>
// //     0h 05m 21.761123s - GetDescriptorRead: resp[0]= < 0x09 0x60 0x09 0x80 0x11 0xFF 0x01 0xF6 0x00 0x00 0x03 0x9E 
//     cmd[0] = 0x00600980;
//     cmd[1] = 0xFFFF0000;
//     cmd[2] = 0x00000000;
//     cmd[2] = 0x00000000;
//      
//     avc1394_transaction_block2(pHandle, 0xffc0 | iNodeId, cmd, 3, &response_len, 10);
//     SleepRelativeUsec(100000);
//     
//     cerr << "Closing descriptor" << endl;
//     cmd[0] = 0x00600880;
//     cmd[1] = 0x00FF0000;
//     avc1394_transaction_block2(pHandle, 0xffc0 | iNodeId, cmd, 2, &response_len, 10);
//     SleepRelativeUsec(100000);

    cerr << "getting signal source" << endl;
//     0h 05m 21.762917s - at line 2283, fMaxAudioOutputChannels=2, fMaxAudioInputChannels=0
//     0h 05m 21.762919s - GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0xFF 0x00>
//     0h 05m 21.763149s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x60 0x60 0x00 0xFF 0x00 > IMPLEMENTED
//     0h 05m 21.763167s - Isoch out 0 gets its signal from sub/unit 0x60 Source Plug 0
//     0h 05m 21.763170s - GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0xFF 0x80>
//     0h 05m 21.763376s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x00 0x60 0x01 0xFF 0x80 > IMPLEMENTED
//     0h 05m 21.763394s - Isoch out 128 gets its signal from sub/unit 0x60 Source Plug 1
//     0h 05m 21.763397s - GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0xFF 0x81>
//     0h 05m 21.763637s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x00 0x60 0x02 0xFF 0x81 > IMPLEMENTED
//     0h 05m 21.763654s - Isoch out 129 gets its signal from sub/unit 0x60 Source Plug 2

//     0h 05m 21.764895s - Starting to look at subunit 1.  fNumberOfSubUnits = 2
//     0h 05m 21.764897s - Subunit 1 GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0x60 0x00>
//     0h 05m 21.765129s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x20 0xFF 0x00 0x60 0x00 > IMPLEMENTED
//     0h 05m 21.765140s - Subunit type12, addr:0x60 dest 0 gets its signal from sub/unit 0xff Source Plug 0
//     0h 05m 21.765142s - subunit 96 dest plug 0 is routed
//     0h 05m 21.765143s - Subunit 1 GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0x60 0x01>
//     0h 05m 21.765364s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x00 0xFF 0x80 0x60 0x01 > IMPLEMENTED
//     0h 05m 21.765382s - Subunit type12, addr:0x60 dest 1 gets its signal from sub/unit 0xff Source Plug 128
//     0h 05m 21.765385s - 	Plug being changed from 0x80 to 1 for internal bookeeping.
//     0h 05m 21.765389s - Subunit 1 GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0x60 0x02>
//     0h 05m 21.765632s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x00 0xFF 0x81 0x60 0x02 > IMPLEMENTED
//     0h 05m 21.765651s - Subunit type12, addr:0x60 dest 2 gets its signal from sub/unit 0xff Source Plug 129
//     0h 05m 21.765653s - 	Plug being changed from 0x81 to 2 for internal bookeeping.
//     0h 05m 21.765657s - Subunit 1 GetSignalSource:  cmd[0]= < 0x01 0xFF 0x1A 0xFF 0xFF 0xFE 0x60 0x03>
//     0h 05m 21.765874s - GetSignalSource: resp[0]= < 0x0c 0xFF 0x1A 0x00 0x60 0x03 0x60 0x03 > IMPLEMENTED
//     0h 05m 21.765892s - Subunit type12, addr:0x60 dest 3 gets its signal from sub/unit 0x60 Source Plug 3

    cmd[0] = 0x01FF1AFF;
    cmd[1] = 0xFFFE6000;
    avc1394_transaction_block2(pHandle, 0xffc0 | iNodeId, cmd, 2, &response_len, 10);
//     SleepRelativeUsec(100000);
    
    cmd[0] = 0x01FF1AFF;
    cmd[1] = 0xFFFEFF00;
    avc1394_transaction_block2(pHandle, 0xffc0 | iNodeId, cmd, 2, &response_len, 10);
    SleepRelativeUsec(100000);



    raw1394_destroy_handle( pHandle );
    return 0;
}
