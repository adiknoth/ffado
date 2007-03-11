/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006,2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
#ifndef __FREEBOB_AMDTPSLAVESTREAMPROCESSOR__
#define __FREEBOB_AMDTPSLAVESTREAMPROCESSOR__

/**
 * This class implements IEC61883-6 / AM824 / AMDTP based streaming
 */
#include "AmdtpStreamProcessor.h"

#include "../debugmodule/debugmodule.h"

#include "cip.h"
#include <libiec61883/iec61883.h>
#include <pthread.h>

#define AMDTP_MAX_PACKET_SIZE 2048

#define IEC61883_STREAM_TYPE_MIDI   0x0D
#define IEC61883_STREAM_TYPE_SPDIF  0x00
#define IEC61883_STREAM_TYPE_MBLA   0x06

#define IEC61883_AM824_LABEL_MASK 			0xFF000000
#define IEC61883_AM824_GET_LABEL(x) 		(((x) & 0xFF000000) >> 24)
#define IEC61883_AM824_SET_LABEL(x,y) 		((x) | ((y)<<24))

#define IEC61883_AM824_LABEL_MIDI_NO_DATA 	0x80 
#define IEC61883_AM824_LABEL_MIDI_1X      	0x81 
#define IEC61883_AM824_LABEL_MIDI_2X      	0x82
#define IEC61883_AM824_LABEL_MIDI_3X      	0x83

namespace Streaming {

class AmdtpSlaveReceiveStreamProcessor;

class AmdtpSlaveTransmitStreamProcessor 
    : public AmdtpTransmitStreamProcessor
{

public:
    AmdtpSlaveTransmitStreamProcessor(int port, int framerate, int dimension);

    virtual ~AmdtpSlaveTransmitStreamProcessor();

    enum raw1394_iso_disposition 
            getPacket(unsigned char *data, unsigned int *length,
                    unsigned char *tag, unsigned char *sy,
                    int cycle, unsigned int dropped, unsigned int max_length);

};

class AmdtpSlaveReceiveStreamProcessor 
    : public AmdtpReceiveStreamProcessor
{

public:
    AmdtpSlaveReceiveStreamProcessor(int port, int framerate, int dimension);

    virtual ~AmdtpSlaveReceiveStreamProcessor();

    enum raw1394_iso_disposition putPacket(unsigned char *data, unsigned int length, 
                    unsigned char channel, unsigned char tag, unsigned char sy, 
                    unsigned int cycle, unsigned int dropped);

};


} // end of namespace Streaming

#endif /* __FREEBOB_AMDTPSLAVESTREAMPROCESSOR__ */

