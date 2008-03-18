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
    Continuous();
    Continuous(std::string n);
    virtual ~Continuous() {};
    
    virtual bool setValue(double v);
    virtual double getValue();

    virtual void show();

private:
    double m_Value;
};

/*!
@brief Base class for discrete control elements
*/
class Discrete
: public Element
{
public:
    Discrete();
    Discrete(std::string n);
    virtual ~Discrete() {};
    
    virtual bool setValue(int v);
    virtual int getValue();

    virtual void show();

private:
    int m_Value;
};

/*!
@brief Base class for textual control elements
*/
class Text
: public Element
{
public:
    Text();
    Text(std::string n);
    virtual ~Text() {};

    virtual bool setValue(std::string v);
    virtual std::string getValue();

    virtual void show();

private:
    std::string m_Value;
};

/*!
@brief Base class for basic enumerated control elements
*/
class Enum
: public Element
{
public:
    Enum();
    Enum(std::string n);
    virtual ~Enum() {};

    virtual bool select(int idx);
    virtual int selected();
    virtual int count();
    virtual std::string getEnumLabel(int idx);

    virtual void show();
//private: // HACK
    int m_selected;
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
    AttributeEnum();
    AttributeEnum(std::string n);
    virtual ~AttributeEnum() {};

    virtual int attributeCount();
    ///> get a specific attribute value for the selected enum 
    virtual std::string getAttributeValue(int attridx);
    ///> get the name of the attribute with a certain index
    virtual std::string getAttributeName(int attridx);

    virtual void show();
private:
};

}; // namespace Control

#endif // CONTROL_BASICELEMENTS_H
