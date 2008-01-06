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

#include "avc_extended_subunit_info.h"
#include "libutil/cmd_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>

#define NR_OF_PAGE_DATA 5
#define SIZE_OF_PAGE_ENTRY 5

namespace AVC {

ExtendedSubunitInfoPageData::ExtendedSubunitInfoPageData()
    : IBusData()
    , m_functionBlockType( 0xff )
    , m_functionBlockId( 0xff )
    , m_functionBlockSpecialPupose( eSP_NoSpecialPupose )
    , m_noOfInputPlugs( 0xff )
    , m_noOfOutputPlugs( 0xff )
{
}

ExtendedSubunitInfoPageData::~ExtendedSubunitInfoPageData()
{
}

bool
ExtendedSubunitInfoPageData::serialize( Util::Cmd::IOSSerialize& se )
{
    se.write( m_functionBlockType, "ExtendedSubunitInfoPageData: function block type" );
    se.write( m_functionBlockId, "ExtendedSubunitInfoPageData: function block id" );
    se.write( m_functionBlockSpecialPupose, "ExtendedSubunitInfoPageData: function block special purpose" );
    se.write( m_noOfInputPlugs, "ExtendedSubunitInfoPageData: number of input plugs" );
    se.write( m_noOfOutputPlugs, "ExtendedSubunitInfoPageData: number of output plugs" );

    return true;
}

bool
ExtendedSubunitInfoPageData::deserialize( Util::Cmd::IISDeserialize& de )
{
    de.read( &m_functionBlockType );
    de.read( &m_functionBlockId );
    de.read( &m_functionBlockSpecialPupose );
    de.read( &m_noOfInputPlugs );
    de.read( &m_noOfOutputPlugs );
    return true;
}

ExtendedSubunitInfoPageData*
ExtendedSubunitInfoPageData::clone() const
{
    return new ExtendedSubunitInfoPageData( *this );
}

//////////////////////////////////////////////

ExtendedSubunitInfoCmd::ExtendedSubunitInfoCmd( Ieee1394Service& ieee1394service )
    : AVCCommand( ieee1394service, AVC1394_CMD_SUBUNIT_INFO )
    , m_page( 0xff )
    , m_fbType( eFBT_AllFunctinBlockType )
{
}

ExtendedSubunitInfoCmd::ExtendedSubunitInfoCmd( const ExtendedSubunitInfoCmd& rhs )
    : AVCCommand( rhs )
    , m_page( rhs.m_page )
    , m_fbType( rhs.m_fbType )
{
    for ( ExtendedSubunitInfoPageDataVector::const_iterator it =
              rhs.m_infoPageDatas.begin();
          it != rhs.m_infoPageDatas.end();
          ++it )
    {
        m_infoPageDatas.push_back( ( *it )->clone() );
    }
}

ExtendedSubunitInfoCmd::~ExtendedSubunitInfoCmd()
{
    for ( ExtendedSubunitInfoPageDataVector::iterator it =
              m_infoPageDatas.begin();
          it != m_infoPageDatas.end();
          ++it )
    {
        delete *it;
    }
}

bool
ExtendedSubunitInfoCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool status = false;
    status = AVCCommand::serialize( se );
    status &= se.write( m_page, "ExtendedSubunitInfoCmd: page" );
    status &= se.write( m_fbType, "ExtendedSubunitInfoCmd: function block type" );
    for ( ExtendedSubunitInfoPageDataVector::const_iterator it =
              m_infoPageDatas.begin();
          it != m_infoPageDatas.end();
          ++it )
    {
        status &= ( *it )->serialize( se );
    }

    int startIndex = m_infoPageDatas.size() * SIZE_OF_PAGE_ENTRY;
    int endIndex = SIZE_OF_PAGE_ENTRY * NR_OF_PAGE_DATA;
    for ( int i = startIndex; i < endIndex; ++i ) {
        byte_t dummy = 0xff;
        se.write( dummy, "ExtendedSubunitInfoCmd: space fill" );
    }
    return status;
}

bool
ExtendedSubunitInfoCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool status = false;
    status = AVCCommand::deserialize( de );
    status &= de.read( &m_page );
    status &= de.read( &m_fbType );
    for ( int i = 0; i < 5; ++i ) {
        byte_t next;
        de.peek( &next );
        if ( next != 0xff ) {
            ExtendedSubunitInfoPageData* infoPageData =
                new ExtendedSubunitInfoPageData();
            if ( !infoPageData->deserialize( de ) ) {
                return false;
            }
            m_infoPageDatas.push_back( infoPageData );
        } else {
            return status;
        }
    }

    return status;
}

}
