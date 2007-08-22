/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef GENERICAVC_AVDEVICE_H
#define GENERICAVC_AVDEVICE_H

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_unit.h"
#include "libavc/general/avc_subunit.h"
#include "libavc/general/avc_plug.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

class ConfigRom;
class Ieee1394Service;

namespace GenericAVC {

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
};

class AvDevice : public FFADODevice, public AVC::Unit {
public:
    AvDevice( std::auto_ptr<ConfigRom>( configRom ),
          Ieee1394Service& ieee1394Service,
          int nodeId );
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom );
    virtual bool discover();
    
    virtual void setVerboseLevel(int l);
    virtual void showDevice();

    virtual bool setSamplingFrequency( int );
    virtual int getSamplingFrequency( );

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
    
    struct VendorModelEntry *m_model;
    
    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;
    
    DECLARE_DEBUG_MODULE;
};

}

#endif //GENERICAVC_AVDEVICE_H
