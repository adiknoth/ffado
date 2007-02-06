/* freebob_test_raw1394.c
 * Copyright (C) 2007 Pieter Palmers
 *
 * This file is part of FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

/*
 * Original file: testlibraw.c
 * from the libraw1394 source tree.
 */

/*
 * libraw1394 - library for raw access to the 1394 bus with the Linux subsystem.
 *
 * Copyright (C) 1999,2000 Andreas Bombe
 *
 * This library is licensed under the GNU Lesser General Public License (LGPL),
 * version 2.1 or later. See the file COPYING.LIB in the distribution for
 * details.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/poll.h>
#include <stdlib.h>

#include <libraw1394/raw1394.h>

quadlet_t buffer;

int main(int argc, char **argv)
{
        raw1394handle_t handle;
        int i, numcards;
        struct raw1394_portinfo pinf[16];

        tag_handler_t std_handler;
        int retval;
        
        struct pollfd pfd;
        unsigned char fcp_test[] = { 0x1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };
        quadlet_t rom[0x100];
        size_t rom_size;
        unsigned char rom_version;

        handle = raw1394_new_handle();

        if (!handle) {
                if (!errno) {
                        printf("kernel-userland interface incompatible.");
                        exit (1);
                } else {
                        perror("couldn't get handle");
                        
                        switch(errno) {
                        case EACCES: exit(2);
                        default: exit(3);
                        }
                }
        }

        printf("successfully got handle\n");
        printf("current generation number: %d\n", raw1394_get_generation(handle));

        numcards = raw1394_get_port_info(handle, pinf, 16);
        if (numcards < 0) {
                perror("couldn't get card info");
                exit(4);
        } else {
                printf("%d card(s) found\n", numcards);
        }

        if (!numcards) {
                exit(5);
        }

        for (i = 0; i < numcards; i++) {
                printf(" card %d:\n",i);
                printf("  nodes on bus: %2d, card name: %s\n", pinf[i].nodes,
                       pinf[i].name);
                       
                if (raw1394_set_port(handle, 0) < 0) {
                        perror("couldn't set port");
                        exit(6);
                }
        
                printf("   local ID is %d, IRM is %d\n",
                      raw1394_get_local_id(handle) & 0x3f,
                      raw1394_get_irm_id(handle) & 0x3f);
        }

        exit(0);
}
