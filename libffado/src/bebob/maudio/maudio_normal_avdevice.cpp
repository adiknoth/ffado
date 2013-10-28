/*
 * Copyright (C) 2013      by Takashi Sakamoto
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "maudio_normal_avdevice.h"

namespace BeBoB {
namespace MAudio {

NormalDevice::NormalDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ),
                           unsigned int modelId)
    : BeBoB::Device( d, configRom)
{
    switch ( modelId ) {
    case 0x00010046: // fw410
        m_id = FW_410;
        break;
    case 0x00010060: // Audiophile
        m_id = FW_AUDIOPHILE;
        break;
    case 0x00010062: // Solo
        m_id = FW_SOLO;
        break;
    case 0x0000000a:
        m_id = FW_OZONIC;
        break;
    }
    updateClkSrc();

    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::MAudio::NormalDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

NormalDevice::~NormalDevice()
{
}

void
NormalDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::MAudio::NormalDevice\n");
    BeBoB::Device::showDevice();
}

bool NormalDevice::updateClkSrc()
{
    int plugid;

    m_internal_clksrc.type = FFADODevice::eCT_Internal;
    m_internal_clksrc.active = false;
    m_internal_clksrc.valid = true;
    m_internal_clksrc.locked = true;
    m_internal_clksrc.id = 0x01;
    m_internal_clksrc.slipping = false;
    m_internal_clksrc.description = "Internal";

    m_spdif_clksrc.type = FFADODevice::eCT_SPDIF;
    m_spdif_clksrc.active = false;
    m_spdif_clksrc.valid = false;
    m_spdif_clksrc.locked = false;
    m_spdif_clksrc.id = 0;
    m_spdif_clksrc.slipping = false;
    m_spdif_clksrc.description = "S/PDIF (Coaxial)";

    m_adat_clksrc.type = FFADODevice::eCT_ADAT;
    m_adat_clksrc.active = false;
    m_adat_clksrc.valid = false;
    m_adat_clksrc.locked = false;
    m_adat_clksrc.id = 0;
    m_adat_clksrc.slipping = false;
    m_adat_clksrc.description = "ADAT (Optical)";

    switch (m_id) {
    case FW_410: // fw410
        m_spdif_clksrc.active = true;
        m_spdif_clksrc.valid = true;
        m_spdif_clksrc.id = 0x82;
        m_adat_clksrc.active = true;
        m_adat_clksrc.valid = true;
        m_adat_clksrc.id = 0x83;
        break;
    case FW_AUDIOPHILE: // Audiophile
        m_spdif_clksrc.active = true;
        m_spdif_clksrc.valid = true;
        m_spdif_clksrc.id = 0x82;
        break;
    case FW_SOLO: // Solo
        m_spdif_clksrc.active = true;
        m_spdif_clksrc.valid = true;
        m_spdif_clksrc.id = 0x81;
        break;
    case FW_OZONIC:
        /* internal only */
        m_active_clksrc = &m_internal_clksrc;
        return true;
    }

    plugid = getClkSrc();
    if (plugid < 0)
        return false;

    if (plugid == 0x01) {
        m_internal_clksrc.active = true;
        m_active_clksrc = &m_internal_clksrc;
    } else if (plugid == 0x83) {
        m_adat_clksrc.active = true;
        m_active_clksrc = &m_adat_clksrc;
    } else {
        m_spdif_clksrc.active = true;
        m_active_clksrc = &m_spdif_clksrc;
    }

    return true;
}

int NormalDevice::getClkSrc()
{
    AVC::SignalSourceCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getNodeId() );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );
    cmd.setVerbose( getDebugLevel() );

    AVC::SignalSubunitAddress dst;
    dst.m_subunitType = AVC::eST_Music;
    dst.m_subunitId = 0x00;
    dst.m_plugId = 0x01;
    cmd.setSignalDestination( dst );

    if ( !cmd.fire() ) {
        debugError( "Signal source command failed\n" );
        return -1;
    }
    AVC::SignalAddress* pSyncPlugSignalAddress = cmd.getSignalSource();

    AVC::SignalSubunitAddress* pSyncPlugSubunitAddress
        = dynamic_cast<AVC::SignalSubunitAddress*>( pSyncPlugSignalAddress );
    if ( pSyncPlugSubunitAddress ) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sync mode 0x%02x\n",
                    ( pSyncPlugSubunitAddress->m_subunitType << 3
                      | pSyncPlugSubunitAddress->m_subunitId ) << 8
                    | pSyncPlugSubunitAddress->m_plugId );
        return pSyncPlugSubunitAddress->m_plugId;
    }

    AVC::SignalUnitAddress* pSyncPlugUnitAddress
      = dynamic_cast<AVC::SignalUnitAddress*>( pSyncPlugSignalAddress );
    if ( pSyncPlugUnitAddress ) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sync mode 0x%02x\n",
                      0xff << 8 | pSyncPlugUnitAddress->m_plugId );
        return pSyncPlugUnitAddress->m_plugId;
    }

    debugError( "Could not retrieve sync mode\n" );
    return -1;
}

FFADODevice::ClockSourceVector
NormalDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;

    r.push_back(m_internal_clksrc);

    if (m_spdif_clksrc.active)
        r.push_back(m_spdif_clksrc);

    if (m_adat_clksrc.active)
        r.push_back(m_adat_clksrc);

    return r;
}

FFADODevice::ClockSource
NormalDevice::getActiveClockSource()
{
    if (!updateClkSrc()) {
        ClockSource s;
        s.type = eCT_Invalid;
        return s;
    }
    return *m_active_clksrc;
}

bool
NormalDevice::setActiveClockSource(ClockSource s)
{
	AVC::SignalSourceCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getNodeId() );
    cmd.setSubunitType( AVC::eST_Unit  );
    cmd.setSubunitId( 0xff );
    cmd.setVerbose( getDebugLevel() );

    AVC::SignalSubunitAddress dst;
    dst.m_subunitType = AVC::eST_Music;
    dst.m_subunitId = 0x00;
    dst.m_plugId = 0x01;
    cmd.setSignalDestination( dst );

    if (s.id == 0x01) {
        AVC::SignalSubunitAddress src;
        src.m_subunitType = AVC::eST_Music;
        src.m_subunitId = 0x00;
        src.m_plugId = 0x01;
        cmd.setSignalSource( src );
    } else {
    	AVC::SignalUnitAddress src;
        src.m_plugId = s.id;
        cmd.setSignalSource( src );
    }

    if ( !cmd.fire() ) {
        debugError( "Signal source command failed\n" );
        return false;
    }

    return true;
}

} // namespace MAudio
} // namespace BeBoB
