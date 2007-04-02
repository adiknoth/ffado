/* avc_defintions.cpp
 * Copyright (C) 2005 by Daniel Wagner
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

#include "avc_definitions.h"


int
convertESamplingFrequency(ESamplingFrequency freq)
{
    int value = 0;

    switch ( freq ) {
    case eSF_22050Hz:
        value = 22050;
        break;
    case eSF_24000Hz:
        value = 24000;
        break;
    case eSF_32000Hz:
        value = 32000;
        break;
    case eSF_44100Hz:
        value = 44100;
        break;
    case eSF_48000Hz:
        value = 48000;
        break;
    case eSF_88200Hz:
        value = 88200;
        break;
    case eSF_96000Hz:
        value = 96000;
        break;
    case eSF_176400Hz:
        value = 176400;
        break;
    case eSF_192000Hz:
        value = 192000;
        break;
    case eSF_AnyLow:
        value = 48000;
        break;
    case eSF_AnyMid:
        value = 96000;
        break;
    case eSF_AnyHigh:
        value = 192000;
        break;
    default:
        value = 0;
    }


    return value;
}

ESamplingFrequency
parseSampleRate( int sampleRate )
{
    ESamplingFrequency efreq;
    switch ( sampleRate ) {
    case 22050:
        efreq = eSF_22050Hz;
        break;
    case 24000:
        efreq = eSF_24000Hz;
        break;
    case 32000:
        efreq = eSF_32000Hz;
        break;
    case 44100:
        efreq = eSF_44100Hz;
        break;
    case 48000:
        efreq = eSF_48000Hz;
        break;
    case 88200:
        efreq = eSF_88200Hz;
        break;
    case 96000:
        efreq = eSF_96000Hz;
        break;
    case 176400:
        efreq = eSF_176400Hz;
        break;
    case 192000:
        efreq = eSF_192000Hz;
        break;
    default:
        efreq = eSF_DontCare;
    }

    return efreq;
}

std::ostream& operator<<( std::ostream& stream, ESamplingFrequency samplingFrequency )
{
    char* str;
    switch ( samplingFrequency ) {
    case eSF_22050Hz:
        str = "22050";
        break;
    case eSF_24000Hz:
        str = "24000";
        break;
    case eSF_32000Hz:
        str = "32000";
        break;
    case eSF_44100Hz:
        str = "44100";
        break;
    case eSF_48000Hz:
        str = "48000";
        break;
    case eSF_88200Hz:
        str = "88200";
        break;
    case eSF_96000Hz:
        str = "96000";
        break;
    case eSF_176400Hz:
        str = "176400";
        break;
    case eSF_192000Hz:
        str = "192000";
        break;
    case eSF_DontCare:
    default:
        str = "unknown";
    }
    return stream << str;
};
