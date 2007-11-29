/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef DEBUG_TOOLS_H
#define DEBUG_TOOLS_H
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>
#include <stdio.h>
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

unsigned char toAscii( unsigned char c );

void quadlet2char( quadlet_t quadlet, unsigned char* buff );

void hexDump( unsigned char *data_start, unsigned int length );
void hexDumpToFile( FILE* fid, unsigned char *data_start, unsigned int length );
void hexDumpQuadlets( quadlet_t *data, unsigned int length );

#endif
