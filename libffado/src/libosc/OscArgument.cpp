/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

int32_t 
OscArgument::toInt() {
    if(m_isInt) {
        return m_intVal;
    } else if(m_isInt64) {
        return (int32_t)(m_int64Val & 0xFFFFFFFF);
    } else if(m_isFloat) {
        return (int32_t)(m_floatVal);
    } else {
        return 0;
    }
}

int64_t 
OscArgument::toInt64() {
    if(m_isInt) {
        return (int64_t)(m_intVal);
    } else if(m_isInt64) {
        return m_int64Val;
    } else if(m_isFloat) {
        return (int64_t)(m_floatVal);
    } else {
        return 0;
    }
}

float 
OscArgument::toFloat() {
    if(m_isInt) {
        return (float)(m_intVal);
    } else if(m_isInt64) {
        return (float)(m_int64Val);
    } else if(m_isFloat) {
        return m_floatVal;
    } else {
        return 0;
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
