/* bebob_avdevice.h
 * Copyright (C) 2005,06 by Daniel Wagner
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

#ifndef BEBOB_AVDEVICE_H
#define BEBOB_AVDEVICE_H

#include <stdint.h>

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebob/xmlparser.h"

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice_subunit.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "libutil/serialize.h"

#include "iavdevice.h"

class ConfigRom;
class Ieee1394Service;
class SubunitPlugSpecificDataPlugAddress;

namespace BeBoB {

class AvDevice : public IAvDevice {
public:
    AvDevice( std::auto_ptr<ConfigRom>( configRom ),
              Ieee1394Service& ieee1394Service,
              int nodeId,
              int verboseLevel );
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom );
    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;

    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );

    virtual int getStreamCount();
    virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();

    virtual int startStreamByIndex(int i);
    virtual int stopStreamByIndex(int i);

    virtual bool addXmlDescription( xmlNodePtr deviceNode );
    virtual void showDevice() const;
    virtual bool setId(unsigned int id);

    Ieee1394Service* get1394Service()
        { return m_1394Service; }

    AvPlugManager& getPlugManager()
        { return m_plugManager; }

    struct SyncInfo {
        SyncInfo( AvPlug& source,
                  AvPlug& destination,
                  std::string description )
            : m_source( &source )
            , m_destination( &destination )
            , m_description( description )
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

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser );
    static AvDevice* deserialize( Glib::ustring basePath,
                                  Util::IODeserialize& deser,
				  Ieee1394Service& ieee1394Service );

protected:
    AvDevice();

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

    bool addPlugToProcessor( AvPlug& plug, FreebobStreaming::StreamProcessor *processor,
                             FreebobStreaming::AmdtpAudioPort::E_Direction direction);

    bool setSamplingFrequencyPlug( AvPlug& plug,
                                   AvPlug::EAvPlugDirection direction,
                                   ESamplingFrequency samplingFrequency );

    void showAvPlugs( AvPlugVector& plugs ) const;

    bool checkSyncConnectionsAndAddToList( AvPlugVector& plhs,
                                           AvPlugVector& prhs,
                                           std::string syncDescription );
protected:
    std::auto_ptr<ConfigRom>( m_pConfigRom );
    Ieee1394Service* m_1394Service;
    int              m_verboseLevel;

    AvPlugVector     m_pcrPlugs;
    AvPlugVector     m_externalPlugs;

    AvPlugConnectionVector m_plugConnections;

    AvDeviceSubunitVector  m_subunits;

    AvPlugManager    m_plugManager;

    SyncInfoVector   m_syncInfos;
    SyncInfo*        m_activeSyncInfo;

    unsigned int m_id;

    // streaming stuff
    FreebobStreaming::AmdtpReceiveStreamProcessor *m_receiveProcessor;
    int m_receiveProcessorBandwidth;

    FreebobStreaming::AmdtpTransmitStreamProcessor *m_transmitProcessor;
    int m_transmitProcessorBandwidth;

    DECLARE_DEBUG_MODULE;
};

}

#endif
