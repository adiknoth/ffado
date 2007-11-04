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

#ifndef __FFADO_OSCSERVER__
#define __FFADO_OSCSERVER__

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

#endif /* __FFADO_OSCSERVER__ */


