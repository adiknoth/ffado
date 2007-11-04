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

#ifndef __FFADO_OSCRESPONSE__
#define __FFADO_OSCRESPONSE__

#include "OscMessage.h"

#include "../debugmodule/debugmodule.h"

namespace OSC {

class OscResponse {
public:
    enum EType {
        eError = -1,
        eUnhandled = 0,
        eNone = 1,
        eMessage = 2,
    };

public:
    OscResponse();
    OscResponse(enum EType);
    OscResponse(OscMessage);
    virtual ~OscResponse();

    OscMessage& getMessage() {return m_message;};

    bool isError() {return m_type==eError;};
    bool isHandled() {return (m_type != eError) && (m_type != eUnhandled);};
    bool hasMessage() {return (m_type == eMessage);};

    void setType(enum EType t) {m_type=t;};
private:
    enum EType m_type;
    OscMessage m_message;

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace OSC

#endif /* __FFADO_OSCRESPONSE__ */


