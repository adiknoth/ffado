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
 
#include "avc_descriptor_audio.h"

#include "../descriptors/avc_descriptor.h"
#include "../descriptors/avc_descriptor_cmd.h"

#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include "../general/avc_subunit.h"
#include "../general/avc_unit.h"

#include "libutil/ByteSwap.h"

namespace AVC {

// ----------------------
bool
AVCAudioClusterInformation::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= se.write(m_cluster_info_length, "AVCAudioClusterInformation m_cluster_info_length");
    result &= se.write(m_number_of_channels, "AVCAudioClusterInformation m_number_of_channels");
    result &= se.write(m_ChConfigType, "AVCAudioClusterInformation m_ChConfigType");
    result &= se.write(m_Predefined_ChannelConfig, "AVCAudioClusterInformation m_Predefined_ChannelConfig");

    if(m_cluster_info_length > 4) {
        for(int i=0; i<m_number_of_channels; i++) {
            uint16_t tmp = m_channel_name_IDs.at(i);
            result &= se.write(tmp, "AVCAudioClusterInformation m_channel_name_IDs");
        }
    }

    return result;
}

bool
AVCAudioClusterInformation::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= de.read(&m_cluster_info_length);
    result &= de.read(&m_number_of_channels);
    result &= de.read(&m_ChConfigType);
    result &= de.read(&m_Predefined_ChannelConfig);

    // only if there is still data in the cluster info
    if(m_cluster_info_length > 4) {
        m_channel_name_IDs.clear();
        for(int i=0; i<m_number_of_channels; i++) {
            uint16_t tmp;
            result &= de.read(&tmp);
            m_channel_name_IDs.push_back(tmp);
        }
    }

    return result;
}

// ----------------------
bool
AVCAudioConfigurationDependentInformation::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= se.write(m_configuration_dependent_info_length, "AVCAudioConfigurationDependentInformation m_configuration_dependent_info_length");
    result &= se.write(m_configuration_ID, "AVCAudioConfigurationDependentInformation m_configuration_ID");

    result &= m_master_cluster_information.serialize(se);

    result &= se.write(m_number_of_subunit_source_plug_link_information, "AVCAudioConfigurationDependentInformation m_number_of_subunit_source_plug_link_information");
    for(int i=0; i<m_number_of_subunit_source_plug_link_information; i++) {
        uint16_t tmp = m_subunit_source_plug_link_informations.at(i);
        result &= se.write(tmp, "AVCAudioConfigurationDependentInformation m_subunit_source_plug_link_informations");
    }

    result &= se.write(m_number_of_function_block_dependent_information, "AVCAudioConfigurationDependentInformation m_number_of_function_block_dependent_information");
    return result;
}

bool
AVCAudioConfigurationDependentInformation::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= de.read(&m_configuration_dependent_info_length);
    result &= de.read(&m_configuration_ID);

    result &= m_master_cluster_information.deserialize(de);

    result &= de.read(&m_number_of_subunit_source_plug_link_information);
    m_subunit_source_plug_link_informations.clear();
    for(int i=0; i<m_number_of_subunit_source_plug_link_information; i++) {
        uint16_t tmp;
        result &= de.read(&tmp);
        m_subunit_source_plug_link_informations.push_back(tmp);
    }

    result &= de.read(&m_number_of_function_block_dependent_information);

    if(m_number_of_function_block_dependent_information) {
        // FIXME: function block dependent information not parsed!
        result = false;
    }
    return result;
}

// ----------------------
bool
AVCAudioSubunitDependentInformation::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= se.write(m_audio_subunit_dependent_info_fields_length, "AVCAudioSubunitDependentInformation m_audio_subunit_dependent_info_fields_length");
    result &= se.write(m_audio_subunit_version, "AVCAudioSubunitDependentInformation m_audio_subunit_version");
    result &= se.write(m_number_of_configurations, "AVCAudioSubunitDependentInformation m_number_of_configurations");

    for(int i=0; i<m_number_of_configurations; i++) {
        AVCAudioConfigurationDependentInformation tmp = m_configurations.at(i);
        result &= tmp.serialize(se);
    }

    return result;
}

bool
AVCAudioSubunitDependentInformation::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= de.read(&m_audio_subunit_dependent_info_fields_length);
    result &= de.read(&m_audio_subunit_version);
    result &= de.read(&m_number_of_configurations);

    m_configurations.clear();
    for(int i=0; i<m_number_of_configurations; i++) {
        AVCAudioConfigurationDependentInformation tmp;
        result &= tmp.deserialize(de);
        m_configurations.push_back(tmp);
    }

    return result;
}

// ----------------------
AVCAudioIdentifierDescriptor::AVCAudioIdentifierDescriptor( Unit* unit, Subunit* subunit )
    : AVCDescriptor(unit, subunit, AVCDescriptorSpecifier::eIndentifier)
{}

bool
AVCAudioIdentifierDescriptor::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    
    result &= AVCDescriptor::serialize(se);

    result &= se.write(m_generation_ID, "AVCAudioIdentifierDescriptor m_generation_ID");
    result &= se.write(m_size_of_list_ID, "AVCAudioIdentifierDescriptor m_size_of_list_ID");
    result &= se.write(m_size_of_object_ID, "AVCAudioIdentifierDescriptor m_size_of_object_ID");
    result &= se.write(m_size_of_object_position, "AVCAudioIdentifierDescriptor m_size_of_object_position");

    result &= se.write(m_number_of_root_object_lists, "AVCAudioIdentifierDescriptor m_number_of_root_object_lists");

    std::vector<byte_t> tmp_vect = m_root_object_list_IDs;
    for(int i=0; i<m_number_of_root_object_lists; i++) {
        for(int j=0; i<m_size_of_list_ID; j++) {
            byte_t dummy = tmp_vect.at(0);
            result &= se.write(dummy);
            tmp_vect.erase(tmp_vect.begin());
        }
    }

    result &= se.write(m_audio_subunit_dependent_length, "AVCAudioIdentifierDescriptor m_audio_subunit_dependent_length");
    if(m_audio_subunit_dependent_length) {
        result &= m_audio_subunit_dependent_info.serialize(se);
    }

    return result;
}

bool
AVCAudioIdentifierDescriptor::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;

    result &= AVCDescriptor::deserialize(de);

    result &= de.read(&m_generation_ID);
    result &= de.read(&m_size_of_list_ID);
    result &= de.read(&m_size_of_object_ID);
    result &= de.read(&m_size_of_object_position);

    result &= de.read(&m_number_of_root_object_lists);

    // we don't support root object lists
    m_root_object_list_IDs.clear();
    for(int i=0; i<m_number_of_root_object_lists; i++) {
        for(int j=0; i<m_size_of_list_ID; j++) {
            byte_t dummy;
            result &= de.read(&dummy);
            m_root_object_list_IDs.push_back(dummy);
        }
    }

    result &= de.read(&m_audio_subunit_dependent_length);
    if(m_audio_subunit_dependent_length) {
        result &= m_audio_subunit_dependent_info.deserialize(de);
    }

    return result;
}

}
