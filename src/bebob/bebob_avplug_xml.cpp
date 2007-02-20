/* bebob_avplug_xml.cpp
 * Copyright (C) 2006,07 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

// AvPlug XML stuff

#include "bebob/bebob_avplug.h"
#include "configrom.h"

#include "libfreebobavc/ieee1394service.h"

namespace BeBoB {

bool
AvPlug::addXmlDescription( xmlNodePtr connectionSet )
{
    char* result;

    int direction;
    switch ( m_direction ) {
        case 0:
            direction = FREEBOB_PLAYBACK;
            break;
        case 1:
            direction = FREEBOB_CAPTURE;
            break;
    default:
        debugError( "plug direction invalid (%d)\n",
                    m_direction );
        return false;
    }

    asprintf( &result, "%d",  direction );
    if ( !xmlNewChild( connectionSet,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST result ) )
    {
        debugError( "Couldn't create 'Direction' node\n" );
        free( result );
        return false;
    }
    free( result );

    xmlNodePtr connection = xmlNewChild( connectionSet, 0,
                                         BAD_CAST "Connection", 0 );
    if ( !connection ) {
        debugError( "Couldn't create 'Connection' node for "
                    "direction %d\n", m_direction );
        return false;
    }

    /*
    asprintf( &result, "%08x%08x",
              ( quadlet_t )( m_configRom->getGuid() >> 32 ),
              ( quadlet_t )( m_configRom->getGuid() & 0xfffffff ) );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "GUID",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'GUID' node\n" );
        return false;
    }
    */

    asprintf( &result, "%d", m_p1394Service->getPort() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Port",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Port' node\n" );
        free( result );
        return false;
    }
    free( result );

    asprintf( &result, "%d",  m_pConfigRom->getNodeId() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Node",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Node' node\n" );
        free( result );
        return false;
    }
    free( result );


    asprintf( &result, "%d",  m_nrOfChannels );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Dimension",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Dimension' node\n" );
        free( result );
        return false;
    }
    free( result );

    asprintf( &result, "%d",  getSampleRate() );
    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "Samplerate",  BAD_CAST result ) ) {
        debugError( "Couldn't create 'Samplerate' node\n" );
        free( result );
        return false;
    }
    free( result );

    if ( !xmlNewChild( connection,  0,
                       BAD_CAST "IsoChannel", BAD_CAST "-1" ) )
    {
        debugError( "Couldn't create 'IsoChannel' node\n" );
        return false;
    }

    xmlNodePtr streams = xmlNewChild( connection,  0,
                                      BAD_CAST "Streams",  0 );
    if ( !streams ) {
        debugError( "Couldn't create 'Streams' node for "
                    "direction %d\n", m_direction );
        return false;
    }

    for ( AvPlug::ClusterInfoVector::const_iterator it = m_clusterInfos.begin();
          it != m_clusterInfos.end();
          ++it )
    {
        const AvPlug::ClusterInfo* clusterInfo = &( *it );

        AvPlug::ChannelInfoVector channelInfos = clusterInfo->m_channelInfos;
        for ( AvPlug::ChannelInfoVector::const_iterator it = channelInfos.begin();
              it != channelInfos.end();
              ++it )
        {
            const AvPlug::ChannelInfo* channelInfo = &( *it );

            xmlNodePtr stream = xmlNewChild( streams,  0,
                                             BAD_CAST "Stream",  0 );
            if ( !stream ) {
                debugError( "Coulnd't create 'Stream' node" );
                return false;
            }

            // \todo: iec61883 backend expects indexing starting from 0
            // but bebob reports it starting from 1. Decide where
            // and how to handle this
            asprintf( &result, "%d", channelInfo->m_streamPosition - 1 );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Position",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Position' node" );
                free( result );
                return false;
            }
            free( result );

            asprintf( &result, "%d", channelInfo->m_location );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Location",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Location' node" );
                free( result );
                return false;
            }
            free( result );

            asprintf( &result, "%d", clusterInfo->m_streamFormat );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Format",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Format' node" );
                free( result );
                return false;
            }
            free( result );

            asprintf( &result, "%d", clusterInfo->m_portType );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Type",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'Type' node" );
                free( result );
                return false;
            }
            free( result );


            // \todo XXX: What do to do with DestinationPort value??
            asprintf( &result, "%d", 0 );
            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "DestinationPort",  BAD_CAST result ) )
            {
                debugError( "Couldn't create 'DestinationPort' node" );
                free( result );
                return false;
            }
            free( result );

            if ( !xmlNewChild( stream,  0,
                               BAD_CAST "Name",
                               BAD_CAST channelInfo->m_name.c_str() ) )
            {
                debugError( "Couldn't create 'Name' node" );
                return false;
            }
        }
    }

    return true;
}


bool
AvPlug::addXmlDescriptionStreamFormats( xmlNodePtr streamFormatNode )
{
    int direction;
    switch ( m_direction ) {
        case 0:
            direction = FREEBOB_PLAYBACK;
            break;
        case 1:
            direction = FREEBOB_CAPTURE;
            break;
    default:
        debugError( "addXmlDescriptionStreamFormats: plug direction invalid (%d)\n",
                    m_direction );
        return false;
    }

    char* result;
    asprintf( &result, "%d",  direction );
    if ( !xmlNewChild( streamFormatNode,
                       0,
                       BAD_CAST "Direction",
                       BAD_CAST result ) )
    {
        debugError( "addXmlDescriptionStreamFormats: Could not  create 'Direction' node\n" );
        free( result );
        return false;
    }
    free( result );


    for ( FormatInfoVector::iterator it =
              m_formatInfos.begin();
          it != m_formatInfos.end();
          ++it )
    {
        AvPlug::FormatInfo formatInfo = *it;
        xmlNodePtr formatNode = xmlNewChild( streamFormatNode, 0,
                                             BAD_CAST "Format", 0 );
        if ( !formatNode ) {
            debugError( "addXmlDescriptionStreamFormats: Could not create 'Format' node\n" );
            return false;
        }

        asprintf( &result, "%d",
                  convertESamplingFrequency( static_cast<ESamplingFrequency>( formatInfo.m_samplingFrequency ) ) );
        if ( !xmlNewChild( formatNode,  0,
                           BAD_CAST "Samplerate",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'Samplerate' node\n" );
            free( result );
            return false;
        }
        free( result );

        asprintf( &result, "%d",  formatInfo.m_audioChannels );
        if ( !xmlNewChild( formatNode,  0,
                           BAD_CAST "AudioChannels",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'AudioChannels' node\n" );
            free( result );
            return false;
        }
        free( result );

        asprintf( &result, "%d",  formatInfo.m_midiChannels );
        if ( !xmlNewChild( formatNode,  0,
                           BAD_CAST "MidiChannels",  BAD_CAST result ) )
        {
            debugError( "Couldn't create 'MidiChannels' node\n" );
            free( result );
            return false;
        }
    }

    return true;
}

}
