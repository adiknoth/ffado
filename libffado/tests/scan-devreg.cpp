/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2008 by Jonathan Woithe
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
#include "libutil/ByteSwap.h"

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/Time.h"

#include <argp.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 1000

#define MOTU_BASE_ADDR 0xfffff0000000ULL

// If changing these be sure to update the option help text
#define DEFAULT_SCAN_START_REG 0x0b00
#define DEFAULT_SCAN_LENGTH    0x0200

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "scan-devreg 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "scan-devreg -- scan device registers for changes.";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', "LEVEL",     0,  "Set verbose level" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"node",      'n', "NODE",      0,  "Set node" },
    {"start",     's', "REG",       0,  "Register to start scan at (default: 0x0b00)" },
    {"length",    'l', "LENGTH",    0,  "Number of bytes of register space to scan (default: 0x0200)" },
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( 0 )
        , test( false )
        , port( -1 )
        , node ( -1 )
        , scan_start ( DEFAULT_SCAN_START_REG )
        , scan_length ( DEFAULT_SCAN_LENGTH )
        {
            args[0] = 0;
        }

    char*        args[MAX_ARGS];
    int          nargs;
    signed int   verbose;
    bool         test;
    signed int   port;
    signed int   node;
    signed int   scan_start;
    signed int   scan_length;
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
        errno = 0;
        arguments->verbose = strtol(arg, &tail, 0);
        if (errno) {
             perror("argument parsing failed:");
             return errno;
        }
        break;
    case 't':
        arguments->test = true;
        break;
    case 's':
        errno = 0;
        arguments->scan_start = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        if (arguments->scan_start < 0) {
            fprintf(stderr, "Start of scan must be >= 0\n");
            return -1;
        }
        break;
    case 'l':
        errno = 0;
        arguments->scan_length = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        if (arguments->scan_length < 0) {
            fprintf(stderr, "Scan length must be >= 0\n");
            return -1;
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
    signed int loop = 0;
    char chr[100];
    signed int node_id = -1;
    signed int port = -1;
    signed int n_ports = -1;
    signed int p1, p2, n1, n2;

    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        fprintf( stderr, "Could not parse command line\n" );
        exit(-1);
    }
    errno = 0;

    setDebugLevel(arguments.verbose);

    // Do a really basic scan to find an audio device on the bus
    n_ports = Ieee1394Service::detectNbPorts();
    if (arguments.port < 0) {
      p1 = 0;
      p2 = n_ports;
    } else {
      // Scan a specific port if given on the command line
      p1 = p2 = arguments.port;
      p2++;
    }
    printf("Scanning %d firewire adapters (ports)\n", n_ports);
    for (signed int i=p1; i<p2; i++) {
        Ieee1394Service *tmp1394 = new Ieee1394Service();
        if ( !tmp1394 ) {
            debugFatal("Could not create Ieee1349Service for port %d\n", i);
            return false;
        }
        tmp1394->setVerboseLevel(getDebugLevel());
        if ( !tmp1394->initialize(i) ) {
            delete tmp1394;
            continue;
        }

        if (arguments.node < 0) {
          n1 = 0;
          n2 = tmp1394->getNodeCount();
        } else {
          // Scan a specific node if given on the command line
          n1 = n2 = arguments.node;
          n2++;
        }
        for (fb_nodeid_t node = n1; node < n2; node++) {
            std::auto_ptr<ConfigRom> configRom =
                std::auto_ptr<ConfigRom>( new ConfigRom(*tmp1394, node));
            if (!configRom->initialize()) {
                continue;
            }
            // Assume anything with suitable vendor IDs is an audio device. 
            // Stop at the first audio device found.  Currently we detect
            // only MOTU devices but this can be expanded on an as-needs
            // basis.
            if (configRom->getNodeVendorId() == 0x1f2) {
                node_id = node;
                port = i;
                break;
            }
        }
        delete tmp1394;
    }
    if (node_id == -1) {
        printf("Could not find a target audio device on the firewire bus\n");
        return false;
    }
    printf("Targetting device at port %d, node %d\n", port, node_id);

    Ieee1394Service *m_1394Service = new Ieee1394Service();
    if ( !m_1394Service ) {
        debugFatal( "Could not create Ieee1349Service object\n" );
        return false;
    }
    m_1394Service->setVerboseLevel(getDebugLevel());
    if ( !m_1394Service->initialize(port) ) {
        // This initialise call should never fail since it worked during the
        // scan.  We live in a strange world though, so trap the error
        // anyway.
        debugFatal("Could not initialise ieee1394 on MOTU port\n");
        delete m_1394Service;
        return false;
    }

    quadlet_t old_vals[arguments.scan_length/4+1], quadlet;
    unsigned int present[arguments.scan_length/(4*32)+1];
    memset(old_vals, 0x00, sizeof(old_vals));
    // Assume all registers are present until proven otherwise
    memset(present, 0xff, sizeof(present));
    
    printf("Scanning initial register values, please wait\n");
    chr[0] = 0;
    while(chr[0]!='q') {
        for (signed int reg=arguments.scan_start; 
             reg<arguments.scan_start+arguments.scan_length; reg+=4) {
            unsigned int reg_index = (reg-arguments.scan_start)/4;
            unsigned int pres_index = (reg-arguments.scan_start)/(4*32);
            unsigned int pres_bit = ((reg-arguments.scan_start)/4) & 0x1f;

            if (!(present[pres_index] & (1<<pres_bit))) {
                continue;
            }

            if (m_1394Service->read(0xffc0 | node_id, MOTU_BASE_ADDR+reg, 1, &quadlet) <= 0) {
                // Flag the register as not being present so we don't keep trying to read it
                present[pres_index] &= ~(1<<pres_bit);
                continue;
            } else {
                quadlet = CondSwap32(quadlet);
            }
            
            if (old_vals[reg_index] != quadlet) {
                if (loop != 0) {
                    printf("0x%04x changed from %08X to %08X\n", reg,  old_vals[reg_index], quadlet);
                }
                old_vals[reg_index] = quadlet;
            }
        }
        printf("Press <Enter> to scan, \"q<Enter>\" to exit\n");
        fgets(chr, sizeof(chr), stdin);
        loop++;
    }

    delete m_1394Service;

    return 0;
}

