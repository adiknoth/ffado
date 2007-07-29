/*
 * Copyright (C)      2007 by Pieter Palmers
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

#include "avc_unit.h"
#include "avc_subunit.h"
#include "avc_plug.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "../general/avc_plug_info.h"
#include "../general/avc_extended_plug_info.h"
#include "../general/avc_subunit_info.h"
#include "../streamformat/avc_extended_stream_format.h"
#include "../util/avc_serialize.h"
#include "../avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include <iostream>
#include <sstream>

namespace AVC {

IMPL_DEBUG_MODULE( Unit, Unit, DEBUG_LEVEL_VERBOSE );

Unit::Unit( )
    : m_pPlugManager( new PlugManager( ) )
    , m_activeSyncInfo( 0 )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created AVC::Unit\n" );
}

Unit::~Unit()
{
    for ( SubunitVector::iterator it = m_subunits.begin();
          it != m_subunits.end();
          ++it )
    {
        delete *it;
    }
    for ( PlugConnectionVector::iterator it = m_plugConnections.begin();
          it != m_plugConnections.end();
          ++it )
    {
        delete *it;
    }
    for ( PlugVector::iterator it = m_pcrPlugs.begin();
          it != m_pcrPlugs.end();
          ++it )
    {
        delete *it;
    }
    for ( PlugVector::iterator it = m_externalPlugs.begin();
          it != m_externalPlugs.end();
          ++it )
    {
        delete *it;
    }
}

void
Unit::setVerboseLevel(int l)
{
    m_pPlugManager->setVerboseLevel(l);
}

bool
Unit::discover()
{

    if ( !enumerateSubUnits() ) {
        debugError( "Could not enumarate sub units\n" );
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
            subunit = new SubunitAudio( *this,
                                                subunitId );
            if ( !subunit ) {
                debugFatal( "Could not allocate SubunitAudio\n" );
                return false;
            }
            
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
            subunit = new SubunitMusic( *this,
                                        subunitId );
            if ( !subunit ) {
                debugFatal( "Could not allocate SubunitMusic\n" );
                return false;
            }
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

void
Unit::showDevice()
{
    m_pPlugManager->showPlugs();
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

template <typename T> bool serializeVector( Glib::ustring path,
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

template <typename T, typename VT> bool deserializeVector( Glib::ustring path,
                                                           Util::IODeserialize& deser,
                                                           Unit& avDevice,
                                                           VT& vec )
{
    int i = 0;
    bool bFinished = false;
    do {
        std::ostringstream strstrm;
        strstrm << path << i << "/";
        T* ptr = T::deserialize( strstrm.str(),
                                 deser,
                                 avDevice );
        if ( ptr ) {
            vec.push_back( ptr );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

bool
Unit::serializeSyncInfoVector( Glib::ustring basePath,
                                   Util::IOSerialize& ser,
                                   const SyncInfoVector& vec )
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
        result &= ser.write( strstrm.str() + "m_description", Glib::ustring( info.m_description ) );

        i++;
    }

    return result;
}

bool
Unit::deserializeSyncInfoVector( Glib::ustring basePath,
                                     Util::IODeserialize& deser,
                                     Unit& avDevice,
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
        Glib::ustring description;

        result  = deser.read( strstrm.str() + "m_source", sourceId );
        result &= deser.read( strstrm.str() + "m_destination", destinationId );
        result &= deser.read( strstrm.str() + "m_description", description );

        if ( result ) {
            SyncInfo syncInfo;
            syncInfo.m_source = avDevice.getPlugManager().getPlug( sourceId );
            syncInfo.m_destination = avDevice.getPlugManager().getPlug( destinationId );
            syncInfo.m_description = description;

            vec.push_back( syncInfo );
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return true;
}

static bool
deserializePlugUpdateConnections( Glib::ustring path,
                                    Util::IODeserialize& deser,
                                    PlugVector& vec )
{
    bool result = true;
    for ( PlugVector::iterator it = vec.begin();
          it != vec.end();
          ++it )
    {
        Plug* pPlug = *it;
        result &= pPlug->deserializeUpdate( path, deser );
    }
    return result;
}

bool
Unit::serialize( Glib::ustring basePath,
                     Util::IOSerialize& ser ) const
{

    bool result;
    result  = ser.write( basePath + "m_verboseLevel_unit", getDebugLevel() );
    result &= m_pPlugManager->serialize( basePath + "Plug", ser ); // serialize all av plugs
    result &= serializeVector( basePath + "PlugConnection", ser, m_plugConnections );
    result &= serializeVector( basePath + "Subunit", ser, m_subunits );
    result &= serializeSyncInfoVector( basePath + "SyncInfo", ser, m_syncInfos );

    int i = 0;
    for ( SyncInfoVector::const_iterator it = m_syncInfos.begin();
          it != m_syncInfos.end();
          ++it )
    {
        const SyncInfo& info = *it;
        if ( m_activeSyncInfo == &info ) {
            result &= ser.write( basePath + "m_activeSyncInfo",  i );
            break;
        }
        i++;
    }

    return result;
}

bool
Unit::deserialize( Glib::ustring basePath,
                   Unit* pDev,
                   Util::IODeserialize& deser,
                   Ieee1394Service& ieee1394Service )
{

    if ( pDev ) {
        bool result;
        
        int verboseLevel;
        result  = deser.read( basePath + "m_verboseLevel_unit", verboseLevel );
        setDebugLevel( verboseLevel );
        
        if (pDev->m_pPlugManager) delete pDev->m_pPlugManager;
        pDev->m_pPlugManager = PlugManager::deserialize( basePath + "Plug", deser, *pDev );
        if ( !pDev->m_pPlugManager ) {
            delete pDev;
            return 0;
        }
        result &= deserializePlugUpdateConnections( basePath + "Plug", deser, pDev->m_pcrPlugs );
        result &= deserializePlugUpdateConnections( basePath + "Plug", deser, pDev->m_externalPlugs );
        result &= deserializeVector<PlugConnection>( basePath + "PlugConnnection", deser, *pDev, pDev->m_plugConnections );
        result &= deserializeVector<Subunit>( basePath + "Subunit",  deser, *pDev, pDev->m_subunits );
        result &= deserializeSyncInfoVector( basePath + "SyncInfo", deser, *pDev, pDev->m_syncInfos );

        unsigned int i;
        result &= deser.read( basePath + "m_activeSyncInfo", i );

        if ( result ) {
            if ( i < pDev->m_syncInfos.size() ) {
                pDev->m_activeSyncInfo = &pDev->m_syncInfos[i];
            }
        }

    }

    return pDev;
}

} // end of namespace
