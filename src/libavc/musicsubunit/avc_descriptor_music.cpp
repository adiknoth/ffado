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
 
#include "avc_descriptor_music.h"

#include "../descriptors/avc_descriptor.h"
#include "../descriptors/avc_descriptor_cmd.h"

#include "../util/avc_serialize.h"
#include "libieee1394/ieee1394service.h"

#include "../general/avc_subunit.h"
#include "../general/avc_unit.h"

#include <netinet/in.h>

// info block implementations

namespace AVC {


AVCMusicGeneralStatusInfoBlock::AVCMusicGeneralStatusInfoBlock( )
    : AVCInfoBlock( 0x8100 )
    , m_current_transmit_capability ( 0 )
    , m_current_receive_capability  ( 0 )
    , m_current_latency_capability  ( 0xFFFFFFFF )
{}

bool
AVCMusicGeneralStatusInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    
    quadlet_t tmp=htonl(m_current_latency_capability);
    
    result &= se.write(m_current_transmit_capability, "AVCMusicGeneralStatusInfoBlock m_current_transmit_capability");
    result &= se.write(m_current_receive_capability, "AVCMusicGeneralStatusInfoBlock m_current_receive_capability");
    result &= se.write(tmp, "AVCMusicGeneralStatusInfoBlock m_current_latency_capability");

    return result;
}

bool
AVCMusicGeneralStatusInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);
    if (m_primary_field_length != 6) {
        debugWarning("Incorrect primary field length: %u, should be 6\n", m_primary_field_length);
        return false;
    }
    
    result &= de.read(&m_current_transmit_capability);
    result &= de.read(&m_current_receive_capability);
    result &= de.read(&m_current_latency_capability);
    m_current_latency_capability=ntohl(m_current_latency_capability);
    
    return result;
}

// ---------
AVCMusicOutputPlugStatusInfoBlock::AVCMusicOutputPlugStatusInfoBlock( )
    : AVCInfoBlock( 0x8101 )
{}

bool
AVCMusicOutputPlugStatusInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    debugWarning("%s not supported\n", getInfoBlockName());
    result=false;
    return result;
}

bool
AVCMusicOutputPlugStatusInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);
    debugWarning("%s not supported, skipping\n", getInfoBlockName());
    de.skip(m_compound_length-4);
    return result;
}
// ---------
AVCMusicClusterInfoBlock::AVCMusicClusterInfoBlock( )
    : AVCInfoBlock( 0x810A )
    , m_stream_format ( 0 )
    , m_port_type ( 0 )
    , m_nb_signals ( 0 )
{}

AVCMusicClusterInfoBlock::~AVCMusicClusterInfoBlock( )
{
    clear();
}

bool
AVCMusicClusterInfoBlock::clear( ) {
    m_stream_format=0;
    m_port_type=0;
    m_nb_signals=0;
    
    m_SignalInfos.clear();
    return true;
}

bool
AVCMusicClusterInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    
    result &= se.write(m_stream_format, "AVCMusicClusterInfoBlock m_stream_format");
    result &= se.write(m_port_type, "AVCMusicClusterInfoBlock m_port_type");
    result &= se.write(m_nb_signals, "AVCMusicClusterInfoBlock m_nb_signals");
    
    if (m_SignalInfos.size() != m_nb_signals) {
        debugError("not enough elements in AVCMusicClusterInfoBlock vector\n");
        return false;
    }
    
    unsigned int cnt;
    for (cnt=0;cnt<m_nb_signals;cnt++) {
//         debugOutput(DEBUG_LEVEL_VERBOSE, "Adding SignalInfo %2u\n",cnt);
        struct sSignalInfo s=m_SignalInfos.at(cnt);
        result &= se.write(s.music_plug_id, "AVCMusicClusterInfoBlock music_plug_id");
        result &= se.write(s.stream_position, "AVCMusicClusterInfoBlock stream_position");
        result &= se.write(s.stream_location, "AVCMusicClusterInfoBlock stream_location");
    }
    
    // do the optional text/name info block
    if(m_RawTextInfoBlock.m_compound_length>0) {
        result &= m_RawTextInfoBlock.serialize(se);
    } else if (m_NameInfoBlock.m_compound_length>0) {
        result &= m_NameInfoBlock.serialize(se);
    }
    
    return result;
}

bool
AVCMusicClusterInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);
    
    unsigned int consumed_at_start=de.getNrOfConsumedBytes();

    result &= de.read(&m_stream_format);
    result &= de.read(&m_port_type);
    result &= de.read(&m_nb_signals);
    
    unsigned int cnt;
    for (cnt=0;cnt<m_nb_signals;cnt++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding SignalInfo %2u\n",cnt);
        struct sSignalInfo s;
        result &= de.read(&s.music_plug_id);
        result &= de.read(&s.stream_position);
        result &= de.read(&s.stream_location);
        m_SignalInfos.push_back(s);
    }
    unsigned int consumed_at_sinfo_end=de.getNrOfConsumedBytes();
    
    // do the optional text info block
    // first find out if the block is present
    int bytes_done=4+consumed_at_sinfo_end-consumed_at_start;
    int bytes_left=m_compound_length-bytes_done;
    debugOutput(DEBUG_LEVEL_VERBOSE,"len=%d, @start=%d @end=%d done=%d, left=%d\n",
        m_compound_length, consumed_at_start, consumed_at_sinfo_end, bytes_done, bytes_left);
    if(bytes_left>0) {
        uint16_t block_type;
        AVCInfoBlock::peekBlockType(de, &block_type);
        if(block_type==m_RawTextInfoBlock.m_supported_info_block_type) {
            result &= m_RawTextInfoBlock.deserialize(de);
        } else if (block_type==m_NameInfoBlock.m_supported_info_block_type) {
            result &= m_NameInfoBlock.deserialize(de);
        } else {
            debugWarning("Unexpected info block, skipping...\n");
            de.skip(bytes_left);
        }
    }

    return result;
}

std::string 
AVCMusicClusterInfoBlock::getName() {
    if(m_RawTextInfoBlock.m_compound_length>0) {
        return m_RawTextInfoBlock.m_text;
    } else if (m_NameInfoBlock.m_compound_length>0) {
        return m_NameInfoBlock.m_text;
    } else {
        return std::string("Unknown");
    }
}


// ---------
AVCMusicSubunitPlugInfoBlock::AVCMusicSubunitPlugInfoBlock( )
    : AVCInfoBlock( 0x8109 )
    , m_subunit_plug_id ( 0 )
    , m_signal_format ( 0 )
    , m_plug_type ( 0xFF )
    , m_nb_clusters ( 0 )
    , m_nb_channels ( 0 )
{}

AVCMusicSubunitPlugInfoBlock::~AVCMusicSubunitPlugInfoBlock( )
{
    clear();
}

bool
AVCMusicSubunitPlugInfoBlock::clear()
{
    m_subunit_plug_id=0;
    m_signal_format=0;
    m_plug_type=0xFF;
    m_nb_clusters=0;
    m_nb_channels=0;
    
    // clean up dynamically allocated stuff
    for ( AVCMusicClusterInfoBlockVectorIterator it = m_Clusters.begin();
      it != m_Clusters.end();
      ++it )
    {
        delete *it;
    }
    m_Clusters.clear();
    
    return true;
}

bool
AVCMusicSubunitPlugInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    
    result &= se.write(m_subunit_plug_id, "AVCMusicPlugInfoBlock m_subunit_plug_id");
    result &= se.write(m_signal_format, "AVCMusicPlugInfoBlock m_signal_format");
    result &= se.write(m_plug_type, "AVCMusicPlugInfoBlock m_plug_type");
    result &= se.write(m_nb_clusters, "AVCMusicPlugInfoBlock m_nb_clusters");
    result &= se.write(m_nb_channels, "AVCMusicPlugInfoBlock m_nb_channels");
    
    unsigned int cnt;
    if (m_Clusters.size() != m_nb_clusters) {
        debugError("not enough elements in AVCMusicClusterInfoBlock vector\n");
        return false;
    }
    for (cnt=0;cnt<m_nb_clusters;cnt++) {
        AVCMusicClusterInfoBlock *p=m_Clusters.at(cnt);
        result &= p->serialize(se);
    }

    // do the optional text/name info block
    if(m_RawTextInfoBlock.m_compound_length>0) {
        result &= m_RawTextInfoBlock.serialize(se);
    } else if (m_NameInfoBlock.m_compound_length>0) {
        result &= m_NameInfoBlock.serialize(se);
    }
    return result;
}

bool
AVCMusicSubunitPlugInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);
    
    if (m_primary_field_length != 8) {
        debugWarning("Incorrect primary field length: %u, should be 4\n", m_primary_field_length);
        return false;
    }
    
    unsigned int consumed_at_start=de.getNrOfConsumedBytes();
    
    result &= de.read(&m_subunit_plug_id);
    result &= de.read(&m_signal_format);
    result &= de.read(&m_plug_type);
    result &= de.read(&m_nb_clusters);
    result &= de.read(&m_nb_channels);
    
    unsigned int cnt;
    for (cnt=0;cnt<m_nb_clusters;cnt++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding AVCMusicClusterInfoBlock %2u\n",cnt);
        AVCMusicClusterInfoBlock *p=new AVCMusicClusterInfoBlock();
        if (p==NULL) {
            debugError("Could not allocate memory for dest AVCMusicClusterInfoBlock\n");
        }
        m_Clusters.push_back(p);
        result &= p->deserialize(de);
    }
    unsigned int consumed_at_cluster_end=de.getNrOfConsumedBytes();
    
    // do the optional text info block
    // first find out if the block is present
    int bytes_done=4+consumed_at_cluster_end-consumed_at_start;
    int bytes_left=m_compound_length-bytes_done;
    debugOutput(DEBUG_LEVEL_VERBOSE,"len=%d, @start=%d @end=%d done=%d, left=%d\n",
        m_compound_length, consumed_at_start, consumed_at_cluster_end, bytes_done, bytes_left);
    if(bytes_left>0) {
        uint16_t block_type;
        AVCInfoBlock::peekBlockType(de, &block_type);
        if(block_type==m_RawTextInfoBlock.m_supported_info_block_type) {
            result &= m_RawTextInfoBlock.deserialize(de);
        } else if (block_type==m_NameInfoBlock.m_supported_info_block_type) {
            result &= m_NameInfoBlock.deserialize(de);
        } else {
            debugWarning("Unexpected info block, skipping...\n");
            de.skip(bytes_left);
        }
    }
    
    return result;
}

std::string 
AVCMusicSubunitPlugInfoBlock::getName() {
    if(m_RawTextInfoBlock.m_compound_length>0) {
        return m_RawTextInfoBlock.m_text;
    } else if (m_NameInfoBlock.m_compound_length>0) {
        return m_NameInfoBlock.m_text;
    } else {
        return std::string("Unknown");
    }
}


// ---------
AVCMusicPlugInfoBlock::AVCMusicPlugInfoBlock( )
    : AVCInfoBlock( 0x810B )
    , m_music_plug_type ( 0 )
    , m_music_plug_id ( 0 )
    , m_routing_support ( 0 )
    , m_source_plug_function_type ( 0 )
    , m_source_plug_id ( 0 )
    , m_source_plug_function_block_id ( 0 )
    , m_source_stream_position ( 0 )
    , m_source_stream_location ( 0 )
    , m_dest_plug_function_type ( 0 )
    , m_dest_plug_id ( 0 )
    , m_dest_plug_function_block_id ( 0 )
    , m_dest_stream_position ( 0 )
    , m_dest_stream_location ( 0 )
{}

bool
AVCMusicPlugInfoBlock::clear( ) {
    m_music_plug_type=0;
    m_music_plug_id=0;
    m_routing_support=0;
    m_source_plug_function_type=0;
    m_source_plug_id=0;
    m_source_plug_function_block_id=0;
    m_source_stream_position=0;
    m_source_stream_location=0;
    m_dest_plug_function_type=0;
    m_dest_plug_id=0;
    m_dest_plug_function_block_id=0;
    m_dest_stream_position=0;
    m_dest_stream_location=0;
    
    return true;
}

bool
AVCMusicPlugInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);

    result &= se.write(m_music_plug_type, "AVCMusicPlugInfoBlock m_music_plug_type" );
    result &= se.write(m_music_plug_id, "AVCMusicPlugInfoBlock m_music_plug_id" );
    result &= se.write(m_routing_support, "AVCMusicPlugInfoBlock m_routing_support" );
    result &= se.write(m_source_plug_function_type, "AVCMusicPlugInfoBlock m_source_plug_function_type" );
    result &= se.write(m_source_plug_id, "AVCMusicPlugInfoBlock m_source_plug_id" );
    result &= se.write(m_source_plug_function_block_id, "AVCMusicPlugInfoBlock m_source_plug_function_block_id" );
    result &= se.write(m_source_stream_position, "AVCMusicPlugInfoBlock m_source_stream_position" );
    result &= se.write(m_source_stream_location, "AVCMusicPlugInfoBlock m_source_stream_location" );
    result &= se.write(m_dest_plug_function_type, "AVCMusicPlugInfoBlock m_dest_plug_function_type" );
    result &= se.write(m_dest_plug_id, "AVCMusicPlugInfoBlock m_dest_plug_id" );
    result &= se.write(m_dest_plug_function_block_id, "AVCMusicPlugInfoBlock m_dest_plug_function_block_id" );
    result &= se.write(m_dest_stream_position, "AVCMusicPlugInfoBlock m_dest_stream_position" );
    result &= se.write(m_dest_stream_location, "AVCMusicPlugInfoBlock m_dest_stream_location" );
    
    // do the optional text/name info block
    if(m_RawTextInfoBlock.m_compound_length>0) {
        result &= m_RawTextInfoBlock.serialize(se);
    } else if (m_NameInfoBlock.m_compound_length>0) {
        result &= m_NameInfoBlock.serialize(se);
    }
    
    return result;
}

bool
AVCMusicPlugInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);
    
    if (m_primary_field_length != 14) {
        debugWarning("Incorrect primary field length: %u, should be 4\n", m_primary_field_length);
        return false;
    }

    result &= de.read(&m_music_plug_type);
    result &= de.read(&m_music_plug_id);
    result &= de.read(&m_routing_support);
    result &= de.read(&m_source_plug_function_type);
    result &= de.read(&m_source_plug_id);
    result &= de.read(&m_source_plug_function_block_id);
    result &= de.read(&m_source_stream_position);
    result &= de.read(&m_source_stream_location);
    result &= de.read(&m_dest_plug_function_type);
    result &= de.read(&m_dest_plug_id);
    result &= de.read(&m_dest_plug_function_block_id);
    result &= de.read(&m_dest_stream_position);
    result &= de.read(&m_dest_stream_location);
    
    if(m_compound_length>18) {
        uint16_t block_type;
        AVCInfoBlock::peekBlockType(de, &block_type);
        if(block_type==m_RawTextInfoBlock.m_supported_info_block_type) {
            result &= m_RawTextInfoBlock.deserialize(de);
        } else if (block_type==m_NameInfoBlock.m_supported_info_block_type) {
            result &= m_NameInfoBlock.deserialize(de);
        } else {
            debugWarning("Unexpected info block, skipping...\n");
            de.skip(m_compound_length-18);
        }
    }
    
    return result;
}

std::string 
AVCMusicPlugInfoBlock::getName() {
    if(m_RawTextInfoBlock.m_compound_length>0) {
        return m_RawTextInfoBlock.m_text;
    } else if (m_NameInfoBlock.m_compound_length>0) {
        return m_NameInfoBlock.m_text;
    } else {
        return std::string("Unknown");
    }
}


// ---------
AVCMusicRoutingStatusInfoBlock::AVCMusicRoutingStatusInfoBlock( )
    : AVCInfoBlock( 0x8108 )
    , m_nb_dest_plugs ( 0 )
    , m_nb_source_plugs ( 0 )
    , m_nb_music_plugs ( 0 )
{}

AVCMusicRoutingStatusInfoBlock::~AVCMusicRoutingStatusInfoBlock( ) {

    clear();
}

bool
AVCMusicRoutingStatusInfoBlock::clear()
{
    m_nb_dest_plugs=0;
    m_nb_source_plugs=0;
    m_nb_music_plugs=0;
    
    // clean up dynamically allocated stuff
    for ( AVCMusicSubunitPlugInfoBlockVectorIterator it = mDestPlugInfoBlocks.begin();
      it != mDestPlugInfoBlocks.end();
      ++it )
    {
        delete *it;
    }
    mDestPlugInfoBlocks.clear();
    
    for ( AVCMusicSubunitPlugInfoBlockVectorIterator it = mSourcePlugInfoBlocks.begin();
      it != mSourcePlugInfoBlocks.end();
      ++it )
    {
        delete *it;
    }
    mSourcePlugInfoBlocks.clear();
    
    for ( AVCMusicPlugInfoBlockVectorIterator it = mMusicPlugInfoBlocks.begin();
      it != mMusicPlugInfoBlocks.end();
      ++it )
    {
        delete *it;
    }
    mMusicPlugInfoBlocks.clear();
    
    return true;
}

bool
AVCMusicRoutingStatusInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    
    result &= se.write(m_nb_dest_plugs, "AVCMusicRoutingStatusInfoBlock m_nb_dest_plugs");
    result &= se.write(m_nb_source_plugs, "AVCMusicRoutingStatusInfoBlock m_nb_source_plugs");
    result &= se.write(m_nb_music_plugs, "AVCMusicRoutingStatusInfoBlock m_nb_music_plugs");
    
    unsigned int cnt;
    if (mDestPlugInfoBlocks.size() != m_nb_dest_plugs) {
        debugError("not enough elements in dest AVCMusicSubunitPlugInfoBlock vector\n");
        return false;
    }
    for (cnt=0;cnt<m_nb_dest_plugs;cnt++) {
        AVCMusicSubunitPlugInfoBlock *p=mDestPlugInfoBlocks.at(cnt);
        result &= p->serialize(se);
    }
    
    if (mSourcePlugInfoBlocks.size() != m_nb_source_plugs) {
        debugError("not enough elements in  src AVCMusicSubunitPlugInfoBlock\n");
        return false;
    }
    for (cnt=0;cnt<m_nb_source_plugs;cnt++) {
        AVCMusicSubunitPlugInfoBlock *p=mSourcePlugInfoBlocks.at(cnt);
        result &= p->serialize(se);
    }
    
    if (mMusicPlugInfoBlocks.size() != m_nb_music_plugs) {
        debugError("not enough elements in AVCMusicPlugInfoBlock\n");
        return false;
    }
    for (cnt=0;cnt<m_nb_music_plugs;cnt++) {
        AVCMusicPlugInfoBlock *p=mMusicPlugInfoBlocks.at(cnt);
        result &= p->serialize(se);
    }

    return result;
}

bool
AVCMusicRoutingStatusInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);
    
    if (m_primary_field_length != 4) {
        debugWarning("Incorrect primary field length: %u, should be 4\n", m_primary_field_length);
        return false;
    }
    
    result &= de.read(&m_nb_dest_plugs);
    result &= de.read(&m_nb_source_plugs);
    result &= de.read(&m_nb_music_plugs);
    
    unsigned int cnt;
    for (cnt=0;cnt<m_nb_dest_plugs;cnt++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding dest AVCMusicSubunitPlugInfoBlock %2u\n",cnt);
        AVCMusicSubunitPlugInfoBlock *p=new AVCMusicSubunitPlugInfoBlock();
        if (p==NULL) {
            debugError("Could not allocate memory for dest AVCMusicSubunitPlugInfoBlock\n");
        }
        mDestPlugInfoBlocks.push_back(p);
        result &= p->deserialize(de);
    }
    
    for (cnt=0;cnt<m_nb_source_plugs;cnt++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding source AVCMusicSubunitPlugInfoBlock %2u\n",cnt);
        AVCMusicSubunitPlugInfoBlock *p=new AVCMusicSubunitPlugInfoBlock();
        if (p==NULL) {
            debugError("Could not allocate memory for src AVCMusicSubunitPlugInfoBlock vector\n");
        }
        mSourcePlugInfoBlocks.push_back(p);
        result &= p->deserialize(de);
    }
    
    for (cnt=0;cnt<m_nb_music_plugs;cnt++) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Adding AVCMusicPlugInfoBlock %2u\n",cnt);
        AVCMusicPlugInfoBlock *p=new AVCMusicPlugInfoBlock();
        if (p==NULL) {
            debugError("Could not allocate memory for AVCMusicPlugInfoBlock vector\n");
        }
        mMusicPlugInfoBlocks.push_back(p);
        result &= p->deserialize(de);
    }
    return result;
}

AVCMusicSubunitPlugInfoBlock *
AVCMusicRoutingStatusInfoBlock::getSubunitPlugInfoBlock(Plug::EPlugDirection direction, plug_id_t id) {
    if (direction == Plug::eAPD_Input) {
        for ( AVCMusicSubunitPlugInfoBlockVectorIterator it = mDestPlugInfoBlocks.begin();
        it != mDestPlugInfoBlocks.end();
        ++it )
        {
            AVCMusicSubunitPlugInfoBlock *b=(*it);
            if (b->m_subunit_plug_id == id) return b;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "no plug info found.\n");
        return NULL;
    } else if (direction == Plug::eAPD_Output) {
        for ( AVCMusicSubunitPlugInfoBlockVectorIterator it = mSourcePlugInfoBlocks.begin();
        it != mSourcePlugInfoBlocks.end();
        ++it )
        {
            AVCMusicSubunitPlugInfoBlock *b=(*it);
            if (b->m_subunit_plug_id == id) return b;
        }
        debugOutput(DEBUG_LEVEL_VERBOSE, "no plug info found.\n");
        return NULL;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Invalid direction.\n");
        return NULL;
    }
}

AVCMusicPlugInfoBlock *
AVCMusicRoutingStatusInfoBlock::getMusicPlugInfoBlock(plug_id_t id) {
    for ( AVCMusicPlugInfoBlockVectorIterator it = mMusicPlugInfoBlocks.begin();
    it != mMusicPlugInfoBlocks.end();
    ++it )
    {
        AVCMusicPlugInfoBlock *b=(*it);
        if (b->m_music_plug_id == id) return b;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, "no music plug info found.\n");
    return NULL;
}

// ----------------------
AVCMusicStatusDescriptor::AVCMusicStatusDescriptor( Unit* unit, Subunit* subunit )
    : AVCDescriptor(unit, subunit, AVCDescriptorSpecifier::eSubunit0x80)
{}

bool
AVCMusicStatusDescriptor::serialize( IOSSerialize& se )
{
    bool result=true;
    
    result &= AVCDescriptor::serialize(se);
    
    result &= m_general_status_infoblock.serialize(se);
    
    if (m_output_plug_status_infoblock.m_compound_length>0) {
        result &= m_output_plug_status_infoblock.serialize(se);
    }
    
    if (m_routing_status_infoblock.m_compound_length>0) {
        result &= m_routing_status_infoblock.serialize(se);
    }

    return true;
}

bool
AVCMusicStatusDescriptor::deserialize( IISDeserialize& de )
{
    bool result=true;
    
    unsigned int blocks_done=0;
    const unsigned int max_blocks=10;
    
    result &= AVCDescriptor::deserialize(de);
    
    uint16_t block_type;
    uint16_t block_length;
    
    // process all infoblocks until done or until failure
    while(AVCInfoBlock::peekBlockType(de, &block_type) && result) {
        AVCInfoBlock::peekBlockLength(de, &block_length);
        
        debugOutput(DEBUG_LEVEL_VERBOSE, "type=0x%04X, length=%u\n",block_type, block_length);
        
        switch (block_type) {
            case 0x8100:
                m_general_status_infoblock.setVerbose(getVerboseLevel());
                result &= m_general_status_infoblock.deserialize(de);
                break;
            case 0x8101:
                m_output_plug_status_infoblock.setVerbose(getVerboseLevel());
                result &= m_output_plug_status_infoblock.deserialize(de);
                break;
            case 0x8108:
                m_routing_status_infoblock.setVerbose(getVerboseLevel());
                result &= m_routing_status_infoblock.deserialize(de);
                break;
            default:
                debugWarning("Unknown info block type: 0x%04X, length=%u, skipping...\n", block_type, block_length);
                de.skip(block_length);
                break;
        }

        if(blocks_done++>max_blocks) {
            debugError("Too much info blocks in descriptor, probably a runaway parser\n");
            break;
        }
    }
    
    return result;
}

AVCMusicSubunitPlugInfoBlock *
AVCMusicStatusDescriptor::getSubunitPlugInfoBlock(Plug::EPlugDirection direction, plug_id_t id) {
    return m_routing_status_infoblock.getSubunitPlugInfoBlock(direction, id);
}

AVCMusicPlugInfoBlock *
AVCMusicStatusDescriptor::getMusicPlugInfoBlock(plug_id_t id) {
    return m_routing_status_infoblock.getMusicPlugInfoBlock(id);
}

}
