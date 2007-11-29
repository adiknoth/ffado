/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
