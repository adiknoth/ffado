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

#ifndef BEBOB_ONYX_MIXER_DEVICE_H
#define BEBOB_ONYX_MIXER_DEVICE_H

#include "bebob/bebob_avdevice.h"

namespace BeBoB {
namespace Mackie {

class OnyxMixerDevice : public BeBoB::AvDevice {

public:
    OnyxMixerDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~OnyxMixerDevice();

    // override these since the mackie does not report
    // any the clock sources (it only supports internal clocking)
    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual void showDevice();
private:
    ClockSource m_fixed_clocksource;
};

} // namespace Mackie
} // namespace BeBoB

#endif
