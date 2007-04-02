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
#ifndef __FREEBOB_OSCARGUMENT__
#define __FREEBOB_OSCARGUMENT__

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

#endif /* __FREEBOB_OSCARGUMENT__ */


