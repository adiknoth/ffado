/*
 * Copyright (C) 2005-2008 by Jonathan Woithe
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

#ifndef RMEDEVICE_H
#define RMEDEVICE_H

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

// #include "libstreaming/rme/RmeStreamProcessor.h"

class ConfigRom;
class Ieee1394Service;

namespace Rme {

// Note: the values in this enum do not have to correspond to the unit
// version reported by the respective devices.  It just so happens that they
// currently do for the Fireface-800 and Fireface-400.
enum ERmeModel {
    RME_MODEL_NONE          = 0x0000,
    RME_MODEL_FIREFACE800   = 0x0001,
    RME_MODEL_FIREFACE400   = 0x0002,
};

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int unit_version;
    enum ERmeModel model;
    char *vendor_name;
    char *model_name;
};

class RmeDevice : public FFADODevice {
public:

    RmeDevice( DeviceManager& d,
               std::auto_ptr<ConfigRom>( configRom ));
    virtual ~RmeDevice();

    static bool probe( ConfigRom& configRom );
    static FFADODevice * createDevice( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ));
    static int getConfigurationId( );
    virtual bool discover();

    virtual void showDevice();

    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

protected:
    struct VendorModelEntry *m_model;
    enum ERmeModel m_rme_model;
};

}

#endif
