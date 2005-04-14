/* stream_format.c
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

#include <libavc1394/avc1394.h>
#include <libraw1394/raw1394.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#define AVC1394_STREAM_FORMAT_SUPPORT            (0x2F<<8)
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_INPUT  0x00
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_OUTPUT 0x01

// BridgeCo extensions
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_SINGLE_REQUEST 0xC0
#define AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_LIST_REQUEST   0xC1


#define AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_DVCR       0x80
#define AVC1394_STREAM_FORMAT_HIERARCHY_ROOT_AUDIOMUSIC 0x90

#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SD525_60                     0x00
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SDL525_60                    0x04
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_HD1125_60                    0x08
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SD625_60                     0x80
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_SDL625_50                    0x84
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_DVCR_HD1250_50                    0x88
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_AM824                  0x00
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_24_4_AUDIO_PACK        0x01
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_1_AUDIOMUSIC_32_FLOATING_POINT_DATA 0x01

#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC60968_3                            0x00
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_3                            0x01
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_4                            0x02
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_5                            0x03
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_6                            0x04
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_IEC61937_7                            0x05
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MULTI_BIT_LINEAR_AUDIO_RAW            0x06
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MULTI_BIT_LINEAR_AUDIO_DVD_AUDIO      0x07
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_PLAIN_RAW               0x08
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_PLAIN_SACD              0x09
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_ENCODED_RAW             0x0A
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_ONE_BIT_AUDIO_ENCODED_SACD            0x0B
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_HIGH_PRECISION_MULTIBIT_LINEAR_AUDIO  0x0C
#define AVC1394_STREAM_FORMAT_HIERARCHY_LEVEL_2_AM824_MIDI_CONFORMANT                       0x0D

char*
decode_response(quadlet_t response)
{
    quadlet_t resp = AVC1394_MASK_RESPONSE(response);
    if (resp == AVC1394_RESPONSE_NOT_IMPLEMENTED)
        return "NOT IMPLEMENTED";
    if (resp == AVC1394_RESPONSE_ACCEPTED)
        return "ACCEPTED";
    if (resp == AVC1394_RESPONSE_REJECTED)
        return "REJECTED";
    if (resp == AVC1394_RESPONSE_IN_TRANSITION)
        return "IN TRANSITION";
    if (resp == AVC1394_RESPONSE_IMPLEMENTED)
        return "IMPLEMENTED / STABLE";
    if (resp == AVC1394_RESPONSE_CHANGED)
        return "CHANGED";
    if (resp == AVC1394_RESPONSE_INTERIM)
        return "INTERIM";
    return "huh?";
}


#define STREAM_FORMAT_REQUEST_SIZE 10

int
parse_stream_format(raw1394handle_t handle, int node_id, int plug_id)
{
    quadlet_t request[STREAM_FORMAT_REQUEST_SIZE];
    quadlet_t* response;

    for (int i = 0; i <  STREAM_FORMAT_REQUEST_SIZE; i++) {
	request[i] = 0xffffffff;
    }

    // XXX seems not be correct.
    request[0] = 
	  AVC1394_CTYPE_STATUS 
	| AVC1394_SUBUNIT_TYPE_UNIT 
	| AVC1394_SUBUNIT_ID_IGNORE 
	| AVC1394_STREAM_FORMAT_SUPPORT
	| AVC1394_STREAM_FORMAT_SUBFUNCTION_EXTENDED_STREAM_FORMAT_LIST_REQUEST;
    request[1] =
	0x00000000; 
    request[2] = 
	0xffff0000;

    puts("request:");
    for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; i++) {
	    printf("  %2d: 0x%08x\n", i, request[i]);
    }

    response = avc1394_transaction_block(handle, node_id, request, STREAM_FORMAT_REQUEST_SIZE, 1);

    if (response) {
	printf("%s\n",  decode_response(*response));
	puts("response:");
	for (int i = 0; i < STREAM_FORMAT_REQUEST_SIZE; i++) {
	    printf("  %2d: 0x%08x\n", i, response[i]);
	}
    } else {
	printf("no response\n");
    }

    return 0;
}

int 
main(int argc, char **argv)
{
    if (argc < 3) {
	printf("usage: %s <NODE_ID> <PLUG_ID>\n", argv[0]);
	return 0;
    }

    errno = 0;
    char* tail;
    int node_id = strtol(argv[1], &tail, 0);
    if (errno) {
	perror("argument parsing failed:");
	return -1;
    }
    int plug_id = strtol(argv[2], &tail, 0);
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

    return parse_stream_format(handle, node_id, plug_id);
}
