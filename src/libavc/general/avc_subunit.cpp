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
#include "../util/avc_serialize.h"

#include <sstream>

namespace AVC {

IMPL_DEBUG_MODULE( Subunit, Subunit, DEBUG_LEVEL_VERBOSE );

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

bool
Subunit::discover()
{
    // There is nothing we can discover for a generic subunit
    // Maybe the plugs, but there are multiple ways of doing that.
    // subclasses should implement this
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
