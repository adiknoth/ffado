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

#ifndef CONTROL_NICKNAME_H
#define CONTROL_NICKNAME_H

#include "debugmodule/debugmodule.h"

#include "BasicElements.h"

#include <string>

class FFADODevice;

namespace Control {

/*!
@brief Nickname control element
@note this is a workaround and should be done more elegant

*/
class Nickname : public Text
{
public:
    Nickname(FFADODevice &);
    virtual ~Nickname() {};

    virtual bool setValue(std::string v);
    virtual std::string getValue();

    virtual void show();

protected:
    FFADODevice &m_Device;
};

}; // namespace Control

#endif // CONTROL_NICKNAME_H
