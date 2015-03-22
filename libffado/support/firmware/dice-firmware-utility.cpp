/*
 * Copyright (C) 2012 by Peter Hinkel
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
 * Implementation of the DICE firmware loader specification based upon
 * TCAT public available document v3.2.0.0 (2008-06-20)
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
#include "dice/dice_firmware_loader.h"
#include "dice/dice_eap.h"

using namespace Dice;

#include <argp.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

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
const char *argp_program_version = "dice-firmware-utility v1.0";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
static char doc[] = "dice-firmware-utility -- utility for flashing DICE device firmware\n\n"
					"Part of the FFADO project -- www.ffado.org\n"
					"Version: 1.0\n"
					"(C) 2012, Peter Hinkel\n"
					"This program comes with absolutely no warranty.\n\n"
					"OPERATIONS:\n"
					"test\n"
					"   POKE = 1 (write addr), PEEK = 2 (read addr)\n"
					"verbose\n"
					"   LEVEL = 0..8, 4 (default), disabled (-1)\n\n"
					"example: dice-firmware-utility -p 0 -f firmware.bin";

static char args_doc[] = "";
static struct argp_option options[] = {
	{"application",	'a', NULL,			0, "Show dice image application information"},
	{"delete",		'd', "IMAGENAME",	0, "Delete image by name"},
	{"dump",		'e', "FILENAME",	0, "Extract DICE flash into file"},
	{"flash",		'f', "FILENAME",	0, "Write firmware file to DICE device"},
	{"image-info",	'i', NULL,			0, "Show detailed information about each image within firmware"},
	{"flash-info",	'm', NULL,			0, "Show information about flash memory"},
	{"dice-info",	's', NULL,			0, "Show dice image vendor and product information"},
	{"test",		't', "OPERATION",	0, "Test and Debug operation"},
	{"node",		'n', "NODE",		0, "Set node"},
	{"port",		'p', "PORT",		0, "Set port"},
	{"verbose",		'v', "LEVEL",		0, "Verbosity for DEBUG output"},
	{0}
};

struct arguments {

	arguments()
		: nargs(0)
		, application(false)
		, del(false)
		, dump(false)
		, flash(false)
		, image_info(false)
		, flash_info(false)
		, dice_info(false)
		, test(0)
		, filename(NULL)
		, imgname(NULL)
		, node(-1)
		, port(-1)
		, verbose(4/*-1*/)

		{
			args[0] = 0;
		}

	char* args[MAX_ARGS];
	int   nargs;
	bool  application;
	bool  del;
	bool  dump;
	bool  flash;
	bool  image_info;
	bool  flash_info;
	bool  dice_info;
	int   test;
	char* filename;
	char* imgname;
	int   node;
	int   port;
	int   verbose;
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
	case 'a':
		arguments->application = true;
		break;
	case 'd':
		arguments->del = true;
		arguments->imgname = arg;
		break;
	case 'e':
		arguments->dump = true;
		arguments->filename = arg;
		break;
	case 'f':
	arguments->flash = true;
		arguments->filename = arg;
		break;
	case 'i':
		arguments->image_info = true;
		break;
	case 'm':
		arguments->flash_info = true;
		break;
	case 's':
		arguments->dice_info = true;
		break;
	case 't':
		arguments->test = strtol(arg, &tail, 0);
		if (errno) {
			perror("argument parsing failed:");
			return errno;
		}
		break;
	case 'v':
		arguments->verbose = strtol(arg, &tail, 0);
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
	if (argc<=1) {
		cout << "Try `" << argv[0] << " --help' or `" << argv[0] << " --usage' for more information." << endl;
		return -1;
	}

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

	if (argc<=5) {

		if (arguments.application) {
			avDevice->showAppInfoFL();
		}

		if (arguments.del) {
			avDevice->deleteImgFL(arguments.imgname);
		}

		if (arguments.dump) {
			avDevice->dumpFirmwareFL(arguments.filename);
		}

		if (arguments.flash) {
			avDevice->flashDiceFL(arguments.filename);
		}

		if (arguments.image_info) {
			avDevice->showImgInfoFL();
		}

		if (arguments.flash_info) {
			avDevice->showFlashInfoFL();
		}

		if (arguments.dice_info) {
			avDevice->showDiceInfoFL();
		}

		if (arguments.test) {
			avDevice->testDiceFL(arguments.test);
		}
	} else {
		printMessage("Too many arguments...");
	}

	// cleanup
	delete m_deviceManager;
	return 0;
}
