/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
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

#ifndef BEBOB_AVPLUG_H
#define BEBOB_AVPLUG_H

#include "libavc/ccm/avc_signal_source.h"
#include "libavc/streamformat/avc_extended_stream_format.h"
#include "libavc/general/avc_extended_plug_info.h"
#include "libavc/general/avc_extended_cmd_generic.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_generic.h"
#include "libavc/general/avc_plug.h"

#include "libutil/serialize.h"

#include "debugmodule/debugmodule.h"

class Ieee1394Service;
class ConfigRom;

namespace BeBoB {

class AvDevice;

class Plug : public AVC::Plug {
public:

    // \todo This constructors sucks. too many parameters. fix it.
    Plug( AVC::Unit* unit,
          AVC::Subunit* subunit,
          AVC::function_block_type_t functionBlockType,
          AVC::function_block_type_t functionBlockId,
          AVC::Plug::EPlugAddressType plugAddressType,
          AVC::Plug::EPlugDirection plugDirection,
          AVC::plug_id_t plugId );
    Plug( const Plug& rhs );
    virtual ~Plug();

    bool discover();
    bool discoverConnections();

 public:

protected:
    Plug();

    bool discoverPlugType();
    bool discoverName();
    bool discoverNoOfChannels();
    bool discoverChannelPosition();
    bool discoverChannelName();
    bool discoverClusterInfo();
    bool discoverConnectionsInput();
    bool discoverConnectionsOutput();

private:
    bool copyClusterInfo(AVC::ExtendedPlugInfoPlugChannelPositionSpecificData&
                         channelPositionData );
    AVC::ExtendedPlugInfoCmd setPlugAddrToPlugInfoCmd();


};

}

#endif // BEBOB_AVPLUG_H
