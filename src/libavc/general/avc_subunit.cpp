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


#include "avc_subunit.h"
#include "../general/avc_unit.h"
#include "../general/avc_plug.h"

#include "libieee1394/configrom.h"

#include "../general/avc_plug_info.h"
#include "../streamformat/avc_extended_stream_format.h"
#include "libutil/cmd_serialize.h"

#include <sstream>

namespace AVC {

IMPL_DEBUG_MODULE( Subunit, Subunit, DEBUG_LEVEL_NORMAL );

////////////////////////////////////////////

Subunit::Subunit( Unit& unit,
                  ESubunitType type,
                  subunit_t id )
    : m_unit( &unit )
    , m_sbType( type )
    , m_sbId( id )
{

}

Subunit::Subunit()
{
}

Subunit::~Subunit()
{
    for ( PlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        delete *it;
    }
}

void
Subunit::setVerboseLevel(int l)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
    setDebugLevel(l);
}


Plug *
Subunit::createPlug( AVC::Unit* unit,
                     AVC::Subunit* subunit,
                     AVC::function_block_type_t functionBlockType,
                     AVC::function_block_type_t functionBlockId,
                     AVC::Plug::EPlugAddressType plugAddressType,
                     AVC::Plug::EPlugDirection plugDirection,
                     AVC::plug_id_t plugId )
{

    return new Plug( unit,
                     subunit,
                     functionBlockType,
                     functionBlockId,
                     plugAddressType,
                     plugDirection,
                     plugId );
}

bool
Subunit::discover()
{
    if ( !discoverPlugs() ) {
        debugError( "plug discovery failed\n" );
        return false;
    }
    return true;
}

bool
Subunit::discoverPlugs()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering plugs...\n");
    
    PlugInfoCmd plugInfoCmd( getUnit().get1394Service(),
                             PlugInfoCmd::eSF_SerialBusIsochronousAndExternalPlug );
    plugInfoCmd.setNodeId( getUnit().getConfigRom().getNodeId() );
    plugInfoCmd.setCommandType( AVCCommand::eCT_Status );
    plugInfoCmd.setSubunitType( getSubunitType() );
    plugInfoCmd.setSubunitId( getSubunitId() );
    plugInfoCmd.setVerbose( getDebugLevel() );

    if ( !plugInfoCmd.fire() ) {
        debugError( "plug info command failed\n" );
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "number of source plugs = %d\n",
                 plugInfoCmd.m_sourcePlugs );
    debugOutput( DEBUG_LEVEL_VERBOSE, "number of destination output "
                 "plugs = %d\n", plugInfoCmd.m_destinationPlugs );

    if ( !discoverPlugs( Plug::eAPD_Input,
                         plugInfoCmd.m_destinationPlugs ) )
    {
        debugError( "destination plug discovering failed\n" );
        return false;
    }

    if ( !discoverPlugs(  Plug::eAPD_Output,
                          plugInfoCmd.m_sourcePlugs ) )
    {
        debugError( "source plug discovering failed\n" );
        return false;
    }

    return true;
}

bool
Subunit::discoverConnections()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Discovering connections...\n");
    
    for ( PlugVector::iterator it = getPlugs().begin();
          it != getPlugs().end();
          ++it )
    {
        Plug* plug = *it;
        if ( !plug->discoverConnections() ) {
            debugError( "plug connection discovering failed ('%s')\n",
                        plug->getName() );
            return false;
        }
    }

    return true;
}

bool
Subunit::discoverPlugs(Plug::EPlugDirection plugDirection,
                                      plug_id_t plugMaxId )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Discovering plugs for direction %d...\n", plugDirection);
    for ( int plugIdx = 0;
          plugIdx < plugMaxId;
          ++plugIdx )
    {
        Plug* plug = createPlug(  &getUnit(),
                                &getSubunit(),
                                0xff,
                                0xff,
                                Plug::eAPA_SubunitPlug,
                                plugDirection,
                                plugIdx );
        if ( !plug ) {
            debugError( "plug creation failed\n" );
            return false;
        }
        
        plug->setVerboseLevel(getDebugLevel());
        
        if ( !plug->discover() ) {
            debugError( "plug discover failed\n" );
            return false;
        }

        debugOutput( DEBUG_LEVEL_VERBOSE, "plug '%s' found\n",
                     plug->getName() );
        getPlugs().push_back( plug );
    }
    return true;
}

bool
Subunit::addPlug( Plug& plug )
{
    m_plugs.push_back( &plug );
    return true;
}


Plug*
Subunit::getPlug(Plug::EPlugDirection direction, plug_id_t plugId)
{
    for ( PlugVector::iterator it = m_plugs.begin();
          it != m_plugs.end();
          ++it )
    {
        Plug* plug = *it;
        if ( ( plug->getPlugId() == plugId )
            && ( plug->getDirection() == direction ) )
        {
            return plug;
        }
    }
    return 0;
}

bool
Subunit::initPlugFromDescriptor( Plug& plug )
{
    debugError("plug loading from descriptor not implemented\n");
    return false;
}

bool
Subunit::serialize( Glib::ustring basePath,
                                   Util::IOSerialize& ser ) const
{
    bool result;

    result  = ser.write( basePath + "m_sbType", m_sbType );
    result &= ser.write( basePath + "m_sbId", m_sbId );
    result &= ser.write( basePath + "m_verboseLevel", getDebugLevel() );
    result &= serializeChild( basePath, ser );

    return result;
}

Subunit*
Subunit::deserialize( Glib::ustring basePath,
                      Util::IODeserialize& deser,
                      Unit& unit )
{
    bool result;
    ESubunitType sbType;

    if ( !deser.isExisting( basePath + "m_sbType" ) ) {
        return 0;
    }

    result  = deser.read( basePath + "m_sbType", sbType );

    Subunit* pSubunit = 0;
    
    #warning FIXME: The derived class should be creating these
    // FIXME: The derived class should be creating these, such that discover() can become pure virtual
    switch( sbType ) {
    case eST_Audio:
        pSubunit = new SubunitAudio;
        break;
    case eST_Music:
        pSubunit = new SubunitMusic;
        break;
    default:
        pSubunit = 0;
    }

    if ( !pSubunit ) {
        return 0;
    }

    pSubunit->m_unit = &unit;
    pSubunit->m_sbType = sbType;
    result &= deser.read( basePath + "m_sbId", pSubunit->m_sbId );
    int verboseLevel;
    result &= deser.read( basePath + "m_verboseLevel", verboseLevel );
    setDebugLevel(verboseLevel);
    result &= pSubunit->deserializeChild( basePath, deser, unit );

    if ( !result ) {
        delete pSubunit;
        return 0;
    }

    return pSubunit;
}

}
