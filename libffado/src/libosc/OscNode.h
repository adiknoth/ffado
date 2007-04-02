/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef __FREEBOB_OSCNODE__
#define __FREEBOB_OSCNODE__

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

#endif /* __FREEBOB_OSCNODE__ */


