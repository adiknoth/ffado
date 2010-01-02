/*
 * Copyright (C) 2005-2009 by Pieter Palmers
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

#ifndef CONTROL_CROSSBAR_ROUTER_H
#define CONTROL_CROSSBAR_ROUTER_H

#include "debugmodule/debugmodule.h"
#include "ffadotypes.h"

#include "Element.h"

#include <vector>
#include <string>
#include <map>

namespace Control {

/*!
@brief Abstract Base class for Crossbar router elements

*/
class CrossbarRouter : public Element
{
public:
    CrossbarRouter(Element *p) : Element(p) {};
    CrossbarRouter(Element *p, std::string n) : Element(p, n) {};
    virtual ~CrossbarRouter() {};

    virtual void show() = 0;

    virtual stringlist getSourceNames() = 0;
    virtual stringlist getDestinationNames() = 0;

    virtual std::string getSourceForDestination(const std::string& dstname) = 0;
    virtual stringlist getDestinationsForSource(const std::string& srcname) = 0;

    virtual bool canConnect(const std::string& srcname, const std::string& dstname) = 0;
    virtual bool setConnectionState(const std::string& srcname, const std::string& dstname, const bool enable) = 0;
    virtual bool getConnectionState(const std::string& srcname, const std::string& dstname) = 0;

    virtual bool clearAllConnections() = 0;

    // peak metering
    virtual bool hasPeakMetering() = 0;
    virtual double getPeakValue(const std::string& dest) = 0;
    virtual std::map<std::string, double> getPeakValues() = 0;

protected:

};


}; // namespace Control

#endif // CONTROL_CROSSBAR_ROUTER_H
