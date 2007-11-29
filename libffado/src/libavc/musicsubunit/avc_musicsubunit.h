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

#ifndef AVC_MUSICSUBUNIT_H
#define AVC_MUSICSUBUNIT_H

#include <vector>
#include "libutil/serialize.h"

namespace AVC {

class Unit;
class Plug;
class AVCMusicStatusDescriptor;

// /////////////////////////////

class SubunitMusic: public Subunit {
 public:
    SubunitMusic( Unit& avDevice,
                  subunit_t id );
    SubunitMusic();
    virtual ~SubunitMusic();
    
    virtual bool discover();
    virtual bool initPlugFromDescriptor( Plug& plug );

    virtual bool loadDescriptors();
    
    virtual void setVerboseLevel(int l);
    virtual const char* getName();
protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   Unit& avDevice );

    class AVCMusicStatusDescriptor*  m_status_descriptor;
};

}

#endif
