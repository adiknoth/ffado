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

#include "presonus_avdevice.h"

namespace BeBoB {
namespace Presonus {

FireboxDevice::FireboxDevice(DeviceManager& d, std::auto_ptr<ConfigRom>(configRom))
    : BeBoB::Device( d, configRom)
{
    m_intl_clksrc.type = FFADODevice::eCT_Internal;
    m_intl_clksrc.valid = true;
    m_intl_clksrc.locked = true;
    m_intl_clksrc.id = 0x00;
    m_intl_clksrc.slipping = false;
    m_intl_clksrc.description = "Internal";

    m_spdif_clksrc.type = FFADODevice::eCT_SPDIF;
    m_spdif_clksrc.valid = true;
    m_spdif_clksrc.locked = true;
    m_spdif_clksrc.id = 0x01;
    m_spdif_clksrc.slipping = false;
    m_spdif_clksrc.description = "S/PDIF (Coaxial)";

    debugOutput( DEBUG_LEVEL_VERBOSE, "Created BeBoB::Presonus::FireboxDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

FireboxDevice::~FireboxDevice()
{
}

void
FireboxDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::Presonus::FireboxDevice\n");
    BeBoB::Device::showDevice();
}

FFADODevice::ClockSourceVector
FireboxDevice::getSupportedClockSources()
{
    FFADODevice::ClockSourceVector r;
    r.push_back(m_intl_clksrc);
    r.push_back(m_spdif_clksrc);
    return r;
}

enum FFADODevice::eClockSourceType
FireboxDevice::getClkSrc()
{
    AVC::SignalSourceCmd cmd(get1394Service());
    cmd.setCommandType(AVC::AVCCommand::eCT_Status);
    cmd.setNodeId(getNodeId());
    cmd.setSubunitType(AVC::eST_Unit);
    cmd.setSubunitId(0xff);
    cmd.setVerbose(getDebugLevel());

    AVC::SignalSubunitAddress dst;
    dst.m_subunitType = AVC::eST_Music;
    dst.m_subunitId = 0x00;
    dst.m_plugId = 0x05;
    cmd.setSignalDestination(dst);

    if (!cmd.fire()) {
        debugError( "Signal source command failed\n" );
        return eCT_Invalid;
    }
    AVC::SignalAddress* pSyncPlugSignalAddress = cmd.getSignalSource();

    AVC::SignalSubunitAddress* pSyncPlugSubunitAddress
        = dynamic_cast<AVC::SignalSubunitAddress*>( pSyncPlugSignalAddress );
    if (pSyncPlugSubunitAddress) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sync mode 0x%02x\n",
                    ( pSyncPlugSubunitAddress->m_subunitType << 3
                      | pSyncPlugSubunitAddress->m_subunitId ) << 8
                    | pSyncPlugSubunitAddress->m_plugId );
        return eCT_Internal;
    }

    AVC::SignalUnitAddress* pSyncPlugUnitAddress
      = dynamic_cast<AVC::SignalUnitAddress*>( pSyncPlugSignalAddress );
    if (pSyncPlugUnitAddress) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Sync mode 0x%02x\n",
                      0xff << 8 | pSyncPlugUnitAddress->m_plugId );
        return eCT_SPDIF;
    }

    debugError( "Could not retrieve sync mode\n" );
    return eCT_Invalid;
}

FFADODevice::ClockSource
FireboxDevice::getActiveClockSource()
{
    switch (getClkSrc()) {
    case eCT_Internal:
        m_intl_clksrc.active = true;
        m_spdif_clksrc.active = false;
        return m_intl_clksrc;
    case eCT_SPDIF:
        m_intl_clksrc.active = false;
        m_spdif_clksrc.active = true;
        return m_spdif_clksrc;
    default:
        ClockSource s;
        s.type = eCT_Invalid;
        return s;
    }
}

bool
FireboxDevice::setActiveClockSource(ClockSource s)
{
	AVC::SignalSourceCmd cmd(get1394Service());
    cmd.setCommandType(AVC::AVCCommand::eCT_Control);
    cmd.setNodeId(getNodeId());
    cmd.setSubunitType(AVC::eST_Unit);
    cmd.setSubunitId(0xff);
    cmd.setVerbose(getDebugLevel());

    AVC::SignalSubunitAddress dst;
    dst.m_subunitType = AVC::eST_Music;
    dst.m_subunitId = 0x00;
    dst.m_plugId = 0x05;
    cmd.setSignalDestination(dst);

    if (s.id == 0x00) {
        AVC::SignalSubunitAddress src;
        src.m_subunitType = AVC::eST_Music;
        src.m_subunitId = 0x00;
        src.m_plugId = 0x06;
        cmd.setSignalSource( src );
    } else {
        AVC::SignalUnitAddress src;
        src.m_plugId = 0x83;
        cmd.setSignalSource(src);
    }

    if (!cmd.fire()) {
        debugError( "Signal source command failed\n" );
        return false;
    }

    return true;
}

} // namespace Presonus
} // namespace BeBoB
