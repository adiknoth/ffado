/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef BEBOB_AVDEVICE_H
#define BEBOB_AVDEVICE_H

#include <stdint.h>

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_extended_cmd_generic.h"
#include "libavc/general/avc_unit.h"
#include "libavc/general/avc_subunit.h"
#include "libavc/general/avc_plug.h"

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/GenericMixer.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "libutil/serialize.h"

#include "ffadodevice.h"

#include <sstream>
#include <vector>

class ConfigRom;
class Ieee1394Service;

namespace BeBoB {

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

    void setVerboseLevel(int l);

    static bool probe( ConfigRom& configRom );
    virtual bool discover();

    virtual bool setSamplingFrequency( int );
    virtual int getSamplingFrequency( );

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();
    bool lock();
    bool unlock();

    bool startStreamByIndex(int i);
    bool stopStreamByIndex(int i);

    virtual void showDevice();

    bool setActiveSync( const SyncInfo& syncInfo );

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static AvDevice* deserialize( Glib::ustring basePath,
                                  Util::IODeserialize& deser,
                                  Ieee1394Service& ieee1394Service );

    // redefinition to resolve ambiguity
    Ieee1394Service& get1394Service()
        { return *m_p1394Service; }
    ConfigRom& getConfigRom() const 
        {return FFADODevice::getConfigRom();};
        
protected:

    bool discoverPlugs();
    bool discoverPlugsPCR( AVC::Plug::EPlugDirection plugDirection,
                           AVC::plug_id_t plugMaxId );
    bool discoverPlugsExternal( AVC::Plug::EPlugDirection plugDirection,
                                AVC::plug_id_t plugMaxId );
    bool discoverPlugConnections();
    bool discoverSyncModes();
    bool discoverSubUnitsPlugConnections();

    bool addPlugToProcessor( AVC::Plug& plug, Streaming::StreamProcessor *processor,
                             Streaming::AmdtpAudioPort::E_Direction direction);

    bool setSamplingFrequencyPlug( AVC::Plug& plug,
                                   AVC::Plug::EPlugDirection direction,
                                   AVC::ESamplingFrequency samplingFrequency );

    bool checkSyncConnectionsAndAddToList( AVC::PlugVector& plhs,
                                           AVC::PlugVector& prhs,
                                           std::string syncDescription );

protected:
    struct VendorModelEntry*  m_model;
    GenericMixer*             m_Mixer;

    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;

    DECLARE_DEBUG_MODULE;
};

}

#endif
