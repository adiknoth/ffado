/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef __FFADO_SYSTEMTIMESOURCE__
#define __FFADO_SYSTEMTIMESOURCE__

#include "../debugmodule/debugmodule.h"
#include "TimeSource.h"

namespace Util {

class SystemTimeSource
    : public Util::TimeSource
{

public:

    SystemTimeSource();
    virtual ~SystemTimeSource();

    // implement the TimeSource interface
    ffado_microsecs_t getCurrentTime();
    ffado_microsecs_t getCurrentTimeAsUsecs();
    inline ffado_microsecs_t unWrapTime(ffado_microsecs_t t) {return t;};
    inline ffado_microsecs_t wrapTime(ffado_microsecs_t t) {return t;};

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace Util

#endif /* __FFADO_SYSTEMTIMESOURCE__ */


