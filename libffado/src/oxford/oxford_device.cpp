/*
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

/*
 * Oxford Semiconductor OXFW970 and OXFW971 support
 */

#include "devicemanager.h"
#include "oxford_device.h"

#include "libavc/general/avc_plug.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libstreaming/amdtp-oxford/AmdtpOxfordReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"
#include "libstreaming/amdtp/AmdtpPortInfo.h"

#include <sstream>

using namespace std;

namespace Oxford {

Device::Device(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : GenericAVC::AvDevice( d, configRom)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Oxford::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );

    m_fixed_clocksource.type = FFADODevice::eCT_Internal;
    m_fixed_clocksource.valid = true;
    m_fixed_clocksource.locked = true;
    m_fixed_clocksource.id = 0;
    m_fixed_clocksource.slipping = false;
    m_fixed_clocksource.description = "Internal";

}

Device::~Device()
{
}

void
Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "This is a Oxford::Device\n");
    GenericAVC::AvDevice::showDevice();
}

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if(generic) {
        // heuristic yet that can tell us whether it's oxford based
        return false;
    } else {
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();
        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_Oxford;
    }
}

bool
Device::discover()
{
    Util::MutexLockHelper lock(m_DeviceMutex);

    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_Oxford) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Using Oxford AV/C support for unsupported device '%s %s'\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }

    // do the actual discovery
    if ( !Unit::discover() ) {
        debugError( "Could not discover unit\n" );
        return false;
    }

    // the FCA202 has only an audio subunit
    if((getAudioSubunit( 0 ) == NULL)) {
        debugError( "Unit doesn't have an Audio subunit.\n");
        return false;
    }

    return true;
}

FFADODevice *
Device::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    unsigned int vendorId = configRom->getNodeVendorId();

    switch(vendorId) {
        default: return new Device(d, configRom );
    }
}

std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    frequencies.push_back(44100);
    frequencies.push_back(48000);
    frequencies.push_back(96000);
    return frequencies;
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    r.push_back(m_fixed_clocksource);
    return r;
}

bool
Device::setActiveClockSource(ClockSource s) {
    // can't change, hence only succeed when identical
    return s.id == m_fixed_clocksource.id;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    return m_fixed_clocksource;
}

bool
Device::prepare()
{
    bool snoopMode=false;
    Util::MutexLockHelper lock(m_DeviceMutex);
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    ///////////
    // get plugs

    AVC::Plug* inputPlug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Input, 0 );
    if ( !inputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso input plug 0\n" );
        return false;
    }
    AVC::Plug* outputPlug = getPlugById( m_pcrPlugs, AVC::Plug::eAPD_Output, 0 );
    if ( !outputPlug ) {
        debugError( "setSampleRate: Could not retrieve iso output plug 0\n" );
        return false;
    }

    // get the device specific and/or global SP configuration
    Util::Configuration &config = getDeviceManager().getConfiguration();
    // base value is the config.h value
    float recv_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;
    float xmit_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;

    int xmit_max_cycles_early_transmit = AMDTP_MAX_CYCLES_TO_TRANSMIT_EARLY;
    int xmit_transfer_delay = AMDTP_TRANSMIT_TRANSFER_DELAY;
    int xmit_min_cycles_before_presentation = AMDTP_MIN_CYCLES_BEFORE_PRESENTATION;

    // we can override that globally
    config.getValueForSetting("streaming.common.recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForSetting("streaming.common.xmit_sp_dll_bw", xmit_sp_dll_bw);
    config.getValueForSetting("streaming.amdtp.xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForSetting("streaming.amdtp.xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForSetting("streaming.amdtp.xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    // or override in the device section
    uint32_t vendorid = getConfigRom().getNodeVendorId();
    uint32_t modelid = getConfigRom().getModelId();
    config.getValueForDeviceSetting(vendorid, modelid, "recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_sp_dll_bw", xmit_sp_dll_bw);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_max_cycles_early_transmit", xmit_max_cycles_early_transmit);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_transfer_delay", xmit_transfer_delay);
    config.getValueForDeviceSetting(vendorid, modelid, "xmit_min_cycles_before_presentation", xmit_min_cycles_before_presentation);

    // initialize the SP's
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing receive processor...\n");
    // create & add streamprocessors
    Streaming::StreamProcessor *p;

    if ( outputPlug->getNrOfChannels() == 0 ) {
        debugError("Receive plug has no channels\n");
        return false;
    }
    p = new Streaming::AmdtpOxfordReceiveStreamProcessor(*this,
                             outputPlug->getNrOfChannels());

    if(!p->init()) {
        debugFatal("Could not initialize receive processor!\n");
        delete p;
        return false;
    }

    if (!addPlugToProcessor(*outputPlug, p,
        Streaming::Port::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        delete p;
        return false;
    }

    if(!p->setDllBandwidth(recv_sp_dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete p;
        return false;
    }

    m_receiveProcessors.push_back(p);

    // do the transmit processor
    debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing transmit processor%s...\n",
            (snoopMode?" in snoop mode":""));
    if (snoopMode) {
        // we are snooping, so this is receive too.
        p=new Streaming::AmdtpOxfordReceiveStreamProcessor(*this,
                                  inputPlug->getNrOfChannels());
    } else {
        Streaming::AmdtpTransmitStreamProcessor * t;
        t=new Streaming::AmdtpTransmitStreamProcessor(*this,
                                inputPlug->getNrOfChannels());

        // NOTE: the oxford devices don't allow payload, otherwise they generate distorted sound
        // if this generates a compile error it means you try to compile something that won't work
        t->sendPayloadForNoDataPackets(false);

        // transmit control parameters
        t->setMaxCyclesToTransmitEarly(xmit_max_cycles_early_transmit);
        t->setTransferDelay(xmit_transfer_delay);
        t->setMinCyclesBeforePresentation(xmit_min_cycles_before_presentation);

        p=t;
    }

    if(!p->init()) {
        debugFatal("Could not initialize transmit processor %s!\n",
            (snoopMode?" in snoop mode":""));
        delete p;
        return false;
    }

    if (snoopMode) {
        if (!addPlugToProcessor(*inputPlug, p,
            Streaming::Port::E_Capture)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
        if(!p->setDllBandwidth(recv_sp_dll_bw)) {
            debugFatal("Could not set DLL bandwidth\n");
            delete p;
            return false;
        }
    } else {
        if (!addPlugToProcessor(*inputPlug, p,
            Streaming::Port::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
            return false;
        }
        if(!p->setDllBandwidth(xmit_sp_dll_bw)) {
            debugFatal("Could not set DLL bandwidth\n");
            delete p;
            return false;
        }
    }

    // we put this SP into the transmit SP vector,
    // no matter if we are in snoop mode or not
    // this allows us to find out what direction
    // a certain stream should have.
    m_transmitProcessors.push_back(p);

    return true;
}


} // Oxford
