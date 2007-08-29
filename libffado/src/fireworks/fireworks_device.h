/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef FIREWORKS_DEVICE_H
#define FIREWORKS_DEVICE_H

#include "debugmodule/debugmodule.h"

#include "genericavc/avc_avdevice.h"

class ConfigRom;
class Ieee1394Service;

namespace FireWorks {

class Device : public GenericAVC::AvDevice {
public:
    Device( Ieee1394Service& ieee1394Service,
              std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Device();
    
    static bool probe( ConfigRom& configRom );
    static FFADODevice * createDevice( Ieee1394Service& ieee1394Service,
                                        std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    virtual void showDevice();

};

} // namespace FireWorks

#endif
