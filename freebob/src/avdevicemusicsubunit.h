/* avdevicemusicsubunit.h
 * Copyright (C) 2004 by Pieter Palmers
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

#include "ieee1394service.h"
#include "avdevice.h"
#include "avdevicesubunit.h"

#ifndef AVDEVICEMUSICSUBUNIT_H
#define AVDEVICEMUSICSUBUNIT_H

class AvMusicStatusDescriptor;
class AvMusicIdentifierDescriptor;

class AvDeviceMusicSubunit : public AvDeviceSubunit {
 public:
    AvDeviceMusicSubunit(AvDevice *parent, unsigned char id);
    virtual ~AvDeviceMusicSubunit();

    void test();
    // functions to demonstrate the usage of the commands defined by the specs
    // we'll see later what to do with them exactly
    void printMusicPlugInfo();
    void printMusicPlugConfigurations();
    
 private:
	AvMusicStatusDescriptor		*cStatusDescriptor;
	AvMusicIdentifierDescriptor 	*cIdentifierDescriptor;
};

#endif
