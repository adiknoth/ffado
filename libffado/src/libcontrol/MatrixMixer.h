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

#ifndef CONTROL_MATRIX_MIXER_H
#define CONTROL_MATRIX_MIXER_H

#include "debugmodule/debugmodule.h"

#include "Element.h"

#include <vector>
#include <string>

namespace Control {

/*!
@brief Abstract Base class for Matrix Mixer elements

*/
class MatrixMixer : public Element
{
public:
    MatrixMixer(Element *p) : Element(p) {};
    MatrixMixer(Element *p, std::string n) : Element(p, n) {};
    virtual ~MatrixMixer() {};

    virtual void show() = 0;

    // per-coefficient access
    virtual std::string getRowName(const int) = 0;
    virtual std::string getColName(const int) = 0;
    virtual int canWrite(const int, const int) = 0;
    virtual double setValue(const int, const int, const double) = 0;
    virtual double getValue(const int, const int) = 0;
    virtual int getRowCount() = 0;
    virtual int getColCount() = 0;

    // functions to access the entire coefficient map at once
    virtual bool getCoefficientMap(int &) = 0;
    virtual bool storeCoefficientMap(int &) = 0;

protected:

};


}; // namespace Control

#endif // CONTROL_MATRIX_MIXER_H
