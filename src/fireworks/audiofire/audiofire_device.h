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

#ifndef FIREWORKS_ECHO_DEVICE_H
#define FIREWORKS_ECHO_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "fireworks/fireworks_device.h"

namespace FireWorks {
namespace ECHO {

class AudioFire : public FireWorks::Device {

public:
    AudioFire(std::auto_ptr<ConfigRom>( configRom ));
    virtual ~AudioFire();

    virtual void showDevice();

};

} // namespace ECHO
} // namespace FireWorks

#endif
