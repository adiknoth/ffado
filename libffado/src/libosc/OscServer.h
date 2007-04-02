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
#ifndef __FREEBOB_OSCSERVER__
#define __FREEBOB_OSCSERVER__

#include <lo/lo.h>
#include <string>

#include "../debugmodule/debugmodule.h"

using namespace std;

namespace OSC {
class OscMessage;
class OscNode;

class OscServer {

public:

    OscServer(string port);
    virtual ~OscServer();
    
    bool init();
    bool start();
    bool stop();
    
    bool registerAtRootNode(OscNode *);
    bool unregisterAtRootNode(OscNode *);

private:
    static void error_cb(int num, const char* msg, const char* path);	
    static int  generic_cb(const char* path, const char* types, lo_arg** argv, int argc, lo_message msg, void* user_data);
    
protected:
    string             m_port;
    lo_server_thread   m_server;
    
    OscNode * m_rootNode;

public:
    void setVerboseLevel(int l);
private:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace OSC

#endif /* __FREEBOB_OSCSERVER__ */


