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

#include "fbtypes.h"
#include "downloader.h"

#include "config.h"

#include "src/fireworks/fireworks_device.h"
#include "src/fireworks/fireworks_firmware.h"
#include "src/fireworks/fireworks_session_block.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libutil/Configuration.h"

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"
#include "version.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

#define MAGIC_THAT_SAYS_I_KNOW_WHAT_IM_DOING 0x001807198000LL

using namespace FireWorks;

DECLARE_GLOBAL_DEBUG_MODULE;

////////////////////////////////////////////////
// arg parsing
////////////////////////////////////////////////
const char *argp_program_version = "fireworks-downloader 0.3";
const char *argp_program_bug_address = "<ffado-devel@lists.sf.net>";
const char *doc = "fireworks-downloader -- firmware downloader application for ECHO Fireworks devices\n\n"
                    "OPERATIONS:\n" 
                    "           list\n"
                    "              Lists devices on the bus\n"
                    "           display\n"
                    "              Display information about a device and it's firmware\n"
                    "           info FILE\n"
                    "              Display information about the firmware contained in FILE\n"
                    "           upload FILE\n"
                    "              Upload the firmware contained in FILE to the device\n"
                    "           download FILE START_ADDR LEN\n"
                    "              Download the flash contents from the device to FILE\n"
                    "              Starts at address START_ADDR and reads LEN quadlets\n"
                    "           verify FILE\n"
                    "              Verify that the firmware contained in the device corresponds\n"
                    "              to the one contained in FILE\n"
                    "           session_display\n"
                    "              show information about the session on the device\n"
                    "           session_info FILE\n"
                    "              show information about the session in FILE\n"
                    "           session_download FILE\n"
                    "              Download the session content from the device to FILE\n"
                    "           session_upload FILE\n"
                    "              Upload the session from FILE to the device\n"
                    ;

static struct argp_option _options[] = {
    {"verbose",   'v', "LEVEL",     0,  "Produce verbose output (set level 0-10)" },
    {"guid",      'g', "GUID",      0,  "GUID of the target device" },
    {"port",      'p', "PORT",      0,  "Port to use" },
    {"magic",     'm', "MAGIC",     0,  "A magic number you have to obtain before this code will work." 
                                        "Specifying it means that you accept the risks that come with this tool."},
    { 0 }
};
struct argp_option* options = _options;

int
main( int argc, char** argv )
{
    using namespace std;

    printf("-----------------------------------------------\n");
    printf("ECHO FireWorks platform firmware downloader\n");
    printf("Part of the FFADO project -- www.ffado.org\n");
    printf("Version: %s\n", PACKAGE_VERSION);
    printf("(C) 2008, Pieter Palmers\n");
    printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
    printf("-----------------------------------------------\n\n");

    memset(args, 0, sizeof(args));

    args->guid = 0xFFFFFFFFFFFFFFFFLL;
    argp_parse (argp, argc, argv, 0, 0, args);

    setDebugLevel(args->verbose);

    errno = 0;
    int node_id = -1;

    fb_octlet_t guid = args->guid;

    if(args->nargs < 1) {
        argp_help(argp, stderr, ARGP_HELP_USAGE | ARGP_HELP_LONG | ARGP_HELP_DOC, argv[0]);
        exit(-1);
    }

    if(args->magic != MAGIC_THAT_SAYS_I_KNOW_WHAT_IM_DOING) {
        printf("Magic number not correct. Please specify the correct magic using the '-m' option.\n");
        printf("Manipulating firmware can cause your device to magically stop working (a.k.a. 'bricking').\n");
        printf("Specifying the magic number indicates that you accept the risks involved\n");
        printf("with using this tool. The magic number can be found in the source code.\n\n");
        return -1;
    } else {
        printf("YOU HAVE SPECIFIED THE CORRECT MAGIC NUMBER.\n");
        printf("HENCE YOU ACCEPT THE RISKS INVOLVED.\n");
    }

    // first do the operations for which we don't need a device
    if ( strcmp( args->args[0], "info" ) == 0 ) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            return -1;
        }
        std::string str( args->args[1] );

        // load the file
        Firmware f = Firmware();
        f.setVerboseLevel( args->verbose );
        
        if (!f.loadFile(str)) {
            printMessage("Could not load firmware\n");
            return -1;
        }
        f.show();
        return 0;
    } else if ( strcmp( args->args[0], "dumpinfo" ) == 0 ) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            return -1;
        }
        std::string str( args->args[1] );

        // load the file
        Firmware f = Firmware();
        f.setVerboseLevel( args->verbose );
        
        if (!f.loadFile(str)) {
            printMessage("Could not load firmware\n");
            return -1;
        }
        f.show();
        f.dumpData();
        return 0;
    } else if ( strcmp( args->args[0], "session_info" ) == 0 ) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            return -1;
        }
        std::string str( args->args[1] );

        // load the file
        Session s = Session();
        s.setVerboseLevel( args->verbose );

        if (!s.loadFromFile(str)) {
            printMessage("Could not load session\n");
            return -1;
        }
        s.show();
        return 0;
    } else if ( strcmp( args->args[0], "list" ) == 0 ) {
        printDeviceList();
        exit(0);
    }

    // we need a device, so find the specified device
    if (guid == 0xFFFFFFFFFFFFFFFFLL) {
        printMessage("No GUID specified\n");
        exit(-1);
    }

    Ieee1394Service service;
    if ( !service.initialize( args->port ) ) {
        printMessage("Could not initialize IEEE 1394 service\n");
        return -1;
    }
    service.setVerboseLevel( args->verbose );
 
    for (int i = 0; i < service.getNodeCount(); i++) {
        ConfigRom configRom(service, i);
        configRom.initialize();
        if (configRom.getGuid() == guid) {
            node_id = configRom.getNodeId();
            break;
        }
    }

    if (node_id < 0) {
        printMessage("Could not find device with GUID 0x%016"PRIX64"\n", guid);
        return -1;
    }

    ConfigRom *configRom = new ConfigRom(service, node_id );
    if (configRom == NULL) {
        printMessage("Could not create ConfigRom\n");
        return -1;
    }
    configRom->setVerboseLevel( args->verbose );

    if ( !configRom->initialize() ) {
        printMessage( "Could not read config rom from device (node id %d).\n",
                    node_id );
        delete configRom;
        return -1;
    }

    Util::Configuration c;
    c.openFile( USER_CONFIG_FILE, Util::Configuration::eFM_ReadOnly );
    c.openFile( SYSTEM_CONFIG_FILE, Util::Configuration::eFM_ReadOnly );

    if ( !FireWorks::Device::probe(c, *configRom) ) {
        printMessage( "Device with node id %d is not an ECHO FireWorks device.\n",
                    node_id );
        delete configRom;
        return -1;
    }

    DeviceManager d = DeviceManager();
    Device *dev = new Device(d, std::auto_ptr<ConfigRom>(configRom) );
    if (dev == NULL) {
        printMessage("Could not create FireWorks::Device\n");
        delete configRom;
        return -1;
    }
    dev->setVerboseLevel(args->verbose);

    if ( strcmp( args->args[0], "display" ) == 0 ) {
        // nothing to do
        if (args->verbose < DEBUG_LEVEL_INFO) {
            dev->setVerboseLevel(DEBUG_LEVEL_INFO);
        }
        dev->showDevice();
    } else if (strcmp( args->args[0], "download" ) == 0) {
        if (args->nargs < 3) {
            printMessage("Address range not specified\n");
            delete dev;
            return -1;
        }
        errno = 0;
        uint32_t start_addr = strtol(args->args[2], NULL, 0);        
        if (errno) {
            printMessage("start address parsing failed: %s\n",
                       strerror(errno));
            delete dev;
            return errno;
        }
        uint32_t len = strtol(args->args[3], NULL, 0);
        if (errno) {
            printMessage("length parsing failed: %s\n",
                       strerror(errno));
            delete dev;
            return errno;
        }
        
        // create the firmware util class
        FirmwareUtil util = FirmwareUtil(*dev);
        util.setVerboseLevel( args->verbose );
        
        Firmware f = util.getFirmwareFromDevice(start_addr, len);
        f.setVerboseLevel( args->verbose );
        f.show();
        f.dumpData();
        printMessage("Saving to file not yet supported.\n");
    } else if (strcmp( args->args[0], "verify" ) == 0) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            delete dev;
            return -1;
        }
        std::string str( args->args[1] );

        printMessage("Verifying device versus file: %s\n", str.c_str());

        printMessage(" loading file...\n");
        // load the file
        Firmware ref = Firmware();
        ref.setVerboseLevel( args->verbose );
        
        if (!ref.loadFile(str)) {
            printMessage("Could not load firmware from file\n");
            delete dev;
            return -1;
        }

        // get the flash position from the loaded file
        uint32_t start_addr = ref.getAddress();
        uint32_t len = ref.getLength();

        // create the firmware util class
        FirmwareUtil util = FirmwareUtil(*dev);
        util.setVerboseLevel( args->verbose );
        
        printMessage(" reading device...\n");
        // read the corresponding part of the device
        Firmware f = util.getFirmwareFromDevice(start_addr, len);
        f.setVerboseLevel( args->verbose );
        f.show();
        
        printMessage(" comparing...\n");
        // compare the two images
        if(!(f == ref)) {
            printMessage(" => Verify failed. Device content not the same as file content.\n");
            delete dev;
            return -1;
        } else {
            printMessage(" => Verify successful. Device content identical to file content.\n");
        }
    } else if (strcmp( args->args[0], "upload" ) == 0) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            delete dev;
            return -1;
        }
        std::string str( args->args[1] );

        // create the firmware util class
        FirmwareUtil util = FirmwareUtil(*dev);
        util.setVerboseLevel( args->verbose );

        printMessage("Uploading %s to device\n", str.c_str());

        printMessage(" loading file...\n");
        // load the file
        Firmware ref = Firmware();
        ref.setVerboseLevel( args->verbose );
        
        if (!ref.loadFile(str)) {
            printMessage("  Could not load firmware from file\n");
            delete dev;
            return -1;
        }

        printMessage(" checking file...\n");
        if(!ref.isValid()) {
            printMessage("  Firmware not valid\n");
            delete dev;
            return -1;
        }

        if(!util.isValidForDevice(ref)) {
            printMessage("  Firmware not valid for this device\n");
            delete dev;
            return -1;
        }
        printMessage("  seems to be valid firmware for this device...\n");

        // get the flash position from the loaded file
        uint32_t start_addr = ref.getAddress();
        uint32_t len = ref.getLength();

        printMessage(" lock flash...\n");
        if (!dev->lockFlash(true)) {
            printMessage("  Could not lock flash\n");
            delete dev;
            return -1;
        }

        printMessage(" erasing memory...\n");
        if (!util.eraseBlocks(start_addr, len)) {
            printMessage("  Could not erase memory\n");
            delete dev;
            return -1;
        }

        printMessage(" uploading to device...\n");
        if (!util.writeFirmwareToDevice(ref)) {
            printMessage("  Could not write firmware to device\n");
            delete dev;
            return -1;
        }

        printMessage(" unlock flash...\n");
        if (!dev->lockFlash(false)) {
            printMessage("  Could not unlock flash\n");
            delete dev;
            return -1;
        }

        // now verify
        printMessage(" verify...\n");


        printMessage("  reading device...\n");
        // read the corresponding part of the device
        Firmware f = util.getFirmwareFromDevice(start_addr, len);
        f.setVerboseLevel( args->verbose );
        f.show();
        
        printMessage("  comparing...\n");
        // compare the two images
        if(!(f == ref)) {
            printMessage(" => Verify failed. Firmware upload failed.\n");
            delete dev;
            return -1;
        } else {
            printMessage(" => Verify successful. Firmware upload successful.\n");
        }
    } else if ( strcmp( args->args[0], "session_display" ) == 0 ) {
        // load the session
        Session s = Session();
        s.setVerboseLevel( args->verbose );

        if (!s.loadFromDevice(*dev)) {
            printMessage("Could not load session\n");
            return -1;
        }
        s.show();
    } else if ( strcmp( args->args[0], "session_download" ) == 0 ) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            delete dev;
            return -1;
        }
        std::string str( args->args[1] );

        printMessage("Downloading session to file: %s\n", str.c_str());

        printMessage(" loading session...\n");
        // load the session
        Session s = Session();
        s.setVerboseLevel( args->verbose );

        if (!s.loadFromDevice(*dev)) {
            printMessage("Could not load session from device\n");
            return -1;
        }

        printMessage(" saving session...\n");
        if (!s.saveToFile(str)) {
            printMessage("Could not save session to file\n");
            return -1;
        }
    } else if ( strcmp( args->args[0], "session_upload" ) == 0 ) {
        if (!args->args[1] ) {
            printMessage("FILE argument is missing\n");
            delete dev;
            return -1;
        }
        std::string str( args->args[1] );

        printMessage("Uploading session from file: %s\n", str.c_str());

        printMessage(" loading session...\n");
        // load the session
        Session s = Session();
        s.setVerboseLevel( args->verbose );
        if (!s.loadFromFile(str)) {
            printMessage("Could not load session from file\n");
            return -1;
        }

        printMessage(" saving session...\n");
        if (!s.saveToDevice(*dev)) {
            printMessage("Could not save session to device\n");
            return -1;
        }

    }  else {
        printMessage("Unknown operation\n");
    }

    delete dev;
    return 0;
}
