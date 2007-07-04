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
#include "libavc/avc_extended_cmd_generic.h"

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/GenericMixer.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "libutil/serialize.h"

#include "iavdevice.h"

#include <sstream>
#include <vector>

class ConfigRom;
class Ieee1394Service;
class SubunitPlugSpecificDataPlugAddress;

namespace BeBoB {

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
};

class AvDevice : public IAvDevice {
public:
    AvDevice( std::auto_ptr<ConfigRom>( configRom ),
              Ieee1394Service& ieee1394Service,
              int nodeId );
    virtual ~AvDevice();

    void setVerboseLevel(int l);

    static bool probe( ConfigRom& configRom );
    virtual bool discover();

    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );
    virtual int getConfigurationId( );

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    virtual void showDevice();

    Ieee1394Service& get1394Service()
        { return *m_p1394Service; }

    AvPlugManager& getPlugManager()
        { return *m_pPlugManager; }

    struct SyncInfo {
        SyncInfo( AvPlug& source,
                  AvPlug& destination,
                  std::string description )
            : m_source( &source )
            , m_destination( &destination )
            , m_description( description )
            {}
        SyncInfo()
            : m_source( 0 )
            , m_destination( 0 )
            , m_description( "" )
            {}
        AvPlug*     m_source;
        AvPlug*     m_destination;
        std::string m_description;
    };

    typedef std::vector<SyncInfo> SyncInfoVector;
    const SyncInfoVector& getSyncInfos() const
        { return m_syncInfos; }
    const SyncInfo* getActiveSyncInfo() const
        { return m_activeSyncInfo; }
    bool setActiveSync( const SyncInfo& syncInfo );

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static AvDevice* deserialize( Glib::ustring basePath,
                                  Util::IODeserialize& deser,
                  Ieee1394Service& ieee1394Service );
    AvDeviceSubunitAudio* getAudioSubunit( subunit_id_t subunitId )
        { return dynamic_cast<AvDeviceSubunitAudio*>(
                   getSubunit( AVC1394_SUBUNIT_AUDIO , subunitId ));};

protected:

    bool enumerateSubUnits();

    bool discoverPlugs();
    bool discoverPlugsPCR( AvPlug::EAvPlugDirection plugDirection,
                           plug_id_t plugMaxId );
    bool discoverPlugsExternal( AvPlug::EAvPlugDirection plugDirection,
                                plug_id_t plugMaxId );
    bool discoverPlugConnections();
    bool discoverSyncModes();
    bool discoverSubUnitsPlugConnections();

    AvDeviceSubunit* getSubunit( subunit_type_t subunitType,
                                 subunit_id_t subunitId ) const;

    unsigned int getNrOfSubunits( subunit_type_t subunitType ) const;
    AvPlugConnection* getPlugConnection( AvPlug& srcPlug ) const;

    AvPlug* getSyncPlug( int maxPlugId, AvPlug::EAvPlugDirection );

    AvPlug* getPlugById( AvPlugVector& plugs,
                         AvPlug::EAvPlugDirection plugDireciton,
                         int id );
    AvPlugVector getPlugsByType( AvPlugVector& plugs,
                 AvPlug::EAvPlugDirection plugDirection,
                 AvPlug::EAvPlugType type);

    bool addPlugToProcessor( AvPlug& plug, Streaming::StreamProcessor *processor,
                             Streaming::AmdtpAudioPort::E_Direction direction);

    bool setSamplingFrequencyPlug( AvPlug& plug,
                                   AvPlug::EAvPlugDirection direction,
                                   ESamplingFrequency samplingFrequency );

    void showAvPlugs( AvPlugVector& plugs ) const;

    bool checkSyncConnectionsAndAddToList( AvPlugVector& plhs,
                                           AvPlugVector& prhs,
                                           std::string syncDescription );

    static bool serializeSyncInfoVector( Glib::ustring basePath,
                                         Util::IOSerialize& ser,
                                         const SyncInfoVector& vec );
    static bool deserializeSyncInfoVector( Glib::ustring basePath,
                                           Util::IODeserialize& deser,
                                           AvDevice& avDevice,
                                           SyncInfoVector& vec );
    int getConfigurationIdSampleRate();
    int getConfigurationIdNumberOfChannel( PlugAddress::EPlugDirection ePlugDirection );
    int getConfigurationIdSyncMode();

protected:
    AvPlugVector              m_pcrPlugs;
    AvPlugVector              m_externalPlugs;
    AvPlugConnectionVector    m_plugConnections;
    AvDeviceSubunitVector     m_subunits;
    AvPlugManager*            m_pPlugManager;
    SyncInfoVector            m_syncInfos;
    SyncInfo*                 m_activeSyncInfo;
    struct VendorModelEntry*  m_model;
    GenericMixer*             m_Mixer;

    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;
};

}

#endif
