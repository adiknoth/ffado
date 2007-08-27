/*
 * Copyright (C) 2007 by Pieter Palmers
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
class Contignous
: public Element
{
public:
    Contignous();
    Contignous(std::string n);
    virtual ~Contignous() {};
    
    virtual bool setValue(double v);
    virtual double getValue();

    virtual void show();

private:
    double m_Value;
};

}; // namespace Control

#endif // CONTROL_BASICELEMENTS_H
