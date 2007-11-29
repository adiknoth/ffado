/*
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

#include "avc_extended_stream_format.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>

namespace AVC {

///////////////////////////////////////////////////////////
std::ostream& operator<<( std::ostream& stream, StreamFormatInfo info )
{
/*    stream << info.m_freq << " Hz (";
    if ( info.m_format ) {
            stream << "sync ";
    } else {
        stream << "compound ";
    }
    stream << "stream, ";
    stream << "audio channels: " << info.m_audioChannels
           << ", midi channels: " << info.m_midiChannels << ")";
*/
     stream << "  NbChannels " << (int)info.m_numberOfChannels << ", Format " << (int)info.m_streamFormat;
    return stream;
}

StreamFormatInfo::StreamFormatInfo()
    : IBusData()
{
}

bool
StreamFormatInfo::serialize( Util::IOSSerialize& se )
{
    se.write( m_numberOfChannels, "StreamFormatInfo numberOfChannels" );
    se.write( m_streamFormat, "StreamFormatInfo streamFormat" );
    return true;
}

bool
StreamFormatInfo::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_numberOfChannels );
    de.read( &m_streamFormat );
    return true;
}

StreamFormatInfo*
StreamFormatInfo::clone() const
{
    return new StreamFormatInfo( *this );
}

////////////////////////////////////////////////////////////

FormatInformationStreamsSync::FormatInformationStreamsSync()
    : FormatInformationStreams()
    , m_reserved0( 0xff )
    , m_samplingFrequency( eSF_DontCare )
    , m_rateControl( eRC_DontCare )
    , m_reserved1( 0xff )
{
}

bool
FormatInformationStreamsSync::serialize( Util::IOSSerialize& se )
{
    se.write( m_reserved0, "FormatInformationStreamsSync reserved" );

    // we have to clobber some bits
    byte_t operand = ( m_samplingFrequency << 4 ) | 0x0e;
    if ( m_rateControl == eRC_DontCare) {
        operand |= 0x1;
    }
    se.write( operand, "FormatInformationStreamsSync sampling frequency and rate control" );

    se.write( m_reserved1, "FormatInformationStreamsSync reserved" );
    return true;
}

bool
FormatInformationStreamsSync::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_reserved0 );

    byte_t operand;
    de.read( &operand );
    m_samplingFrequency = operand >> 4;
    m_rateControl = operand & 0x01;

    de.read( &m_reserved1 );
    return true;
}

FormatInformationStreamsSync*
FormatInformationStreamsSync::clone() const
{
    return new FormatInformationStreamsSync( *this );
}

////////////////////////////////////////////////////////////
std::ostream& operator<<( std::ostream& stream, FormatInformationStreamsCompound info )
{
    stream << (int)info.m_samplingFrequency << " Hz (rate control: ";
    stream << (int)info.m_rateControl << ")" << std::endl;

    for ( FormatInformationStreamsCompound::StreamFormatInfoVector::iterator it = info.m_streamFormatInfos.begin();
        it != info.m_streamFormatInfos.end();
        ++it )
    {
        StreamFormatInfo* sfi=*it;
        stream << "     > " << *sfi << std::endl;
    }

    return stream;
}

FormatInformationStreamsCompound::FormatInformationStreamsCompound()
    : FormatInformationStreams()
    , m_samplingFrequency( eSF_DontCare )
    , m_rateControl( eRC_DontCare )
    , m_numberOfStreamFormatInfos( 0 )
{
}

FormatInformationStreamsCompound::~FormatInformationStreamsCompound()
{
    for ( StreamFormatInfoVector::iterator it = m_streamFormatInfos.begin();
          it != m_streamFormatInfos.end();
          ++it )
    {
        delete *it;
    }
}

bool
FormatInformationStreamsCompound::serialize( Util::IOSSerialize& se )
{
    se.write( m_samplingFrequency, "FormatInformationStreamsCompound samplingFrequency" );
    se.write( m_rateControl, "FormatInformationStreamsCompound rateControl" );
    se.write( m_numberOfStreamFormatInfos, "FormatInformationStreamsCompound numberOfStreamFormatInfos" );
    for ( StreamFormatInfoVector::iterator it = m_streamFormatInfos.begin();
          it != m_streamFormatInfos.end();
          ++it )
    {
        ( *it )->serialize( se );
    }
    return true;
}

bool
FormatInformationStreamsCompound::deserialize( Util::IISDeserialize& de )
{
    de.read( &m_samplingFrequency );
    de.read( &m_rateControl );
    de.read( &m_numberOfStreamFormatInfos );
    for ( int i = 0; i < m_numberOfStreamFormatInfos; ++i ) {
        StreamFormatInfo* streamFormatInfo = new StreamFormatInfo;
        if ( !streamFormatInfo->deserialize( de ) ) {
            return false;
        }
        m_streamFormatInfos.push_back( streamFormatInfo );
    }
    return true;
}

FormatInformationStreamsCompound*
FormatInformationStreamsCompound::clone() const
{
    return new FormatInformationStreamsCompound( *this );
}

////////////////////////////////////////////////////////////

FormatInformation::FormatInformation()
    : IBusData()
    , m_root( eFHR_Invalid )
    , m_level1( eFHL1_AUDIOMUSIC_DONT_CARE )
    , m_level2( eFHL2_AM824_DONT_CARE )
    , m_streams( 0 )
{
}

FormatInformation::FormatInformation( const FormatInformation& rhs )
    : IBusData()
    , m_root( rhs.m_root )
    , m_level1( rhs.m_level1 )
    , m_level2( rhs.m_level2 )
    , m_streams( 0 )
{
    if ( rhs.m_streams ) {
        m_streams = dynamic_cast<FormatInformationStreams*>( rhs.m_streams->clone() );
    }
}

FormatInformation::~FormatInformation()
{
    delete m_streams;
    m_streams = 0;
}

bool
FormatInformation::serialize( Util::IOSSerialize& se )
{
    if ( m_root != eFHR_Invalid ) {
        se.write( m_root, "FormatInformation hierarchy root" );
        if ( m_level1 != eFHL1_AUDIOMUSIC_DONT_CARE ) {
            se.write( m_level1, "FormatInformation hierarchy level 1" );
            if ( m_level2 != eFHL2_AM824_DONT_CARE ) {
                se.write( m_level2, "FormatInformation hierarchy level 2" );
            }
        }
    }
    if ( m_streams ) {
        return m_streams->serialize( se );
    }
    return true;
}

bool
FormatInformation::deserialize( Util::IISDeserialize& de )
{
    bool result = false;

    delete m_streams;
    m_streams = 0;

    // this code just parses BeBoB replies.
    de.read( &m_root );

    // just parse an audio and music hierarchy
    if ( m_root == eFHR_AudioMusic ) {
        de.read( &m_level1 );

        switch ( m_level1 ) {
        case eFHL1_AUDIOMUSIC_AM824:
        {
            // note: compound streams don't have a second level
            de.read( &m_level2 );

            if (m_level2 == eFHL2_AM824_SYNC_STREAM ) {
                m_streams = new FormatInformationStreamsSync();
                result = m_streams->deserialize( de );
            } else {
                // anything but the sync stream workds currently.
                printf( "could not parse format information. (format hierarchy level 2 not recognized)\n" );
            }
        }
        break;
        case  eFHL1_AUDIOMUSIC_AM824_COMPOUND:
        {
            m_streams = new FormatInformationStreamsCompound();
            result = m_streams->deserialize( de );
        }
        break;
        default:
            printf( "could not parse format information. (format hierarchy level 1 not recognized)\n" );
        }
    }

    return result;
}

FormatInformation*
FormatInformation::clone() const
{
    return new FormatInformation( *this );
}

////////////////////////////////////////////////////////////

ExtendedStreamFormatCmd::ExtendedStreamFormatCmd( Ieee1394Service& service,
                                                  ESubFunction eSubFunction )
    : AVCCommand( service, AVC1394_STREAM_FORMAT_SUPPORT )
    , m_subFunction( eSubFunction )
    , m_status( eS_NotUsed )
    , m_indexInStreamFormat( 0 )
    , m_formatInformation( new FormatInformation )
{
    UnitPlugAddress unitPlugAddress( UnitPlugAddress::ePT_PCR, 0x00 );
    m_plugAddress = new PlugAddress( PlugAddress::ePD_Output, PlugAddress::ePAM_Unit, unitPlugAddress );
}

ExtendedStreamFormatCmd::ExtendedStreamFormatCmd(
    const ExtendedStreamFormatCmd& rhs )
    : AVCCommand( rhs )
{
    m_subFunction = rhs.m_subFunction;
    m_plugAddress = new PlugAddress( *rhs.m_plugAddress );
    m_formatInformation =
        new FormatInformation( *rhs.m_formatInformation );
}

ExtendedStreamFormatCmd::~ExtendedStreamFormatCmd()
{
    delete m_plugAddress;
    m_plugAddress = 0;
    delete m_formatInformation;
    m_formatInformation = 0;
}

bool
ExtendedStreamFormatCmd::setPlugAddress( const PlugAddress& plugAddress )
{
    delete m_plugAddress;
    m_plugAddress = plugAddress.clone();
    return true;
}

bool
ExtendedStreamFormatCmd::setIndexInStreamFormat( const int index )
{
    m_indexInStreamFormat = index;
    return true;
}

bool
ExtendedStreamFormatCmd::serialize( Util::IOSSerialize& se )
{
    AVCCommand::serialize( se );
    se.write( m_subFunction, "ExtendedStreamFormatCmd subFunction" );
    m_plugAddress->serialize( se );
    se.write( m_status, "ExtendedStreamFormatCmd status" );
    if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
        se.write( m_indexInStreamFormat, "indexInStreamFormat" );
    }
    m_formatInformation->serialize( se );
    return true;
}

bool
ExtendedStreamFormatCmd::deserialize( Util::IISDeserialize& de )
{
    AVCCommand::deserialize( de );
    de.read( &m_subFunction );
    m_plugAddress->deserialize( de );
    de.read( &m_status );
    if ( m_subFunction == eSF_ExtendedStreamFormatInformationCommandList ) {
        de.read( &m_indexInStreamFormat );
    }
    m_formatInformation->deserialize( de );
    return true;
}

ExtendedStreamFormatCmd::EStatus
ExtendedStreamFormatCmd::getStatus()
{
    EStatus status = static_cast<EStatus>( m_status );
    return status;
}

FormatInformation*
ExtendedStreamFormatCmd::getFormatInformation()
{
    return m_formatInformation;
}

ExtendedStreamFormatCmd::index_in_stream_format_t
ExtendedStreamFormatCmd::getIndex()
{
    return m_indexInStreamFormat;
}

bool
ExtendedStreamFormatCmd::setSubFunction( ESubFunction subFunction )
{
    m_subFunction = subFunction;
    return true;
}

}
