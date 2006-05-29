/* iavdevice.h
 * Copyright (C) 2006 by Daniel Wagner
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

#ifndef IAVDEVICE_H
#define IAVDEVICE_H

#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

class ConfigRom;
class Ieee1394Service;

class IAvDevice {
public:
    virtual ~IAvDevice() {}

    virtual ConfigRom& getConfigRom() const = 0;
    virtual bool discover() = 0;
    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency ) = 0;
    virtual bool addXmlDescription( xmlNodePtr deviceNode ) = 0;
    virtual void showDevice() const = 0;
    virtual bool setId(unsigned int id) = 0;
};

#endif
