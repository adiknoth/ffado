// xmlparser.c
//
/****************************************************************************
   libfreebobctl - Freebob Control API
   
   Copyright (C) 2005, Pieter Palmers. All rights reserved.
   <pieterpalmers@users.sourceforge.net>   
   
   Freebob = FireWire Audio for Linux... 
   http://freebob.sf.net
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "freebobctl/xmlparser.h"
#include "freebobctl/freebobctl.h"

/* XML parsing functions
 */
freebob_stream_spec_t *
freebobctl_xmlparse_stream(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *key;
	
	freebob_stream_spec_t *stream_spec;
	
	// allocate memory
	stream_spec=malloc(sizeof(freebob_stream_spec_t));
	if (!stream_spec) {
		fprintf(stderr,"Could not allocate memory for stream_spec...");
		return NULL;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Position"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\t\tposition: %s\n", key);
		    stream_spec->position=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Location"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\t\tlocation: %s\n", key);
		    stream_spec->location=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Format"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\t\tformat: %s\n", key);
		    stream_spec->format=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Type"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\t\ttype: %s\n", key);
		    stream_spec->type=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"DestinationPort"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\t\tdestination_port: %s\n", key);
		    stream_spec->destination_port=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Name"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\t\tname: %s\n", key);
		    strncpy(stream_spec->name,key,FREEBOBCTL_MAX_NAME_LEN);
		    xmlFree(key);
 	    }
  
	cur = cur->next;
	}
    return stream_spec;
}

freebob_stream_info_t *
freebobctl_xmlparse_streams (xmlDocPtr doc, xmlNodePtr node) {
	xmlNodePtr cur;
	int i=0;
	
	freebob_stream_info_t *stream_info;
	
	// allocate memory
	stream_info=malloc(sizeof(freebob_stream_info_t));
	if (!stream_info) {
		fprintf(stderr,"Could not allocate memory for stream_info...");
		return NULL;
	}
	
	// count number of child streams
	stream_info->nb_streams=0;
	cur = node->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Stream"))) {
			stream_info->nb_streams++;
		}
		cur = cur->next;
	}
	if (stream_info->nb_streams) {
		// allocate memory for the stream_spec pointer array
		stream_info->streams=(freebob_stream_spec_t**) calloc(stream_info->nb_streams,sizeof(freebob_stream_spec_t *));
		
		if (!stream_info->streams) {
			fprintf(stderr,"Could not allocate memory for stream specs...");
			free(stream_info);
			return NULL;
		}
		
		// parse the child stream_specs
		cur = node->xmlChildrenNode;
		i=0;
		while (cur != NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"Stream"))) {
				stream_info->streams[i]=freebobctl_xmlparse_stream(doc,cur);
				
				if (!stream_info->streams[i]) {
					// invalid XML or memory problem, clean up.
					while(--i) {
						free(stream_info->streams[i]);
					}
					free(stream_info->streams);
					stream_info->streams=NULL;
					free(stream_info);
					return NULL;
				}
				i++;
			}
			cur = cur->next;
		}
	}	
	
    return stream_info;
}

freebob_connection_spec_t *
freebobctl_xmlparse_connection (xmlDocPtr doc, xmlNodePtr cur) {

	xmlChar *key;
	freebob_connection_spec_t *connection_spec;
	
	// allocate memory
	connection_spec=calloc(1,sizeof(freebob_connection_spec_t));
	if (!connection_spec) {
		fprintf(stderr,"Could not allocate memory for connection_spec...");
		return NULL;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Id"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\tid: %s\n", key);
		    connection_spec->id=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Node"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\tnode: %s\n", key);
		    connection_spec->node=strtol(key, (char **)NULL, 10);
		    
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Port"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\tport: %s\n", key);
		    connection_spec->port=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Plug"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\tplug: %s\n", key);
		    connection_spec->plug=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Dimension"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\tdimension: %s\n", key);
		    connection_spec->dimension=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Samplerate"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("\tsamplerate: %s\n", key);
		    connection_spec->samplerate=strtol(key, (char **)NULL, 10);
		    xmlFree(key);
 	    }

	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"Streams"))) {
		connection_spec->stream_info=freebobctl_xmlparse_streams(doc,cur);
		if(!connection_spec->stream_info) {
			free(connection_spec);
			return NULL;
		}
 	    }	    
	cur = cur->next;
	}
    return connection_spec;
}

freebob_connection_info_t *
freebobctl_xmlparse_connectionset (xmlDocPtr doc, xmlNodePtr node) {
	xmlChar *key;
	xmlNodePtr cur;
	int i=0;
	
	freebob_connection_info_t *connection_info;
	
	// allocate memory
	connection_info=malloc(sizeof(freebob_connection_info_t));
	if (!connection_info) {
		fprintf(stderr,"Could not allocate memory for connection_info...");
		return NULL;
	}
	
	// count number of child streams
	connection_info->nb_connections=0;
	cur = node->xmlChildrenNode;
	while (cur != NULL) {
		printf("%s\n",cur->name);
		
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Connection"))) {
			connection_info->nb_connections=connection_info->nb_connections+1;
			printf("nb_connections: %d\n", connection_info->nb_connections);
		}
		
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Direction"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			printf("\tdirection: %s\n", key);
			connection_info->direction=strtol(key, (char **)NULL, 10);
			xmlFree(key);
		}
		cur = cur->next;
	}
	
	fprintf(stderr,"ConnectionInfo contains %d connection_specs\n",connection_info->nb_connections);
	
	if (connection_info->nb_connections) {
		// allocate memory for the connection_spec pointer array
		connection_info->connections=(freebob_connection_spec_t**) calloc(connection_info->nb_connections,sizeof(freebob_connection_spec_t *));
		
		if (!connection_info->connections) {
			fprintf(stderr,"Could not allocate memory for connection specs...");
			free(connection_info);
			return NULL;
		}
		
		// parse the child stream_specs
		cur = node->xmlChildrenNode;
		i=0;
		while (cur != NULL) {
		
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"Connection"))) {
				connection_info->connections[i]=freebobctl_xmlparse_connection(doc,cur);
				
				if (!connection_info->connections[i]) {
					// invalid XML or memory problem, clean up.
					while(--i) {
						freebobctl_free_connection_spec(connection_info->connections[i]);
					}
					free(connection_info->connections);
					connection_info->connections=NULL;
					free(connection_info);
					return NULL;
				}
				i++;
			}
			cur = cur->next;
		}
	}	
	
    return connection_info;
}

xmlNodePtr 
freebobctl_xmlparse_get_connectionset_node(xmlDocPtr doc, xmlNodePtr cur, int direction) {

	xmlNodePtr cur2;
	int tmp_direction;
	xmlChar *key;

	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"ConnectionSet"))) {
			cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if ((!xmlStrcmp(cur2->name, (const xmlChar *)"Direction"))) {
					key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					tmp_direction=strtol(key, (char **)NULL, 10);
					xmlFree(key);
					if(tmp_direction==direction) return cur;
				}
				cur2 = cur2->next;
			}
		}
		cur = cur->next;
	}

	return NULL;
}

freebob_connection_info_t * freebobctl_xmlparse_get_connection_info(xmlDocPtr doc, int direction) {
	
	xmlNodePtr cur;
	freebob_connection_info_t *connection_info;

	cur = xmlDocGetRootElement(doc);
	
	if (cur == NULL) {
		fprintf(stderr,"empty document\n");
		return NULL;
	}
	
	if (xmlStrcmp(cur->name, (const xmlChar *) "FreeBobConnectionInfo")) {
		fprintf(stderr,"document of the wrong type, root node != FreeBobConnectionInfo\n");
		return NULL;
	}
	
	cur = cur->xmlChildrenNode;
	
	cur = freebobctl_xmlparse_get_connectionset_node(doc, cur, direction);
	
	connection_info=freebobctl_xmlparse_connectionset (doc, cur);
	
	freebobctl_print_connection_info(connection_info);

	return connection_info;

}


freebob_connection_info_t * freebobctl_xmlparse_get_connection_info_from_file(char *filename, int direction) {
	xmlDocPtr doc;
	freebob_connection_info_t *connection_info;

	doc = xmlParseFile(filename);
	
	if (doc == NULL ) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return NULL;
	}
	
	connection_info=freebobctl_xmlparse_get_connection_info(doc, direction);
	
	xmlFreeDoc(doc);
	return connection_info;

}

freebob_connection_info_t * freebobctl_xmlparse_get_connection_info_from_mem(char *buffer, int direction) {
	xmlDocPtr doc;
	freebob_connection_info_t *connection_info;
	
	// convert the char * buffer into an xmlchar buffer
	xmlChar * xmlCharBuffer=xmlCharStrdup(buffer);
	
	doc=xmlParseDoc (xmlCharBuffer);
	
	if (doc == NULL ) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return NULL;
	}
	
	connection_info=freebobctl_xmlparse_get_connection_info(doc, direction);
	
	xmlFreeDoc(doc);
	xmlFree(xmlCharBuffer);
	return connection_info;

}

void
freebobctl_xmlparse_file(char *filename) {

	xmlDocPtr doc;
	xmlNodePtr cur;
	freebob_connection_info_t *connection_info;

	doc = xmlParseFile(filename);
	
	if (doc == NULL ) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return;
	}
	
	cur = xmlDocGetRootElement(doc);
	
	if (cur == NULL) {
		fprintf(stderr,"empty document\n");
		xmlFreeDoc(doc);
		return;
	}
	
	if (xmlStrcmp(cur->name, (const xmlChar *) "FreeBobConnectionInfo")) {
		fprintf(stderr,"document of the wrong type, root node != FreeBobConnectionInfo\n");
		xmlFreeDoc(doc);
		return;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"ConnectionSet"))){
			connection_info=freebobctl_xmlparse_connectionset (doc, cur);
			freebobctl_print_connection_info(connection_info);
			freebobctl_free_connection_info(connection_info);
		}
		 
	cur = cur->next;
	}
	
	xmlFreeDoc(doc);
	return;
}

// end of xmlparser.c