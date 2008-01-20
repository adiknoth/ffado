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

#include "bebob/bebob_dl_codes.h"
#include "bebob/bebob_dl_bcd.h"

unsigned short BeBoB::CommandCodes::m_gCommandId = 0;

BeBoB::CommandCodes::CommandCodes( fb_quadlet_t protocolVersion,
                                   fb_byte_t commandCode,
                                   size_t     msgSize,
                                   fb_byte_t operandSizeRequest,
                                   fb_byte_t operandSizeRespone )
    : m_commandId( m_gCommandId++ )
    , m_protocolVersion( protocolVersion )
    , m_commandCode( commandCode )
    , m_msgSize( msgSize )
    , m_operandSizeRequest( operandSizeRequest )
    , m_operandSizeResponse( operandSizeRespone )
    , m_resp_protocolVersion( 0 )
    , m_resp_commandId( 0 )
    , m_resp_commandCode( 0 )
    , m_resp_operandSize( 0 )
{
}

BeBoB::CommandCodes::~CommandCodes()
{
}

bool
BeBoB::CommandCodes::serialize( Util::Cmd::IOSSerialize& se )
{
    byte_t tmp;

    bool result = se.write( m_protocolVersion, "CommandCodes: protocol version" );
    tmp = m_commandId & 0xff;
    result &= se.write( tmp, "CommandCodes: command id low" );
    tmp = m_commandId >> 8;
    result &= se.write( tmp, "CommandCodes: command id high" );
    result &= se.write( m_commandCode, "CommandCodes: command code" );
    result &= se.write( m_operandSizeRequest, "CommandCodes: request operand size" );

    return result;
}

bool
BeBoB::CommandCodes::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = de.read( &m_resp_protocolVersion );
    fb_byte_t tmp;
    result &= de.read( &tmp );
    m_resp_commandId = tmp;
    result &= de.read( &tmp );
    m_resp_commandId |= tmp << 8;
    result &= de.read( &m_resp_commandCode );
    result &= de.read( &m_resp_operandSize );

    return result;
}

size_t
BeBoB::CommandCodes::getMaxSize()
{
    return 2 * sizeof( fb_quadlet_t ) + m_msgSize;
}


////////////////////////////////

BeBoB::CommandCodesReset::CommandCodesReset( fb_quadlet_t protocolVersion,
                                             EStartMode startMode )
    : CommandCodes( protocolVersion, eCmdC_Reset, sizeof( m_startMode ), 1, 0 )
    , m_startMode( startMode )
{
}

BeBoB::CommandCodesReset::~CommandCodesReset()
{
}

bool
BeBoB::CommandCodesReset::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = CommandCodes::serialize( se );
    result &= se.write( m_startMode, "CommandCodesReset: boot mode" );

    return result;
}

bool
BeBoB::CommandCodesReset::deserialize( Util::Cmd::IISDeserialize& de )
{
    return CommandCodes::deserialize( de );
}

////////////////////////////////

BeBoB::CommandCodesProgramGUID::CommandCodesProgramGUID(
    fb_quadlet_t protocolVersion,
    fb_octlet_t guid )
    : CommandCodes( protocolVersion, eCmdC_ProgramGUID, sizeof( m_guid ), 2, 0 )
    , m_guid( guid )
{
}

BeBoB::CommandCodesProgramGUID::~CommandCodesProgramGUID()
{
}

bool
BeBoB::CommandCodesProgramGUID::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = CommandCodes::serialize( se );
    fb_quadlet_t tmp = m_guid >> 32;
    result &= se.write( tmp, "CommandCodesProgramGUID: GUID (high)" );
    tmp = m_guid & 0xffffffff;
    result &= se.write( tmp, "CommandCodesProgramGUID: GUID (low)" );

    return result;
}

bool
BeBoB::CommandCodesProgramGUID::deserialize( Util::Cmd::IISDeserialize& de )
{
    return CommandCodes::deserialize( de );
}

////////////////////////////////

BeBoB::CommandCodesDownloadStart::CommandCodesDownloadStart(
    fb_quadlet_t protocolVersion,
    EObject object )
    : CommandCodes( protocolVersion, eCmdC_DownloadStart, 10*4, 10, 1 )
    , m_object( object )
    , m_date( 0 )
    , m_time( 0 )
    , m_id( 0 )
    , m_version( 0 )
    , m_address( 0 )
    , m_length( 0 )
    , m_crc( 0 )
{
}

BeBoB::CommandCodesDownloadStart::~CommandCodesDownloadStart()
{
}

bool
BeBoB::CommandCodesDownloadStart::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = CommandCodes::serialize( se );

    result &= se.write( m_object,  "CommandCodesDownloadStart: object" );
    for (  unsigned int i = 0; i < sizeof( m_date ); ++i ) {
        fb_byte_t* tmp = ( fb_byte_t* )( &m_date );
        result &= se.write( tmp[i], "CommandCodesDownloadStart: date" );
    }
    for (  unsigned int i = 0; i < sizeof( m_date ); ++i ) {
        fb_byte_t* tmp = ( fb_byte_t* )( &m_time );
        result &= se.write( tmp[i], "CommandCodesDownloadStart: time" );
    }
    result &= se.write( m_id,      "CommandCodesDownloadStart: id" );
    result &= se.write( m_version, "CommandCodesDownloadStart: version" );
    result &= se.write( m_address, "CommandCodesDownloadStart: address" );
    result &= se.write( m_length,  "CommandCodesDownloadStart: length" );
    result &= se.write( m_crc,     "CommandCodesDownloadStart: crc" );

    return result;
}

bool
BeBoB::CommandCodesDownloadStart::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = CommandCodes::deserialize( de );
    result &= de.read( reinterpret_cast<fb_quadlet_t*>( &m_resp_max_block_size ) );

    return result;
}

////////////////////////////////

BeBoB::CommandCodesDownloadBlock::CommandCodesDownloadBlock(
    fb_quadlet_t protocolVersion )
    : CommandCodes( protocolVersion,
                    eCmdC_DownloadBlock,
                    12,
                    3,
                    2 )
    , m_seqNumber( 0 )
    , m_address ( 0 )
    , m_resp_seqNumber( 0 )
    , m_resp_errorCode( 0 )
{
}

BeBoB::CommandCodesDownloadBlock::~CommandCodesDownloadBlock()
{
}

bool
BeBoB::CommandCodesDownloadBlock::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = CommandCodes::serialize( se );
    result &= se.write( m_seqNumber, "CommandCodesDownloadBlock: sequence number" );
    result &= se.write( m_address, "CommandCodesDownloadBlock: address" );
    result &= se.write( m_numBytes, "CommandCodesDownloadBlock: number of bytes" );
    return result;
}

bool
BeBoB::CommandCodesDownloadBlock::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = CommandCodes::deserialize( de );
    result &= de.read( &m_resp_seqNumber );
    result &= de.read( &m_resp_errorCode );

    return result;
}

////////////////////////////////

BeBoB::CommandCodesDownloadEnd::CommandCodesDownloadEnd(
    fb_quadlet_t protocolVersion )
    : CommandCodes( protocolVersion, eCmdC_DownloadEnd, 2, 0, 2 )
{
}

BeBoB::CommandCodesDownloadEnd::~CommandCodesDownloadEnd()
{
}

bool
BeBoB::CommandCodesDownloadEnd::serialize( Util::Cmd::IOSSerialize& se )
{
    return CommandCodes::serialize( se );
}

bool
BeBoB::CommandCodesDownloadEnd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = CommandCodes::deserialize( de );
    result &= de.read( &m_resp_crc32 );
    result &= de.read( &m_resp_valid );

    return result;
}


////////////////////////////////

BeBoB::CommandCodesInitializePersParam::CommandCodesInitializePersParam(
    fb_quadlet_t protocolVersion )
    : CommandCodes( protocolVersion, eCmdC_InitPersParams, 0, 0, 0 )
{
}

BeBoB::CommandCodesInitializePersParam::~CommandCodesInitializePersParam()
{
}

bool
BeBoB::CommandCodesInitializePersParam::serialize( Util::Cmd::IOSSerialize& se )
{
    return CommandCodes::serialize( se );
}

bool
BeBoB::CommandCodesInitializePersParam::deserialize( Util::Cmd::IISDeserialize& de )
{
    return CommandCodes::deserialize( de );
}

////////////////////////////////

BeBoB::CommandCodesInitializeConfigToFactorySetting::CommandCodesInitializeConfigToFactorySetting(
    fb_quadlet_t protocolVersion )
    : CommandCodes( protocolVersion, eCmdC_InitConfigToFactorySetting, 0, 0, 0 )
{
}

BeBoB::CommandCodesInitializeConfigToFactorySetting::~CommandCodesInitializeConfigToFactorySetting()
{
}

bool
BeBoB::CommandCodesInitializeConfigToFactorySetting::serialize( Util::Cmd::IOSSerialize& se )
{
    return CommandCodes::serialize( se );
}

bool
BeBoB::CommandCodesInitializeConfigToFactorySetting::deserialize( Util::Cmd::IISDeserialize& de )
{
    return CommandCodes::deserialize( de );
}

////////////////////////////////

BeBoB::CommandCodesGo::CommandCodesGo( fb_quadlet_t protocolVersion,
                                             EStartMode startMode )
    : CommandCodes( protocolVersion, eCmdC_Go, sizeof( m_startMode ), 1, 1 )
    , m_startMode( startMode )
{
}

BeBoB::CommandCodesGo::~CommandCodesGo()
{
}

bool
BeBoB::CommandCodesGo::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = CommandCodes::serialize( se );
    result &= se.write( m_startMode, "CommandCodesGo: start mode" );

    return result;
}

bool
BeBoB::CommandCodesGo::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = CommandCodes::deserialize( de );
    result &= de.read( &m_resp_validCRC );

    return result;
}
