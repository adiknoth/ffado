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

#ifndef AVC_SUBUNIT_H
#define AVC_SUBUNIT_H

#include "debugmodule/debugmodule.h"

#include "../avc_definitions.h"
#include "../general/avc_plug.h"
#include "../general/avc_extended_subunit_info.h"
#include "../general/avc_generic.h"
#include "../audiosubunit/avc_function_block.h"

#include <vector>

#warning merge with bebob functionblock
#include "bebob/bebob_functionblock.h"

namespace AVC {

class Unit;

class Subunit {
 public:
    Subunit( Unit& avDevice,
             ESubunitType type,
             subunit_t id );
    virtual ~Subunit();

    virtual bool discover();
    virtual bool discoverConnections()
    {//FIXME:
    #warning FIXME
    };

    virtual const char* getName() = 0;

    subunit_t getSubunitId()
    { return m_sbId; }
    ESubunitType getSubunitType()
    { return m_sbType; }

    bool addPlug( Plug& plug );

    PlugVector& getPlugs()
    { return m_plugs; }
    Plug* getPlug(Plug::EPlugDirection direction, plug_id_t plugId);


    Unit& getUnit() const
        { return *m_unit; }


    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static Subunit* deserialize( Glib::ustring basePath,
                     Util::IODeserialize& deser,
                                         Unit& avDevice );
 protected:
    Subunit();

//     bool discoverPlugs();
//     bool discoverPlugs(Plug::EPlugDirection plugDirection,
//                        plug_id_t plugMaxId );
// 
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const = 0;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   Unit& avDevice ) = 0;

 protected:
    Unit*           m_unit;
    ESubunitType    m_sbType;
    subunit_t       m_sbId;

    PlugVector             m_plugs;

    DECLARE_DEBUG_MODULE;
};

typedef std::vector<Subunit*> SubunitVector;

}

#endif
