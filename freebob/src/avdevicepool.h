/* avdevicepool.h
 * Copyright (C) 2004 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#ifndef AVDEVICEPOOL_H
#define AVDEVICEPOOL_H

#include "freebob.h"
#include <vector>

class AvDevice;

class AvDevicePool {
 public:
    AvDevicePool();
    ~AvDevicePool();

    FBReturnCodes registerAvDevice(AvDevice* pAvDevice);
    FBReturnCodes unregisterAvDevice(AvDevice* pAvDevice);
 private:
    typedef std::vector<AvDevice*> AvDeviceVector;
    AvDeviceVector m_avDevices;
};

#endif
