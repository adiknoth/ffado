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

#ifndef __FFADO_OSCNODE__
#define __FFADO_OSCNODE__

#include "../debugmodule/debugmodule.h"

#include <string>
#include <vector>

#include "OscMessage.h"
#include "OscResponse.h"

using namespace std;

namespace OSC {
class OscMessage;
class OscResponse;

class OscNode {

public:

    OscNode();
    OscNode(std::string);
    OscNode(std::string, bool);
    virtual ~OscNode();

    bool addChildOscNode(OscNode *);
    bool addChildOscNode(OscNode *n, string path);
    bool removeChildOscNode(OscNode *);

    string getOscBase() {return m_oscBase;};

    void setOscNodeAutoDelete(bool b) {m_oscAutoDelete=b;};
    bool doOscNodeAutoDelete() {return m_oscAutoDelete;};

    void printOscNode(string path);
    void printOscNode();

    virtual OscResponse processOscMessage(OscMessage *m);
    virtual OscResponse processOscMessage(string path, OscMessage *m);

protected:
    void setOscBase(string s) {m_oscBase=s;};

private:
    OscResponse processOscMessageDefault(OscMessage *m, OscResponse);
    OscMessage oscListChildren(OscMessage);

    string m_oscBase;
    bool m_oscAutoDelete;

    typedef vector< OscNode * > OscNodeVector;
    typedef vector< OscNode * >::iterator OscNodeVectorIterator;
    OscNodeVector m_ChildNodes;

public:
    void setVerboseLevel(int l);
protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace OSC

#endif /* __FFADO_OSCNODE__ */


