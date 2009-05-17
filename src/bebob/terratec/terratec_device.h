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

#ifndef BEBOB_TERRATEC_DEVICE_H
#define BEBOB_TERRATEC_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "bebob/bebob_avdevice.h"

namespace BeBoB {
namespace Terratec {

class Phase88Device : public BeBoB::AvDevice {
public:
    Phase88Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Phase88Device();

    virtual bool discover();

    virtual void showDevice();

    // override these since the phase88 does not support
    // the usual clock source selection mechanism
    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

protected:
    virtual uint16_t getConfigurationIdSyncMode();

private:
    void updateClockSources();
    ClockSource m_internal_clocksource;
    ClockSource m_spdif_clocksource;
    ClockSource m_wordclock_clocksource;
};

} // namespace Terratec
} // namespace BeBoB

#endif
