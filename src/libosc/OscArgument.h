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

    /**
     * @brief Returns the argument's integer value
     * @return 32 bit integer argument value
     */
    int32_t getInt() { return m_intVal;};
    /**
     * @brief Returns the argument's value converted to integer.
     * if the argument type is float or int64 it will be 
     * cast (and trunctacted) to 32bit integer
     * @return 
     */
    int32_t toInt();
    /**
     * @brief Is the argument a 32 bit integer?
     * @return true if the argument is a 32 bit integer 
     */
    bool isInt() { return m_isInt;};
    
    /**
     * @brief Returns the argument's 64 bit integer value
     * @return 64 bit integer argument value 
     */
    int64_t getInt64() { return m_int64Val;};
    /**
     * @brief Returns the argument's value converted to a 64bit integer.
     * if the argument type is float or int it will be 
     * cast to a 64 bit integer
     * @return 
     */
    int64_t toInt64();
    /**
     * @brief Is the argument a 64 bit integer?
     * @return true if the argument is a 64 bit integer 
     */
    bool isInt64() { return m_isInt64;};
    
    /**
     * @brief Returns the argument's float value
     * @return float argument value 
     */
    float getFloat() { return m_floatVal;};
    /**
     * @brief Returns the argument's value converted to float.
     * if the argument type is int32 or int64 it will be 
     * cast to a float
     * @return 
     */
    float toFloat();
    /**
     * @brief Is the argument a float?
     * @return true if the argument is a float 
     */
    bool isFloat() { return m_isFloat;};
    
    /**
     * @brief Returns the argument's string value
     * @return string argument value 
     */
    string& getString() { return m_stringVal;};
    /**
     * @brief Is the argument a string?
     * @return true if the argument is a string 
     */
    bool isString() { return m_isString;};
    
    
    /**
     * @brief Is the argument a numeric?
     * @return true if the argument is a numeric value 
     */
    bool isNumeric() { return (m_isInt|| m_isInt64 || m_isFloat);};
    

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


