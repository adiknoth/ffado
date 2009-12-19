/*
 * Copyright (C) 2005-2009 by Pieter Palmers et. al.
 * Copyright (C) 2009 by Adrian Knoth
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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
#ifndef __FFADO_AMDTPTRANSMITSTREAMPROCESSOR_COMMON__
#define __FFADO_AMDTPTRANSMITSTREAMPROCESSOR_COMMON__

#include "config.h"

#include "debugmodule/debugmodule.h"

#include "../generic/StreamProcessor.h"
#include "../util/cip.h"

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

#define IEC61883_AM824_HAS_LABEL(x, lbl)         (((x) & 0xFF000000) == (((quadlet_t)(lbl))<<24))
#endif /* __FFADO_AMDTPTRANSMITSTREAMPROCESSOR_COMMON__ */
