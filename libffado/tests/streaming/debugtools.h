#ifndef DEBUG_TOOLS_H
#define DEBUG_TOOLS_H
#include <libfreebob/freebob.h>
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
