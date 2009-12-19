/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2009 by Jonathan Woithe
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

/* Provide access to mixer details for devices which utilise the
 * original "pre-Mark3" mixer control protocol.
 */

#ifndef MOTU_MIXERDEFS_H
#define MOTU_MIXERDEFS_H

#include "motu/motu_avdevice.h"

namespace Motu {

extern const MotuMixer Mixer_Traveler;
extern const MotuMixer Mixer_Ultralite;
extern const MotuMixer Mixer_828Mk2;
extern const MotuMixer Mixer_896HD;
extern const MotuMixer Mixer_8pre;

}

#endif
