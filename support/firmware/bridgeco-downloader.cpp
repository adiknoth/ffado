/*
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

#include "downloader.h"

#include "src/bebob/bebob_dl_mgr.h"
#include "src/bebob/bebob_dl_bcd.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include <iostream>

using namespace std;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "bridgeco-downloader 0.2";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
const char *doc = "bridgeco-downloader -- firmware downloader application for BridgeCo devices\n\n"
                    "OPERATION: display\n"
                    "           setguid GUID\n"
                    "           firmware FILE\n"
                    "           cne FILE\n"
                    "           bcd FILE\n";
static struct argp_option _options[] = {
    {"verbose",   'v', "level",     0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    {"force",     'f', 0,           0,  "Force firmware download" },
    {"noboot",    'b', 0,           0,  "Do no start bootloader (bootloader is already running)" },
    { 0 }
};
struct argp_option* options = _options;

int
main( int argc, char** argv )
{

    // arg parsing
    argp_parse (argp, argc, argv, 0, 0, args);

    errno = 0;
    char* tail;
    int node_id = -1;

    fb_octlet_t guid = strtoll(args->args[0], &tail, 0);
    if (errno) {
        perror("argument parsing failed:");
        return -1;
    }

    Ieee1394Service service;
    if ( !service.initialize( args->port ) ) {
        cerr << "Could not initialize IEEE 1394 service" << endl;
        return -1;
    }
    service.setVerboseLevel( args->verbose );

    for (int i = 0; i < service.getNodeCount(); i++) {
        ConfigRom configRom(service, i);
        configRom.initialize();
        
        if (configRom.getGuid() == guid)
            node_id = configRom.getNodeId();
    }

    if (node_id < 0) {
        cerr << "Could not find device with matching GUID" << endl;
        return -1;
    }

    service.setVerboseLevel( args->verbose );

    BeBoB::BootloaderManager blMgr( service, node_id );
    if ( args->force == 1 ) {
        blMgr.setForceOperations( true );
    }
    blMgr.getConfigRom()->printConfigRom();
    blMgr.printInfoRegisters();

    if ( strcmp( args->args[1], "setguid" ) == 0 ) {
        if (!args->args[2] ) {
            cerr << "guid argument is missing" << endl;
            return -1;
        }

        char* tail;
        fb_octlet_t guid = strtoll(args->args[2], &tail, 0 );

        if ( !blMgr.programGUID( guid ) ) {
            cerr << "Failed to set GUID" << endl;
            return -1;
        } else {
            cout << "new GUID programmed" << endl;
        }
    } else if ( strcmp( args->args[1], "firmware" ) == 0 ) {
        if (!args->args[2] ) {
            cerr << "FILE argument is missing" << endl;
            return -1;
        }
        std::string str( args->args[2] );

        blMgr.setStartBootloader( args->no_bootloader_restart != 1 );
        if ( !blMgr.downloadFirmware( str ) ) {
            cerr << "Failed to download firmware" << endl;
            return -1;
        } else {
            cout << "Firmware download was successful" << endl;
        }
    } else if ( strcmp( args->args[1], "cne" ) == 0 ) {
        if (!args->args[2] ) {
            cerr << "FILE argument is missing" << endl;
            return -1;
        }
        std::string str( args->args[2] );

        if ( !blMgr.downloadCnE( str ) ) {
            cerr << "Failed to download CnE" << endl;
            return -1;
        } else {
            cout << "CnE download was successful" << endl;
        }
    } else if ( strcmp( args->args[1], "display" ) == 0 ) {
        // nothing to do
    } else if ( strcmp( args->args[1], "bcd" ) == 0 ) {
        if ( !args->args[2] ) {
            cerr << "FILE arguments is missing" << endl;
            return -1;
        }
        BeBoB::BCD* bcd = new BeBoB::BCD::BCD( args->args[2] );
        if ( !bcd ) {
            cerr << "Could no open file " << args->args[2] << endl;
            return -1;
        }
        if ( !bcd->parse() ) {
            cerr << "Could not parse file " << args->args[2] << endl;
            return -1;
        }

        bcd->displayInfo();
        delete bcd;
    } else {
        cout << "Unknown operation" << endl;
    }

    return 0;
}
