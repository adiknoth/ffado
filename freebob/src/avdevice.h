/* avdevice.h
 * Copyright (C) 2004 by Daniel Wagner, Pieter Palmers
 *
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

#include "ieee1394service.h"

#include <vector>
using std::vector;


#ifndef AVDEVICE_H
#define AVDEVICE_H

class AvDeviceSubunit;

class AvDevice {
 public:
    AvDevice(int node,int port);
    virtual ~AvDevice();

    quadlet_t * avcExecuteTransaction(quadlet_t *request, unsigned int request_len, unsigned int response_len);

    FBReturnCodes AvDevice::Initialize();

    bool AvDevice::isInitialised();

 private:
	int iNodeId;
	raw1394handle_t m_handle;
	int m_iPort;
	bool m_bInitialised;
	vector<AvDeviceSubunit *> cSubUnits;

	unsigned char iNbAsyncDestinationPlugs;
	unsigned char iNbAsyncSourcePlugs;
	unsigned char iNbIsoDestinationPlugs;
	unsigned char iNbIsoSourcePlugs;
	unsigned char iNbExtDestinationPlugs;
	unsigned char iNbExtSourcePlugs;

};

#endif
