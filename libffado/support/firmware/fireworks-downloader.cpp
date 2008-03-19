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

#include "downloader.h"

#include "src/fireworks/fireworks_device.h"
#include "src/fireworks/fireworks_firmware.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include <iostream>

#include <string.h>
#include <string>

using namespace FireWorks;

DECLARE_GLOBAL_DEBUG_MODULE;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "fireworks-downloader 0.2";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
const char *doc = "fireworks-downloader -- firmware downloader application for ECHO Fireworks devices\n\n"
                    "OPERATION: display\n"
                    "           firmware FILE\n";

static struct argp_option _options[] = {
    {"verbose",   'v', "level",     0,  "Produce verbose output" },
    {"port",      'p', "PORT",      0,  "Set port" },
    { 0 }
};
struct argp_option* options = _options;

int
main( int argc, char** argv )
{
    using namespace std;

    argp_parse (argp, argc, argv, 0, 0, args);

    errno = 0;
    char* tail;
    int node_id = -1;

    fb_octlet_t guid = strtoll(args->args[0], &tail, 0);
    if (errno) {
        debugError("argument parsing failed: %s\n",
                    strerror(errno));
        return -1;
    }

    Ieee1394Service service;
    if ( !service.initialize( args->port ) ) {
        debugError("Could not initialize IEEE 1394 service\n");
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

    ConfigRom *configRom = new ConfigRom(service, node_id );
    if (configRom == NULL) {
        debugError("Could not create ConfigRom\n");
        return -1;
    }
    configRom->setVerboseLevel( args->verbose );

    if ( !configRom->initialize() ) {
        debugError( "Could not read config rom from device (node id %d).\n",
                    node_id );
        delete configRom;
        return -1;
    }

    if ( !Device::probe(*configRom) ) {
        debugError( "Device with node id %d is not an ECHO FireWorks device.\n",
                    node_id );
        delete configRom;
        return -1;
    }
    
    DeviceManager d = DeviceManager();
    Device *dev = new Device(d, std::auto_ptr<ConfigRom>(configRom) );
    if (dev == NULL) {
        debugError("Could not create FireWorks::Device\n");
        delete configRom;
        return -1;
    }

    // create the firmware util class
    FirmwareUtil util = FirmwareUtil(*dev);
    util.setVerboseLevel( args->verbose );
    
    if ( strcmp( args->args[1], "firmware" ) == 0 ) {
        if (!args->args[2] ) {
            cerr << "FILE argument is missing" << endl;
            delete dev;
            return -1;
        }
        std::string str( args->args[2] );

        // load the file
        Firmware f = Firmware();
        f.setVerboseLevel( args->verbose );
        
        f.loadFile(str);
        f.show();

    } else if ( strcmp( args->args[1], "display" ) == 0 ) {
        // nothing to do
        dev->showDevice();
    } else {
        cout << "Unknown operation" << endl;
    }

    delete dev;
    return 0;
}
