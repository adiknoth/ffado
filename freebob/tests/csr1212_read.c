/* csr1212_read.c
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libraw1394/raw1394.h>
#include "../src/csr1212.h"

struct freebob_csr_info {
    raw1394handle_t handle;
    int node_id;
};

static int 
freebob_bus_read(struct csr1212_csr* csr, u_int64_t addr, u_int16_t length,
		 void* buffer, void* private)
{
    struct freebob_csr_info* csr_info = (struct freebob_csr_info*) private;
    printf("freebob_bus_read: node id = %d, addr = 0x%08x%08x, length = %d\n", 
	   csr_info->node_id, 
	   (unsigned int)(addr >> 32), (unsigned int)(addr &0xffffffff), 
	   length);

    unsigned int retval;
    retval=raw1394_read(csr_info->handle, csr_info->node_id, addr,
			length, buffer); 
    if (retval) {
	perror("read failed");
    } else {
	#if 0
	    fprintf(stdout,"read succeeded. Data follows (hex):\n");
	    int i;
	    for (i=0; i < length; i++) {
		fprintf(stdout,(i+1) % 4?"%.2X ":"%.2X\n", ((unsigned char*)buffer)[i]);
	    }
	    if (i % 4) fprintf(stdout,"\n");
	#endif
    }

    return retval;
}

static int
freebob_get_max_rom(u_int32_t* bus_info_data, void* private)
{
    return (CSR1212_BE32_TO_CPU(bus_info_data[2]) >> 8) & 0x3;
}

static struct csr1212_bus_ops freebob_csr_ops = {
    .bus_read = freebob_bus_read,
    .get_max_rom = freebob_get_max_rom
};

int
main(int argc, char* argv[])
{
    if (argc != 2) {
	printf("usage: %s NODE_ID\n", argv[0]);
	return 0;
    }
    errno = 0;
    char* tail;
    int node_id = strtol(argv[1], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }

    raw1394handle_t handle;
    handle = raw1394_new_handle ();
    if (!handle) {
	if (!errno) {
	    perror("lib1394raw not compatable\n");
	} else {
	    fprintf(stderr, "Could not get 1394 handle");
	    fprintf(stderr, "Is ieee1394, driver, and raw1394 loaded?\n");
	}
	return -1;
    }
    
    if (raw1394_set_port(handle, 0) < 0) {
	fprintf(stderr, "Could not set port");
	raw1394_destroy_handle (handle);
	return -1;
    }

    struct freebob_csr_info csr_info;
    csr_info.handle = handle;
    csr_info.node_id = 0xffc0 | node_id;
    
    struct csr1212_csr* csr;
    csr = csr1212_create_csr(&freebob_csr_ops, 
			     5 * sizeof(quadlet_t), 
			     &csr_info);
    if (!csr || csr1212_parse_csr(csr) != CSR1212_SUCCESS) {
	fprintf(stderr, "couldn't parse config rom\n");
	if (csr) {
	    csr1212_destroy_csr(csr);
	}
	return -1;
    }

    // still no fancy stuff here...
        
    csr1212_destroy_csr(csr);

    return 0;
}
