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

#include "config.h"

#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/Configuration.h"

#include "libcontrol/MatrixMixer.h"
#include "libcontrol/CrossbarRouter.h"

#include "dice/dice_avdevice.h"
#include "dice/dice_eap.h"

using namespace Dice;

#include <argp.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <signal.h>
int run;

static void sighandler(int sig)
{
    run = 0;
}

using namespace std;
using namespace Util;

DECLARE_GLOBAL_DEBUG_MODULE;

#define MAX_ARGS 1000

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "set-default-router-config-dice-eap 0.1";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "set-default-router-config-dice-eap -- use set up default router configuration of dice eap code.";
static char args_doc[] = "NODE_ID";
static struct argp_option options[] = {
    {"verbose",   'v', "LEVEL",      0,  "Produce verbose output" },
    {"port",      'p', "PORT",       0,  "Set port" },
    {"node",      'n', "NODE",       0,  "Set node" },
    {"samplerate",'s', "SAMPLERATE", 0,  "Prescribe the sample rate"},
   { 0 }
};

struct arguments
{
    arguments()
        : nargs ( 0 )
        , verbose( DEBUG_LEVEL_NORMAL )
        , test( false )
        , port( -1 )
        , node( -1 )
        , samplerate( 48000 )
        {
            args[0] = 0;
        }

    char* args[MAX_ARGS];
    int   nargs;
    int   verbose;
    bool  test;
    int   port;
    int   node;
    int   samplerate;
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
        arguments->verbose = strtol(arg, &tail, 0);
        break;
    case 't':
        arguments->test = true;
        break;
    case 's':
        arguments->samplerate = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case 'p':
        arguments->port = strtol(arg, &tail, 0);
        if (errno) {
            perror("argument parsing failed:");
            return errno;
        }
        break;
    case 'n':
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
        if(arguments->nargs<0) {
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
    run=1;

    signal (SIGINT, sighandler);
    signal (SIGPIPE, sighandler);

    // arg parsing
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        printMessage("Could not parse command line\n" );
        exit(-1);
    }
    errno = 0;

    DeviceManager *m_deviceManager = new DeviceManager();
    if ( !m_deviceManager ) {
        printMessage("Could not allocate device manager\n" );
        return -1;
    }

    if ( arguments.verbose ) {
        m_deviceManager->setVerboseLevel(arguments.verbose);
    }

    if ( !m_deviceManager->initialize() ) {
        printMessage("Could not initialize device manager\n" );
        delete m_deviceManager;
        return -1;
    }

    char s[1024];
    if(arguments.node > -1) {
        snprintf(s, 1024, "hw:%d,%d", arguments.port, arguments.node);
        if ( !m_deviceManager->addSpecString(s) ) { 
            printMessage("Could not add spec string %s to device manager\n", s );
            delete m_deviceManager;
            return -1;
        }
    } else {
        snprintf(s, 1024, "hw:%d", arguments.port);
        if ( !m_deviceManager->addSpecString(s) ) { 
            printMessage("Could not add spec string %s to device manager\n", s );
            delete m_deviceManager;
            return -1;
        }
    }

    if ( !m_deviceManager->discover(false) ) {
        printMessage("Could not discover devices\n" );
        delete m_deviceManager;
        return -1;
    }

    if(m_deviceManager->getAvDeviceCount() == 0) {
        printMessage("No devices found\n");
        delete m_deviceManager;
        return -1;
    }

    Dice::Device* avDevice = dynamic_cast<Dice::Device*>(m_deviceManager->getAvDeviceByIndex(0));

    if(avDevice == NULL) {
        printMessage("Device is not a DICE device\n" );
        delete m_deviceManager;
        return -1;
    }

    // now play
    //avDevice->setVerboseLevel(DEBUG_LEVEL_VERY_VERBOSE);

    bool supports_eap = EAP::supportsEAP(*avDevice);
    if (!supports_eap) {
        printMessage("EAP not supported on this device\n");
        delete m_deviceManager;
        return -1;
    }
    printMessage("device supports EAP\n");

    EAP &eap = *(avDevice->getEAP());

    // Fix the samplerate if necessary
    int samplerate = avDevice->getSamplingFrequency();
    if (arguments.samplerate != samplerate) {
        if (avDevice->setSamplingFrequency(arguments.samplerate)) {
          samplerate = avDevice->getSamplingFrequency();
        }
        else {
          printMessage("Unable to fix samplerate\n");
          delete m_deviceManager;
          return -1;
        }
    }

    printMessage(" Initial configuration\n");
    eap.showFullRouter();
    eap.showFullPeakSpace();

    // set default router configuration at present sample rate
    printMessage(" Setting default configuration at samplerate %d\n", samplerate);
    eap.setupDefaultRouterConfig();

    printMessage(" Present configuration\n");
    eap.showFullRouter();
    eap.showFullPeakSpace();

    // cleanup
    delete m_deviceManager;
    return 0;
}

