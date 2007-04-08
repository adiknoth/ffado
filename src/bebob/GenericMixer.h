/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __FFAD0_GENERICMIXER__
#define __FFAD0_GENERICMIXER__

#include "src/debugmodule/debugmodule.h"

#include "libosc/OscNode.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/configrom.h"

namespace BeBoB {
class AvDevice;

class GenericMixer 
    : public OSC::OscNode {

public:

    GenericMixer(
        Ieee1394Service& ieee1394service,
        AvDevice &d);
    virtual ~GenericMixer();

    virtual OSC::OscResponse processOscMessage(OSC::OscMessage *m);

protected:
    OSC::OscMessage mixerListChildren();
    OSC::OscResponse mixerGetSelectorValue(int id);
    OSC::OscResponse mixerSetSelectorValue(int id, int value);
protected:
    Ieee1394Service      &m_p1394Service;
    AvDevice             &m_device;

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace BeBoB

#endif /* __FFAD0_GENERICMIXER__ */


