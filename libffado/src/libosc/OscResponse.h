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
#ifndef __FREEBOB_OSCRESPONSE__
#define __FREEBOB_OSCRESPONSE__

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

#endif /* __FREEBOB_OSCRESPONSE__ */


