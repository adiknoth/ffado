/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "OscResponse.h"

namespace OSC {

IMPL_DEBUG_MODULE( OscResponse, OscResponse, DEBUG_LEVEL_VERBOSE );

OscResponse::OscResponse()
    : m_type(eNone)
{

}

OscResponse::OscResponse(enum EType t)
    : m_type(t)
{

}
OscResponse::OscResponse(OscMessage m)
    : m_type(OscResponse::eMessage)
    , m_message(m)
{

}
OscResponse::~OscResponse() {

}

} // end of namespace OSC
