// xmlparser.h
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

#ifndef __FREEBOBCTL_XMLPARSER_H
#define __FREEBOBCTL_XMLPARSER_H

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "freebobctl/freebobctl.h"

void freebobctl_xmlparse_file(char *filename);

freebob_connection_info_t * freebobctl_xmlparse_get_connection_info_from_file(char *filename, int direction);
freebob_connection_info_t * freebobctl_xmlparse_get_connection_info_from_mem(char *buffer, int direction);

freebob_connection_info_t * freebobctl_xmlparse_get_connection_info(xmlDocPtr doc, int direction);


#endif // __FREEBOBCTL_XMLPARSER_H

// end of xmlparser.h

