/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "OptionContainer.h"

#include <iostream>
#include <sstream>

namespace Util {

IMPL_DEBUG_MODULE( OptionContainer, OptionContainer, DEBUG_LEVEL_NORMAL );

OptionContainer::Option::Option()
    : m_Name(""),
    m_stringValue(""),
    m_boolValue(false),
    m_doubleValue(0.0),
    m_intValue(0),
    m_uintValue(0),
    m_Type(EInvalid)
{}

OptionContainer::Option::Option(std::string n)
    : m_Name(n),
    m_stringValue(""),
    m_boolValue(false),
    m_doubleValue(0.0),
    m_intValue(0),
    m_uintValue(0),
    m_Type(EInvalid)
{}

OptionContainer::Option::Option(std::string n, std::string v)
    : m_Name(n),
    m_stringValue(v),
    m_boolValue(false),
    m_doubleValue(0.0),
    m_intValue(0),
    m_uintValue(0),
    m_Type(EString)
{}

OptionContainer::Option::Option(std::string n, bool v)
    : m_Name(n),
    m_stringValue(""),
    m_boolValue(v),
    m_doubleValue(0.0),
    m_intValue(0),
    m_uintValue(0),
    m_Type(EBool)
{}

OptionContainer::Option::Option(std::string n, double v)
    : m_Name(n),
    m_stringValue(""),
    m_boolValue(false),
    m_doubleValue(v),
    m_intValue(0),
    m_uintValue(0),
    m_Type(EDouble)
{}

OptionContainer::Option::Option(std::string n, int64_t v)
    : m_Name(n),
    m_stringValue(""),
    m_boolValue(false),
    m_doubleValue(0.0),
    m_intValue(v),
    m_uintValue(0),
    m_Type(EInt)
{}

OptionContainer::Option::Option(std::string n, uint64_t v)
    : m_Name(n),
    m_stringValue(""),
    m_boolValue(false),
    m_doubleValue(0.0),
    m_intValue(0),
    m_uintValue(v),
    m_Type(EUInt)
{}

void OptionContainer::Option::set(std::string v) { m_stringValue = v; m_Type=EString;}
void OptionContainer::Option::set(bool v)        { m_boolValue = v; m_Type=EBool;}
void OptionContainer::Option::set(double v)      { m_doubleValue = v; m_Type=EDouble;}
void OptionContainer::Option::set(int64_t v)     { m_intValue = v; m_Type=EInt;}
void OptionContainer::Option::set(uint64_t v)    { m_uintValue = v; m_Type=EUInt;}

bool
OptionContainer::Option::serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const
{
    bool result;
    result  = ser.write( basePath + "m_Name", Glib::ustring(m_Name) );
    result &= ser.write( basePath + "m_stringValue", Glib::ustring(m_stringValue) );
    result &= ser.write( basePath + "m_boolValue", m_boolValue );
    result &= ser.write( basePath + "m_doubleValue", m_doubleValue );
    result &= ser.write( basePath + "m_intValue", m_intValue );
    result &= ser.write( basePath + "m_uintValue", m_uintValue );
    result &= ser.write( basePath + "m_Type", m_Type );

    return result;
}


OptionContainer::Option
OptionContainer::Option::deserialize( Glib::ustring basePath,
                     Util::IODeserialize& deser )
{
    bool result;
    Option op=Option();
    Glib::ustring tmpstr;

    result  = deser.read( basePath + "m_Name", tmpstr );
    op.m_Name = tmpstr;
    result &= deser.read( basePath + "m_stringValue", tmpstr );
    op.m_stringValue = tmpstr;
    result &= deser.read( basePath + "m_boolValue", op.m_boolValue );
    result &= deser.read( basePath + "m_doubleValue", op.m_doubleValue );
    result &= deser.read( basePath + "m_intValue", op.m_intValue );
    result &= deser.read( basePath + "m_uintValue", op.m_uintValue );
    result &= deser.read( basePath + "m_Type", op.m_Type );

    if(result) {
        return op;
    } else {
        return Option();
    }
}

// ------------------------
OptionContainer::OptionContainer() {

}

OptionContainer::~OptionContainer() {

}

// -------------- SETTERS --------------------
bool OptionContainer::setOption(std::string name, std::string v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set(v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, bool v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set(v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, double v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set(v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, int64_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((int64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, uint64_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((uint64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, int32_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((int64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, uint32_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((uint64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, int16_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((int64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, uint16_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((uint64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, int8_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((int64_t)v);
    return setOption(o);
}

bool OptionContainer::setOption(std::string name, uint8_t v) {
    Option o=getOption(name);
    if (o.getType()==OptionContainer::Option::EInvalid) return false;
    o.set((uint64_t)v);
    return setOption(o);
}

// -------------- GETTERS --------------------

bool OptionContainer::getOption(std::string name, std::string &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EString) return false;
    v=o.getString();
    return true;
}

bool OptionContainer::getOption(std::string name, bool &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EBool) return false;
    v=o.getBool();
    return true;
}

bool OptionContainer::getOption(std::string name, double &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EDouble) return false;
    v=o.getDouble();
    return true;
}
bool OptionContainer::getOption(std::string name, float &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EDouble) return false;
    v=o.getDouble();
    return true;
}

bool OptionContainer::getOption(std::string name, int64_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EInt) return false;
    v=o.getInt();
    return true;
}
bool OptionContainer::getOption(std::string name, int32_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EInt) return false;
    v=o.getInt();
    return true;
}
bool OptionContainer::getOption(std::string name, int16_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EInt) return false;
    v=o.getInt();
    return true;
}
bool OptionContainer::getOption(std::string name, int8_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EInt) return false;
    v=o.getInt();
    return true;
}

bool OptionContainer::getOption(std::string name, uint64_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EUInt) return false;
    v=o.getUInt();
    return true;
}
bool OptionContainer::getOption(std::string name, uint32_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EUInt) return false;
    v=o.getUInt();
    return true;
}
bool OptionContainer::getOption(std::string name, uint16_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EUInt) return false;
    v=o.getUInt();
    return true;
}
bool OptionContainer::getOption(std::string name, uint8_t &v) {
    Option o=getOption(name);
    if (o.getType()!=OptionContainer::Option::EUInt) return false;
    v=o.getUInt();
    return true;
}

OptionContainer::Option::EType OptionContainer::getOptionType(std::string name) {
    Option o=getOption(name);
    return o.getType();

}

OptionContainer::Option OptionContainer::getOption(std::string name) {
    int i=findOption(name);
    if (i<0) {
        return Option();
    } else {
        return m_Options.at(i);
    }
}

bool OptionContainer::addOption(Option o) {
    if (o.getType()==OptionContainer::Option::EInvalid) {
        return false;
    }
    if (hasOption(o)){
        return false;
    }

    m_Options.push_back(o);

    return true;
}

bool OptionContainer::setOption(Option o) {
    int i=findOption(o);
    if (i<0) {
        return false;
    } else {
        m_Options.erase(m_Options.begin()+i);
        m_Options.push_back(o);
        return true;
    }
}

bool OptionContainer::removeOption(Option o) {
    int i=findOption(o);
    if (i<0) {
        return false;
    } else {
        m_Options.erase(m_Options.begin()+i);
        return true;
    }
}

bool OptionContainer::removeOption(std::string name) {
    int i=findOption(name);
    if (i<0) {
        return false;
    } else {
        m_Options.erase(m_Options.begin()+i);
        return true;
    }
}

bool OptionContainer::hasOption(std::string name) {
    return (findOption(name) >= 0);
}

bool OptionContainer::hasOption(Option o) {
    return (findOption(o) >= 0);
}

int OptionContainer::findOption(Option o) {
    int i=0;
    for ( OptionVectorIterator it = m_Options.begin();
      it != m_Options.end();
      ++it )
    {
        if((*it).getName() == o.getName()) {
            return i;
        }
        i++;
    }
    return -1;
}

int OptionContainer::findOption(std::string name) {
    int i=0;
    for ( OptionVectorIterator it = m_Options.begin();
      it != m_Options.end();
      ++it )
    {
        if((*it).getName() == name) {
            return i;
        }
        i++;
    }
    return -1;
}

// serialization support

bool
OptionContainer::serializeOptions( Glib::ustring basePath,
                                   Util::IOSerialize& ser) const
{
    bool result = true;
    int i = 0;

    for ( OptionVector::const_iterator it = m_Options.begin();
          it != m_Options.end();
          ++it )
    {
        const Option& pOption = *it;

        std::ostringstream strstrm;
        strstrm << basePath << "/" << "Option" << i;
        result &= pOption.serialize( strstrm.str() + "/", ser );
        i++;
    }

    return result;
}

bool
OptionContainer::deserializeOptions( Glib::ustring basePath,
                                     Util::IODeserialize& deser,
                                     OptionContainer& container)
{
    int i = 0;
    bool bFinished = false;
    bool result=true;
    do {
        std::ostringstream strstrm;
        strstrm << basePath << "/" << "Option" << i;

        Option pOption = Option::deserialize( strstrm.str() + "/",
                                              deser );
        if ( pOption.getType() != Option::EInvalid ) {
            result &= container.addOption(pOption);
            i++;
        } else {
            bFinished = true;
        }
    } while ( !bFinished );

    return result;
}


} // end of namespace Util
