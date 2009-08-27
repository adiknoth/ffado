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

#include "Element.h"

#include <vector>
#include <string>

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

    typedef std::vector<std::string> NameVector;
    typedef std::vector<std::string>::iterator NameVectorIterator;
    typedef std::vector<int> IntVector;
    typedef std::vector<int>::iterator IntVectorIterator;

    virtual std::string getSourceName(const int) = 0;
    virtual std::string getDestinationName(const int) = 0;
    virtual int getSourceIndex(std::string) = 0;
    virtual int getDestinationIndex(std::string) = 0;

    virtual NameVector getSourceNames() = 0;
    virtual NameVector getDestinationNames() = 0;

    virtual IntVector getDestinationsForSource(const int) = 0;
    virtual int getSourceForDestination(const int) = 0;

    virtual bool canConnect(const int source, const int dest) = 0;
    virtual bool setConnectionState(const int source, const int dest, const bool enable) = 0;
    virtual bool getConnectionState(const int source, const int dest) = 0;

    virtual bool canConnect(std::string, std::string) = 0;
    virtual bool setConnectionState(std::string, std::string, const bool enable) = 0;
    virtual bool getConnectionState(std::string, std::string) = 0;

    virtual bool clearAllConnections() = 0;

    virtual int getNbSources() = 0;
    virtual int getNbDestinations() = 0;

    // functions to access the entire routing map at once
    // idea is that the row/col nodes that are 1 get a routing entry
    virtual bool getConnectionMap(int *) = 0;
    virtual bool setConnectionMap(int *) = 0;

    // peak metering
    virtual bool hasPeakMetering() = 0;
    virtual double getPeakValue(const int source, const int dest) = 0;
    virtual bool getPeakValues(double &) = 0;

protected:

};


}; // namespace Control

#endif // CONTROL_CROSSBAR_ROUTER_H
