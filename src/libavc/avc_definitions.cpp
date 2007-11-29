/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "avc_definitions.h"
#include <libiec61883/iec61883.h>

namespace AVC {

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

enum ESubunitType byteToSubunitType(byte_t s) {
    switch (s) {
    case eST_Monitor:
        return eST_Monitor;
    case eST_Audio:
        return eST_Audio;
    case eST_Printer:
        return eST_Printer;
    case eST_Disc:
        return eST_Disc;
    case eST_VCR:
        return eST_VCR;
    case eST_Tuner:
        return eST_Tuner;
    case eST_CA:
        return eST_CA;
    case eST_Camera:
        return eST_Camera;
    case eST_Panel:
        return eST_Panel;
    case eST_BulltinBoard:
        return eST_BulltinBoard;
    case eST_CameraStorage:
        return eST_CameraStorage;
    case eST_Music:
        return eST_Music;
    case eST_VendorUnique:
        return eST_VendorUnique;
    case eST_Reserved:
        return eST_Reserved;
    case eST_Extended:
        return eST_Extended;
    default:
    case eST_Unit:
        return eST_Unit;
    }
}

unsigned int fdfSfcToSampleRate(byte_t fdf) {
    switch(fdf & 0x07) {
        default:                       return 0;
        case IEC61883_FDF_SFC_32KHZ:   return 32000;
        case IEC61883_FDF_SFC_44K1HZ:  return 44100;
        case IEC61883_FDF_SFC_48KHZ:   return 48000;
        case IEC61883_FDF_SFC_88K2HZ:  return 88200;
        case IEC61883_FDF_SFC_96KHZ:   return 96000;
        case IEC61883_FDF_SFC_176K4HZ: return 176400;
        case IEC61883_FDF_SFC_192KHZ:  return 192000;
    }
}

byte_t sampleRateToFdfSfc(unsigned int rate) {
    switch(rate) {
        default:      return 0x07;
        case 32000:   return IEC61883_FDF_SFC_32KHZ;
        case 44100:   return IEC61883_FDF_SFC_44K1HZ;
        case 48000:   return IEC61883_FDF_SFC_48KHZ;
        case 88200:   return IEC61883_FDF_SFC_88K2HZ;
        case 96000:   return IEC61883_FDF_SFC_96KHZ;
        case 176400:  return IEC61883_FDF_SFC_176K4HZ;
        case 192000:  return IEC61883_FDF_SFC_192KHZ;
    }
}

}
