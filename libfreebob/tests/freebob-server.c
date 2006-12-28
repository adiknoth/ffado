/*
* avc_vcr.c - An example of an AV/C Tape Recorder target implementation
*
* Copyright Dan Dennedy <dan@dennedy.org>
* 
* Inspired by virtual_vcr from Bonin Franck <boninf@free.fr>
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/poll.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/rom1394.h>

#include "../libfreebob/freebob.h"
#include "../libfreebob/freebob_bounce.h"

const char not_compatible[] = "\n"
	"This libraw1394 does not work with your version of Linux. You need a different\n"
	"version that matches your kernel (see kernel help text for the raw1394 option to\n"
	"find out which is the correct version).\n";

const char not_loaded[] = "\n"
	"This probably means that you don't have raw1394 support in the kernel or that\n"
	"you haven't loaded the raw1394 module.\n";


static int g_done = 0;

static void sighandler (int sig)
{
	printf("signal!\n");
	g_done = 1;
}

/**** subunit handlers ****/
int subunit_control( avc1394_cmd_rsp *cr )
{
	switch ( cr->opcode )
	{
	default:
		fprintf( stderr, "subunit control command 0x%02x non supported\n", cr->opcode );
		return 0;
	}
	return 1;
}

int subunit_status( avc1394_cmd_rsp *cr )
{

	fprintf( stderr, "subunit STATUS\n");
	char *buffer;
	switch ( cr->opcode )
	{
	case (AVC1394_CMD_INPUT_PLUG_SIGNAL_FORMAT):

		cr->status = AVC1394_RESP_STABLE;
		buffer=(char*)&cr->operand[1];
		fprintf( stderr, "subunit AVC1394_COMMAND_INPUT_PLUG_SIGNAL_FORMAT\n");
		
		strncpy(buffer,"TEST123",sizeof(byte_t)*9);
		break;
	default:
		fprintf( stderr, "subunit status command 0x%02x not supported\n", cr->opcode );
		return 0;
	}
	return 1;
}


int subunit_inquiry( avc1394_cmd_rsp *cr )
{
	switch ( cr->opcode )
	{
	default:
		fprintf( stderr, "subunit inquiry command 0x%02x not supported\n", cr->opcode );
		return 0;
	}
	return 1;
}


/**** Unit handlers ****/
int unit_control( avc1394_cmd_rsp *cr )
{
	switch ( cr->opcode )
	{
	default:
		fprintf( stderr, "unit control command 0x%02x not supported\n", cr->opcode );
		return 0;
	}
	return 1;
}


int unit_status( avc1394_cmd_rsp *cr )
{
	cr->operand[1] = 0x00;
	cr->operand[2] = 0x00;
	cr->operand[3] = 0x00;
	cr->operand[4] = 0x00;
	switch ( cr->opcode )
	{
	case AVC1394_CMD_UNIT_INFO:
		cr->status = AVC1394_RESP_STABLE;
		cr->operand[0] = AVC1394_OPERAND_UNIT_INFO_EXTENSION_CODE;
		cr->operand[1] = AVC1394_SUBUNIT_TYPE_FREEBOB_BOUNCE_SERVER >> 19;
		break;
	case AVC1394_CMD_SUBUNIT_INFO:
	{
		int page = ( cr->operand[0] >> 4 ) & 7;
		if ( page == 0 )
		{
			cr->status = AVC1394_RESP_STABLE;
			cr->operand[0] = (page << 4) | AVC1394_OPERAND_UNIT_INFO_EXTENSION_CODE;
			cr->operand[1] = AVC1394_SUBUNIT_TYPE_FREEBOB_BOUNCE_SERVER >> 19 << 3;
		}

		else
		{
			fprintf( stderr, "invalid page %d for subunit\n", page );
			return 0;
		}
		break;
	}
	default:
		fprintf( stderr, "unit status command 0x%02x not supported\n", cr->opcode );
		return 0;
	}
	return 1;
}


int unit_inquiry( avc1394_cmd_rsp *cr )
{
	switch ( cr->opcode )
	{
	case AVC1394_CMD_SUBUNIT_INFO:
	case AVC1394_CMD_UNIT_INFO:
		cr->status = AVC1394_RESP_IMPLEMENTED;
	default:
		fprintf( stderr, "unit inquiry command 0x%02x not supported\n", cr->opcode );
		return 0;
	}
	return 1;
}


/**** primary avc1394 target callback ****/
int command_handler( avc1394_cmd_rsp *cr )
{
	switch ( cr->subunit_type )
	{
	case AVC1394_SUBUNIT_TYPE_FREEBOB_BOUNCE_SERVER:
		if ( cr->subunit_id != 0 )
		{
			fprintf( stderr, "subunit id 0x%02x not supported\n", cr->subunit_id );
			return 0;
		}
		switch ( cr->status )
		{
		case AVC1394_CTYP_CONTROL:
			return subunit_control( cr );
			break;
		case AVC1394_CTYP_STATUS:
			return subunit_status( cr );
			break;
		case AVC1394_CTYP_GENERAL_INQUIRY:
			return subunit_inquiry( cr );
			break;
		}
		break;
	case AVC1394_SUBUNIT_UNIT:
		switch ( cr->status )
		{
		case AVC1394_CTYP_CONTROL:
			return unit_control( cr );
			break;
		case AVC1394_CTYP_STATUS:
			return unit_status( cr );
			break;
		case AVC1394_CTYP_GENERAL_INQUIRY:
			return unit_inquiry( cr );
			break;
		}
		break;
	default:
		fprintf( stderr, "subunit type 0x%02x not supported\n", cr->subunit_type );
		return 0;
	}
	return 1;
}

struct configrom_backup {
	quadlet_t rom[0x100];
	size_t rom_size;
	unsigned char rom_version;
	
};

struct configrom_backup save_config_rom(raw1394handle_t handle)
{
	int retval;
	struct configrom_backup tmp;
	/* get the current rom image */
	retval=raw1394_get_config_rom(handle, tmp.rom, 0x100, &tmp.rom_size, &tmp.rom_version);
// 	tmp.rom_size=rom1394_get_size(tmp.rom);
	printf("save_config_rom get_config_rom returned %d, romsize %d, rom_version %d:\n",retval,tmp.rom_size,tmp.rom_version);

	return tmp;
}

int restore_config_rom(raw1394handle_t handle, struct configrom_backup old)
{
	int retval,i ;

	quadlet_t current_rom[0x100];
	size_t current_rom_size;
	unsigned char current_rom_version;

	retval=raw1394_get_config_rom(handle, current_rom, 0x100, &current_rom_size, &current_rom_version);
	printf("restore_config_rom get_config_rom returned %d, romsize %d, rom_version %d:\n",retval,current_rom_size,current_rom_version);

	printf("restore_config_rom restoring to romsize %d, rom_version %d:\n",old.rom_size,old.rom_version);

	retval = raw1394_update_config_rom(handle, old.rom, old.rom_size, current_rom_version);
	printf("restore_config_rom update_config_rom returned %d\n",retval);

	/* get the current rom image */
	retval=raw1394_get_config_rom(handle, current_rom, 0x100, &current_rom_size, &current_rom_version);
	current_rom_size = rom1394_get_size(current_rom);
	printf("get_config_rom returned %d, romsize %d, rom_version %d:",retval,current_rom_size,current_rom_version);
	for (i = 0; i < current_rom_size; i++)
	{
		if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
		printf(" %08x", ntohl(current_rom[i]));
	}
	printf("\n");

	return retval;
}

int init_config_rom(raw1394handle_t handle)
{
	int retval, i;
	quadlet_t rom[0x100];
	size_t rom_size;
	unsigned char rom_version;
	rom1394_directory dir;
	char *leaf;
	
	/* get the current rom image */
	retval=raw1394_get_config_rom(handle, rom, 0x100, &rom_size, &rom_version);
	rom_size = rom1394_get_size(rom);
	printf("get_config_rom returned %d, romsize %d, rom_version %d:",retval,rom_size,rom_version);
	for (i = 0; i < rom_size; i++)
	{
		if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
		printf(" %08x", ntohl(rom[i]));
	}
	printf("\n");
	
	/* get the local directory */
	rom1394_get_directory( handle, raw1394_get_local_id(handle) & 0x3f, &dir);
	
	/* change the vendor description for kicks */
	i = strlen(dir.textual_leafs[0]);
	strncpy(dir.textual_leafs[0], FREEBOB_BOUNCE_SERVER_VENDORNAME "                                          ", i);
	retval = rom1394_set_directory(rom, &dir);
	printf("rom1394_set_directory returned %d, romsize %d:",retval,rom_size);
	for (i = 0; i < rom_size; i++)
	{
		if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
		printf(" %08x", ntohl(rom[i]));
	}
	printf("\n");
	
	/* free the allocated mem for the textual leaves */
	rom1394_free_directory( &dir);
	
	/* add an AV/C unit directory */
	dir.unit_spec_id    = 0x0000a02d;
	dir.unit_sw_version = 0x00010001;
	leaf = FREEBOB_BOUNCE_SERVER_MODELNAME;
	dir.nr_textual_leafs = 1;
	dir.textual_leafs = &leaf;
	
	/* manipulate the rom */
	retval = rom1394_add_unit( rom, &dir);
	
	/* get the computed size of the rom image */
	rom_size = rom1394_get_size(rom);
	
	printf("rom1394_add_unit_directory returned %d, romsize %d:",retval,rom_size);
	for (i = 0; i < rom_size; i++)
	{
		if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
		printf(" %08x", ntohl(rom[i]));
	}
	printf("\n");
	
	/* convert computed rom size from quadlets to bytes before update */
	rom_size *= sizeof(quadlet_t);
	retval = raw1394_update_config_rom(handle, rom, rom_size, rom_version);
	printf("update_config_rom returned %d\n",retval);
	
	retval=raw1394_get_config_rom(handle, rom, 0x100, &rom_size, &rom_version);
	printf("get_config_rom returned %d, romsize %d, rom_version %d:",retval,rom_size,rom_version);
	for (i = 0; i < rom_size; i++)
	{
		if (i % 4 == 0) printf("\n0x%04x:", CSR_CONFIG_ROM+i*4);
		printf(" %08x", ntohl(rom[i]));
	}
	printf("\n");
	
// 	printf("You need to reload your ieee1394 modules to reset the rom.\n");
	
	return 0;
}


int main( int argc, char **argv )
{
	raw1394handle_t handle;

	signal (SIGINT, sighandler);
	signal (SIGPIPE, sighandler);

	handle = raw1394_new_handle();

	if ( !handle )
	{
		if ( !errno )
		{
			printf( not_compatible );
		}
		else
		{
			perror( "couldn't get handle" );
			printf( not_loaded );
		}
		exit( EXIT_FAILURE );
	}

	if ( raw1394_set_port( handle, 0 ) < 0 )
	{
		perror( "couldn't set port" );
		exit( EXIT_FAILURE );
	}

	struct configrom_backup cfgrom_backup;
 	cfgrom_backup=save_config_rom( handle );

	if ( init_config_rom( handle ) < 0 )
	{
		perror( "couldn't init config rom!" );
		exit( EXIT_FAILURE );
	}


	avc1394_init_target( handle, command_handler );
	
	printf( "Starting AV/C target; press Ctrl+C to quit...\n" );
	struct pollfd pfd = {
		fd: raw1394_get_fd (handle),
		events: POLLIN | POLLPRI,
		revents: 0
	};
	int result = 0;
	

	do {
		if (poll (&pfd, 1, 100) > 0 && (pfd.revents & POLLIN))
		result = raw1394_loop_iterate (handle);
		
	} while (g_done == 0 && result == 0);
	
	avc1394_close_target( handle );
	
 	restore_config_rom( handle, cfgrom_backup);

	printf( "Bye...\n" );
	exit( EXIT_SUCCESS );
}
