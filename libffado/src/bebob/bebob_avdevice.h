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

#ifndef BEBOB_DEVICE_H
#define BEBOB_DEVICE_H

#include <stdint.h>

#include "debugmodule/debugmodule.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"
#include "libavc/general/avc_extended_cmd_generic.h"
#include "libavc/general/avc_unit.h"
#include "libavc/general/avc_subunit.h"
#include "libavc/general/avc_plug.h"
#include "libavc/audiosubunit/avc_function_block.h"

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_avdevice_subunit.h"
#include "bebob/bebob_mixer.h"

#include "libutil/serialize.h"

#include "genericavc/avc_avdevice.h"

#include "ffadodevice.h"

#include <sstream>
#include <vector>

namespace BeBoB {

class Device : public GenericAVC::Device {
public:
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Device();

    static bool probe( Util::Configuration&, ConfigRom& configRom, bool generic = false );
    virtual bool loadFromCache();
    virtual bool saveCache();
    virtual bool discover();

    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));

    virtual AVC::Subunit* createSubunit(AVC::Unit& unit,
                                        AVC::ESubunitType type,
                                        AVC::subunit_t id );
    virtual AVC::Plug *createPlug( AVC::Unit* unit,
                                   AVC::Subunit* subunit,
                                   AVC::function_block_type_t functionBlockType,
                                   AVC::function_block_type_t functionBlockId,
                                   AVC::Plug::EPlugAddressType plugAddressType,
                                   AVC::Plug::EPlugDirection plugDirection,
                                   AVC::plug_id_t plugId,
                                   int globalId = -1 );

    virtual int getSelectorFBValue(int id);
    virtual bool setSelectorFBValue(int id, int v);

    virtual int getFeatureFBVolumeMinimum(int id, int channel);
    virtual int getFeatureFBVolumeMaximum(int id, int channel);
    virtual int getFeatureFBVolumeCurrent(int id, int channel);
    virtual bool setFeatureFBVolumeCurrent(int id, int channel, int v);

    virtual int getFeatureFBLRBalanceMinimum(int id, int channel);
    virtual int getFeatureFBLRBalanceMaximum(int id, int channel);
    virtual int getFeatureFBLRBalanceCurrent(int id, int channel);
    virtual bool setFeatureFBLRBalanceCurrent(int id, int channel, int v);

    virtual void showDevice();
    virtual void setVerboseLevel(int l);
protected:
    virtual bool propagatePlugInfo();

    virtual bool buildMixer();
    virtual bool destroyMixer();

    virtual int getFeatureFBVolumeValue(int id, int channel, AVC::FunctionBlockCmd::EControlAttribute controlAttribute);
    virtual int getFeatureFBLRBalanceValue(int id, int channel, AVC::FunctionBlockCmd::EControlAttribute controlAttribute);

public:
    virtual bool serialize( std::string basePath, Util::IOSerialize& ser ) const;
    virtual bool deserialize( std::string basePath, Util::IODeserialize& deser );

    virtual uint64_t getConfigurationId();
    virtual bool needsRediscovery();

    std::string getCachePath();

protected:
    virtual uint8_t getConfigurationIdSampleRate();
    virtual uint8_t getConfigurationIdNumberOfChannel( AVC::PlugAddress::EPlugDirection ePlugDirection );
    virtual uint16_t getConfigurationIdSyncMode();

    std::vector<int> m_supported_frequencies;
    uint64_t         m_last_discovery_config_id;

protected:
    Mixer*             m_Mixer;

};

}

#endif
