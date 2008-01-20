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

#ifndef AVC_UNIT_H
#define AVC_UNIT_H

#include <stdint.h>

#include "debugmodule/debugmodule.h"

#include "../avc_definitions.h"
#include "../general/avc_extended_cmd_generic.h"
#include "../general/avc_subunit.h"
#include "../general/avc_plug.h"
#include "../musicsubunit/avc_musicsubunit.h"
#include "../audiosubunit/avc_audiosubunit.h"

#include "libutil/serialize.h"

#include <sstream>
#include <vector>

class ConfigRom;
class Ieee1394Service;

namespace AVC {

class Unit {
public:
    Unit( );
    virtual ~Unit();

    virtual void setVerboseLevel(int l);
    virtual void show();

    // these have to be implemented by the parent class
    /// Returns the 1394 service
    virtual Ieee1394Service& get1394Service() = 0;
    /// Returns the ConfigRom
    virtual ConfigRom& getConfigRom() const = 0;

    /// Discovers the unit's internals
    virtual bool discover();

    PlugManager& getPlugManager()
        { return *m_pPlugManager; }

    struct SyncInfo {
        SyncInfo( Plug& source,
                  Plug& destination,
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
        Plug*     m_source;
        Plug*     m_destination;
        std::string m_description;
    };

    typedef std::vector<SyncInfo> SyncInfoVector;
    virtual const SyncInfoVector& getSyncInfos() const
        { return m_syncInfos; }
    virtual const SyncInfo* getActiveSyncInfo() const
        { return m_activeSyncInfo; }

    virtual bool setActiveSync( const SyncInfo& syncInfo );

    virtual bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    virtual bool deserialize( Glib::ustring basePath, Util::IODeserialize& deser );

    SubunitAudio* getAudioSubunit( subunit_id_t subunitId )
        { return dynamic_cast<SubunitAudio*>(
                   getSubunit( AVC1394_SUBUNIT_AUDIO , subunitId ));};
    SubunitMusic* getMusicSubunit( subunit_id_t subunitId )
        { return dynamic_cast<SubunitMusic*>(
                   getSubunit( AVC1394_SUBUNIT_MUSIC , subunitId ));};
    Subunit* getSubunit( subunit_type_t subunitType,
                         subunit_id_t subunitId ) const;

    virtual AVC::Subunit* createSubunit(Unit& unit,
                                        ESubunitType type,
                                        subunit_t id );
    virtual AVC::Plug* createPlug( AVC::Unit* unit,
                                   AVC::Subunit* subunit,
                                   AVC::function_block_type_t functionBlockType,
                                   AVC::function_block_type_t functionBlockId,
                                   AVC::Plug::EPlugAddressType plugAddressType,
                                   AVC::Plug::EPlugDirection plugDirection,
                                   AVC::plug_id_t plugId );

protected:
    /// cleans the internal data structures that are created by discovery
    virtual bool clean();

    virtual bool enumerateSubUnits();
    virtual bool discoverPlugConnections();
    virtual bool discoverSubUnitsPlugConnections();
    virtual bool discoverPlugs();
    virtual bool discoverPlugsPCR( AVC::Plug::EPlugDirection plugDirection,
                           AVC::plug_id_t plugMaxId );
    virtual bool discoverPlugsExternal( AVC::Plug::EPlugDirection plugDirection,
                                AVC::plug_id_t plugMaxId );
    virtual bool propagatePlugInfo();
    virtual bool discoverSyncModes();
    virtual bool checkSyncConnectionsAndAddToList( AVC::PlugVector& plhs,
                                           AVC::PlugVector& prhs,
                                           std::string syncDescription );
    virtual Plug* getSyncPlug( int maxPlugId, Plug::EPlugDirection );

    unsigned int getNrOfSubunits( subunit_type_t subunitType ) const;
    PlugConnection* getPlugConnection( Plug& srcPlug ) const;

    Plug* getPlugById( PlugVector& plugs,
                         Plug::EPlugDirection plugDireciton,
                         int id );
    PlugVector getPlugsByType( PlugVector& plugs,
                 Plug::EPlugDirection plugDirection,
                 Plug::EPlugType type);

//     bool setSamplingFrequencyPlug( Plug& plug,
//                                    Plug::EPlugDirection direction,
//                                    ESamplingFrequency samplingFrequency );

    void showPlugs( PlugVector& plugs ) const;


    bool serializeSyncInfoVector( Glib::ustring basePath,
                                  Util::IOSerialize& ser,
                                  const SyncInfoVector& vec ) const;
    bool deserializeSyncInfoVector( Glib::ustring basePath,
                                    Util::IODeserialize& deser,
                                    SyncInfoVector& vec );
protected:
    SubunitVector             m_subunits;
    PlugVector                m_pcrPlugs;
    PlugVector                m_externalPlugs;
    PlugConnectionVector      m_plugConnections;
    PlugManager*              m_pPlugManager;
    SyncInfoVector            m_syncInfos;
    SyncInfo*                 m_activeSyncInfo;

private:
    DECLARE_DEBUG_MODULE;

};

}

#endif
