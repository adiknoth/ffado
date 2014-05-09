/*
 * Copyright (C) 2014      by Takashi Sakamoto
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

#include "libutil/Configuration.h"
#include "libutil/ByteSwap.h"
#include "libutil/SystemTimeSource.h"

#include <libraw1394/csr.h>
#include <libieee1394/IEC61883.h>

#include "bebob/maudio/special_avdevice.h"

namespace BeBoB {
namespace MAudio {
namespace Special {

Device::Device(DeviceManager& d, std::auto_ptr<ConfigRom>(configRom))
    : BeBoB::Device(d, configRom)
{
    is1814 = (getConfigRom().getModelId() == 0x00010071);

    debugOutput(DEBUG_LEVEL_VERBOSE, "Created BeBoB::MAudio::Device (NodeID %d)\n",
                getConfigRom().getNodeId());
    updateClockSources();
}

void Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "This is a BeBoB::MAudio::Device\n");
}

/*
 * Discoveing plugs:
 *  The firmware can't respond any commands related to plug info.
 */
bool Device::discover()
{
    /* keep track of the config id of this discovery */
    m_last_discovery_config_id = getConfigurationId();

    return true;
}

/* don't lock to prevent from streaming */
bool Device::lock()
{
	return false;
}
bool Device::Unlock()
{
	return false;
}

/* don't cache */
bool Device::loadFromCache()
{
	return buildMixer();
}
bool Device::saveCache()
{
	return true;
}

bool Device::buildMixer()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a maudio special mixer...\n");

    delete m_special_mixer;

    m_special_mixer = new Mixer(*this);
    if (m_special_mixer)
    	m_special_mixer->setVerboseLevel(getDebugLevel());

    return (m_special_mixer != NULL);
}

bool Device::destroyMixer()
{
    delete m_special_mixer;
    return true;
}

/*
 * Sampling frequency support:
 *  set: Input/Output Signal Format command
 *  get: Input Signal Format command (or metering information)
 *
 * TODO
 */
std::vector<int> Device::getSupportedSamplingFrequencies()
{
    std::vector<int> freqs;
    freqs.push_back(0);
    return freqs;
}
bool Device::supportsSamplingFrequency(int s)
{
    return true;
}
int Device::getSamplingFrequency()
{
    return 0;
}

/*
 * Clock Source support:
 * TODO: use ALSA control interface
 *
 */
void Device::updateClockSources()
{
    m_fixed_clksrc.type = FFADODevice::eCT_Internal;
    m_fixed_clksrc.active = true;
    m_fixed_clksrc.valid = true;
    m_fixed_clksrc.locked = true;
    m_fixed_clksrc.id = 0;
    m_fixed_clksrc.slipping = false;
    m_fixed_clksrc.description = "Controlled by ALSA";
}
FFADODevice::ClockSourceVector
Device::getSupportedClockSources()
{
    FFADODevice::ClockSourceVector r;
    r.push_back(m_fixed_clksrc);
    return r;
}
FFADODevice::ClockSource
Device::getActiveClockSource()
{
    return m_fixed_clksrc;
}
bool
Device::setActiveClockSource(ClockSource s)
{
    return true;
}

/*
 * Streaming State:
 *  The firmware can't respond against frequent requests.
 * TODO: how to void frequent transaction?
 */
enum FFADODevice::eStreamingState
Device::getStreamingState()
{
    return eSS_Both;
}

/*
 * The firmware can't speak:
 *  'Extended Plug Info command' (BridgeCo)
 *  'Connection and Compatibility Management' (1394TA)
 */
uint8_t
Device::getConfigurationIdSampleRate()
{
    return 1;
}
uint8_t
Device::getConfigurationIdNumberOfChannel( AVC::PlugAddress::EPlugDirection ePlugDirection )
{
	return 2;
}
uint16_t
Device::getConfigurationIdSyncMode()
{
	return 3;
}

bool Device::readReg(uint64_t offset, uint32_t *data)
{
    Util::MutexLockHelper lock(m_DeviceMutex);

    /* read cache because it's 'write-only' */
    *data = m_regs[offset / 4];
    return true;
}

bool Device::writeReg(uint64_t offset, uint32_t data)
{
    int trials;
    fb_nodeid_t nodeId;
    fb_nodeaddr_t addr;
    fb_quadlet_t quad;

    Util::MutexLockHelper lock(m_DeviceMutex);

    nodeId = getNodeId() | 0xffc0;
    addr = MAUDIO_SPECIFIC_ADDRESS + MAUDIO_CONTROL_OFFSET + offset;
    quad = CondSwapToBus32(data);

    /* cache because it's 'write-only' */
    m_regs[offset / 4] = data;

    trials = 0;
    do {
        if (get1394Service().write_quadlet(nodeId, addr, quad))
            break;
        Util::SystemTimeSource::SleepUsecRelative(100);
    } while (++trials < 4);

    return true;
}

bool Device::writeBlk(uint64_t offset, unsigned int size, uint32_t *data)
{
    fb_nodeid_t nodeId;
    fb_nodeaddr_t addr;
    unsigned int i, length, trials;

    nodeId = getNodeId() | 0xffc0;
    addr = MAUDIO_SPECIFIC_ADDRESS + MAUDIO_CONTROL_OFFSET + offset;
    length = size /4;

    for (i = 0; i < length; i++) {
        /* the device applies the setting even if the device don't respond */
        m_regs[i] = data[i];
        data[i] = CondSwapToBus32(data[i]);
    }

    trials = 0;
    do {
        if (get1394Service().write(nodeId, addr, length, (fb_quadlet_t*)data))
            break;
        Util::SystemTimeSource::SleepUsecRelative(100);
    } while (++trials < 4);

    return true;
}

} // namespace Special
} // namespace MAudio
} // namespace BeBoB
