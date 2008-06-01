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

#ifndef MAUDIODEVICE_H
#define MAUDIODEVICE_H

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_extended_cmd_generic.h"

#include "bebob/bebob_avdevice.h"

#include "libstreaming/amdtp/AmdtpReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"
#include "libstreaming/amdtp/AmdtpPortInfo.h"

#include "ffadodevice.h"

class ConfigRom;
class Ieee1394Service;

namespace MAudio {

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    const char *vendor_name;
    const char *model_name;
    const char *filename;
};

class AvDevice : public BeBoB::AvDevice {
public:
    AvDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    virtual uint64_t getConfigurationId( );
    
    virtual void showDevice();

    virtual bool setSamplingFrequency( int );
    virtual int getSamplingFrequency( );

    virtual bool prepare();

protected:
    struct VendorModelEntry*  m_model;

};

}

#endif
