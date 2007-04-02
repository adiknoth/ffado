/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
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

#ifndef __FFADO_AMDTPSLAVESTREAMPROCESSOR__
#define __FFADO_AMDTPSLAVESTREAMPROCESSOR__

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

#define IEC61883_AM824_LABEL_MASK             0xFF000000
#define IEC61883_AM824_GET_LABEL(x)         (((x) & 0xFF000000) >> 24)
#define IEC61883_AM824_SET_LABEL(x,y)         ((x) | ((y)<<24))

#define IEC61883_AM824_LABEL_MIDI_NO_DATA     0x80
#define IEC61883_AM824_LABEL_MIDI_1X          0x81
#define IEC61883_AM824_LABEL_MIDI_2X          0x82
#define IEC61883_AM824_LABEL_MIDI_3X          0x83

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

#endif /* __FFADO_AMDTPSLAVESTREAMPROCESSOR__ */

