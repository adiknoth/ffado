/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#ifndef BEBOB_ESI_QUATAFIRE_DEVICE_H
#define BEBOB_ESI_QUATAFIRE_DEVICE_H

#include "bebob/bebob_avdevice.h"

namespace BeBoB {
namespace ESI {

class QuataFireDevice : public BeBoB::Device {

public:
    QuataFireDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~QuataFireDevice();

    // override these since the quatafire does not support
    // setting the clock source (it automatically syncs to SPDIF)
    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual void showDevice();
private:
    ClockSource m_fixed_clocksource;
};

} // namespace BeBoB
} // namespace ESI

#endif
