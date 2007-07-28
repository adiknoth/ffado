/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

class AvDevice : public FFADODevice {
public:
    AvDevice( std::auto_ptr<ConfigRom>( configRom ),
          Ieee1394Service& ieee1394Service,
          int nodeId );
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom );
    bool discover();

    void showDevice();

    bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    int getSamplingFrequency( );

    int getStreamCount();
    Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    bool prepare();
    bool lock();
    bool unlock();

    bool startStreamByIndex(int i);
    bool stopStreamByIndex(int i);

protected:
    struct VendorModelEntry *m_model;

};

}

#endif //GENERICAVC_AVDEVICE_H
