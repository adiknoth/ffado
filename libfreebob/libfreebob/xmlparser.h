/* freebob.h
 * Copyright (C) 2005 Pieter Palmers
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

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include "libfreebob/freebob.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef __cplusplus
extern "C" {
#endif

freebob_connection_info_t* 
freebob_xmlparse_get_connection_info( xmlDocPtr doc,
				      int node_id,
                                      int direction );

freebob_supported_stream_format_info_t*
freebob_xmlparse_get_stream_formats( xmlDocPtr doc, 
				     int node_id, 
				     int direction );
#ifdef __cplusplus
}
#endif

#endif /* XMLPARSER_H */
