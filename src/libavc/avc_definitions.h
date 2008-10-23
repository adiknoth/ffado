/*
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

#ifndef AVCDEFINITIONS_H
#define AVCDEFINITIONS_H

#include <cstring>
#include <cstdlib>
#include <ostream>
#include <libraw1394/raw1394.h>

namespace AVC {

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
typedef byte_t control_attribute_t;
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
typedef byte_t function_block_type_t;
typedef byte_t function_block_id_t;
typedef byte_t function_block_special_purpose_t;
typedef byte_t no_of_input_plugs_t;
typedef byte_t no_of_output_plugs_t;
typedef byte_t info_type_t;
typedef byte_t audio_channel_number_t;
typedef byte_t selector_length_t;
typedef byte_t control_selector_t;
typedef byte_t control_data_length_t;
typedef uint16_t control_data_ext_length_t;
typedef uint16_t mixer_level_t;
typedef byte_t mixer_programmable_state_t;
typedef byte_t input_fb_plug_number_t;
typedef byte_t input_audio_channel_number_t;
typedef byte_t output_audio_channel_number_t;
typedef byte_t status_selector_t;

typedef quadlet_t company_id_t;

enum ESubunitType {
    eST_Monitor       = 0x00,
    eST_Audio         = 0x01,
    eST_Printer       = 0x02,
    eST_Disc          = 0x03,
    eST_VCR           = 0x04,
    eST_Tuner         = 0x05,
    eST_CA            = 0x06,
    eST_Camera        = 0x07,
    eST_Panel         = 0x09,
    eST_BulltinBoard  = 0x0A,
    eST_CameraStorage = 0x0B,
    eST_Music         = 0x0C,
    eST_VendorUnique  = 0x1C,
    eST_Reserved      = 0x1D,
    eST_Extended      = 0x1E,
    eST_Unit          = 0x1F,
};

#define AVC1394_SUBUNIT_ID_RESERVED 0x06

enum ESubunitType byteToSubunitType(byte_t s);

/**
 * \brief the possible sampling frequencies
 */
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
    eSF_AnyLow   = 0x0B,
    eSF_AnyMid   = 0x0C,
    eSF_AnyHigh  = 0x0D,
    eSF_None     = 0x0E,
    eSF_DontCare = 0x0F,
};

/**
 * \brief Convert from ESamplingFrequency to an integer
 * @param freq
 * @return
 */
int convertESamplingFrequency(ESamplingFrequency freq);
/**
 * \brief Convert from integer to ESamplingFrequency
 * @param sampleRate
 * @return
 */
ESamplingFrequency parseSampleRate( int sampleRate );

std::ostream& operator<<( std::ostream& stream, ESamplingFrequency samplingFrequency );

/**
 * \brief Convert from a FDF SFC field value to an integer sample rate
 * @param fdf fdf sfc field value
 * @return sample rate
 */
unsigned int fdfSfcToSampleRate(byte_t fdf);

/**
 * \brief Convert from an integer sample rate to a78 FDF SFC field value
 * @param rate integer sample rate
 * @return fdf sfc field value
 */
byte_t sampleRateToFdfSfc(unsigned int rate);

}

#endif // AVCDEFINITIONS_H
