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

#include "avc_signal_source.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

#define AVC1394_CMD_SIGNAL_SOURCE 0x1A

namespace AVC {


SignalUnitAddress::SignalUnitAddress()
    : m_plugId( ePI_Invalid )
{
}

bool
SignalUnitAddress::serialize( Util::Cmd::IOSSerialize& se )
{
    byte_t reserved = 0xff;
    se.write( reserved, "SignalUnitAddress" );
    se.write( m_plugId, "SignalUnitAddress plugId" );
    return true;
}

bool
SignalUnitAddress::deserialize( Util::Cmd::IISDeserialize& de )
{
    byte_t operand;
    de.read( &operand );
    de.read( &m_plugId );
    return true;
}

SignalUnitAddress*
SignalUnitAddress::clone() const
{
    return new SignalUnitAddress( *this );
}

////////////////////////////////////////

SignalSubunitAddress::SignalSubunitAddress()
    : m_subunitType( AVC1394_SUBUNIT_RESERVED )
    , m_subunitId( AVC1394_SUBUNIT_ID_RESERVED )
    , m_plugId( ePI_Invalid )
{
}

bool
SignalSubunitAddress::serialize( Util::Cmd::IOSSerialize& se )
{
    byte_t operand = ( m_subunitType << 3 ) | ( m_subunitId & 0x7 );
    se.write( operand,  "SignalSubunitAddress subunitType & subunitId" );
    se.write( m_plugId, "SignalSubunitAddress plugId" );
    return true;
}

bool
SignalSubunitAddress::deserialize( Util::Cmd::IISDeserialize& de )
{
    byte_t operand;
    de.read( &operand );
    m_subunitType = operand >> 3;
    m_subunitId = operand & 0x7;
    de.read( &m_plugId );
    return true;
}

SignalSubunitAddress*
SignalSubunitAddress::clone() const
{
    return new SignalSubunitAddress( *this );
}

////////////////////////////////////////


SignalSourceCmd::SignalSourceCmd( Ieee1394Service& ieee1394service )
    : AVCCommand( ieee1394service, AVC1394_CMD_SIGNAL_SOURCE )
    , m_resultStatus( 0xff )
    , m_outputStatus( 0xff )
    , m_conv( 0xff )
    , m_signalStatus( 0xff )
    , m_signalSource( 0 )
    , m_signalDestination( 0 )
{
}

SignalSourceCmd::~SignalSourceCmd()
{
    delete m_signalSource;
    m_signalSource = 0;
    delete m_signalDestination;
    m_signalDestination = 0;
}

bool
SignalSourceCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    AVCCommand::serialize( se );

    byte_t operand;
    switch ( getCommandType() ) {
    case eCT_Status:
        operand = ( m_outputStatus << 5 )
                  | ( ( m_conv & 0x1 ) << 4 )
                  | ( m_signalStatus & 0xf );
        se.write( operand, "SignalSourceCmd outputStatus & conv & signalStatus" );
        break;
    case eCT_Control:
    case eCT_SpecificInquiry:
        operand = m_resultStatus & 0xf;
        se.write( operand, "SignalSourceCmd resultStatus" );
        break;
    default:
        cerr << "Can't handle command type " << getCommandType() << endl;
        return false;
    }

    switch( getSubunitType() ) {
    case eST_Unit:
    case eST_Audio:
    case eST_Music:
        {
            if ( m_signalSource ) {
                m_signalSource->serialize( se );
            } else {
                byte_t reserved = 0xff;
                se.write( reserved, "SignalSourceCmd" );
                se.write( reserved, "SignalSourceCmd" );
            }

            if ( m_signalDestination ) {
                m_signalDestination->serialize( se );
            } else {
                byte_t reserved = 0xff;
                se.write( reserved, "SignalSourceCmd" );
                se.write( reserved, "SignalSourceCmd" );
            }
        }
        break;
    default:
        cerr << "Can't handle subunit type " << getSubunitType() << endl;
        return false;
    }

    return true;
}

bool
SignalSourceCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    delete m_signalSource;
    m_signalSource = 0;
    delete m_signalDestination;
    m_signalDestination = 0;

    AVCCommand::deserialize( de );

    byte_t operand;
    switch ( getCommandType() ) {
    case eCT_Status:
        de.read( &operand );
        m_outputStatus = operand >> 5;
        m_conv = ( operand & 0x10 ) >> 4;
        m_signalStatus = operand & 0xf;
        break;
    case eCT_Control:
    case eCT_SpecificInquiry:
        de.read( &operand );
        m_resultStatus = operand & 0xf;
        break;
    default:
        cerr << "Can't handle command type " << getCommandType() << endl;
        return false;
    }

    switch( getSubunitType() ) {
    case eST_Unit:
    case eST_Audio:
    case eST_Music:
        {
            byte_t operand;
            de.peek( &operand );
            if ( operand == 0xff ) {
                m_signalSource = new SignalUnitAddress;
            } else {
                m_signalSource = new SignalSubunitAddress;
            }

            m_signalSource->deserialize( de );

            de.peek( &operand );
            if ( operand == 0xff ) {
                m_signalDestination = new SignalUnitAddress;
            } else {
                m_signalDestination = new SignalSubunitAddress;
            }
            m_signalDestination->deserialize( de );
        }
        break;
    default:
        cerr << "Can't handle subunit type " << getSubunitType() << endl;
        return false;
    }

    return true;
}

bool
SignalSourceCmd::setSignalSource( SignalUnitAddress& signalAddress )
{
    if ( m_signalSource ) {
        delete m_signalSource;
    }
    m_signalSource = signalAddress.clone();
    return true;
}

bool
SignalSourceCmd::setSignalSource( SignalSubunitAddress& signalAddress )
{
    if ( m_signalSource ) {
        delete m_signalSource;
    }
    m_signalSource = signalAddress.clone();
    return true;
}

bool
SignalSourceCmd::setSignalDestination( SignalUnitAddress& signalAddress )
{
    if ( m_signalDestination ) {
        delete m_signalDestination;
    }
    m_signalDestination = signalAddress.clone();
    return true;
}

bool
SignalSourceCmd::setSignalDestination( SignalSubunitAddress& signalAddress )
{
    if ( m_signalDestination ) {
        delete m_signalDestination;
    }
    m_signalDestination = signalAddress.clone();
    return true;
}

SignalAddress*
SignalSourceCmd::getSignalSource()
{
    return m_signalSource;
}

SignalAddress*
SignalSourceCmd::getSignalDestination()
{
    return m_signalDestination;
}

}
