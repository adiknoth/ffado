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

#ifndef __FFADO_OSCMESSAGE__
#define __FFADO_OSCMESSAGE__

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

#endif /* __FFADO_OSCMESSAGE__ */


