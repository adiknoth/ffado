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

#include "avc_descriptor.h"
#include "avc_descriptor_cmd.h"

#include "../util/avc_serialize.h"
#include "libieee1394/ieee1394service.h"

AVCDescriptorSpecifier::AVCDescriptorSpecifier( enum EType type )
    : m_type ( type )
    , m_listid_size ( 0 )
    , m_objectid_size ( 0 )
    , m_entrypos_size ( 0 )
    , m_info_block_type ( 0 )
    , m_info_block_instance ( 0 )
    , m_info_block_position ( 0 )
{

}

bool
AVCDescriptorSpecifier::serialize( IOSSerialize& se ) 
{
    se.write( (byte_t)m_type, "AVCDescriptorSpecifier descriptor_specifier_type" );
    switch ( m_type ) {
        case eIndentifier:
            // nothing to serialize
            return true;
        case eInfoBlockByType:
            se.write( m_info_block_type, "AVCDescriptorSpecifier info_block_type" );
            se.write( m_info_block_instance, "AVCDescriptorSpecifier instance_count" );
            return true;
        case eInfoBlockByPosition:
            se.write( m_info_block_position, "AVCDescriptorSpecifier info_block_position" );
            return true;
        case eSubunit0x80:
            // nothing to serialize
            return true;
        case eInvalid:
        default:
            debugError("Unsupported Descriptor Specifier type: 0x%02X\n",m_type);
            return false;
    }
}

bool 
AVCDescriptorSpecifier::deserialize( IISDeserialize& de ) 
{
    de.read( (byte_t *)&m_type );
    switch ( m_type ) {
        case eIndentifier:
            // nothing to deserialize
            return true;
        case eInfoBlockByType:
            de.read( &m_info_block_type);
            de.read( &m_info_block_instance );
        case eInfoBlockByPosition:
            de.read( &m_info_block_position);
            
            return true;
        case eSubunit0x80:
            // nothing to deserialize
            return true;
        case eInvalid:
        default:
            debugError("Unsupported Descriptor Specifier type: 0x%02X\n",m_type);
            return false;
    }

    return true;
}

AVCDescriptorSpecifier* 
AVCDescriptorSpecifier::clone() const
{
    return new AVCDescriptorSpecifier( *this );
}

//----------------------
AVCDescriptor::AVCDescriptor( Ieee1394Service& ieee1394service )
    : IBusData()
    , m_p1394Service( &ieee1394service )
    , m_nodeId( 0 )
    , m_subunit_type( eST_Unit )
    , m_subunit_id( 0x07 )
    , m_specifier ( AVCDescriptorSpecifier::eInvalid )
    , m_data ( NULL )
    , m_descriptor_length(0)
{
}

AVCDescriptor::AVCDescriptor( Ieee1394Service& ieee1394service,
                              fb_nodeid_t nodeId, ESubunitType subunitType, subunit_id_t subunitId )
    : IBusData()
    , m_p1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_subunit_type( subunitType )
    , m_subunit_id( subunitId )
    , m_specifier ( AVCDescriptorSpecifier::eInvalid )
    , m_data ( NULL )
    , m_descriptor_length(0)
{
}

AVCDescriptor::AVCDescriptor( Ieee1394Service& ieee1394service,
                              fb_nodeid_t nodeId, ESubunitType subunitType, subunit_id_t subunitId,
                              AVCDescriptorSpecifier s )
    : IBusData()
    , m_p1394Service( &ieee1394service )
    , m_nodeId( nodeId )
    , m_subunit_type( subunitType )
    , m_subunit_id( subunitId )
    , m_specifier ( s )
    , m_data ( NULL )
    , m_descriptor_length(0)
{
}

AVCDescriptor::~AVCDescriptor()
{
    if (m_data != NULL) free(m_data);
}

bool
AVCDescriptor::load()
{
    bool result;
    OpenDescriptorCmd openDescCmd(*m_p1394Service);

    debugOutput(DEBUG_LEVEL_VERBOSE, " Open descriptor (%s)\n",getDescriptorName());
    openDescCmd.setMode( OpenDescriptorCmd::eRead );
    openDescCmd.m_specifier=&m_specifier;
    openDescCmd.setNodeId( m_nodeId );
    openDescCmd.setCommandType( AVCCommand::eCT_Control );
    openDescCmd.setSubunitType( getSubunitType() );
    openDescCmd.setSubunitId( getSubunitId() );
    openDescCmd.setVerbose( getVerboseLevel() );
    
    result=openDescCmd.fire();

    if (!result || (openDescCmd.getResponse() != AVCCommand::eR_Accepted)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Could not open descriptor\n");
        return false;
    }
    
    debugOutput(DEBUG_LEVEL_VERBOSE, " Read status descriptor\n");
    ReadDescriptorCmd readDescCmd(*m_p1394Service);
    readDescCmd.m_specifier=&m_specifier;
    readDescCmd.setNodeId( m_nodeId );
    readDescCmd.setCommandType( AVCCommand::eCT_Control );
    readDescCmd.setSubunitType( getSubunitType() );
    readDescCmd.setSubunitId( getSubunitId() );
    readDescCmd.setVerbose( getVerboseLevel() );
    readDescCmd.m_data_length=2;
    readDescCmd.m_address=0;
    
    result=readDescCmd.fire();
    
    if (!result || (readDescCmd.getResponse() != AVCCommand::eR_Accepted)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Could not read descriptor\n");
        return false;
    }
    
    size_t bytes_read=readDescCmd.m_data_length;
    if (bytes_read < 2) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Descriptor length field not present\n");
        return false;        
    }
    
    // obtain descriptor length
    m_descriptor_length=(readDescCmd.m_data[0]<<8) + (readDescCmd.m_data[1]);
    debugOutput(DEBUG_LEVEL_VERBOSE, " Descriptor length: %u\n", m_descriptor_length);
    
    if (m_data != NULL) free(m_data);
    
    m_data=(byte_t *)calloc(m_descriptor_length, 1);
    if (m_data == NULL) {
        debugError("Could not allocate memory for descriptor\n");
        return false;
    }
    
    // we reread everything from here
    bytes_read=0;
    while(bytes_read<m_descriptor_length) {
    
        // read the full descriptor
        readDescCmd.clear();
        readDescCmd.m_specifier=&m_specifier;
        readDescCmd.setNodeId( m_nodeId );
        readDescCmd.setCommandType( AVCCommand::eCT_Control );
        readDescCmd.setSubunitType( getSubunitType() );
        readDescCmd.setSubunitId( getSubunitId() );
        readDescCmd.setVerbose( getVerboseLevel() );
        readDescCmd.m_data_length=m_descriptor_length-bytes_read;
        // account for the length field
        readDescCmd.m_address=bytes_read+2;
        
        result=readDescCmd.fire();
        
        if (!result || (readDescCmd.getResponse() != AVCCommand::eR_Accepted)) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " Could not read descriptor data\n");
            return false;
        }
        
        // copy the payload
        
        if (bytes_read+readDescCmd.m_data_length>m_descriptor_length) {
            debugWarning("Device returned too much data, truncating\n");
            readDescCmd.m_data_length=m_descriptor_length-bytes_read;
        }
        
        debugOutput(DEBUG_LEVEL_VERBOSE, " copying %u bytes to internal buffer offset %u\n",readDescCmd.m_data_length, bytes_read);
        
        memcpy(m_data+bytes_read,readDescCmd.m_data, readDescCmd.m_data_length);
        bytes_read += readDescCmd.m_data_length;
        
        if((readDescCmd.getStatus() != ReadDescriptorCmd::eMoreToRead) 
           && ( bytes_read<m_descriptor_length )) {
            debugError(" Still bytes to read but device claims not.\n");
            return false;
        }
        
    }
    //-------------
    
    debugOutput(DEBUG_LEVEL_VERBOSE, " Close descriptor\n");
    openDescCmd.clear();
    openDescCmd.setMode( OpenDescriptorCmd::eClose );
    openDescCmd.m_specifier=&m_specifier;
    openDescCmd.setNodeId( m_nodeId );
    openDescCmd.setCommandType( AVCCommand::eCT_Control );
    openDescCmd.setSubunitType( getSubunitType() );
    openDescCmd.setSubunitId( getSubunitId() );
    openDescCmd.setVerbose( getVerboseLevel() );
    
    result=openDescCmd.fire();

    if (!result || (openDescCmd.getResponse() != AVCCommand::eR_Accepted)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Could not close descriptor\n");
        return false;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, " Parse descriptor\n");
    // parse the descriptor
    BufferDeserialize de( m_data, m_descriptor_length );
    result = deserialize( de );
    if (!result) {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Could not parse descriptor\n");
        return false;
    }
    
#ifdef DEBUG
    if(getDebugLevel() >= DEBUG_LEVEL_VERY_VERBOSE) {
        StringSerializer se_dbg;
        serialize( se_dbg );
        
        // output the debug message in smaller chunks to avoid problems
        // with a max message size
        unsigned int chars_to_write=se_dbg.getString().size();
        unsigned int chars_written=0;
        while (chars_written<chars_to_write) {
            debugOutputShort(DEBUG_LEVEL_VERBOSE, "%s\n",
                            se_dbg.getString().substr(chars_written, DEBUG_MAX_MESSAGE_LENGTH).c_str());
            chars_written += DEBUG_MAX_MESSAGE_LENGTH-1;
        }
    }
#endif
    return true;
}

bool
AVCDescriptor::serialize( IOSSerialize& se )
{
    return true;
}

bool
AVCDescriptor::deserialize( IISDeserialize& de )
{
    return true;
}

AVCDescriptor* 
AVCDescriptor::clone() const
{
    return new AVCDescriptor( *this );
}
bool
AVCDescriptor::setSubunitType(ESubunitType subunitType)
{
    m_subunit_type = subunitType;
    return true;
}

bool
AVCDescriptor::setNodeId( fb_nodeid_t nodeId )
{
    m_nodeId = nodeId;
    return true;
}

bool
AVCDescriptor::setSubunitId(subunit_id_t subunitId)
{
    m_subunit_id = subunitId & 0x7;
    return true;
}

ESubunitType
AVCDescriptor::getSubunitType()
{
    return m_subunit_type;
}

subunit_id_t
AVCDescriptor::getSubunitId()
{
    return m_subunit_id;
}

bool
AVCDescriptor::setVerbose( int verboseLevel )
{
    setDebugLevel(verboseLevel);
    return true;
}

int
AVCDescriptor::getVerboseLevel()
{
    return getDebugLevel();
}

// --- Info block
AVCInfoBlock::AVCInfoBlock( )
    : IBusData()
    , m_compound_length ( 0 )
    , m_info_block_type ( 0 )
    , m_primary_field_length ( 0 )
    , m_supported_info_block_type ( 0xFFFF )
{}

AVCInfoBlock::AVCInfoBlock( uint16_t supported_type )
    : IBusData()
    , m_compound_length ( 0 )
    , m_info_block_type ( 0 )
    , m_primary_field_length ( 0 )
    , m_supported_info_block_type ( supported_type )
{}

bool
AVCInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    if((m_supported_info_block_type != 0xFFFF) 
       && (m_info_block_type != m_supported_info_block_type)) {
        debugError("%s: Incorrect block type: 0x%04X, should be 0x%04X\n", 
            getInfoBlockName(), m_info_block_type, m_supported_info_block_type);
        return false;
    }
    result &= se.write( m_compound_length, "AVCInfoBlock m_compound_length" );
    result &= se.write( m_info_block_type, "AVCInfoBlock m_info_block_type" );
    result &= se.write( m_primary_field_length, "AVCInfoBlock m_primary_field_length" );
    return result;
}

bool
AVCInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= de.read( &m_compound_length );
    result &= de.read( &m_info_block_type );
    result &= de.read( &m_primary_field_length );
    
    if((m_supported_info_block_type != 0xFFFF) 
       && (m_info_block_type != m_supported_info_block_type)) {
        debugError("%s: Incorrect block type: 0x%04X, should be 0x%04X\n", 
            getInfoBlockName(), m_info_block_type, m_supported_info_block_type);
        return false;
    }
    
    debugOutput(DEBUG_LEVEL_VERBOSE, "%s length=0x%04X (%u), type=0x%04X, primary field length=0x%04X (%u)\n",
        getInfoBlockName(), m_compound_length, m_compound_length,
        m_info_block_type, m_primary_field_length, m_primary_field_length);

    return result;
}

bool
AVCInfoBlock::peekBlockType( IISDeserialize& de, uint16_t *type )
{
    return de.peek(type, 2);
}

bool
AVCInfoBlock::peekBlockLength( IISDeserialize& de, uint16_t *type )
{
    return de.peek(type, 0);
}

AVCInfoBlock* 
AVCInfoBlock::clone() const
{
    return new AVCInfoBlock( *this );
}
bool
AVCInfoBlock::setVerbose( int verboseLevel )
{
    setDebugLevel(verboseLevel);
    return true;
}

int
AVCInfoBlock::getVerboseLevel()
{
    return getDebugLevel();
}

// ---------

//FIXME: find out the correct id for this
AVCRawTextInfoBlock::AVCRawTextInfoBlock( )
    : AVCInfoBlock( 0x000A )
{}

AVCRawTextInfoBlock::~AVCRawTextInfoBlock( )
{
    clear();
}

bool
AVCRawTextInfoBlock::clear()
{
    return true;
}

bool
AVCRawTextInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    if (m_text.size()) {
        se.write(m_text.c_str(),m_text.size(), "AVCRawTextInfoBlock text");
    }
    return result;
}

bool
AVCRawTextInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);

    char *txt;
    result &= de.read(&txt,m_compound_length-4);
    m_text.clear();
    m_text.append(txt);

    return result;
}

// ---------

AVCNameInfoBlock::AVCNameInfoBlock( )
    : AVCInfoBlock( 0x000B )
{}

AVCNameInfoBlock::~AVCNameInfoBlock( )
{
    clear();
}

bool
AVCNameInfoBlock::clear()
{
    return true;
}

bool
AVCNameInfoBlock::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCInfoBlock::serialize(se);
    
    if (m_text.size()) {
        result &= se.write((uint16_t)0x0000, "AVCNameInfoBlock unknown");
        result &= se.write((uint16_t)0x0000, "AVCNameInfoBlock unknown");
        result &= se.write((uint16_t)0x0000, "AVCNameInfoBlock unknown length");
        result &= se.write((uint16_t)0x0000, "AVCNameInfoBlock unknown");
        result &= se.write((uint16_t)m_text.size(), "AVCNameInfoBlock text length");
        
        se.write(m_text.c_str(),m_text.size(), "AVCNameInfoBlock text");
    }
    return result;
}

bool
AVCNameInfoBlock::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCInfoBlock::deserialize(de);

    // FIXME: get the spec somewhere to do this correctly
    uint16_t dummy16;
    uint16_t length1;
    uint16_t text_length;
    
    result &= de.read(&dummy16);
    result &= de.read(&dummy16);
    result &= de.read(&length1);
    result &= de.read(&dummy16);
    result &= de.read(&text_length);

    char *txt;
    result &= de.read(&txt,text_length);
    m_text.clear();
    m_text.append(txt);

    return result;
}
