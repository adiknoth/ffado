/*
 * Copyright (C) 2009-2010 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

/*
 * Test programs for the new firewire kernel stack.
 * Dumpiso.
 */

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <argp.h>

#include <stdlib.h>

#include <string.h>

#include "debugmodule/debugmodule.h"
DECLARE_GLOBAL_DEBUG_MODULE;

// Program documentation.
static char doc[] = "FFADO -- List devices and busses available (new stack)\n\n";

// A description of the arguments we accept.
static char args_doc[] = "";

#define MAX_EXTRA_ARGS 2
struct arguments
{
    long int verbose;
    long int maxindex;
    char* args[MAX_EXTRA_ARGS];
};

// The options we understand.
static struct argp_option options[] = {
    {"verbose",  'v', "level",    0,  "Verbose level" },
    {"maxindex",  'i', "index",    0,  "Max X to check /dev/fwX for" },
    { 0 }
};

// Parse a single option.
#define PARSE_ARG_LONG(XXletterXX, XXvarXX, XXdescXX) \
    case XXletterXX: \
        if (arg) { \
            XXvarXX = strtol( arg, &tail, 0 ); \
            if ( errno ) { \
                fprintf( stderr,  "Could not parse '%s' argument\n", XXdescXX ); \
                return ARGP_ERR_UNKNOWN; \
            } \
        } \
        break;

static error_t
parse_opt( int key, char* arg, struct argp_state* state )
{
    // Get the input argument from `argp_parse', which we
    // know is a pointer to our arguments structure.
    struct arguments* arguments = ( struct arguments* ) state->input;
    char* tail;

    errno = 0;
    switch (key) {
    PARSE_ARG_LONG('v', arguments->verbose, "verbose");
    PARSE_ARG_LONG('i', arguments->maxindex, "maxindex");

    case ARGP_KEY_ARG:
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// Our argp parser.
static struct argp argp = { options, parse_opt, args_doc, doc };

// the global arguments struct
struct arguments arguments;
#define U(x) ((__u64)(x))

int main(int argc, char **argv)
{

    // Default values.
    arguments.verbose  = DEBUG_LEVEL_INFO;
    arguments.maxindex = 10;

    // Parse our arguments; every option seen by `parse_opt' will
    // be reflected in `arguments'.
    if ( argp_parse ( &argp, argc, argv, 0, 0, &arguments ) ) {
        debugError("Could not parse command line\n" );
        return -1;
    }

    setDebugLevel(arguments.verbose);
    
    printMessage("Devices\n");
    printMessage("=======\n");

    printMessage(" %10s  %4s %4s %5s\n",
                 "device", "CARD", "NODE", "HOST?");

    char is_card[arguments.maxindex];
    memset(is_card, 0, arguments.maxindex);

    int i;
    for(i=0; i<arguments.maxindex; i++) {
        char *device = NULL;
        if(asprintf(&device, "/dev/fw%d", i) < 0) {
            debugError("Failed create device string\n");
            exit(-1);
        }

        int fd = open( device, O_RDWR );
        if(fd < 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "Failed to open %s\n",
                        device);
            free(device);
            continue;
        }

        int status;
        
#define ROM_LENGTH (10*1024)
        char configrom[ROM_LENGTH];
        memset(configrom, 0, ROM_LENGTH);
        struct fw_cdev_event_bus_reset businfo;

        struct fw_cdev_get_info info;
        info.rom_length = ROM_LENGTH;
        info.rom = U( configrom );
        info.bus_reset = U( &businfo );

        status = ioctl(fd, FW_CDEV_IOC_GET_INFO, &info );
        if(status < 0) {
            debugError("Failed to execute info ioctl\n");
            exit(-1);
        }

        if(businfo.node_id == businfo.local_node_id) {
            is_card[i] = 1;
        }
        printMessage(" %10s: %4d %4d %5d\n",
                     device, info.card,
                     businfo.node_id & ~0xffc0,
                     is_card[i]);
        close(fd);
        free(device);
    }

    printMessage("\n");
    printMessage("Cards\n");
    printMessage("=====\n");

    printMessage(" %10s  %4s %4s %4s %4s %4s\n",
                 "device", "NODE", "BM", "IRM",
                 "ROOT", "GEN");

    for(i=0; i<arguments.maxindex; i++) {
        if(is_card[i] == 0) continue;

        char *device = NULL;
        if(asprintf(&device, "/dev/fw%d", i) < 0) {
            debugError("Failed create device string\n");
            exit(-1);
        }

        int fd = open( device, O_RDWR );
        if(fd < 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE,
                        "Failed to open %s\n",
                        device);
            free(device);
            continue;
        }

        int status;

        struct fw_cdev_event_bus_reset businfo;
        struct fw_cdev_get_info info;
        info.rom_length = 0;
        info.rom = 0;
        info.bus_reset = U( &businfo );

        status = ioctl(fd, FW_CDEV_IOC_GET_INFO, &info );
        if(status < 0) {
            debugError("Failed to execute info ioctl\n");
            exit(-1);
        }

        printMessage(" %10s: %4d %4d %4d %4d %4d\n", 
                     device, 
                     businfo.node_id      & ~0xffc0,
                     businfo.bm_node_id   & ~0xffc0,
                     businfo.irm_node_id  & ~0xffc0,
                     businfo.root_node_id & ~0xffc0,
                     businfo.generation
                    );

        close(fd);
        free(device);
    }

}