/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "efc_cmds_hardware.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace FireWorks {

EfcHardwareInfoCmd::EfcHardwareInfoCmd()
: EfcCmd(EFC_CAT_HARDWARE_INFO, EFC_CMD_HW_HWINFO_GET_CAPS)
, m_nb_out_groups( 0 )
, m_nb_in_groups( 0 )
{}

bool
EfcHardwareInfoCmd::serialize( Util::IOSSerialize& se )
{
    bool result=true;
    
    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;
    
    result &= EfcCmd::serialize ( se );
    return result;
}

bool
EfcHardwareInfoCmd::deserialize( Util::IISDeserialize& de )
{
    bool result=true;
    uint32_t tmp;
    
    result &= EfcCmd::deserialize ( de );
    
    // the serialization is different from the deserialization
    EFC_DESERIALIZE_AND_SWAP(de, &m_flags, result);
    
    EFC_DESERIALIZE_AND_SWAP(de, &tmp, result);
    m_guid=tmp;
    m_guid <<= 32;
    
    EFC_DESERIALIZE_AND_SWAP(de, &tmp, result);
    m_guid |= tmp;
    
    EFC_DESERIALIZE_AND_SWAP(de, &m_type, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_version, result);

    int i=0;
    byte_t *vname=(byte_t *)m_vendor_name;
    for (i=0; i<HWINFO_NAME_SIZE_BYTES; i++) {
        result &= de.read(vname++);
    }
    byte_t *mname=(byte_t *)m_model_name;
    for (i=0; i<HWINFO_NAME_SIZE_BYTES; i++) {
        result &= de.read(mname++);
    }
    
    EFC_DESERIALIZE_AND_SWAP(de, &m_supported_clocks, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_1394_playback_channels, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_1394_record_channels, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_phys_audio_out, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_phys_audio_in, result);
    
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_out_groups, result);
    for (i=0; i<HWINFO_MAX_CAPS_GROUPS; i++) {
        result &= de.read(&(out_groups[i].type));
        result &= de.read(&(out_groups[i].count));
    }
    
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_in_groups, result);
    for (i=0; i<HWINFO_MAX_CAPS_GROUPS; i++) {
        result &= de.read(&(in_groups[i].type));
        result &= de.read(&(in_groups[i].count));
    }
    
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_midi_out, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_midi_in, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_max_sample_rate, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_min_sample_rate, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_dsp_version, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_arm_version, result);
    
    EFC_DESERIALIZE_AND_SWAP(de, &num_mix_play_chan, result);
    EFC_DESERIALIZE_AND_SWAP(de, &num_mix_rec_chan, result);
    
    if (m_header.version >= 1) {
        EFC_DESERIALIZE_AND_SWAP(de, &m_fpga_version, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_nb_1394_play_chan_2x, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_nb_1394_rec_chan_2x, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_nb_1394_play_chan_4x, result);
        EFC_DESERIALIZE_AND_SWAP(de, &m_nb_1394_rec_chan_4x, result);
    }
    
    return result;
}

void 
EfcHardwareInfoCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC HW CAPS info:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Flags   : 0x%08X\n", m_flags);
    debugOutput(DEBUG_LEVEL_NORMAL, " GUID    : %016llX\n", m_guid);
    debugOutput(DEBUG_LEVEL_NORMAL, " HwType  : 0x%08X\n", m_type);
    debugOutput(DEBUG_LEVEL_NORMAL, " Version : %lu\n", m_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " Vendor  : %s\n", m_vendor_name);
    debugOutput(DEBUG_LEVEL_NORMAL, " Model   : %s\n", m_model_name);
    
    debugOutput(DEBUG_LEVEL_NORMAL, " Supported Clocks   : 0x%08X\n", m_supported_clocks);
    debugOutput(DEBUG_LEVEL_NORMAL, " # 1394 Playback    : %d\n", m_nb_1394_playback_channels);
    debugOutput(DEBUG_LEVEL_NORMAL, " # 1394 Record      : %d\n", m_nb_1394_record_channels);
    debugOutput(DEBUG_LEVEL_NORMAL, " # Physical out     : %d\n", m_nb_phys_audio_out);
    debugOutput(DEBUG_LEVEL_NORMAL, " # Physical in      : %d\n", m_nb_phys_audio_in);
    
    unsigned int i=0;
    debugOutput(DEBUG_LEVEL_NORMAL, " # Output Groups    : %d\n", m_nb_out_groups);
    for (i=0;i<m_nb_out_groups;i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "     Group %d: Type 0x%02X, count %d\n",
                                        i, out_groups[i].type, out_groups[i].count);
    }
    debugOutput(DEBUG_LEVEL_NORMAL, " # Input Groups     : %d\n", m_nb_in_groups);
    for (i=0;i<m_nb_in_groups;i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "     Group %d: Type 0x%02X, count %d\n",
                                        i, in_groups[i].type, in_groups[i].count);
    }
    debugOutput(DEBUG_LEVEL_NORMAL, " # Midi out         : %d\n", m_nb_midi_out);
    debugOutput(DEBUG_LEVEL_NORMAL, " # Midi in          : %d\n", m_nb_midi_in);
    debugOutput(DEBUG_LEVEL_NORMAL, " Max Sample Rate    : %d\n", m_max_sample_rate);
    debugOutput(DEBUG_LEVEL_NORMAL, " Min Sample Rate    : %d\n", m_min_sample_rate);
    debugOutput(DEBUG_LEVEL_NORMAL, " DSP version        : 0x%08X\n", m_dsp_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " ARM version        : 0x%08X\n", m_arm_version);
    debugOutput(DEBUG_LEVEL_NORMAL, " # Mix play chann.  : %d\n", num_mix_play_chan);
    debugOutput(DEBUG_LEVEL_NORMAL, " # Mix rec chann.   : %d\n", num_mix_rec_chan);

    if (m_header.version >= 1) {
        debugOutput(DEBUG_LEVEL_NORMAL, " FPGA version         : 0x%08X\n", m_fpga_version);
        debugOutput(DEBUG_LEVEL_NORMAL, " # 1394 Playback (x2) : %d\n", m_nb_1394_play_chan_2x);
        debugOutput(DEBUG_LEVEL_NORMAL, " # 1394 Record   (x2) : %d\n", m_nb_1394_rec_chan_2x);
        debugOutput(DEBUG_LEVEL_NORMAL, " # 1394 Playback (x4) : %d\n", m_nb_1394_play_chan_4x);
        debugOutput(DEBUG_LEVEL_NORMAL, " # 1394 Record   (x4) : %d\n", m_nb_1394_rec_chan_4x);
    }
}

// --- polled info command
EfcPolledValuesCmd::EfcPolledValuesCmd()
: EfcCmd(EFC_CAT_HARDWARE_INFO, EFC_CMD_HW_GET_POLLED)
, m_nb_output_meters ( 0 )
, m_nb_input_meters ( 0 )
{}

bool
EfcPolledValuesCmd::serialize( Util::IOSSerialize& se )
{
    bool result=true;
    
    // the length should be specified before
    // the header is serialized
    m_length=EFC_HEADER_LENGTH_QUADLETS;
    
    result &= EfcCmd::serialize ( se );
    return result;
}

bool
EfcPolledValuesCmd::deserialize( Util::IISDeserialize& de )
{
    bool result=true;
    
    result &= EfcCmd::deserialize ( de );
    
    // the serialization is different from the deserialization
    EFC_DESERIALIZE_AND_SWAP(de, &m_status, result);
    
    EFC_DESERIALIZE_AND_SWAP(de, &m_detect_spdif, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_detect_adat, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_reserved3, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_reserved4, result);

    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_output_meters, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_nb_input_meters, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_reserved5, result);
    EFC_DESERIALIZE_AND_SWAP(de, &m_reserved6, result);

    int i=0;
    int nb_meters=m_nb_output_meters+m_nb_input_meters;
    
    assert(nb_meters<POLLED_MAX_NB_METERS);
    for (i=0; i<nb_meters; i++) {
        EFC_DESERIALIZE_AND_SWAP(de, (uint32_t *)&m_meters[i], result);
    }
    
    return result;
}

void
EfcPolledValuesCmd::showEfcCmd()
{
    EfcCmd::showEfcCmd();
    
    debugOutput(DEBUG_LEVEL_NORMAL, "EFC POLLED info:\n");
    debugOutput(DEBUG_LEVEL_NORMAL, " Status          : 0x%08X\n", m_status);
    debugOutput(DEBUG_LEVEL_NORMAL, " Detect SPDIF    : 0x%08X\n", m_detect_spdif);
    debugOutput(DEBUG_LEVEL_NORMAL, " Detect ADAT     : 0x%08X\n", m_detect_adat);
    
    unsigned int i=0;
    debugOutput(DEBUG_LEVEL_NORMAL, " # Output Meters : %d\n", m_nb_output_meters);
    for (i=0;i<m_nb_output_meters;i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "     Meter %d: %ld\n", i, m_meters[i]);
    }
    
    debugOutput(DEBUG_LEVEL_NORMAL, " # Input Meters  : %d\n", m_nb_input_meters);
    for (;i<m_nb_output_meters+m_nb_input_meters;i++) {
        debugOutput(DEBUG_LEVEL_NORMAL, "     Meter %d: %ld\n", i, m_meters[i]);
    }
}


} // namespace FireWorks
