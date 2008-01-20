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

#ifndef FIREWORKS_DEVICE_H
#define FIREWORKS_DEVICE_H

#include "debugmodule/debugmodule.h"

#include "genericavc/avc_avdevice.h"

#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"

#include <pthread.h>

class ConfigRom;
class Ieee1394Service;

namespace FireWorks {

class Device : public GenericAVC::AvDevice {
public:
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ) );
    virtual ~Device();
    
    static bool probe( ConfigRom& configRom );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    virtual void showDevice();
    
    virtual bool buildMixer();
    virtual bool destroyMixer();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    const EfcHardwareInfoCmd getHwInfo()
        {return m_HwInfo;};
    
    bool doEfcOverAVC(EfcCmd& c);

// Echo specific stuff
private:
    
    bool discoverUsingEFC();

    FFADODevice::ClockSource clockIdToClockSource(uint32_t clockflag);
    bool isClockValid(uint32_t id);
    uint32_t getClock();
    bool setClock(uint32_t);

    uint32_t            m_efc_version;

    EfcHardwareInfoCmd  m_HwInfo;

    bool updatePolledValues();
    pthread_mutex_t     m_polled_mutex;
    EfcPolledValuesCmd  m_Polled;

    bool                m_efc_discovery_done;

private:
    Control::Container *m_MixerContainer;

};

} // namespace FireWorks

#endif
