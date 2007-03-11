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

#include "OscArgument.h"

namespace OSC {

IMPL_DEBUG_MODULE( OscArgument, OscArgument, DEBUG_LEVEL_VERBOSE );

OscArgument::OscArgument(int32_t i) 
    : m_isInt(true)
    , m_intVal(i)
    , m_isInt64(false)
    , m_int64Val(0)
    , m_isFloat(false)
    , m_floatVal(0.0)
    , m_isString(false)
    , m_stringVal("")
{}

OscArgument::OscArgument(int64_t i) 
    : m_isInt(false)
    , m_intVal(0)
    , m_isInt64(true)
    , m_int64Val(i)
    , m_isFloat(false)
    , m_floatVal(0.0)
    , m_isString(false)
    , m_stringVal("")
{}

OscArgument::OscArgument(float f) 
    : m_isInt(false)
    , m_intVal(0)
    , m_isInt64(false)
    , m_int64Val(0)
    , m_isFloat(true)
    , m_floatVal(f)
    , m_isString(false)
    , m_stringVal("")
{}

OscArgument::OscArgument(string s) 
    : m_isInt(false)
    , m_intVal(0)
    , m_isInt64(false)
    , m_int64Val(0)
    , m_isFloat(false)
    , m_floatVal(0.0)
    , m_isString(true)
    , m_stringVal(s)
{}

OscArgument::~OscArgument()
{}

bool
OscArgument::operator == ( const OscArgument& rhs )
{
    if(m_isInt && rhs.m_isInt) {
        return m_intVal == rhs.m_intVal;
    } else if(m_isInt64 && rhs.m_isInt64) {
        return m_int64Val == rhs.m_int64Val;
    } else if(m_isFloat && rhs.m_isFloat) {
        return m_floatVal == rhs.m_floatVal;
    } else if(m_isString && rhs.m_isString) {
        return m_stringVal == rhs.m_stringVal;
    } else {
        return false;
    }
}

void
OscArgument::print()
{
    debugOutput(DEBUG_LEVEL_NORMAL, " Argument ");
    if(m_isInt) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, " Int    : %ld\n", m_intVal);
    } else if(m_isInt64) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, " Int64  : %lld\n", m_int64Val);
    } else if(m_isFloat) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, " Float  : %f\n", m_floatVal);
    } else if(m_isString) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, " String : %s\n", m_stringVal.c_str());
    } else {
        debugOutputShort(DEBUG_LEVEL_NORMAL, " Empty\n");
    }
}

} // end of namespace OSC
