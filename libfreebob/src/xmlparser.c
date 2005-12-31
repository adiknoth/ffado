/* xmlparser.cpp
 * Copyright (C) 2005 Pieter Palmers, Daniel Wagner
 *
 * This file is part of FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "libfreebob/xmlparser.h"
#include "libfreebob/freebob.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#undef DEBUG
#ifdef DEBUG
    #define debugPrint( format, args... ) printf( format, ##args )
#else
    #define debugPrint( format, args... )
#endif

/* XML parsing functions
 */
freebob_stream_spec_t*
freebob_xmlparse_stream( xmlDocPtr doc, xmlNodePtr cur )
{
    freebob_stream_spec_t *stream_spec;

    // allocate memory
    stream_spec = malloc( sizeof(freebob_stream_spec_t) );
    if ( !stream_spec ) {
	fprintf( stderr, "Could not allocate memory for stream_spec" );
	return 0;
    }

    #define StreamSpecParseNode( NodeName, Member )                     \
	if ( !xmlStrcmp( cur->name, (const xmlChar*) NodeName ) ) {     \
	    xmlChar* key =                                              \
		xmlNodeListGetString( doc, cur->xmlChildrenNode, 1 );   \
	    debugPrint( "\t\t"#NodeName": %s\n", key );                 \
	    stream_spec->Member = strtol( (const char*) key,            \
					  (char**) 0, 10 );             \
	    xmlFree( key );                                             \
	}

    cur = cur->xmlChildrenNode;
    while ( cur ) {
        StreamSpecParseNode( "Position", position );
        StreamSpecParseNode( "Location", location );
        StreamSpecParseNode( "Format", format );
        StreamSpecParseNode( "Type", type );
        StreamSpecParseNode( "DestinationPort", destination_port );

        if ( !xmlStrcmp( cur->name, (const xmlChar *) "Name" ) ) {
            xmlChar* key =
                xmlNodeListGetString( doc, cur->xmlChildrenNode, 1 );
            debugPrint( "\t\tname: %s\n", key );
            strncpy( stream_spec->name, (const char*) key,
		     FREEBOB_MAX_NAME_LEN );
            xmlFree( key );
        }
        cur = cur->next;
    }
    #undef StreamSpecParseNode

    return stream_spec;
}

freebob_stream_info_t *
freebob_xmlparse_streams( xmlDocPtr doc, xmlNodePtr node )
{
    freebob_stream_info_t* stream_info;

    // allocate memory
    stream_info = malloc( sizeof(freebob_stream_info_t) );
    if ( !stream_info ) {
        fprintf( stderr, "Could not allocate memory for stream_info" );
        return 0;
    }

    // count number of child streams
    stream_info->nb_streams=0;
    xmlNodePtr cur = node->xmlChildrenNode;
    while (cur != NULL) {
        if ( !xmlStrcmp( cur->name, (const xmlChar*) "Stream" ) ) {
            stream_info->nb_streams++;
        }
        cur = cur->next;
    }

    if ( stream_info->nb_streams ) {
        // allocate memory for the stream_spec pointer array
        stream_info->streams =
            (freebob_stream_spec_t**) calloc( stream_info->nb_streams,
                                              sizeof(freebob_stream_spec_t*) );

        if ( !stream_info->streams ) {
            fprintf( stderr, "Could not allocate memory for stream specs" );
            free( stream_info );
            return 0;
        }

        // parse the child stream_specs
        cur = node->xmlChildrenNode;
        int i = 0;
        while ( cur ) {
            if ( !xmlStrcmp( cur->name, (const xmlChar*) "Stream" ) ) {
                stream_info->streams[i] =
                    freebob_xmlparse_stream( doc, cur );

                if ( !stream_info->streams[i] ) {
                    // invalid XML or memory problem, clean up.
                    while ( --i ) {
                        free( stream_info->streams[i] );
                    }
                    free( stream_info->streams );
                    stream_info->streams = 0;
                    free( stream_info );
                    return 0;
                }
                i++;
            }
            cur = cur->next;
        }
    }
    return stream_info;
}




freebob_connection_spec_t*
freebob_xmlparse_connection( xmlDocPtr doc, xmlNodePtr cur )
{
    #define ConnectionSpecParseNode( NodeName, Member )                  \
         if ( !xmlStrcmp( cur->name, (const xmlChar*) NodeName ) ) {     \
             xmlChar* key =                                              \
                 xmlNodeListGetString( doc, cur->xmlChildrenNode, 1 );   \
             debugPrint( "\t"#NodeName": %s\n",  key );                  \
             connection_spec->Member = strtol( (const char*) key,        \
					       (char**) 0, 10 );         \
             xmlFree( key );                                             \
         }

    // allocate memory
    freebob_connection_spec_t* connection_spec =
        calloc( 1, sizeof(freebob_connection_spec_t) );
    if ( !connection_spec ) {
        fprintf( stderr, "Could not allocate memory for connection_spec" );
        return 0;
    }

    cur = cur->xmlChildrenNode;
    while ( cur ) {
        ConnectionSpecParseNode( "Id", id );
        ConnectionSpecParseNode( "Node", node );
        ConnectionSpecParseNode( "Port", port );
        ConnectionSpecParseNode( "Plug", plug );
        ConnectionSpecParseNode( "Dimension", dimension );
        ConnectionSpecParseNode( "Samplerate", samplerate );
	ConnectionSpecParseNode( "IsoChannel", iso_channel );

        if ( !xmlStrcmp( cur->name, (const xmlChar*) "Streams" ) ) {
            connection_spec->stream_info
                = freebob_xmlparse_streams( doc, cur );
            if ( !connection_spec->stream_info ) {
                free( connection_spec );
                return 0;
            }
        }
        cur = cur->next;
    }
    #undef ConnectionSpecParseNode

    return connection_spec;
}

freebob_connection_info_t*
freebob_xmlparse_connectionset( xmlDocPtr doc, xmlNodePtr node )
{
    assert( node );

    // allocate memory
    freebob_connection_info_t* connection_info =
        malloc( sizeof(freebob_connection_info_t) );
    if ( !connection_info ) {
        fprintf( stderr, "Could not allocate memory for connection_info" );
        return 0;
    }

    // count number of child streams
    connection_info->nb_connections = 0;
    xmlNodePtr cur = node->xmlChildrenNode;
    while ( cur ) {
        if ( !xmlStrcmp( cur->name, (const xmlChar*) "Connection" ) ) {
            connection_info->nb_connections =
                connection_info->nb_connections + 1;
            debugPrint( "nb_connections: %d\n",
                        connection_info->nb_connections );
        }

        if ( !xmlStrcmp( cur->name, (const xmlChar*) "Direction" ) ) {
            xmlChar* key =
                xmlNodeListGetString( doc, cur->xmlChildrenNode, 1 );
            debugPrint( "\tdirection: %s\n", key );
            connection_info->direction = strtol( (const char*) key,
						 (char**) 0, 10);
            xmlFree( key );
        }
        cur = cur->next;
    }

    debugPrint( "ConnectionInfo contains %d connection_specs\n",
		connection_info->nb_connections );

    if ( connection_info->nb_connections ) {
        // allocate memory for the connection_spec pointer array
        connection_info->connections =
            (freebob_connection_spec_t**)
            calloc( connection_info->nb_connections,
                    sizeof(freebob_connection_spec_t*) );

        if ( !connection_info->connections ) {
            fprintf( stderr,
                     "Could not allocate memory for connection specs" );
            free( connection_info );
            return 0;
        }

        // parse the child stream_specs
        cur = node->xmlChildrenNode;
        int i = 0;
        while ( cur ) {
            if ( !xmlStrcmp( cur->name, (const xmlChar*) "Connection" ) ) {
                connection_info->connections[i] =
                    freebob_xmlparse_connection( doc, cur );

                if ( !connection_info->connections[i] ) {
                    // invalid XML or memory problem, clean up.
                    while ( --i ) {
                        freebob_free_connection_spec( connection_info->connections[i] );
                    }
                    free( connection_info->connections );
                    connection_info->connections = 0;
                    free( connection_info );
                    return 0;
                }
                i++;
            }
            cur = cur->next;
        }
    }

    return connection_info;
}

xmlNodePtr
freebob_xmlparse_get_connectionset_node( xmlDocPtr doc,
                                         xmlNodePtr cur,
                                         int direction )
{
    while ( cur ) {
        if ( !xmlStrcmp( cur->name, (const xmlChar*) "ConnectionSet" ) ) {
            xmlNodePtr cur2 = cur->xmlChildrenNode;
            while ( cur2 ) {
                if ( !xmlStrcmp( cur2->name, (const xmlChar*) "Direction" ) ) {
                    xmlChar* key = xmlNodeListGetString( doc,
                                                         cur2->xmlChildrenNode,
                                                         1 );
                    int tmp_direction = strtol( (const char*) key,
						(char**) 0, 10 );
                    xmlFree( key );
                    if( tmp_direction == direction) {
                        return cur;
                    }
                }
                cur2 = cur2->next;
            }
        }
        cur = cur->next;
    }
    return 0;
}

xmlNodePtr
freebob_xmlparse_get_connection_set_by_node_id( xmlDocPtr doc,
                                                xmlNodePtr cur,
                                                int nodeId )
{
    while ( cur ) {
	if ( !xmlStrcmp( cur->name, (const xmlChar*) "Device" ) ) {
	    xmlNodePtr cur2 = cur->xmlChildrenNode;
	    while ( cur2 ) {
                if ( !xmlStrcmp( cur2->name,
                                 (const xmlChar*) "NodeId" ) )
                {
                    xmlChar* key = xmlNodeListGetString( doc,
                                                         cur2->xmlChildrenNode,
                                                         1 );
                    int tmp_node_id = strtol( (const char*) key,
                                              (char**) 0, 10 );
                    xmlFree( key );

                    if ( tmp_node_id == nodeId ) {
                        xmlNodePtr cur3 = cur->xmlChildrenNode;
                        while ( cur3 ) {
                            if ( !xmlStrcmp( cur3->name,
                                             (const xmlChar*) "ConnectionSet" ) )
                            {
                                return cur3;
                            }
                            cur3 = cur3->next;
                        }
                    }
                }
                cur2 = cur2->next;
            }
	}
	cur = cur->next;
    }

    return 0;
}

freebob_connection_info_t*
freebob_xmlparse_get_connection_info( xmlDocPtr doc,
                                      int node_id,
                                      int direction )
{
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if ( !cur ) {
        fprintf( stderr, "empty document\n");
        return 0;
    }

    if ( xmlStrcmp( cur->name, (const xmlChar*) "FreeBobConnectionInfo") ) {
        fprintf( stderr,
                 "document of the wrong type, root node "
                 "!= FreeBobConnectionInfo\n" );
        return 0;
    }

    cur = cur->xmlChildrenNode;
    if ( !cur ) {
        fprintf( stderr, "Root node has no children!\n" );
        return 0;
    }

    cur = freebob_xmlparse_get_connection_set_by_node_id( doc, cur, node_id );
    if ( !cur ) {
        fprintf( stderr,
                 "Could not get find description for node id %d\n", node_id );
        return 0;
    }
    cur = freebob_xmlparse_get_connectionset_node( doc, cur, direction );
    if ( !cur ) {
        fprintf( stderr,
                 "Could not get a connection set for direction %d\n",
                 direction );
        return 0;
    }

    freebob_connection_info_t* connection_info =
        freebob_xmlparse_connectionset( doc, cur );

    return connection_info;
}
