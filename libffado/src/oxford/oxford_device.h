/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef OXFORD_DEVICE_H
#define OXFORD_DEVICE_H

#include "debugmodule/debugmodule.h"

#include "genericavc/avc_avdevice.h"

#include <pthread.h>
#include "libutil/Mutex.h"

class ConfigRom;
class Ieee1394Service;

namespace Oxford {

class Device : public GenericAVC::Device {

public:
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ) );
    virtual ~Device();

    static bool probe( Util::Configuration&, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    virtual void showDevice();

    virtual std::vector<int> getSupportedSamplingFrequencies();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual bool prepare();

private:
    ClockSource m_fixed_clocksource;

};

} // namespace Oxford

#endif
