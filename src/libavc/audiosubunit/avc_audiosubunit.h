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

#ifndef AVC_AUDIOSUBUNIT_H
#define AVC_AUDIOSUBUNIT_H

#include "debugmodule/debugmodule.h"

#include "../general/avc_subunit.h"

#include <vector>

#include "../audiosubunit/avc_function_block.h"

namespace AVC {

class Unit;
// /////////////////////////////

class SubunitAudio: public Subunit {
 public:
    SubunitAudio( Unit& avDevice,
                  subunit_t id );
    SubunitAudio();
    virtual ~SubunitAudio();

    virtual bool discover();

    virtual const char* getName();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const { return true; }
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   Unit& avDevice ) { return true; }

};

}

#endif
