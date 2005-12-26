/* avdevicesubunit.h
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

#ifndef AVDEVICESUBUNIT_H
#define AVDEVICESUBUNIT_H

#include "avplug.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_generic.h"

#include <vector>

class AvDevice;

class AvDeviceSubunit {
 public:
    AvDeviceSubunit( AvDevice* avDevice, subunit_type_t type, subunit_t id );
    virtual ~AvDeviceSubunit();

    virtual bool discover() = 0;
    virtual const char* getName() = 0;

    bool addPlug( AvPlug& plug );

    subunit_t getSubunitId()
	{ return m_sbId; }
    subunit_type_t getSubunitType()
	{ return m_sbType; }

 protected:
    AvDevice*      m_avDevice;
    subunit_type_t m_sbType;
    subunit_t      m_sbId;

    AvPlugVector   m_plugs;

    DECLARE_DEBUG_MODULE;
};

typedef std::vector<AvDeviceSubunit*> AvDeviceSubunitVector;

/////////////////////////////

class AvDeviceSubunitAudio: public AvDeviceSubunit {
 public:
    AvDeviceSubunitAudio( AvDevice* avDevice, subunit_t id );
    virtual ~AvDeviceSubunitAudio();

    virtual bool discover();
    virtual const char* getName();
};

/////////////////////////////

class AvDeviceSubunitMusic: public AvDeviceSubunit {
 public:
    AvDeviceSubunitMusic( AvDevice* avDevice, subunit_t id );
    virtual ~AvDeviceSubunitMusic();

    virtual bool discover();
    virtual const char* getName();
};

#endif
