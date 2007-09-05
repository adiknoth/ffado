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

#ifndef AVCDESCRIPTORCMD_H
#define AVCDESCRIPTORCMD_H

#include "../general/avc_generic.h"

#include <libavc1394/avc1394.h>

namespace AVC {


// defined in avc1394.h
// #define AVC1394_CMD_OPEN_DESCRIPTOR         0x08
// #define AVC1394_CMD_READ_DESCRIPTOR         0x09
// #define AVC1394_CMD_WRITE_DESCRIPTOR        0x0A
// #define AVC1394_CMD_SEARCH_DESCRIPTOR       0x0B
// #define AVC1394_CMD_OBJECT_NUMBER_SELECT    0x0D

// not yet defined
#define AVC1394_CMD_CREATE_DESCRIPTOR       0x0C
#define AVC1394_CMD_OPEN_INFOBLOCK          0x05
#define AVC1394_CMD_READ_INFOBLOCK          0x06
#define AVC1394_CMD_WRITE_INFOBLOCK         0x07

class AVCDescriptorSpecifier;

class OpenDescriptorCmd: public AVCCommand
{
public:
    enum EMode {
        eClose = 0x00,
        eRead  = 0x01,
        eWrite = 0x03,
    };
    
    enum EStatus {
        eReady          = 0x00,
        eReadOpened     = 0x01,
        eNonExistent    = 0x04,
        eListOnly       = 0x05,
        eAtCapacity     = 0x11,
        eWriteOpened    = 0x33,
    };
 
    OpenDescriptorCmd(Ieee1394Service& );
    virtual ~OpenDescriptorCmd();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual bool clear();

    virtual const char* getCmdName() const
    { return "OpenDescriptorCmd"; }

    virtual void setMode( enum EMode m ) {m_mode=m;};
    AVCDescriptorSpecifier *m_specifier;
    enum EMode m_mode;

    byte_t m_status;
    byte_t m_reserved;
    uint16_t m_locked_node_id;
    
private:
};

class ReadDescriptorCmd: public AVCCommand
{
public:
    enum EReadStatus {
        eComplete     = 0x10,
        eMoreToRead   = 0x11,
        eTooLarge     = 0x12,
        eInvalid      = 0xFF,
    };

    ReadDescriptorCmd(Ieee1394Service& ieee1394service);
    virtual ~ReadDescriptorCmd();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );
    
    virtual bool clear();
    
    enum EReadStatus getStatus();

    virtual const char* getCmdName() const
    { return "ReadDescriptorCmd"; }
    
    byte_t   m_status;
    byte_t   m_reserved;
    uint16_t m_data_length;
    uint16_t m_address;
    
    byte_t *m_data;
    
    AVCDescriptorSpecifier *m_specifier;
private:

};

}

#endif // AVCDESCRIPTORCMD_H
