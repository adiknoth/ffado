/* avc_defintions.cpp
 * Copyright (C) 2005 by Daniel Wagner
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

#include "avc_definitions.h"

std::ostream& operator<<( std::ostream& stream, ESampleRate sampleRate )
{
    char* str;
    switch ( sampleRate ) {
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
