/* debugmodule.h
 * Copyright (C) 2004 by Pieter Palmers
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#include "debugmodule.h"

unsigned char toAscii(unsigned char c) {
	if (c>31 && c<126) {
		return c;
	} else {
		return '.';
	}

}

/* converts a quadlet to a uchar * buffer 
 * not implemented optimally, but clear
 */
void quadlet2char(quadlet_t quadlet,unsigned char* buff) {
	*(buff)=(quadlet>>24)&0xFF;
	*(buff+1)=(quadlet>>16)&0xFF;
	*(buff+2)=(quadlet>>8)&0xFF;
	*(buff+3)=(quadlet)&0xFF;
}

void hexDump(unsigned char *data_start, unsigned int length) {
	unsigned int i=0;
	unsigned int byte_pos;
	unsigned int bytes_left;
	//printf("hexdump: %p %d\n",data_start,length);
	
	if(length<=0) return;
	if(length>=7) {
		for(i=0;i<(length-7);i+=8) {
			printf("%04X: %02X %02X %02X %02X %02X %02X %02X %02X - [%c%c%c%c%c%c%c%c]\n",i,
				*(data_start+i+0),*(data_start+i+1),*(data_start+i+2),*(data_start+i+3),*(data_start+i+4),*(data_start+i+5),*(data_start+i+6),*(data_start+i+7),
				toAscii(*(data_start+i+0)),toAscii(*(data_start+i+1)),toAscii(*(data_start+i+2)),toAscii(*(data_start+i+3)),toAscii(*(data_start+i+4)),toAscii(*(data_start+i+5)),toAscii(*(data_start+i+6)),toAscii(*(data_start+i+7))
				);
		}
	}
	byte_pos=i;
	bytes_left=length-byte_pos;
	
	printf("%04X:",i);
	for (i=byte_pos;i<length;i+=1) {
		printf(" %02X",*(data_start+i));
	}
	for (i=0;i<8-bytes_left;i+=1) {
		printf("   ");
	}

	printf(" - [");
	for (i=byte_pos;i<length;i+=1) {
		printf("%c",toAscii(*(data_start+i)));
	}
	for (i=0;i<8-bytes_left;i+=1) {
		printf(" ");
	}
	
	printf("]");
	printf("\n");

}
