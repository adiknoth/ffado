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

#ifndef __FFADO_OSCARGUMENT__
#define __FFADO_OSCARGUMENT__

#include "../debugmodule/debugmodule.h"

#include <string>

using namespace std;

namespace OSC {

class OscArgument {

public:

    OscArgument(int32_t);
    OscArgument(int64_t);
    OscArgument(float);
    OscArgument(string);
    virtual ~OscArgument();

    bool operator == ( const OscArgument& rhs );

    int32_t getInt() { return m_intVal;};
    bool isInt() { return m_isInt;};
    int64_t getInt64() { return m_int64Val;};
    bool isInt64() { return m_isInt64;};
    float getFloat() { return m_floatVal;};
    bool isFloat() { return m_isFloat;};
    string& getString() { return m_stringVal;};
    bool isString() { return m_isString;};

    void print();

protected:
    bool m_isInt;
    int32_t m_intVal;

    bool m_isInt64;
    int64_t m_int64Val;

    bool m_isFloat;
    float m_floatVal;

    bool m_isString;
    string m_stringVal;

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace OSC

#endif /* __FFADO_OSCARGUMENT__ */


