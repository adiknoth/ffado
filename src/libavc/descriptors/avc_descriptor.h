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
 * Partially implements AV/C Descriptors/InfoBlocks as in TA2001021
 *
 * The idea is to treat a descriptor as an object that can fetch and store
 * it's state from/to a device. It will call the necessary AV/C commands to
 * achieve this. This (hopefully) simplifies handling the fact that there are
 * so many different descriptor types. It also handles the fact that descriptors
 * are not state-less.
 *
 */

#ifndef AVCDESCRIPTOR_H
#define AVCDESCRIPTOR_H

#include "../avc_definitions.h"

#include "../general/avc_generic.h"
#include "debugmodule/debugmodule.h"

#include <libavc1394/avc1394.h>
#include <string>

class Ieee1394Service;

namespace AVC {

class Unit;
class Subunit;

class Util::Cmd::IOSSerialize;
class Util::Cmd::IISDeserialize;
/**
 * The specifier used to indicate the target descriptor
 */
 
// NOTE: how are we going to do this? all lengths of the
// arguments are dependent on the (sub)unit descriptor
class AVCDescriptorSpecifier : public IBusData
{
public:
    enum EType {
        eIndentifier            = 0x00,
        eListById               = 0x10,
        eListByType             = 0x11,
        eEntryByListId          = 0x20,
        eEntryByObjectIdInList  = 0x21,
        eEntryByType            = 0x22,
        eEntryByObjectId        = 0x23,
        eInfoBlockByType        = 0x30,
        eInfoBlockByPosition    = 0x31,
        eSubunit0x80            = 0x80,
        eInvalid                = 0xFF,
    };
    
public:
    AVCDescriptorSpecifier( enum EType type );
    virtual ~AVCDescriptorSpecifier() {};
    
    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    
    virtual AVCDescriptorSpecifier* clone() const;
    
/*    void setType( enum EType type ) {m_type=type;};
    void setListIdSize( unsigned int l ) {m_listid_size=l;};
    void setObjectIdSize( unsigned int l ) {m_objectid_size=l;};
    void setEntryPositionSize( unsigned int l ) {m_entrypos_size=l;};*/
    
    enum EType      m_type;
    uint16_t        m_listid_size;
    uint16_t        m_objectid_size;
    uint16_t        m_entrypos_size;
    
    uint16_t        m_info_block_type;
    byte_t          m_info_block_instance;
    byte_t          m_info_block_position;

private:


};

/**
 * The descriptor class
 */
class AVCDescriptor : public IBusData
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    // note: in the end these have to be protected
    AVCDescriptor( Unit* unit );
    AVCDescriptor( Unit* unit, Subunit* subunit  );
    AVCDescriptor( Unit* unit, Subunit* subunit, AVCDescriptorSpecifier s );
    virtual ~AVCDescriptor();
    
    virtual AVCDescriptor* clone() const;
    
    void setSpecifier(AVCDescriptorSpecifier s) {m_specifier=s;};

    ESubunitType getSubunitType() const;
    subunit_id_t getSubunitId() const;

    bool setVerboseLevel( int verboseLevel );
    int getVerboseLevel();

    virtual const char* getDescriptorName() const
        {return "AVCDescriptor";};
    
    bool load();
    bool reload();
    
protected:
    void printBufferBytes(unsigned int level, size_t length, byte_t* buffer) const;

    Unit*            m_unit;
    Subunit*         m_subunit;
    
    AVCDescriptorSpecifier m_specifier;
    
    byte_t*          m_data;
    uint16_t         m_descriptor_length;
    
    bool             m_loaded;

};

/**
 * The info block class
 */
class AVCInfoBlock : public IBusData
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );
    static bool peekBlockType( Util::Cmd::IISDeserialize& de, uint16_t * );
    static bool peekBlockLength( Util::Cmd::IISDeserialize& de, uint16_t * );

    // note: in the end these have to be protected
    AVCInfoBlock( );
    AVCInfoBlock( uint16_t );
    virtual ~AVCInfoBlock() {};
    
    virtual AVCInfoBlock* clone() const;
    
//     EInfoBlockType getType();
    
    bool setVerbose( int verboseLevel );
    int getVerboseLevel();
    
    virtual const char* getInfoBlockName() const
        {return "AVCInfoBlock";};
    
    uint16_t    m_compound_length;
    uint16_t    m_info_block_type;
    uint16_t    m_primary_field_length;
    
    uint16_t    m_supported_info_block_type;
private:

};

class AVCRawTextInfoBlock : public AVCInfoBlock
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual bool clear();
    
    AVCRawTextInfoBlock( );
    virtual ~AVCRawTextInfoBlock();
    virtual const char* getInfoBlockName() const
        {return "AVCRawTextInfoBlock";};

    std::string m_text;

protected:

private:

};

class AVCNameInfoBlock : public AVCInfoBlock
{
public:

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual bool clear();
    
    AVCNameInfoBlock( );
    virtual ~AVCNameInfoBlock();
    virtual const char* getInfoBlockName() const
        {return "AVCNameInfoBlock";};

    std::string m_text;

protected:

private:

};
/**
 * 
 */
// class AVCUnitIdentifierDescriptor : public AVCDescriptor
// {
// 
// public:
//     AVCUnitIdentifierDescriptor(  );
//     virtual ~AVCUnitIdentifierDescriptor() {}
// 
// };

}

#endif // AVCDESCRIPTOR_H
