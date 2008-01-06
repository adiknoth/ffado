/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#include "avc_descriptor_cmd.h"
#include "avc_descriptor.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace AVC {

OpenDescriptorCmd::OpenDescriptorCmd(Ieee1394Service& ieee1394service)
    : AVCCommand( ieee1394service, AVC1394_CMD_OPEN_DESCRIPTOR )
    , m_specifier( NULL )
    , m_mode( eClose )
    , m_status ( 0xFF )
    , m_reserved ( 0x00 )
    , m_locked_node_id ( 0xFFFF )
{
}

OpenDescriptorCmd::~OpenDescriptorCmd()
{
}

bool
OpenDescriptorCmd::clear()
{
    m_status = 0xFF;
    m_reserved =  0x00;
    m_locked_node_id = 0xFFFF;
    return true;
}

bool
OpenDescriptorCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    AVCCommand::serialize( se );
    
    if(m_specifier==NULL) {
        debugError("m_specifier==NULL");
        return false;
    }
    
    m_specifier->serialize( se );
    
    switch (getCommandType()) {
    case eCT_Status:
        se.write( (byte_t)m_status, "OpenDescriptorCmd status" );
        se.write( (byte_t)m_reserved, "OpenDescriptorCmd reserved" );
        se.write( (uint16_t)m_locked_node_id, "OpenDescriptorCmd node_id" );
        break;
    case eCT_Control:
        se.write( (byte_t)m_mode, "OpenDescriptorCmd subfunction" );
        se.write( (byte_t)m_reserved, "OpenDescriptorCmd reserved" );
        break;
    default:
        debugError("Unsupported type for this command: %02X\n", getCommandType());
        return false;
    }
    return true;
}

bool
OpenDescriptorCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    AVCCommand::deserialize( de );
    
    if(m_specifier==NULL) {
        debugError("m_specifier==NULL");
        return false;
    }
    
    m_specifier->deserialize( de );
    
    switch ( getCommandType() ) {
    case eCT_Status:
        de.read( &m_status );
        de.read( &m_reserved );
        de.read( &m_locked_node_id );

        break;
    case eCT_Control:
        de.read( &m_status );
        de.read( &m_reserved );
        switch (m_status) {
            case (byte_t)eClose: m_mode=eClose; break;
            case (byte_t)eRead: m_mode=eRead; break;
            case (byte_t)eWrite: m_mode=eWrite; break;
            default:
            debugError("Unknown response subfunction 0x%02X\n", m_status);
        }
        
        break;
    default:
        debugError("Can't handle command type %s\n", getCommandType());
        return false;
    }


    return true;
}

//

ReadDescriptorCmd::ReadDescriptorCmd(Ieee1394Service& ieee1394service)
    : AVCCommand( ieee1394service, AVC1394_CMD_READ_DESCRIPTOR )
    , m_status ( 0xFF )
    , m_reserved ( 0xFF )
    , m_data_length ( 0 )
    , m_address ( 0 )
    , m_data ( NULL )
    , m_specifier( NULL )
{
}

ReadDescriptorCmd::~ReadDescriptorCmd()
{
    delete[] m_data;
}

bool
ReadDescriptorCmd::clear()
{
    m_status = 0xFF;
    m_reserved =  0x00;
    m_data_length = 0x0000;
    m_address = 0x0000;
    delete[] m_data;
    m_data = NULL;
    return true;
}

bool
ReadDescriptorCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    AVCCommand::serialize( se );
    
    if(m_specifier==NULL) {
        debugError("m_specifier==NULL");
        return false;
    }
    
    m_specifier->serialize( se );
    
    switch (getCommandType()) {
    case eCT_Control:
        se.write( (byte_t)m_status, "ReadDescriptorCmd read_result_status" );
        se.write( (byte_t)m_reserved, "ReadDescriptorCmd reserved" );
        se.write( (uint16_t)m_data_length, "ReadDescriptorCmd data_length" );
        se.write( (uint16_t)m_address, "ReadDescriptorCmd address" );

        break;
    default:
        debugError("Unsupported type for this command: %02X\n", getCommandType());
        return false;
    }
    return true;
}

bool
ReadDescriptorCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    AVCCommand::deserialize( de );

    if(m_specifier==NULL) {
        debugError("m_specifier==NULL");
        return false;
    }

    m_specifier->deserialize( de );

    switch (getCommandType()) {
    case eCT_Control:
        de.read( (byte_t *)&m_status );
        de.read( (byte_t *)&m_reserved );
        de.read( (uint16_t *)&m_data_length );
        de.read( (uint16_t *)&m_address );

        if (getResponse()==eR_Accepted) {
            if (m_data_length>0) {
                // the pointer returned by de.read is not valid outside this function
                // hence we copy the data to an internal buffer
                m_data = new byte_t[m_data_length];
                if(m_data == NULL) {
                    debugError("Could not allocate memory for payload data");
                    return false;
                }
                char * cmd_data = NULL;
                if (!de.read( (char **)&cmd_data, m_data_length )) {
                    delete[] m_data;
                    m_data = NULL;
                    debugError("Could not read payload data");
                    return false;
                }
                memcpy(m_data, cmd_data, m_data_length);

            } else {
                debugWarning("Read descriptor command accepted but no payload data returned.\n");
                m_data=NULL;
            }
        }
        break;
    default:
        debugError("Unsupported type for this command: %02X\n", getCommandType());
        return false;
    }

    return true;
}

enum ReadDescriptorCmd::EReadStatus
ReadDescriptorCmd::getStatus()
{
    switch(m_status) {
        case 0x10: return eComplete;
        case 0x11: return eMoreToRead;
        case 0x12: return eTooLarge;
        default: return eInvalid;
    }
}

}
