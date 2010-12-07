/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#ifndef GENERICAVC_DEVICE_H
#define GENERICAVC_DEVICE_H

#include "libffado/ffado.h"

#include "ffadodevice.h"
#include "libutil/Configuration.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_unit.h"
#include "libavc/general/avc_subunit.h"
#include "libavc/general/avc_plug.h"

#include "debugmodule/debugmodule.h"

class ConfigRom;
class Ieee1394Service;

// from the streaming library
struct am824_settings;
struct stream_settings;

namespace GenericAVC {

class Device : public FFADODevice, public AVC::Unit {
public:
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Device();

    static bool probe( Util::Configuration&, ConfigRom& configRom, bool generic = false );
    virtual bool discover();
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));

    virtual bool serialize( std::string basePath, Util::IOSerialize& ser ) const;
    virtual bool deserialize( std::string basePath, Util::IODeserialize& deser );

    virtual void setVerboseLevel(int l);
    virtual void showDevice();

    virtual bool setSamplingFrequency( int );
    virtual bool supportsSamplingFrequency( int s );
    virtual int getSamplingFrequency( );
    virtual std::vector<int> getSupportedSamplingFrequencies();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual int getStreamCount();
    virtual struct stream_settings *getSettingsForStream(unsigned int i);

    virtual enum eStreamingState getStreamingState();

    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    // redefinition to resolve ambiguity
    virtual Ieee1394Service& get1394Service()
        { return FFADODevice::get1394Service(); };
    virtual ConfigRom& getConfigRom() const
        { return FFADODevice::getConfigRom(); };

protected:
    bool discoverGeneric();

protected:
    bool addPlugToStreamSettings(
        AVC::Plug& plug,
        struct stream_settings *s);
    size_t m_nb_stream_settings;
    struct stream_settings *m_stream_settings;
    struct am824_settings *m_amdtp_settings;

    DECLARE_DEBUG_MODULE;

private:
    ClockSource syncInfoToClockSource(const SyncInfo& si);
    std::vector<int> m_supported_frequencies_cache;
};

}

#endif //GENERICAVC_AVDEVICE_H
