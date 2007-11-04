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

#include "SystemTimeSource.h"
#include "Time.h"

namespace Util {

IMPL_DEBUG_MODULE( SystemTimeSource, SystemTimeSource, DEBUG_LEVEL_NORMAL );

SystemTimeSource::SystemTimeSource() {
//     InitTime();
}

SystemTimeSource::~SystemTimeSource() {

}

ffado_microsecs_t SystemTimeSource::getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;

//     return GetMicroSeconds();
}

ffado_microsecs_t SystemTimeSource::getCurrentTimeAsUsecs() {
    return getCurrentTime();
}

} // end of namespace Util
