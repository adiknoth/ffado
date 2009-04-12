/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

/**
 * Implements the AV/C Descriptors/InfoBlocks for the Audio Subunit as in TA1999008
 *
 */

#ifndef AVCDESCRIPTORAUDIO_H
#define AVCDESCRIPTORAUDIO_H

#include "../descriptors/avc_descriptor.h"
#include "../avc_definitions.h"

#include "../general/avc_generic.h"
#include "../general/avc_plug.h"

#include "debugmodule/debugmodule.h"

#include <libavc1394/avc1394.h>

#include <vector>
#include <string>

class Ieee1394Service;

namespace AVC {


class Util::Cmd::IOSSerialize;
class Util::Cmd::IISDeserialize;

class AVCAudioClusterInformation
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCAudioClusterInformation( ) {};
    virtual ~AVCAudioClusterInformation() {};

protected:

private:
    uint16_t m_cluster_info_length;
    byte_t m_number_of_channels;
    byte_t m_ChConfigType;
    uint16_t m_Predefined_ChannelConfig;
    std::vector<uint16_t> m_channel_name_IDs;
};

class AVCAudioConfigurationDependentInformation
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCAudioConfigurationDependentInformation( ) {};
    virtual ~AVCAudioConfigurationDependentInformation() {};

protected:

private:
    uint16_t m_configuration_dependent_info_length;
    uint16_t m_configuration_ID;

    AVCAudioClusterInformation m_master_cluster_information;

    byte_t m_number_of_subunit_source_plug_link_information;
    std::vector<uint16_t> m_subunit_source_plug_link_informations;

    byte_t m_number_of_function_block_dependent_information;
};

class AVCAudioSubunitDependentInformation
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    AVCAudioSubunitDependentInformation( ) {};
    virtual ~AVCAudioSubunitDependentInformation() {};

protected:

private:
    uint16_t m_audio_subunit_dependent_info_fields_length;
    byte_t m_audio_subunit_version;
    byte_t m_number_of_configurations;
    std::vector<AVCAudioConfigurationDependentInformation> m_configurations;
};

/**
 * Audio Subunit Identifier Descriptor
 */
class AVCAudioIdentifierDescriptor : public AVCDescriptor
{

public:
    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    
    AVCAudioIdentifierDescriptor( Unit* unit, Subunit* subunit );
    virtual ~AVCAudioIdentifierDescriptor() {}
    
    virtual const char* getDescriptorName() const
        {return "AVCAudioIdentifierDescriptor";};

private:
    byte_t m_generation_ID;
    byte_t m_size_of_list_ID;
    byte_t m_size_of_object_ID;
    byte_t m_size_of_object_position;

    uint16_t m_number_of_root_object_lists;
    std::vector<byte_t> m_root_object_list_IDs;

    uint16_t m_audio_subunit_dependent_length;
    AVCAudioSubunitDependentInformation m_audio_subunit_dependent_info;
};

}

#endif
