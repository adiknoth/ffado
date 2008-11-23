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

#ifndef CONTROL_CLOCK_SELECT_H
#define CONTROL_CLOCK_SELECT_H

#include "debugmodule/debugmodule.h"

#include "BasicElements.h"

#include <vector>
#include <string>

class FFADODevice;

namespace Control {

/*!
@brief Clock selection control element
@note this is a workaround and should be done more elegant

*/
class ClockSelect : public AttributeEnum
{
public:
    ClockSelect(FFADODevice &);
    virtual ~ClockSelect() {};

    virtual bool select(int idx);
    virtual int selected();
    virtual int count();
    virtual std::string getEnumLabel(int idx);

    virtual int attributeCount();
    ///> get a specific attribute value for the selected enum 
    virtual std::string getAttributeValue(int attridx);
    ///> get the name of the attribute with a certain index
    virtual std::string getAttributeName(int attridx);

    virtual bool canChangeValue();

    virtual void show();

protected:
    FFADODevice &m_Device;
};

/*!
@brief Samplerate selection control element
@note this is a workaround and should be done more elegant
*/
class SamplerateSelect
: public Enum
{
public:
    SamplerateSelect(FFADODevice &);
    virtual ~SamplerateSelect() {};

    virtual bool select(int idx);
    virtual int selected();
    virtual int count();
    virtual std::string getEnumLabel(int idx);

    virtual bool canChangeValue();

    virtual void show();

protected:
    FFADODevice &m_Device;
};

/*!
 * @brief Stream status control element
 */
class StreamingStatus
: public Enum
{
public:
    StreamingStatus(FFADODevice &);
    virtual ~StreamingStatus() {};

    virtual bool select(int idx);
    virtual int selected();
    virtual int count();
    virtual std::string getEnumLabel(int idx);

    virtual bool canChangeValue();

    virtual void show();

protected:
    FFADODevice &m_Device;
};

}; // namespace Control

#endif // CONTROL_CLOCK_SELECT_H
