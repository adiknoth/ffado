/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
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

#ifndef FIREWORKS_EFC_CMDS_HARDWARE_H
#define FIREWORKS_EFC_CMDS_HARDWARE_H

#include "efc_cmd.h"

namespace FireWorks {

#define HWINFO_NAME_SIZE_BYTES      32
#define HWINFO_MAX_CAPS_GROUPS      8
#define POLLED_MAX_NB_METERS        100

class EfcHardwareInfoCmd : public EfcCmd
{
    typedef struct 
    {
        uint8_t type;
        uint8_t count;
    } caps_phys_group;


public:
    EfcHardwareInfoCmd();
    virtual ~EfcHardwareInfoCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcHardwareInfoCmd"; }
    
    virtual void showEfcCmd();

    uint32_t            m_flags;
    
    uint64_t            m_guid;
    uint32_t            m_type;
    uint32_t            m_version;
    char                m_vendor_name[ HWINFO_NAME_SIZE_BYTES ];
    char                m_model_name[ HWINFO_NAME_SIZE_BYTES ];

    uint32_t            m_supported_clocks;

    uint32_t            m_nb_1394_playback_channels;
    uint32_t            m_nb_1394_record_channels;

    uint32_t            m_nb_phys_audio_out;
    uint32_t            m_nb_phys_audio_in;
    
    uint32_t            m_nb_out_groups;
    caps_phys_group     out_groups[ HWINFO_MAX_CAPS_GROUPS ];

    uint32_t            m_nb_in_groups;
    caps_phys_group     in_groups[ HWINFO_MAX_CAPS_GROUPS ];
    
    uint32_t            m_nb_midi_out;
    uint32_t            m_nb_midi_in;
    
    uint32_t            m_max_sample_rate;
    uint32_t            m_min_sample_rate;
    
    uint32_t            m_dsp_version;
    uint32_t            m_arm_version;

    uint32_t            num_mix_play_chan;
    uint32_t            num_mix_rec_chan;

    // Only present when efc_version == 1 (?or >= 1?)
    uint32_t            m_fpga_version;    // version # for FPGA

    uint32_t            m_nb_1394_play_chan_2x;
    uint32_t            m_nb_1394_rec_chan_2x;

    uint32_t            m_nb_1394_play_chan_4x;
    uint32_t            m_nb_1394_rec_chan_4x;

    uint32_t            m_reserved[16];

};


class EfcPolledValuesCmd : public EfcCmd
{

public:
    EfcPolledValuesCmd();
    virtual ~EfcPolledValuesCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "EfcPolledValuesCmd"; }
    
    virtual void showEfcCmd();


    uint32_t            m_status;
    
    uint32_t            m_detect_spdif;
    uint32_t            m_detect_adat;
    uint32_t            m_reserved3;
    uint32_t            m_reserved4;
    
    uint32_t            m_nb_output_meters;
    uint32_t            m_nb_input_meters;
    uint32_t            m_reserved5;
    uint32_t            m_reserved6;
    int32_t             m_meters[POLLED_MAX_NB_METERS];

};

} // namespace FireWorks

#endif // FIREWORKS_EFC_CMDS_HARDWARE_H
