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

#ifndef GENERICAVC_AVDEVICE_H
#define GENERICAVC_AVDEVICE_H

#include "ffadodevice.h"
#include "genericavc/avc_vendormodel.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_unit.h"
#include "libavc/general/avc_subunit.h"
#include "libavc/general/avc_plug.h"

#include "libstreaming/amdtp/AmdtpReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"
#include "libstreaming/amdtp/AmdtpPortInfo.h"

#include "debugmodule/debugmodule.h"

class ConfigRom;
class Ieee1394Service;

namespace GenericAVC {

class AvDevice : public FFADODevice, public AVC::Unit {
public:
    AvDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~AvDevice();
    
    static bool probe( ConfigRom& configRom, bool generic = false );
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
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

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
    virtual bool addPlugToProcessor( AVC::Plug& plug, Streaming::StreamProcessor *processor,
                             Streaming::AmdtpAudioPort::E_Direction direction);
/*    bool setSamplingFrequencyPlug( AVC::Plug& plug,
                                   AVC::Plug::EPlugDirection direction,
                                   AVC::ESamplingFrequency samplingFrequency );*/

    struct VendorModelEntry m_model;

    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    typedef std::vector< Streaming::StreamProcessor * >::iterator StreamProcessorVectorIterator;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;

    DECLARE_DEBUG_MODULE;

private:
    ClockSource syncInfoToClockSource(const SyncInfo& si);
    std::vector<int> m_supported_frequencies_cache;
};

}

#endif //GENERICAVC_AVDEVICE_H
