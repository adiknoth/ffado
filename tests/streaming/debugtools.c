#include <libfreebob/freebob.h>
#include <libraw1394/raw1394.h>
#include <libiec61883/iec61883.h>
#include "debugtools.h"
#include <stdio.h>
#include <netinet/in.h>

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

unsigned char
toAscii( unsigned char c )
{
    if ( ( c > 31 ) && ( c < 126) ) {
        return c;
    } else {
        return '.';
    }
}

/* converts a quadlet to a uchar * buffer
 * not implemented optimally, but clear
 */
void
quadlet2char( quadlet_t quadlet, unsigned char* buff )
{
    *(buff)   = (quadlet>>24)&0xFF;
    *(buff+1) = (quadlet>>16)&0xFF;
    *(buff+2) = (quadlet>> 8)&0xFF;
    *(buff+3) = (quadlet)    &0xFF;
}

void
hexDump( unsigned char *data_start, unsigned int length )
{
    unsigned int i=0;
    unsigned int byte_pos;
    unsigned int bytes_left;
    //printf("hexdump: %p %d\n",data_start,length);

    if ( length <= 0 ) {
        return;
    }
    if ( length >= 7 ) {
        for ( i = 0; i < (length-7); i += 8 ) {
            printf( "%04X: %02X %02X %02X %02X %02X %02X %02X %02X "
					"- [%c%c%c%c%c%c%c%c] - %2.5F %2.5F\n",

                    i,

                    *(data_start+i+0),
                    *(data_start+i+1),
                    *(data_start+i+2),
                    *(data_start+i+3),
                    *(data_start+i+4),
                    *(data_start+i+5),
                    *(data_start+i+6),
                    *(data_start+i+7),

                    toAscii( *(data_start+i+0) ),
                    toAscii( *(data_start+i+1) ),
                    toAscii( *(data_start+i+2) ),
                    toAscii( *(data_start+i+3) ),
                    toAscii( *(data_start+i+4) ),
                    toAscii( *(data_start+i+5) ),
                    toAscii( *(data_start+i+6) ),
                    toAscii( *(data_start+i+7) ),
					
					(*((float *)(data_start+i+0))),
					(*((float *)(data_start+i+4)))
				  );
        }
    }
    byte_pos = i;
    bytes_left = length - byte_pos;

    printf( "%04X:" ,i );
    for ( i = byte_pos; i < length; i += 1 ) {
        printf( " %02X", *(data_start+i) );
    }
    for ( i=0; i < 8-bytes_left; i+=1 ) {
        printf( "   " );
    }

    printf( " - [" );
    for ( i = byte_pos; i < length; i += 1) {
        printf( "%c", toAscii(*(data_start+i)));
    }
    for ( i = 0; i < 8-bytes_left; i += 1) {
        printf( " " );
    }

    printf( "]" );
    printf( "\n" );
}

void
hexDumpToFile( FILE* fid, unsigned char *data_start, unsigned int length )
{
    unsigned int i=0;
    unsigned int byte_pos;
    unsigned int bytes_left;
    //printf("hexdump: %p %d\n",data_start,length);

    if ( length <= 0 ) {
        return;
    }
    if ( length >= 7 ) {
        for ( i = 0; i < (length-7); i += 8 ) {
			float *f1=(float *)(data_start+i+0);
			
            fprintf(fid, "%04X: %02X %02X %02X %02X %02X %02X %02X %02X "
					"- [%c%c%c%c%c%c%c%c] - %2.5e %2.5e\n",

                    i,

                    *(data_start+i+0),
                    *(data_start+i+1),
                    *(data_start+i+2),
                    *(data_start+i+3),
                    *(data_start+i+4),
                    *(data_start+i+5),
                    *(data_start+i+6),
                    *(data_start+i+7),

                    toAscii( *(data_start+i+0) ),
                    toAscii( *(data_start+i+1) ),
                    toAscii( *(data_start+i+2) ),
                    toAscii( *(data_start+i+3) ),
                    toAscii( *(data_start+i+4) ),
                    toAscii( *(data_start+i+5) ),
                    toAscii( *(data_start+i+6) ),
                    toAscii( *(data_start+i+7) ),
                					
					(*(f1)),
					(*(f1+1))
				   );
        }
    }
    byte_pos = i;
    bytes_left = length - byte_pos;

    fprintf(fid, "%04X:" ,i );
    for ( i = byte_pos; i < length; i += 1 ) {
        fprintf(fid, " %02X", *(data_start+i) );
    }
    for ( i=0; i < 8-bytes_left; i+=1 ) {
        fprintf(fid, "   " );
    }

    fprintf(fid, " - [" );
    for ( i = byte_pos; i < length; i += 1) {
        fprintf(fid, "%c", toAscii(*(data_start+i)));
    }
    for ( i = 0; i < 8-bytes_left; i += 1) {
        fprintf(fid, " " );
    }

    fprintf(fid, "]" );
    fprintf(fid, "\n" );
}


void
hexDumpQuadlets( quadlet_t *data, unsigned int length )
{
    unsigned int i=0;

    if ( length <= 0 ) {
        return;
    }
    for (i=0;i<length;i+=1) {
            printf( "%02d %04X: %08X (%08X)"
                    "\n", i, i*4, data[i],ntohl(data[i]));
    }
}
