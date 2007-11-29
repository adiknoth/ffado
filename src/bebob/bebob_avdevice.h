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

#ifndef BEBOB_AVDEVICE_H
#define BEBOB_AVDEVICE_H

#include <stdint.h>

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_extended_cmd_generic.h"
#include "libavc/general/avc_unit.h"
#include "libavc/general/avc_subunit.h"
#include "libavc/general/avc_plug.h"

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/bebob_mixer.h"

#include "libstreaming/amdtp/AmdtpReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"
#include "libstreaming/amdtp/AmdtpPortInfo.h"

#include "libutil/serialize.h"

#include "genericavc/avc_avdevice.h"

#include "ffadodevice.h"

#include <sstream>
#include <vector>

namespace BeBoB {

class AvDevice : public GenericAVC::AvDevice {
public:
    AvDevice( std::auto_ptr<ConfigRom>( configRom ));
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom );
    virtual bool loadFromCache();
    virtual bool saveCache();
    virtual bool discover();

    static FFADODevice * createDevice( std::auto_ptr<ConfigRom>( configRom ));

    virtual AVC::Subunit* createSubunit(AVC::Unit& unit,
                                        AVC::ESubunitType type,
                                        AVC::subunit_t id );
    virtual AVC::Plug *createPlug( AVC::Unit* unit,
                                   AVC::Subunit* subunit,
                                   AVC::function_block_type_t functionBlockType,
                                   AVC::function_block_type_t functionBlockId,
                                   AVC::Plug::EPlugAddressType plugAddressType,
                                   AVC::Plug::EPlugDirection plugDirection,
                                   AVC::plug_id_t plugId );

    virtual void showDevice();
    virtual void setVerboseLevel(int l);
protected:
    virtual bool propagatePlugInfo();
    
    virtual bool buildMixer();
    virtual bool destroyMixer();

public:
    virtual bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    virtual bool deserialize( Glib::ustring basePath, Util::IODeserialize& deser );

    int getConfigurationIdSampleRate();
    int getConfigurationIdNumberOfChannel( AVC::PlugAddress::EPlugDirection ePlugDirection );
    int getConfigurationIdSyncMode();
    int getConfigurationId();

    Glib::ustring getCachePath();

protected:
    Mixer*             m_Mixer;

};

}

#endif
