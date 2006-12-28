/* byte_order.h
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
#include <netinet/in.h>
#include <stdio.h>

typedef struct {
    unsigned char m_cts          : 4;
    unsigned char m_ctype        : 4;
    unsigned char m_subunit_type : 5;
    unsigned char m_subunit_ID   : 3;
} FCP_Header;

typedef struct {
    FCP_Header   m_header;
    unsigned char m_command;
    unsigned char m_subfunction;
    unsigned char m_destination_plugs;
    unsigned char m_source_plugs;
    unsigned char m_dummy[2];
} FCP_Command_PlugInfo;

typedef union {
    FCP_Command_PlugInfo m_command;
    unsigned int m_sel[2];
} UFCP_Command_PlugInfo;
    
int
main( int argc, char** argv )
{
    quadlet_t request;
    request = AVC1394_CTYPE_STATUS
	      | AVC1394_SUBUNIT_TYPE_UNIT
	      | AVC1394_SUBUNIT_ID_IGNORE
	      | AVC1394_COMMAND_PLUG_INFO
	      | 0x00;

    printf( "request:\t0x%08x\n", request );

    UFCP_Command_PlugInfo fcpCPlugInfo;
    fcpCPlugInfo.m_sel[0] = 0;
    fcpCPlugInfo.m_sel[1] = 0;

    fcpCPlugInfo.m_command.m_header.m_cts          = 0x00;
    fcpCPlugInfo.m_command.m_header.m_ctype        = 0x01;
    fcpCPlugInfo.m_command.m_header.m_subunit_type = 0x1f;    
    fcpCPlugInfo.m_command.m_header.m_subunit_ID   = 0x07;

    fcpCPlugInfo.m_command.m_command = 0x2;

    printf( "FCP PlugInfo (be):\t0x%08x\n",fcpCPlugInfo.m_sel[0] );
    printf( "FCP PlugInfo (le):\t0x%08x\n", ntohl(fcpCPlugInfo.m_sel[0]) );

    return 0;
}

/*
 * Local variables:
 *  compile-command: "gcc -Wall -g -std=c99 -o byte_order byte_order.c -I/usr/local/includ -L/usr/local/lib -lavc1394 -lrom1394 -lraw1394"
 * End:
 */
