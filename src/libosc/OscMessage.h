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
 * 
 *
 */
#ifndef __FREEBOB_OSCMESSAGE__
#define __FREEBOB_OSCMESSAGE__

#include <string>
#include <lo/lo.h>

#include "OscArgument.h"

#include "../debugmodule/debugmodule.h"

using namespace std;

namespace OSC {

class OscMessage {

public:

    OscMessage();
    OscMessage(string path, const char* types, lo_arg** argv, int argc);
    virtual ~OscMessage();
    
    lo_message makeLoMessage();
    
    void addArgument(int32_t v);
    void addArgument(int64_t v);
    void addArgument(float v);
    void addArgument(string v);
    
    OscArgument& getArgument(unsigned int idx);
    unsigned int nbArguments();
    
    void setPath(string v);
    string getPath() {return m_Path;};
    void print();
    
protected:
    typedef std::vector< OscArgument > OscArgumentVector;
    typedef std::vector< OscArgument >::iterator OscArgumentVectorIterator;
    OscArgumentVector m_arguments;

    string m_Path;
protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace OSC

#endif /* __FREEBOB_OSCMESSAGE__ */


