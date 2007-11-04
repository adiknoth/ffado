/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#ifndef MAUDIODEVICE_H
#define MAUDIODEVICE_H

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_extended_cmd_generic.h"

#include "bebob/bebob_avdevice.h"

#include "libstreaming/amdtp/AmdtpStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"
#include "libstreaming/amdtp/AmdtpPortInfo.h"

#include "ffadodevice.h"

class ConfigRom;
class Ieee1394Service;

namespace MAudio {

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
    char *filename;
};

class AvDevice : public BeBoB::AvDevice {
public:
    AvDevice( Ieee1394Service& ieee1394Service,
              std::auto_ptr<ConfigRom>( configRom ));
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom );
    static FFADODevice * createDevice( Ieee1394Service& ieee1394Service,
                                        std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    static int getConfigurationId( );
    
    virtual void showDevice();

    virtual bool setSamplingFrequency( int );
    virtual int getSamplingFrequency( );

    virtual bool prepare();

protected:
    struct VendorModelEntry*  m_model;

};

}

#endif
