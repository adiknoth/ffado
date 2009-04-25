/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "bounce/bounce_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libutil/Configuration.h"

#include "debugmodule/debugmodule.h"

#include "devicemanager.h"

#include <iostream>
#include <sstream>
#include <stdint.h>

#include <string>
#include "libutil/ByteSwap.h"

namespace Bounce {

Device::Device( DeviceManager& d, std::auto_ptr< ConfigRom >( configRom ) )
    : FFADODevice( d, configRom )
    , m_samplerate (44100)
    , m_Notifier ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Bounce::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

Device::~Device()
{

}

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if(generic) {
        return false;
    } else {
        // check if device is in supported devices list
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();

        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_Bounce;
    }
    return false;
}

FFADODevice *
Device::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new Device(d, configRom );
}

bool
Device::discover()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "discovering Device (NodeID %d)\n",
                 getNodeId() );

    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_Bounce) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Using generic Bounce support for unsupported device '%s %s'\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }
    return true;
}

int Device::getSamplingFrequency( ) {
    return m_samplerate;
}

bool Device::setSamplingFrequency( int s ) {
    if (s) {
        m_samplerate=s;
        return true;
    } else return false;
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    return r;
}

bool
Device::setActiveClockSource(ClockSource s) {
    return false;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    ClockSource s;
    return s;
}

std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    return frequencies;
}


bool
Device::lock() {

    return true;
}


bool
Device::unlock() {

    return true;
}

void
Device::showDevice()
{

    debugOutput(DEBUG_LEVEL_NORMAL, "\nI am the bouncedevice, the bouncedevice I am...\n" );
    debugOutput(DEBUG_LEVEL_NORMAL, "Vendor            :  %s\n", getConfigRom().getVendorName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Model             :  %s\n", getConfigRom().getModelName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Node              :  %d\n", getNodeId());
    debugOutput(DEBUG_LEVEL_NORMAL, "GUID              :  0x%016llX\n", getConfigRom().getGuid());
    debugOutput(DEBUG_LEVEL_NORMAL, "\n" );
}

bool
Device::addPortsToProcessor(
    Streaming::StreamProcessor *processor,
    Streaming::Port::E_Direction direction) {

    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to processor\n");

    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defauling to 'dev?'\n");
    }

    int i=0;
    for (i=0;i<BOUNCE_NB_AUDIO_CHANNELS;i++) {
        char *buff;
        asprintf(&buff,"%s%s_Port%d",id.c_str(),direction==Streaming::Port::E_Playback?"p":"c",i);

        Streaming::Port *p=NULL;
        p=new Streaming::AmdtpAudioPort(
                *processor,
                buff,
                direction,
                // \todo: streaming backend expects indexing starting from 0
                // but bebob reports it starting from 1. Decide where
                // and how to handle this (pp: here)
                i,
                0,
                Streaming::AmdtpPortInfo::E_MBLA
        );

        if (!p) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
        }
        free(buff);
    }

    for (i=0;i<BOUNCE_NB_MIDI_CHANNELS;i++) {
        char *buff;
        asprintf(&buff,"%s_Midi%s%d",id.c_str(),direction==Streaming::Port::E_Playback?"Out":"In",i);

        Streaming::Port *p=NULL;
        p=new Streaming::AmdtpMidiPort(
                *processor,
                buff,
                direction,
                // \todo: streaming backend expects indexing starting from 0
                // but bebob reports it starting from 1. Decide where
                // and how to handle this (pp: here)
                BOUNCE_NB_AUDIO_CHANNELS,
                i,
                Streaming::AmdtpPortInfo::E_Midi
        );

        if (!p) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
        }
        free(buff);
     }

    return true;
}

bool
Device::prepare() {
    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing Device...\n" );

    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    // create & add streamprocessors
    Streaming::StreamProcessor *p;

    p=new Streaming::AmdtpReceiveStreamProcessor(*this,
                             BOUNCE_NB_AUDIO_CHANNELS+(BOUNCE_NB_MIDI_CHANNELS?1:0));

    if(!p->init()) {
        debugFatal("Could not initialize receive processor!\n");
        delete p;
        return false;
    }

    if (!addPortsToProcessor(p,
            Streaming::Port::E_Capture)) {
        debugFatal("Could not add plug to processor!\n");
        delete p;
        return false;
    }

    m_receiveProcessors.push_back(p);

    // do the transmit processor
    if (snoopMode) {
        // we are snooping, so this is receive too.
        p=new Streaming::AmdtpReceiveStreamProcessor(*this,
                                BOUNCE_NB_AUDIO_CHANNELS+(BOUNCE_NB_MIDI_CHANNELS?1:0));
    } else {
        p=new Streaming::AmdtpTransmitStreamProcessor(*this,
                                BOUNCE_NB_AUDIO_CHANNELS+(BOUNCE_NB_MIDI_CHANNELS?1:0));
    }

    if(!p->init()) {
        debugFatal("Could not initialize transmit processor %s!\n",
            (snoopMode?" in snoop mode":""));
        delete p;
        return false;
    }

    if (snoopMode) {
        if (!addPortsToProcessor(p,
            Streaming::Port::E_Capture)) {
            debugFatal("Could not add plug to processor!\n");
            delete p;
            return false;
        }
        m_receiveProcessors.push_back(p);
    } else {
        if (!addPortsToProcessor(p,
            Streaming::Port::E_Playback)) {
            debugFatal("Could not add plug to processor!\n");
            delete p;
            return false;
        }
        m_transmitProcessors.push_back(p);
    }

    return true;
}

int
Device::getStreamCount() {
    return m_receiveProcessors.size() + m_transmitProcessors.size();
}

Streaming::StreamProcessor *
Device::getStreamProcessorByIndex(int i) {
    if (i<(int)m_receiveProcessors.size()) {
        return m_receiveProcessors.at(i);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        return m_transmitProcessors.at(i-m_receiveProcessors.size());
    }

    return NULL;
}

bool
Device::startStreamByIndex(int i) {
    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);

        // allocate ISO channel
        int isochannel=allocateIsoChannel(p->getMaxPacketSize());
        if(isochannel<0) {
            debugError("Could not allocate iso channel for SP %d\n",i);
            return false;
        }
        p->setChannel(isochannel);

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readReg(BOUNCE_REGISTER_TX_ISOCHANNEL, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register\n", n);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }
        if(reg_isoch != 0xFFFFFFFFUL) {
            debugError("ISO_CHANNEL register != 0xFFFFFFFF (=0x%08X)\n", reg_isoch);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=isochannel;
        if(!writeReg(BOUNCE_REGISTER_TX_ISOCHANNEL, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register\n");
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        // allocate ISO channel
        int isochannel=allocateIsoChannel(p->getMaxPacketSize());
        if(isochannel<0) {
            debugError("Could not allocate iso channel for SP %d\n",i);
            return false;
        }
        p->setChannel(isochannel);

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readReg(BOUNCE_REGISTER_RX_ISOCHANNEL, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register\n");
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }
        if(reg_isoch != 0xFFFFFFFFUL) {
            debugError("ISO_CHANNEL register != 0xFFFFFFFF (=0x%08X)\n", reg_isoch);
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=isochannel;
        if(!writeReg(BOUNCE_REGISTER_RX_ISOCHANNEL, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register\n");
            p->setChannel(-1);
            deallocateIsoChannel(isochannel);
            return false;
        }

        return true;
    }

    debugError("SP index %d out of range!\n",i);

    return false;
}

bool
Device::stopStreamByIndex(int i) {
    if (i<(int)m_receiveProcessors.size()) {
        int n=i;
        Streaming::StreamProcessor *p=m_receiveProcessors.at(n);
        unsigned int isochannel=p->getChannel();

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readReg(BOUNCE_REGISTER_TX_ISOCHANNEL, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register\n");
            return false;
        }
        if(reg_isoch != isochannel) {
            debugError("ISO_CHANNEL register != 0x%08X (=0x%08X)\n", isochannel, reg_isoch);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=0xFFFFFFFFUL;
        if(!writeReg(BOUNCE_REGISTER_TX_ISOCHANNEL, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register\n" );
            return false;
        }

        // deallocate ISO channel
        if(!deallocateIsoChannel(isochannel)) {
            debugError("Could not deallocate iso channel for SP\n",i);
            return false;
        }

        p->setChannel(-1);
        return true;

    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        int n=i-m_receiveProcessors.size();
        Streaming::StreamProcessor *p=m_transmitProcessors.at(n);

        unsigned int isochannel=p->getChannel();

        fb_quadlet_t reg_isoch;
        // check value of ISO_CHANNEL register
        if(!readReg(BOUNCE_REGISTER_RX_ISOCHANNEL, &reg_isoch)) {
            debugError("Could not read ISO_CHANNEL register\n");
            return false;
        }
        if(reg_isoch != isochannel) {
            debugError("ISO_CHANNEL register != 0x%08X (=0x%08X)\n", isochannel, reg_isoch);
            return false;
        }

        // write value of ISO_CHANNEL register
        reg_isoch=0xFFFFFFFFUL;
        if(!writeReg(BOUNCE_REGISTER_RX_ISOCHANNEL, reg_isoch)) {
            debugError("Could not write ISO_CHANNEL register\n");
            return false;
        }

        // deallocate ISO channel
        if(!deallocateIsoChannel(isochannel)) {
            debugError("Could not deallocate iso channel for SP (%d)\n",i);
            return false;
        }

        p->setChannel(-1);
        return true;
    }

    debugError("SP index %d out of range!\n",i);
    return false;
}

// helper functions

// allocate ISO resources for the SP's
int Device::allocateIsoChannel(unsigned int packet_size) {
    unsigned int bandwidth=8+packet_size;

    int ch=get1394Service().allocateIsoChannelGeneric(bandwidth);

    debugOutput(DEBUG_LEVEL_VERBOSE, "allocated channel %d, bandwidth %d\n",
        ch, bandwidth);

    return ch;
}
// deallocate ISO resources
bool Device::deallocateIsoChannel(int channel) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "freeing channel %d\n",channel);
    return get1394Service().freeIsoChannel(channel);
}

// I/O functions

bool
Device::readReg(fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08llX\n", offset);

    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().read_quadlet( nodeId, addr, result ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012X\n", nodeId, addr);
        return false;
    }
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Read result: 0x%08X\n", *result);

    return true;
}

bool
Device::writeReg(fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08llX, data: 0x%08X\n",
        offset, data);

    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().write_quadlet( nodeId, addr, data ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012X\n", nodeId, addr);
        return false;
    }
    return true;
}

bool
Device::readRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08llX, length %u\n",
        offset, length);

    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().read( nodeId, addr, length, data ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012llX\n", nodeId, addr);
        return false;
    }
    return true;
}

bool
Device::writeRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08llX, length: %u\n",
        offset, length);

    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }

    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=getNodeId() | 0xFFC0;

    if(!get1394Service().write( nodeId, addr, length, data ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012llX\n", nodeId, addr);
        return false;
    }
    return true;
}


} // namespace
