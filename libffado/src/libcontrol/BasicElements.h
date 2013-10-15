/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#ifndef CONTROL_BASICELEMENTS_H
#define CONTROL_BASICELEMENTS_H

#include "debugmodule/debugmodule.h"

#include <vector>
#include <string>

#include "Element.h"

namespace Control {

/*!
@brief Base class for contignous control elements
*/
class Continuous
: public Element
{
public:
    Continuous(Element *p) : Element(p) {};
    Continuous(Element *p, std::string n) : Element(p, n) {};
    virtual ~Continuous() {};

    virtual bool setValue(double v) = 0;
    virtual double getValue() = 0;
    virtual bool setValue(int idx, double v) = 0;
    virtual double getValue(int idx) = 0;

    virtual double getMinimum() = 0;
    virtual double getMaximum() = 0;
};

/*!
@brief Base class for discrete control elements
*/
class Discrete
: public Element
{
public:
    Discrete(Element *p) : Element(p) {};
    Discrete(Element *p, std::string n) : Element(p, n) {};
    virtual ~Discrete() {};

    virtual bool setValue(int v) = 0;
    virtual int getValue() = 0;
    virtual bool setValue(int idx, int v) = 0;
    virtual int getValue(int idx) = 0;

    virtual int getMinimum() = 0;
    virtual int getMaximum() = 0;

};

/*!
@brief Base class for textual control elements
*/
class Text
: public Element
{
public:
    Text(Element *p) : Element(p) {};
    Text(Element *p, std::string n) : Element(p, n) {};
    virtual ~Text() {};

    virtual bool setValue(std::string v) = 0;
    virtual std::string getValue() = 0;
};

/*!
@brief Base class for register access control elements
*/
class Register
: public Element
{
public:
    Register(Element *p) : Element(p) {};
    Register(Element *p, std::string n) : Element(p, n) {};
    virtual ~Register() {};

    virtual bool setValue(uint64_t addr, uint64_t value) = 0;
    virtual uint64_t getValue(uint64_t addr) = 0;
};

/*!
@brief Base class for basic enumerated control elements
*/
class Enum
: public Element
{
public:
    Enum(Element *p) : Element(p) {};
    Enum(Element *p, std::string n) : Element(p, n) {};
    virtual ~Enum() {};

    virtual bool select(int idx) = 0;
    virtual int selected() = 0;
    virtual int count() = 0;
    virtual std::string getEnumLabel(int idx) = 0;
    ///> Some devices update their internal configuration on select
    virtual bool devConfigChanged(int idx) { return false; };
};

/*!
@brief Base class for attribute enumerated control elements

The idea of this is that one can have a set of config values
available for a certain enum choice.

Example: for clock source selection:
idx Label     signal  locked  available
  0 WordClock   0       0        1
  1 S/PDIF      1       0        1
  ...

Attributes:
 0 signal
 1 locked
 2 available

*/
class AttributeEnum
: public Enum
{
public:
    AttributeEnum(Element *p) : Enum(p) {};
    AttributeEnum(Element *p, std::string n) : Enum(p, n) {};
    virtual ~AttributeEnum() {};

    virtual int attributeCount() = 0;
    ///> get a specific attribute value for the selected enum 
    virtual std::string getAttributeValue(int attridx) = 0;
    ///> get the name of the attribute with a certain index
    virtual std::string getAttributeName(int attridx) = 0;
};
/*!
@brief Base class for basic boolean control elements
*/
class Boolean
: public Element
{
public:
    Boolean(Element *p) : Element(p) {};
    Boolean(Element *p, std::string n) : Element(p, n) {};
    virtual ~Boolean() {};

    virtual bool select(bool) = 0;
    virtual bool selected() = 0;
    virtual std::string getBooleanLabel(bool n) {
        if (n) return "True";
        return "False";
    }
};

}; // namespace Control

#endif // CONTROL_BASICELEMENTS_H
