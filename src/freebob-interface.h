/* freebob-interface.h
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

#include <ostream>

/**
 * \brief the possible sampling frequencies
 */
enum ESampleRate {
    eSF_22050Hz = 0x00,
    eSF_24000Hz = 0x01,
    eSF_32000Hz = 0x02,
    eSF_44100Hz = 0x03,
    eSF_48000Hz = 0x04,
    eSF_88200Hz = 0x0A,
    eSF_96000Hz = 0x05,
    eSF_176400Hz = 0x06,
    eSF_192000Hz = 0x07,
    eSF_AnyLow   = 0x0B,
    eSF_AnyMid   = 0x0C,
    eSF_AnyHigh  = 0x0D,
    eSF_None     = 0x0E,
    eSF_DontCare = 0x0F,
};

std::ostream& operator<<( std::ostream&, ESampleRate );

/**
 * \brief Convert from ESampleRate to an integer
 * @param freq 
 * @return 
 */
int convertESampleRate( const ESampleRate freq);
/**
 * \brief Convert from integer to ESampleRate
 * @param sampleRate 
 * @return 
 */
ESampleRate parseSampleRate( const int sampleRate );

#endif // INTERFACE_H
