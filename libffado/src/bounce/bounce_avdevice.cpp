/* bounce_avdevice.cpp
 * Copyright (C) 2006 by Pieter Palmers
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#include "bounce/bounce_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_plug_info.h"
#include "libavc/avc_extended_plug_info.h"
#include "libavc/avc_subunit_info.h"
#include "libavc/avc_extended_stream_format.h"
#include "libavc/avc_serialize.h"
#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>
#include <stdint.h>

#include <string>
#include <netinet/in.h>

namespace Bounce {

// to define the supported devices
static VendorModelEntry supportedDeviceList[] =
{
    {0x0B0001LU, 0x0B0001LU, 0x0B0001LU, "FreeBoB", "Bounce"},
};

BounceDevice::BounceDevice( std::auto_ptr< ConfigRom >( configRom ),
                            Ieee1394Service& ieee1394service,
                            int nodeId )
    : IAvDevice( configRom, ieee1394service, nodeId )
    , m_samplerate (44100)
    , m_model( NULL )
    , m_Notifier ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Bounce::BounceDevice (NodeID %d)\n",
                 nodeId );
    addOption(Util::OptionContainer::Option("snoopMode",false));
}

BounceDevice::~BounceDevice()
{

}

bool
BounceDevice::probe( ConfigRom& configRom )
{

    debugOutput( DEBUG_LEVEL_VERBOSE, "probing BounceDevice\n");
//     unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int modelId = configRom.getModelId();
    unsigned int unitSpecifierId = configRom.getUnitSpecifierId();
    debugOutput( DEBUG_LEVEL_VERBOSE, "modelId = 0x%08X, specid = 0x%08X\n", modelId, unitSpecifierId);

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( 
//             ( supportedDeviceList[i].vendor_id == vendorId )
             ( supportedDeviceList[i].model_id == modelId )
             && ( supportedDeviceList[i].unit_specifier_id == unitSpecifierId ) 
           )
        {
            return true;
        }
    }

    return false;
}

bool
BounceDevice::discover()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "discovering BounceDevice (NodeID %d)\n",
                 m_nodeId );
                 
//     unsigned int vendorId = m_pConfigRom->getNodeVendorId();
    unsigned int modelId = m_pConfigRom->getModelId();
    unsigned int unitSpecifierId = m_pConfigRom->getUnitSpecifierId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( //( supportedDeviceList[i].vendor_id == vendorId )
             ( supportedDeviceList[i].model_id == modelId ) 
             && ( supportedDeviceList[i].unit_specifier_id == unitSpecifierId ) 
           )
        {
            m_model = &(supportedDeviceList[i]);
        }
    }

    if (m_model != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                m_model->vendor_name, m_model->model_name);
        return true;
    }
    return false;
}

int BounceDevice::getSamplingFrequency( ) {
    return m_samplerate;
}

bool BounceDevice::setSamplingFrequency( ESamplingFrequency samplingFrequency ) {
    int retval=convertESamplingFrequency( samplingFrequency );
    if (retval) {
        m_samplerate=retval;
        return true;
    } else return false;
}

bool
BounceDevice::lock() {

    return true;
}


bool
BounceDevice::unlock() {

    return true;
}

void
BounceDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "\nI am the bouncedevice, the bouncedevice I am...\n" );
    debugOutput(DEBUG_LEVEL_NORMAL, "Vendor            :  %s\n", m_pConfigRom->getVendorName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Model             :  %s\n", m_pConfigRom->getModelName().c_str());
    debugOutput(DEBUG_LEVEL_NORMAL, "Vendor Name       :  %s\n", m_model->vendor_name);
    debugOutput(DEBUG_LEVEL_NORMAL, "Model Name        :  %s\n", m_model->model_name);
    debugOutput(DEBUG_LEVEL_NORMAL, "Node              :  %d\n", m_nodeId);
    debugOutput(DEBUG_LEVEL_NORMAL, "GUID              :  0x%016llX\n", m_pConfigRom->getGuid());
    debugOutput(DEBUG_LEVEL_NORMAL, "\n" );
}

bool
BounceDevice::addPortsToProcessor(
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
        } else {

            if (!processor->addPort(p)) {
                debugWarning("Could not register port with stream processor\n");
                free(buff);
                return false;
            } else {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
            }
        }
        free(buff);
    }
    
    for (i=0;i<BOUNCE_NB_MIDI_CHANNELS;i++) {
        char *buff;
        asprintf(&buff,"%s_Midi%s%d",id.c_str(),direction==Streaming::Port::E_Playback?"Out":"In",i);

        Streaming::Port *p=NULL;
        p=new Streaming::AmdtpMidiPort(
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
        } else {

            if (!processor->addPort(p)) {
                debugWarning("Could not register port with stream processor\n");
                free(buff);
                return false;
            } else {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
            }
        }
        free(buff);
     }

	return true;
}

bool
BounceDevice::prepare() {
    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing BounceDevice...\n" );
    
    bool snoopMode=false;
    if(!getOption("snoopMode", snoopMode)) {
        debugWarning("Could not retrieve snoopMode parameter, defauling to false\n");
    }

    // create & add streamprocessors
    Streaming::StreamProcessor *p;
    
    p=new Streaming::AmdtpReceiveStreamProcessor(
                             m_p1394Service->getPort(),
                             m_samplerate,
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
        p=new Streaming::AmdtpReceiveStreamProcessor(
                                m_p1394Service->getPort(),
                                m_samplerate,
                                BOUNCE_NB_AUDIO_CHANNELS+(BOUNCE_NB_MIDI_CHANNELS?1:0));
    } else {
        p=new Streaming::AmdtpTransmitStreamProcessor(
                                m_p1394Service->getPort(),
                                m_samplerate,
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
BounceDevice::getStreamCount() {
    return m_receiveProcessors.size() + m_transmitProcessors.size();
}

Streaming::StreamProcessor *
BounceDevice::getStreamProcessorByIndex(int i) {
    if (i<(int)m_receiveProcessors.size()) {
        return m_receiveProcessors.at(i);
    } else if (i<(int)m_receiveProcessors.size() + (int)m_transmitProcessors.size()) {
        return m_transmitProcessors.at(i-m_receiveProcessors.size());
    }

    return NULL;
}

bool
BounceDevice::startStreamByIndex(int i) {
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
BounceDevice::stopStreamByIndex(int i) {
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
            debugError("Could not write ISO_CHANNEL register" );
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
int BounceDevice::allocateIsoChannel(unsigned int packet_size) {
    unsigned int bandwidth=8+packet_size;
    
    int ch=m_p1394Service->allocateIsoChannelGeneric(bandwidth);
        
    debugOutput(DEBUG_LEVEL_VERBOSE, "allocated channel %d, bandwidth %d\n",
        ch, bandwidth);
    
    return ch;
}
// deallocate ISO resources
bool BounceDevice::deallocateIsoChannel(int channel) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "freeing channel %d\n",channel);
    return m_p1394Service->freeIsoChannel(channel);
}

// I/O functions

bool
BounceDevice::readReg(fb_nodeaddr_t offset, fb_quadlet_t *result) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08llX\n", offset);
    
    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }
    
    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=m_nodeId | 0xFFC0;
    
    if(!m_p1394Service->read_quadlet( nodeId, addr, result ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012X\n", nodeId, addr);
        return false;
    }
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Read result: 0x%08X\n", *result);
   
    return true;
}

bool
BounceDevice::writeReg(fb_nodeaddr_t offset, fb_quadlet_t data) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08llX, data: 0x%08X\n",  
        offset, data);
    
    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }
    
    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=m_nodeId | 0xFFC0;
    
    if(!m_p1394Service->write_quadlet( nodeId, addr, data ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012X\n", nodeId, addr);
        return false;
    }
    return true;
}

bool 
BounceDevice::readRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Reading base register offset 0x%08llX, length %u\n", 
        offset, length);
    
    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }
    
    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=m_nodeId | 0xFFC0;
    
    if(!m_p1394Service->read( nodeId, addr, length, data ) ) {
        debugError("Could not read from node 0x%04X addr 0x%012llX\n", nodeId, addr);
        return false;
    }
    return true;
}

bool 
BounceDevice::writeRegBlock(fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Writing base register offset 0x%08llX, length: %u\n",  
        offset, length);
    
    if(offset >= BOUNCE_INVALID_OFFSET) {
        debugError("invalid offset: 0x%016llX\n", offset);
        return false;
    }
    
    fb_nodeaddr_t addr=BOUNCE_REGISTER_BASE + offset;
    fb_nodeid_t nodeId=m_nodeId | 0xFFC0;

    if(!m_p1394Service->write( nodeId, addr, length, data ) ) {
        debugError("Could not write to node 0x%04X addr 0x%012llX\n", nodeId, addr);
        return false;
    }
    return true;
}


} // namespace
