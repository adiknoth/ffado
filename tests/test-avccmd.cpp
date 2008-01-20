/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_generic.h"

#include <argp.h>
#include <stdlib.h>
#include <iostream>

using namespace std;
using namespace AVC;
using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 1000

class TestCmd: public AVCCommand
{
public:
    TestCmd(Ieee1394Service& ieee1394service, opcode_t opcode );
    virtual ~TestCmd();

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TestCmd"; }
    
    byte_t args[MAX_ARGS];
    int nargs;
};

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "test-avccmd 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "test-avccmd -- test program to send a custom avc command to a specific node.";
static char args_doc[] = "NODE_ID [avc cmd byte sequence]";
static struct argp_option options[] = {
    {"verbose",   'v', 0,           0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"node",      'n', "NODE",      0,  "Set node" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( false )
        , test( false )
        , port( 0 )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    bool  verbose;
    bool  test;
    int   port;
    int   node;
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
    case 'n':
        errno = 0;
        arguments->node = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
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
        if(arguments->nargs<4) {
            printf("not enough arguments\n");
            return -1;
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
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }
    errno = 0;
    char* tail;


    Ieee1394Service *m_1394Service = new Ieee1394Service();
    if ( !m_1394Service ) {
        debugFatal( "Could not create Ieee1349Service object\n" );
        return false;
    }

    if ( !m_1394Service->initialize( arguments.port ) ) {
        debugFatal( "Could not initialize Ieee1349Service object\n" );
        delete m_1394Service;
        m_1394Service = 0;
        return false;
    }

    char cmdtype = strtol(arguments.args[0], &tail, 0);
    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    char unit_subunit = strtol(arguments.args[1], &tail, 0);
    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    opcode_t opcode = strtol(arguments.args[2], &tail, 0);
    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }
    
    TestCmd cmd( *m_1394Service, opcode );
    switch (cmdtype) {
        case AVC::AVCCommand::eCT_Control: cmd.setCommandType( AVC::AVCCommand::eCT_Control ); break;
        case AVC::AVCCommand::eCT_Status: cmd.setCommandType( AVC::AVCCommand::eCT_Status ); break;
        case AVC::AVCCommand::eCT_SpecificInquiry: cmd.setCommandType( AVC::AVCCommand::eCT_SpecificInquiry ); break;
        case AVC::AVCCommand::eCT_Notify: cmd.setCommandType( AVC::AVCCommand::eCT_Notify ); break;
        case AVC::AVCCommand::eCT_GeneralInquiry: cmd.setCommandType( AVC::AVCCommand::eCT_GeneralInquiry ); break;
        default: printf("Invalid command type: 0x%02X.\n", cmdtype); exit(-1);
    }
    cmd.setNodeId( arguments.node );
    
    cmd.setSubunitType( byteToSubunitType(unit_subunit >> 3) );
    cmd.setSubunitId( (unit_subunit & 0x7) );
    
    cmd.setVerbose( DEBUG_LEVEL_VERY_VERBOSE );
    
    int i=0;

    for (;i<arguments.nargs-3;i++) {
        char tmp = strtol(arguments.args[i+3], &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            break;
        }
        cmd.args[i]=tmp;
    }

    cmd.nargs=i;

    if ( !cmd.fire() ) {
        debugError( "TestCmd info command failed\n" );
        return 0;
    }

    delete m_1394Service;

    return 0;
}

TestCmd::TestCmd(Ieee1394Service& ieee1394service, opcode_t opcode )
    : AVCCommand( ieee1394service, opcode )
{
}

TestCmd::~TestCmd()
{
}

bool
TestCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= AVCCommand::serialize( se );
    for (int i=0;i<nargs;i++) {
        byte_t byte=args[i];
        result &= se.write(byte, "arg");
    }
    return result;
}

bool
TestCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= AVCCommand::deserialize( de );
    
    bool tmpresult=true;
    nargs=0;
    while(tmpresult=de.read(&args[nargs])) {
        nargs++;
    }
    
    return result;
}

