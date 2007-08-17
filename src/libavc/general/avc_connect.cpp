/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#include "avc_connect.h"
#include "../util/avc_serialize.h"
#include "libieee1394/ieee1394service.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;

namespace AVC {

ConnectCmd::ConnectCmd(Ieee1394Service& ieee1394service)
    : AVCCommand( ieee1394service, AVC1394_CMD_CONNECT )
{
}

ConnectCmd::~ConnectCmd()
{
}

bool
ConnectCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= AVCCommand::serialize( se );
    return result;
}

bool
ConnectCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= AVCCommand::deserialize( de );
    return result;
}

}
