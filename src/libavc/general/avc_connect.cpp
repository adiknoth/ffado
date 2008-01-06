/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#include "avc_connect.h"
#include "libutil/cmd_serialize.h"
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
ConnectCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result=true;
    result &= AVCCommand::serialize( se );
    return result;
}

bool
ConnectCmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result=true;
    result &= AVCCommand::deserialize( de );
    return result;
}

}
