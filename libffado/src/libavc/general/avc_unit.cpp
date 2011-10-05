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

#include "avc_unit.h"
#include "avc_subunit.h"
#include "avc_plug.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "../general/avc_plug_info.h"
#include "../general/avc_extended_plug_info.h"
#include "../general/avc_subunit_info.h"
#include "../streamformat/avc_extended_stream_format.h"
#include "libutil/cmd_serialize.h"
#include "../avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>

namespace AVC {

IMPL_DEBUG_MODULE( Unit, Unit, DEBUG_LEVEL_NORMAL );

Unit::Unit( )
    : m_pPlugManager( new PlugManager( ) )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Unit\n" );
    m_pPlugManager->setVerboseLevel( getDebugLevel() );
}

Unit::~Unit()
{
    clean();
}

Plug *
Unit::createPlug( Unit* unit,
                  Subunit* subunit,
                  function_block_type_t functionBlockType,
                  function_block_type_t functionBlockId,
                  Plug::EPlugAddressType plugAddressType,
                  Plug::EPlugDirection plugDirection,
                  plug_id_t plugId,
                  int globalId )
{

    Plug *p= new Plug( unit,
                       subunit,
                       functionBlockType,
                       functionBlockId,
                       plugAddressType,
                       plugDirection,
                       plugId,
                       globalId );
    if (p) p->setVerboseLevel(getDebugLevel());
    return p;
}

Subunit*
Unit::createSubunit(Unit& unit,
                    ESubunitType type,
                    subunit_t id )
{
    Subunit* s=NULL;
    switch (type) {
        case eST_Audio:
            s=new SubunitAudio(unit, id );
            break;
        case eST_Music:
            s=new SubunitMusic(unit, id );
            break;
        default:
            s=NULL;
            break;
    }
    if(s) s->setVerboseLevel(getDebugLevel());
    return s;
}

void
Unit::setVerboseLevel(int l)
{
    setDebugLevel(l);
    for ( SubunitVector::const_iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        (*it)->setVerboseLevel(l);
    }
    m_pPlugManager->setVerboseLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

// prepare the device for a rediscovery
bool
Unit::clean()
{
    for ( SubunitVector::iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        delete *it;
    }
    m_subunits.clear();

    for ( PlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        delete *it;
    }
    m_pcrPlugs.clear();

    for ( PlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        delete *it;
    }
    m_externalPlugs.clear();

    for ( PlugConnectionVector::iterator it = m_plugConnections.begin();
          it != m_plugConnections.end();
          ++it )
    {
        delete *it;
    }
    m_plugConnections.clear();

    delete m_pPlugManager;
    m_pPlugManager = new PlugManager();

    if (m_pPlugManager == NULL) {
        debugError("Could not create new plugmanager\n");
        return false;
    }

    m_syncInfos.clear();

    return true;
}

bool
Unit::discover()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Discovering AVC::Unit...\n");
    if( !clean() ) {
        debugError( "Could not clean unit data structures\n" );
        return false;
    }

    if ( !enumerateSubUnits() ) {
        debugError( "Could not enumarate sub units\n" );
        return false;
    }

    if ( !discoverPlugs() ) {
        debugError( "Detecting plugs failed\n" );
        return false;
    }

    if ( !rediscoverConnections() ) {
        debugError( "Detecting connections failed\n" );
        return false;
    }

    if ( !discoverSyncModes() ) {
        debugError( "Detecting sync modes failed\n" );
        return false;
    }

    if ( !propagatePlugInfo() ) {
        debugError( "Failed to propagate plug info\n" );
        return false;
    }

    return true;
}

bool
Unit::rediscoverConnections() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Re-discovering plug connections...\n");

    // clear the previous connections
    for ( PlugConnectionVector::iterator it = m_plugConnections.begin();
          it != m_plugConnections.end();
          ++it )
    {
        delete *it;
    }
    m_plugConnections.clear();

    if ( !discoverPlugConnections() ) {
        debugError( "Detecting plug connections failed\n" );
        return false;
    }

    if ( !discoverSubUnitsPlugConnections() ) {
        debugError( "Detecting subunit plug connections failed\n" );
        return false;
    }

    if ( !m_pPlugManager->tidyPlugConnections(m_plugConnections) ) {
        debugError( "Tidying of plug connections failed\n" );
        return false;
    }
    return true;
}

bool
Unit::enumerateSubUnits()
{
    SubUnitInfoCmd subUnitInfoCmd( get1394Service() );
    subUnitInfoCmd.setCommandType( AVCCommand::eCT_Status );

    // NOTE: BeBoB has always exactly one audio and one music subunit. This
    // means is fits into the first page of the SubUnitInfo command.
    // So there is no need to do more than needed
    // FIXME: to be fully spec compliant this needs to be fixed, but let's not
    //        do that for now

    subUnitInfoCmd.m_page = 0;
    subUnitInfoCmd.setNodeId( getConfigRom().getNodeId() );
    subUnitInfoCmd.setVerbose( getDebugLevel() );
    if ( !subUnitInfoCmd.fire() ) {
        debugError( "Subunit info command failed\n" );
        // shouldn't this be an error situation?
        return false;
    }

    for ( int i = 0; i < subUnitInfoCmd.getNrOfValidEntries(); ++i ) {
        subunit_type_t subunit_type
            = subUnitInfoCmd.m_table[i].m_subunit_type;

        unsigned int subunitId = getNrOfSubunits( subunit_type );

        debugOutput( DEBUG_LEVEL_VERBOSE,
                     "subunit_id = %2d, subunit_type = %2d (%s)\n",
                     subunitId,
                     subunit_type,
                     subunitTypeToString( subunit_type ) );

        Subunit* subunit = 0;
        switch( subunit_type ) {
        case eST_Audio:
            subunit = createSubunit( *this, eST_Audio, subunitId );
            if ( !subunit ) {
                debugFatal( "Could not allocate SubunitAudio\n" );
                return false;
            }

            subunit->setVerboseLevel(getDebugLevel());

            if ( !subunit->discover() ) {
                debugError( "enumerateSubUnits: Could not discover "
                            "subunit_id = %2d, subunit_type = %2d (%s)\n",
                            subunitId,
                            subunit_type,
                            subunitTypeToString( subunit_type ) );
                delete subunit;
                return false;
            } else {
                m_subunits.push_back( subunit );
            }

            break;
        case eST_Music:
            subunit = createSubunit( *this, eST_Music, subunitId );
            if ( !subunit ) {
                debugFatal( "Could not allocate SubunitMusic\n" );
                return false;
            }

            subunit->setVerboseLevel(getDebugLevel());

            if ( !subunit->discover() ) {
                debugError( "enumerateSubUnits: Could not discover "
                            "subunit_id = %2d, subunit_type = %2d (%s)\n",
                            subunitId,
                            subunit_type,
                            subunitTypeToString( subunit_type ) );
                delete subunit;
                return false;
            } else {
                m_subunits.push_back( subunit );
            }

            break;
        default:
            debugOutput( DEBUG_LEVEL_NORMAL,
                         "Unsupported subunit found, subunit_type = %d (%s)\n",
                         subunit_type,
                         subunitTypeToString( subunit_type ) );
            continue;

        }
    }

    return true;
}

Subunit*
Unit::getSubunit( subunit_type_t subunitType,
                      subunit_id_t subunitId ) const
{
    for ( SubunitVector::const_iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        Subunit* subunit = *it;
        if ( ( subunitType == subunit->getSubunitType() )
             && ( subunitId == subunit->getSubunitId() ) )
        {
            return subunit;
        }
    }

    return 0;
}

unsigned int
Unit::getNrOfSubunits( subunit_type_t subunitType ) const
{
    unsigned int nrOfSubunits = 0;

    for ( SubunitVector::const_iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        Subunit* subunit = *it;
        if ( subunitType == subunit->getSubunitType() ) {
            nrOfSubunits++;
        }
    }

    return nrOfSubunits;
}

bool
Unit::discoverPlugs()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Discovering plugs...\n");

    //////////////////////////////////////////////
    // Get number of available isochronous input
    // and output plugs of unit

    PlugInfoCmd plugInfoCmd( get1394Service() );
    plugInfoCmd.setNodeId( getConfigRom().getNodeId() );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setVerbose( getDebugLevel() );

    if ( !plugInfoCmd.fire() ) {
        debugError( "plug info command failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_NORMAL, "number of iso input plugs = %d\n",
                 plugInfoCmd.m_serialBusIsochronousInputPlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of iso output plugs = %d\n",
                 plugInfoCmd.m_serialBusIsochronousOutputPlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of external input plugs = %d\n",
                 plugInfoCmd.m_externalInputPlugs );
    debugOutput( DEBUG_LEVEL_NORMAL, "number of external output plugs = %d\n",
                 plugInfoCmd.m_externalOutputPlugs );

    if ( !discoverPlugsPCR( Plug::eAPD_Input,
                            plugInfoCmd.m_serialBusIsochronousInputPlugs ) )
    {
        debugError( "pcr input plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugsPCR( Plug::eAPD_Output,
                            plugInfoCmd.m_serialBusIsochronousOutputPlugs ) )
    {
        debugError( "pcr output plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugsExternal( Plug::eAPD_Input,
                                 plugInfoCmd.m_externalInputPlugs ) )
    {
        debugError( "external input plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugsExternal( Plug::eAPD_Output,
                                 plugInfoCmd.m_externalOutputPlugs ) )
    {
        debugError( "external output plug discovering failed\n" );
        return false;
    }

    return true;
}

bool
Unit::discoverPlugsPCR( Plug::EPlugDirection plugDirection,
                        plug_id_t plugMaxId )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Discovering PCR plugs, direction %d...\n",plugDirection);
    for ( int plugId = 0;
          plugId < plugMaxId;
          ++plugId )
    {
        Plug* plug  = createPlug( this,
                                  NULL,
                                  0xff,
                                  0xff,
                                  Plug::eAPA_PCR,
                                  plugDirection,
                                  plugId );

        if( plug ) plug->setVerboseLevel(getDebugLevel());

        if ( !plug || !plug->discover() ) {
            debugError( "plug discovering failed\n" );
            delete plug;
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "plug '%s' found\n",
                     plug->getName() );
        m_pcrPlugs.push_back( plug );
    }

    return true;
}

bool
Unit::discoverPlugsExternal( Plug::EPlugDirection plugDirection,
                             plug_id_t plugMaxId )
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Discovering External plugs, direction %d...\n",plugDirection);
    for ( int plugId = 0;
          plugId < plugMaxId;
          ++plugId )
    {
        Plug* plug  = createPlug( this, NULL,
                                0xff,
                                0xff,
                                Plug::eAPA_ExternalPlug,
                                plugDirection,
                                plugId );

        if( plug ) plug->setVerboseLevel(getDebugLevel());

        if ( !plug || !plug->discover() ) {
            debugError( "plug discovering failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_NORMAL, "plug '%s' found\n",
                     plug->getName() );
        m_externalPlugs.push_back( plug );
    }

    return true;
}

bool
Unit::discoverPlugConnections()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Discovering PCR plug connections...\n");
    for ( PlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "Could not discover PCR plug connections\n" );
            return false;
        }
    }
    debugOutput( DEBUG_LEVEL_NORMAL, "Discovering External plug connections...\n");
    for ( PlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "Could not discover External plug connections\n" );
            return false;
        }
    }

    return true;
}

bool
Unit::discoverSubUnitsPlugConnections()
{
    for ( SubunitVector::iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        Subunit* subunit = *it;

        if ( !subunit->discoverConnections() ) {
            debugError( "Subunit '%s'  plug connections failed\n",
                        subunit->getName() );
            return false;
        }
    }
    return true;
}

bool
Unit::propagatePlugInfo()
{
    debugOutput( DEBUG_LEVEL_NORMAL, "Propagating info to PCR plugs...\n");
    for ( PlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        Plug* plug = *it;
        debugOutput( DEBUG_LEVEL_NORMAL, "plug: %s\n", plug->getName());
        if (!plug->propagateFromConnectedPlug()) {
            debugWarning( "Could not propagate info for plug '%s'\n", plug->getName());
        }
    }
    debugOutput( DEBUG_LEVEL_NORMAL, "Propagating info to External plugs...\n");
    for ( PlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        Plug* plug = *it;
        debugOutput( DEBUG_LEVEL_NORMAL, "plug: %s\n", plug->getName());
        if (!plug->propagateFromConnectedPlug()) {
            debugWarning( "Could not propagate info for plug '%s'\n", plug->getName());
        }
    }

    return true;

}


PlugConnection*
Unit::getPlugConnection( Plug& srcPlug ) const
{
    for ( PlugConnectionVector::const_iterator it
              = m_plugConnections.begin();
          it != m_plugConnections.end();
          ++it )
    {
        PlugConnection* plugConnection = *it;
        if ( &( plugConnection->getSrcPlug() ) == &srcPlug ) {
            return plugConnection;
        }
    }

    return 0;
}

Plug*
Unit::getPlugById( PlugVector& plugs,
                   Plug::EPlugDirection plugDirection,
                   int id )
{
    for ( PlugVector::iterator it = plugs.begin();
          it != plugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( ( id == plug->getPlugId() )
             && ( plugDirection == plug->getPlugDirection() ) )
        {
            return plug;
        }
    }

    return 0;
}

PlugVector
Unit::getPlugsByType( PlugVector& plugs,
                      Plug::EPlugDirection plugDirection,
                      Plug::EPlugType type)
{
    PlugVector plugVector;
    for ( PlugVector::iterator it = plugs.begin();
          it != plugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( ( type == plug->getPlugType() )
             && ( plugDirection == plug->getPlugDirection() ) )
        {
            plugVector.push_back( plug );
        }
    }

    return plugVector;
}

Plug*
Unit::getSyncPlug( int maxPlugId, Plug::EPlugDirection )
{
    return 0;
}

bool
Unit::discoverSyncModes()
{
    // Following possible sync plugs exists:
    // - Music subunit sync output plug = internal sync (CSP)
    // - Unit input plug 0 = SYT match
    // - Unit input plut 1 = Sync stream
    //
    // If last sync mode is reported it is mostelikely not
    // implemented *sic*
    //
    // Following sync sources are device specific:
    // - All unit external input plugs which have a
    //   sync information (WS, SPDIF, ...)

    // First we have to find the sync input and output plug
    // in the music subunit.

    // Note PCR input means 1394bus-to-device where as
    // MSU input means subunit-to-device

    PlugVector syncPCRInputPlugs = getPlugsByType( m_pcrPlugs,
                                                   Plug::eAPD_Input,
                                                   Plug::eAPT_Sync );
    if ( !syncPCRInputPlugs.size() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, "No PCR sync input plug found\n" );
    }

    PlugVector syncPCROutputPlugs = getPlugsByType( m_pcrPlugs,
                                                    Plug::eAPD_Output,
                                                    Plug::eAPT_Sync );
    if ( !syncPCROutputPlugs.size() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, "No PCR sync output plug found\n" );
    }

    PlugVector isoPCRInputPlugs = getPlugsByType( m_pcrPlugs,
                                                  Plug::eAPD_Input,
                                                  Plug::eAPT_IsoStream );
    if ( !isoPCRInputPlugs.size() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, "No PCR iso input plug found\n" );

    }

    PlugVector isoPCROutputPlugs = getPlugsByType( m_pcrPlugs,
                                                   Plug::eAPD_Output,
                                                   Plug::eAPT_IsoStream );
    if ( !isoPCROutputPlugs.size() ) {
        debugOutput(DEBUG_LEVEL_NORMAL, "No PCR iso output plug found\n" );

    }

    PlugVector digitalExternalInputPlugs = getPlugsByType( m_externalPlugs,
                                                           Plug::eAPD_Input,
                                                           Plug::eAPT_Digital );
    if ( !digitalExternalInputPlugs.size() ) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "No external digital input plugs found\n" );

    }

    PlugVector syncExternalInputPlugs = getPlugsByType( m_externalPlugs,
                                                        Plug::eAPD_Input,
                                                        Plug::eAPT_Sync );
    if ( !syncExternalInputPlugs.size() ) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "No external sync input plugs found\n" );

    }

    PlugVector syncMSUInputPlugs = m_pPlugManager->getPlugsByType(
        eST_Music,
        0,
        0xff,
        0xff,
        Plug::eAPA_SubunitPlug,
        Plug::eAPD_Input,
        Plug::eAPT_Sync );
    if ( !syncMSUInputPlugs.size() ) {
        debugWarning( "No sync input plug for MSU subunit found\n" );
    }

    PlugVector syncMSUOutputPlugs = m_pPlugManager->getPlugsByType(
        eST_Music,
        0,
        0xff,
        0xff,
        Plug::eAPA_SubunitPlug,
        Plug::eAPD_Output,
        Plug::eAPT_Sync );
    if ( !syncMSUOutputPlugs.size() ) {
        debugWarning( "No sync output plug for MSU subunit found\n" );
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Sync Input Plugs:\n" );
    showPlugs( syncPCRInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Sync Output Plugs:\n" );
    showPlugs( syncPCROutputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Iso Input Plugs:\n" );
    showPlugs( isoPCRInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "PCR Iso Output Plugs:\n" );
    showPlugs( isoPCROutputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "External digital Input Plugs:\n" );
    showPlugs( digitalExternalInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "External sync Input Plugs:\n" );
    showPlugs( syncExternalInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "MSU Sync Input Plugs:\n" );
    showPlugs( syncMSUInputPlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "MSU Sync Output Plugs:\n" );
    showPlugs( syncMSUOutputPlugs );

    m_syncInfos.clear();
    // Currently there is no usable setup for sync streams.
    // There is no point in wasting time here. Let's skip
    // 'sync stream input' and 'sync stream output'.

    // Check all PCR iso input to MSU input connections
    // -> SYT match
    checkSyncConnectionsAndAddToList( isoPCRInputPlugs,
                                      syncMSUInputPlugs,
                                      "Syt Match" );

    // Check all MSU sync output to MSU input connections
    // -> CSP
    checkSyncConnectionsAndAddToList( syncMSUOutputPlugs,
                                      syncMSUInputPlugs,
                                      "Internal (CSP)" );

    // Check all external digital input to MSU input connections
    // -> SPDIF/ADAT sync
    checkSyncConnectionsAndAddToList( digitalExternalInputPlugs,
                                      syncMSUInputPlugs,
                                      "Digital Input Sync" );

    // Check all external sync input to MSU input connections
    // -> SPDIF/ADAT sync
    checkSyncConnectionsAndAddToList( syncExternalInputPlugs,
                                      syncMSUInputPlugs,
                                      "Digital Input Sync" );

    return true;
}

const Unit::SyncInfo*
Unit::getActiveSyncInfo()
{
    SyncInfo* activeSyncInfo = NULL;
    PlugVector syncMSUInputPlugs = m_pPlugManager->getPlugsByType(
        eST_Music,
        0,
        0xff,
        0xff,
        Plug::eAPA_SubunitPlug,
        Plug::eAPD_Input,
        Plug::eAPT_Sync );
    if ( !syncMSUInputPlugs.size() ) {
        debugWarning( "No sync input plug for MSU subunit found\n" );
    }

    // Currently active connection signal source cmd, command type
    // status, source unknown, destination MSU sync input plug
    for ( PlugVector::const_iterator it = syncMSUInputPlugs.begin();
          it != syncMSUInputPlugs.end();
          ++it )
    {
        AVC::Plug* msuPlug = *it;
        for ( PlugVector::const_iterator jt =
                  msuPlug->getInputConnections().begin();
              jt != msuPlug->getInputConnections().end();
              ++jt )
        {
            AVC::Plug* plug = *jt;

            for ( SyncInfoVector::iterator it = m_syncInfos.begin();
                  it != m_syncInfos.end();
                  ++it )
            {
                SyncInfo* pSyncInfo = &*it;
                if ( ( pSyncInfo->m_source == plug )
                     && ( pSyncInfo->m_destination == msuPlug ) )
                {
                    activeSyncInfo = pSyncInfo;
                    break;
                }
            }
            if(activeSyncInfo) {
                debugOutput( DEBUG_LEVEL_NORMAL,
                            "Active Sync Connection: %s, '%s' -> '%s'\n",
                            activeSyncInfo->m_description.c_str(),
                            plug->getName(),
                            msuPlug->getName() );
            }
        }
    }
    return activeSyncInfo;
}

bool
Unit::checkSyncConnectionsAndAddToList( PlugVector& plhs,
                                        PlugVector& prhs,
                                        std::string syncDescription )
{
    for ( PlugVector::iterator plIt = plhs.begin();
          plIt != plhs.end();
          ++plIt )
    {
        AVC::Plug* pl = *plIt;
        for ( PlugVector::iterator prIt = prhs.begin();
              prIt != prhs.end();
              ++prIt )
        {
            AVC::Plug* pr = *prIt;
            if ( pl->inquireConnnection( *pr ) ) {
                m_syncInfos.push_back( SyncInfo( *pl, *pr, syncDescription ) );
                debugOutput( DEBUG_LEVEL_NORMAL,
                             "%s, sync connection '%s' -> '%s'\n",
                             syncDescription.c_str(),
                             pl->getName(),
                             pr->getName() );
            }
        }
    }
    return true;
}

bool Unit::setActiveSync(const SyncInfo& syncInfo)
{
    bool retval = true;

    if ( ! syncInfo.m_source->inquireConnnection( *syncInfo.m_destination ) ) {
        // this should not happen
        debugError("Sync connection '%s' -> '%s' not possible. This might be a bug.\n",
                   syncInfo.m_source->getName(), syncInfo.m_destination->getName());
    }

    if(!syncInfo.m_source->setConnection( *syncInfo.m_destination )) {
        debugError("Could not set sync source connection while device reported it as possible.\n");
        retval = false; // proceed to rediscovery anyway
    }

    // we now have to rediscover the connections
    if ( !rediscoverConnections() ) {
        debugError( "Re-discovery of plug connections failed\n" );
        return false;
    }
    return retval;
}

void
Unit::show()
{
    if (getDebugLevel() >= DEBUG_LEVEL_VERY_VERBOSE) {
        m_pPlugManager->showPlugs();
    }
    //SubunitMusic* s=getMusicSubunit(0);
    //if(s) s->showMusicPlugs();
}

void
Unit::showPlugs( PlugVector& plugs ) const
{
    int i = 0;
    for ( PlugVector::const_iterator it = plugs.begin();
          it != plugs.end();
          ++it, ++i )
    {
        Plug* plug = *it;
        debugOutput( DEBUG_LEVEL_VERBOSE, "Plug %d\n", i );
        plug->showPlug();
    }
}

template <typename T>
bool
serializeVector( std::string path,
                 Util::IOSerialize& ser,
                 const T& vec )
{
    bool result = true; // if vec.size() == 0
    int i = 0;
    for ( typename T::const_iterator it = vec.begin(); it != vec.end(); ++it ) {
        std::ostringstream strstrm;
        strstrm << path << i;
        result &= ( *it )->serialize( strstrm.str() + "/", ser );
        i++;
    }
    return result;
}

template <typename T, typename VT>
bool
deserializeVector( std::string path,
                   Util::IODeserialize& deser,
                   Unit& unit,
                   VT& vec )
{
    int i = 0;
    T* ptr = 0;
    do {
        std::ostringstream strstrm;
        strstrm << path << i << "/";
        ptr = T::deserialize( strstrm.str(),
                              deser,
                              unit );
        if ( ptr ) {
            vec.push_back( ptr );
        }
        i++;
    } while ( ptr );

    return true;
}

bool
Unit::serializeSyncInfoVector( std::string basePath,
                               Util::IOSerialize& ser,
                               const SyncInfoVector& vec ) const
{
    bool result = true;
    int i = 0;

    for ( SyncInfoVector::const_iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        const SyncInfo& info = *it;

        std::ostringstream strstrm;
        strstrm << basePath << i << "/";

        result &= ser.write( strstrm.str() + "m_source", info.m_source->getGlobalId() );
        result &= ser.write( strstrm.str() + "m_destination", info.m_destination->getGlobalId() );
        result &= ser.write( strstrm.str() + "m_description", std::string( info.m_description ) );

        i++;
    }

    return result;
}

bool
Unit::deserializeSyncInfoVector( std::string basePath,
                                 Util::IODeserialize& deser,
                                 SyncInfoVector& vec )
{
    int i = 0;
    bool bFinished = false;
    do {
        bool result;
        std::ostringstream strstrm;
        strstrm << basePath << i << "/";

        plug_id_t sourceId;
        plug_id_t destinationId;
        std::string description;

        if ( deser.isExisting( strstrm.str() + "m_source" ) ) {
            result  = deser.read( strstrm.str() + "m_source", sourceId );
            result &= deser.read( strstrm.str() + "m_destination", destinationId );
            result &= deser.read( strstrm.str() + "m_description", description );
        } else {
            result = false;
        }

        if ( result ) {
            SyncInfo syncInfo;
            syncInfo.m_source = m_pPlugManager->getPlug( sourceId );
            syncInfo.m_destination = m_pPlugManager->getPlug( destinationId );
            syncInfo.m_description = description;

            vec.push_back( syncInfo );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

bool
Unit::serialize( std::string basePath,
                 Util::IOSerialize& ser ) const
{
    bool result;
    result =  serializeVector( basePath + "Subunit", ser, m_subunits );
    result &= serializePlugVector( basePath + "PcrPlug", ser, m_pcrPlugs );
    result &= serializePlugVector( basePath + "ExternalPlug",  ser, m_externalPlugs );
    result &= serializeVector( basePath + "PlugConnection", ser, m_plugConnections );
    result &= m_pPlugManager->serialize( basePath + "Plug", ser ); // serialize all av plugs
    result &= serializeSyncInfoVector( basePath + "SyncInfo", ser, m_syncInfos );

    return result;
}

bool
Unit::deserialize( std::string basePath,
                   Util::IODeserialize& deser )
{
    bool result = true;

    result &= deserializeVector<Subunit>( basePath + "Subunit", deser, *this, m_subunits );

    if (m_pPlugManager)
        delete m_pPlugManager;

    // load all plugs
    m_pPlugManager = PlugManager::deserialize( basePath + "Plug", deser, *this );

    if ( !m_pPlugManager )
        return false;

    // update the plug related stuff in the subunits. we have to
    // do that in 2 steps because we have a circular dependency.
    for ( SubunitVector::iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        result &= (*it)->deserializeUpdate( basePath + "Subunit", deser );
    }

    // load path /PcrPlug0/global_id
    result &= deserializePlugVector( basePath + "PcrPlug", deser,
                                     getPlugManager(), m_pcrPlugs );
    // load path /ExternalPlug0/global_id
    result &= deserializePlugVector( basePath + "ExternalPlug", deser,
                                     getPlugManager(), m_externalPlugs );
    result &= deserializeVector<PlugConnection>( basePath + "PlugConnection", deser,
                                                 *this, m_plugConnections );
    result &= deserializeVector<Subunit>( basePath + "Subunit",  deser, *this, m_subunits );
    result &= deserializeSyncInfoVector( basePath + "SyncInfo", deser, m_syncInfos );

    // update connectsion between plugs (plug.m_inputConnections
    // and plug.m_outputConnnections list)
    m_pPlugManager->deserializeUpdate( basePath, deser );

    // this might have changed since the cache was saved
    // if the config ID doesn't account for the clock source
    if(!rediscoverConnections()) {
        debugError("Could not rediscover plug connections\n");
    }

    return result;
}

} // end of namespace
