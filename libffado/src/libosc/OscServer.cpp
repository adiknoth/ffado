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

#include <lo/lo.h>

#include "OscServer.h"
#include "OscMessage.h"
#include "OscResponse.h"
#include "OscNode.h"

#include <assert.h>

namespace OSC {

IMPL_DEBUG_MODULE( OscServer, OscServer, DEBUG_LEVEL_VERBOSE );

OscServer::OscServer(string port)
    : m_port(port)
    , m_server(NULL)
    , m_rootNode(new OscNode("/root"))
{

}

OscServer::~OscServer() {
    lo_server_thread_free(m_server);
    if (m_rootNode) delete m_rootNode;
}

void
OscServer::setVerboseLevel(int l) {
    setDebugLevel(l);
    if (m_rootNode) m_rootNode->setVerboseLevel(l);
}

bool
OscServer::start()
{
    if (!m_server) {
        debugWarning("no server thread\n");
        return false;
    }
    lo_server_thread_start(m_server);
    return true;
}

bool
OscServer::stop()
{
    if (!m_server) {
        debugWarning("no server thread\n");
        return false;
    }
    lo_server_thread_stop(m_server);
    return true;
}

bool
OscServer::init()
{
    if (m_rootNode == NULL) { // should be done at creation
        debugError("Could not initialize root node\n");
        return false;
    }
    
    m_server = lo_server_thread_new(m_port.c_str(), error_cb);
    
    if (m_server == NULL) {
        debugWarning("Could not start OSC server on port %s, trying other port...\n", m_port.c_str());
        m_server = lo_server_thread_new(NULL, error_cb);
        if (!m_server) {
            debugError("Could not start OSC server.\n");
            return false;
        }
        debugWarning("Started OSC server at %s\n", 
                    lo_server_get_url(lo_server_thread_get_server(m_server)));
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "Started OSC server at %s\n", 
                    lo_server_get_url(lo_server_thread_get_server(m_server)));
    }
    
    // For debugging, print all incoming OSC messages
    lo_server_thread_add_method(m_server, NULL, NULL, generic_cb, this);

    return true;
}

bool
OscServer::registerAtRootNode(OscNode *n) {
    return m_rootNode->addChildOscNode(n);
}

bool
OscServer::unregisterAtRootNode(OscNode * n) {
    return m_rootNode->removeChildOscNode(n);
}

// callbacks

void
OscServer::error_cb(int num, const char* msg, const char* path)
{
    debugError("liblo server error %d in path %s, message: %s\n", num, path, msg);
}

// Display incoming OSC messages (for debugging purposes)
int
OscServer::generic_cb(const char* path, const char* types, lo_arg** argv, int argc, lo_message msg, void* user_data)
{
    OscServer *server=reinterpret_cast<OscServer *>(user_data);
    assert(server);
    
    debugOutput(DEBUG_LEVEL_VERBOSE, "Message on: %s\n", path);

    if(!server->m_rootNode) {
        debugError("No root node registered");
        return 1;
    }

    // construct the message
    OscMessage m = OscMessage(path, types, argv, argc);
    
#ifdef DEBUG
    if (getDebugLevel()>=DEBUG_LEVEL_VERY_VERBOSE) {
        for (int i=0; i < argc; ++i) {
            printf("arg %d '%c' ", i, types[i]);
            lo_arg_pp(lo_type(types[i]), argv[i]);
            printf("\n");
        }
        m.print();
    }
#endif

    // process it
    OscResponse r=server->m_rootNode->processOscMessage(string("/root")+string(path), &m);
    if(r.isHandled()) {
        if(r.hasMessage()) {
            // send response
            lo_address addr = lo_message_get_source(msg);
            
            lo_message lo_msg;
            lo_msg=r.getMessage().makeLoMessage();
            
            debugOutput(DEBUG_LEVEL_VERBOSE, " Sending response to %s\n",lo_address_get_url(addr));
            
            #ifdef DEBUG
                if(getDebugLevel()>=DEBUG_LEVEL_VERY_VERBOSE) r.getMessage().print();
            #endif
            
            if (lo_send_message(addr, "/response", lo_msg) < 0) {
                debugError("Failed to send response\n");
            }
            lo_message_free(lo_msg);
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, " No response...\n");
        }
        return 0;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, " Not handled...\n");
        m.print();
        return 1;  // not handled
    }
}


} // end of namespace OSC
