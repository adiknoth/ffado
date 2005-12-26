/* avc_definitions.h
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

#ifndef AVCDefinitions_h
#define AVCDefinitions_h

#include <libavc1394/avc1394.h>
#include <ostream>

typedef byte_t ctype_t;
typedef byte_t unit_t;
typedef byte_t unit_type_t;
typedef byte_t subunit_t;
typedef byte_t opcode_t;
typedef byte_t plug_type_t;
typedef byte_t plug_id_t;
typedef byte_t reserved_t;
typedef byte_t function_block_type_t;
typedef byte_t function_block_id_t;
typedef byte_t plug_direction_t;
typedef byte_t plug_address_mode_t;
typedef byte_t status_t;
typedef byte_t number_of_channels_t;
typedef byte_t stream_format_t;
typedef byte_t sampling_frequency_t;
typedef byte_t rate_control_t;
typedef byte_t number_of_stream_format_infos_t;
typedef byte_t nr_of_plugs_t;
typedef byte_t subunit_id_t;
typedef byte_t subfunction_t;
typedef byte_t opcode_t;
typedef byte_t page_t;
typedef byte_t extension_code_t;
typedef byte_t subunit_type_t;
typedef byte_t max_subunit_id_t;
typedef byte_t nr_of_channels_t;
typedef byte_t stream_position_t;
typedef byte_t stream_position_location_t;
typedef byte_t nr_of_clusters_t;
typedef byte_t string_length_t;
typedef byte_t cluster_index_t;
typedef byte_t port_type_t;
typedef byte_t number_of_output_plugs_t;

typedef quadlet_t company_id_t;

enum ESamplingFrequency {
    eSF_22050Hz = 0x00,
    eSF_24000Hz = 0x01,
    eSF_32000Hz = 0x02,
    eSF_44100Hz = 0x03,
    eSF_48000Hz = 0x04,
    eSF_88200Hz = 0x0A,
    eSF_96000Hz = 0x05,
    eSF_176400Hz = 0x06,
    eSF_192000Hz = 0x07,
    eSF_DontCare = 0x0F,
};

std::ostream& operator<<( std::ostream& stream, ESamplingFrequency freq );

#define AVC1394_SUBUNIT_AUDIO 1
#define AVC1394_SUBUNIT_PRINTER 2
#define AVC1394_SUBUNIT_CA 6
#define AVC1394_SUBUNIT_PANEL 9
#define AVC1394_SUBUNIT_BULLETIN_BOARD 0xA
#define AVC1394_SUBUNIT_CAMERA_STORAGE 0xB
#define AVC1394_SUBUNIT_MUSIC 0xC
#define AVC1394_SUBUNIT_RESERVED 0x1D

#define AVC1394_SUBUNIT_ID_RESERVED 0x06

#endif // AVCDefinitions_h
