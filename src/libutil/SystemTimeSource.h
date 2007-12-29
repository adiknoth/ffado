/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
    
    static void SleepUsecRelative(ffado_microsecs_t usecs);
    static void SleepUsecAbsolute(ffado_microsecs_t wake_time);

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace Util

#endif /* __FFADO_SYSTEMTIMESOURCE__ */


