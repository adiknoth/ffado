/*
 * Copyright (C) 2009-2010 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#ifndef _FFADO_AM824_H
#define _FFADO_AM824_H

#include "libstreaming/util/cip.h"
#include "libffado/ffado.h"

#define IEC61883_FDF_SFC_32KHZ   0x00
#define IEC61883_FDF_SFC_44K1HZ  0x01
#define IEC61883_FDF_SFC_48KHZ   0x02
#define IEC61883_FDF_SFC_88K2HZ  0x03
#define IEC61883_FDF_SFC_96KHZ   0x04
#define IEC61883_FDF_SFC_176K4HZ 0x05
#define IEC61883_FDF_SFC_192KHZ  0x06

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

#define IEC61883_AM824_HAS_LABEL(x, lbl)         (((x) & 0xFF000000) == (((quadlet_t)(lbl))<<24))


struct am824_settings {
    unsigned int samplerate;

#if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
    // controls whether nodata payload is sent
    uint8_t send_nodata_payload;
#endif

    unsigned int transfer_delay;

    // === cached values calculated by init() ===

    // length of the nominal transmit packet in bytes
    uint16_t packet_length;
    uint8_t syt_interval;
    uint8_t fdf;

    // packet header to use for next xmit packet
    //  * dbc should be updated after each packet
    //  * syt and fdf can be updated on each packet
    struct iec61883_packet header;
};

int am824_init(struct am824_settings *settings, int node_id, 
               unsigned int nb_channels, unsigned int samplerate);

enum conn_callback_status
receive_am824_packet_data(struct stream_info *stream, uint32_t tsp, uint32_t *data, int* len);

enum conn_callback_status
transmit_am824_packet_data(struct stream_info *stream, uint32_t tsp, uint32_t *data, int* len);



#endif
