/* detect_avc_unit.c
 * Copyright (C) 2004,05 by Daniel Wagner
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

#include <libavc1394/rom1394.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* XXX: add those to avc1394.h */
#define AVC1394_SUBUNIT_TYPE_AUDIO (1 <<19) 
#define AVC1394_SUBUNIT_TYPE_PRINTER (2 <<19) 
#define AVC1394_SUBUNIT_TYPE_CA (6 <<19) 
#define AVC1394_SUBUNIT_TYPE_PANEL (9 <<19) 
#define AVC1394_SUBUNIT_TYPE_BULLETIN_BOARD (0xA <<19) 
#define AVC1394_SUBUNIT_TYPE_CAMERA_STORAGE (0xB <<19) 

void
print_rom_directory (raw1394handle_t handle, 
		      rom1394_directory* rom_dir, 
		      int node_id)
{
  int length;
  octlet_t guid;

  rom1394_bus_options bus_options;

  printf ( "\nNode %d: \n", node_id);
  printf ( "-------------------------------------------------\n");
  length = rom1394_get_bus_info_block_length (handle, node_id);
  printf ("bus info block length = %d\n", length);
  printf ("bus id = 0x%08x\n", rom1394_get_bus_id(handle, node_id));
  rom1394_get_bus_options (handle, node_id, &bus_options);
  printf ("bus options:\n");
  printf ("    isochronous resource manager capable: %d\n", bus_options.irmc);
  printf ("    cycle master capable                : %d\n", bus_options.cmc);
  printf ("    isochronous capable                 : %d\n", bus_options.isc);
  printf ("    bus manager capable                 : %d\n", bus_options.bmc);
  printf ("    cycle master clock accuracy         : %d ppm\n", bus_options.cyc_clk_acc);
  printf ("    maximum asynchronous record size    : %d bytes\n", bus_options.max_rec);
  guid = rom1394_get_guid (handle, node_id);
  printf ("GUID: 0x%08x%08x\n", (quadlet_t) (guid>>32), 
	  (quadlet_t) (guid & 0xffffffff));
  rom1394_get_directory ( handle, node_id, rom_dir);
  printf ("directory:\n");
  printf ("    node capabilities    : 0x%08x\n", rom_dir->node_capabilities);
  printf ("    vendor id            : 0x%08x\n", rom_dir->vendor_id);
  printf ("    unit spec id         : 0x%08x\n", rom_dir->unit_spec_id);
  printf ("    unit software version: 0x%08x\n", rom_dir->unit_sw_version);
  printf ("    model id             : 0x%08x\n", rom_dir->model_id);
  printf ("    textual leaves       : %s\n", rom_dir->label);
 
  return;
}

/* used for debug output */
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

void
print_avc_unit_info (raw1394handle_t handle, int node_id)
{
  printf ("node %d AVC video monitor? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_VIDEO_MONITOR) ? 
	  "yes":"no");
  printf ("node %d AVC audio? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				       AVC1394_SUBUNIT_TYPE_AUDIO) ? 
	  "yes":"no");
  printf ("node %d AVC printer? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				       AVC1394_SUBUNIT_TYPE_PRINTER) ? 
	  "yes":"no");
  printf ("node %d AVC disk recorder? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_DISC_RECORDER) ? 
	  "yes":"no");
  printf ("node %d AVC video recorder? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_TAPE_RECORDER) ? 
	  "yes":"no");
  printf ("node %d AVC vcr? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_VCR) ? 
	  "yes":"no");
  printf ("node %d AVC tuner? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_TUNER) ? 
	  "yes":"no");
  printf ("node %d AVC CA? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				       AVC1394_SUBUNIT_TYPE_CA) ? 
	  "yes":"no");
  printf ("node %d AVC video camera? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_VIDEO_CAMERA) ? 
	  "yes":"no");
  printf ("node %d AVC panel? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				       AVC1394_SUBUNIT_TYPE_PANEL) ? 
	  "yes":"no");
  printf ("node %d AVC camera storage? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				       AVC1394_SUBUNIT_TYPE_CAMERA_STORAGE) ? 
	  "yes":"no");
  printf ("node %d AVC bulletin board? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				       AVC1394_SUBUNIT_TYPE_BULLETIN_BOARD) ? 
	  "yes":"no");
  printf ("node %d AVC vendor specificr? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_VENDOR_UNIQUE) ? 
	  "yes":"no");
  printf ("node %d AVC extended? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_EXTENDED) ? 
	  "yes":"no");
  printf ("node %d AVC unit? %s\n", node_id, 
	  avc1394_check_subunit_type (handle, node_id, 
				      AVC1394_SUBUNIT_TYPE_UNIT) ? 
	  "yes":"no");
  return;
}

void 
plug_discovery (raw1394handle_t handle, int node_id)
{
  quadlet_t request[2];
  quadlet_t* response;

  printf ("Plug discovery:\n");

#define REQUEST  (AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_UNIT | AVC1394_SUBUNIT_ID_IGNORE | AVC1394_COMMAND_PLUG_INFO)

  request[0] = REQUEST;
  request[1] = 0xffffffff;

  printf("Request quadlet: 0x%08x\n", request[0]);

  response = avc1394_transaction_block (handle, node_id, request, 2, 1);

  if ( response == NULL )
    {
      fprintf (stderr, "Could not read plug info.\n");
      return;
    }

  puts(decode_response(response[0]));

#undef AVC1394_GET_OPERAND
#define AVC1394_GET_OPERAND(x, n) (((x) & (0xFF000000 >> ((((n)-1)%4)*8))) >> (((4-(n))%4)*8))

  printf ("  Response quadlet: 0x%08x\n", response[1]);

  printf ("  Serial_Bus_input_plugs: %u\n", 
	  AVC1394_GET_OPERAND(response[1], 1));
  printf ("  Serial_Bus_output_plugs: %u\n",
	  AVC1394_GET_OPERAND(response[1], 2));
  printf ("  External_input_plugs: %u\n",
	  AVC1394_GET_OPERAND(response[1], 3));
  printf ("  External_output_plugs: %u\n", 
	  AVC1394_GET_OPERAND(response[1], 4));

  return;
}

int 
main(int argc, char **argv)
{
  rom1394_directory rom_dir;
  raw1394handle_t handle;

  handle = raw1394_new_handle ();
  if (handle == NULL) 
    {
      if (!errno) 
	{
	  fprintf (stderr, "lib1394raw not compatable.\n");
	} 
      else
	{
	  perror ("Could not get 1394 handle");
	  fprintf (stderr, "Is ieee1394, driver, and raw1394 loaded?\n");
	}
      exit(1);
    }

  if (raw1394_set_port(handle, 0) < 0) 
    {
      perror ("Could not set port");
      raw1394_destroy_handle (handle);
      exit(1);
    }

  for (int node_id = 0; node_id < raw1394_get_nodecount (handle); ++node_id)
    {
      if (rom1394_get_directory (handle, node_id, &rom_dir) < 0)
	{
	  fprintf (stderr, "Error reading config rom directory for node %d\n",
		   node_id);
 	  raw1394_destroy_handle (handle);
	  exit(1);
	}

      print_rom_directory (handle, &rom_dir, node_id);

      switch (rom1394_get_node_type (&rom_dir)) 
	{
	case ROM1394_NODE_TYPE_UNKNOWN:
	  printf ("Node %d has node type UNKNOWN\n", node_id);
	  break;
	case ROM1394_NODE_TYPE_DC:
	  printf ("Node %d has node type DC\n", node_id);
	  break;
	case ROM1394_NODE_TYPE_AVC:
	  printf ("Node %d has node type AVC\n", node_id);
	  print_avc_unit_info (handle, node_id);

	  if (avc1394_check_subunit_type (handle, node_id, 
					  AVC1394_SUBUNIT_TYPE_AUDIO)) 
	    {
	      /* That's an interesting one... */
 	      plug_discovery (handle, node_id);
	    } 
	  break;
	case ROM1394_NODE_TYPE_SBP2:
	  printf ("Node %d has node type SBP2\n", node_id);
	  break;
	case ROM1394_NODE_TYPE_CPU:
	  printf ("Node %d has node type CPU\n", node_id);
	  break;
	default:
	  printf("No matching node type found for node %d\n", node_id);
	}
    }

  raw1394_destroy_handle (handle);

  return 0;
}

/*
 * Local variables:
 *  compile-command: "gcc -Wall -g -std=c99 -o detect_avc_unit detect_avc_unit.c -I/usr/local/include -L/usr/local/lib -lavc1394 -lrom1394 -lraw1394"
 * End:
 */
