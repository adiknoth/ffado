/*
 * Copyright (C) 2014      by Takashi Sakamoto
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef BEBOB_MAUDIO_SPECIAL_DEVICE_H
#define BEBOB_MAUDIO_SPECIAL_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "bebob/bebob_avdevice.h"

#include "bebob/maudio/special_mixer.h"

/*
 * Firewire 1814 and ProjectMix I/O uses special firmware. It will be freezed
 * if receiving any commands which the firmware can't understand. These devices
 * utilize completely different system to control. It is read/write transaction
 * directly into a certain address.
 */
#define MAUDIO_SPECIFIC_ADDRESS 0xffc700000000

/*
 * FW1814/ProjectMix don't use AVC for control. The driver cannot refer to
 * current parameters by asynchronous transaction. The driver is allowed to
 * write transaction so MUST remember the current values.
 */
#define	MAUDIO_CONTROL_OFFSET 0x00700000

/*
 * GAIN for inputs:
 * Write 32bit. upper 16bit for left chennal and lower 16bit for right.
 * The value is between 0x8000(low) to 0x0000(high) as the same as '10.3.1
 * Volume Control' in 'AV/C Audio Subunit Specification 1.0 (1394TA 1999008)'.
 */
#define GAIN_STM_12_IN   0x00
#define GAIN_STM_34_IN   0x04
#define GAIN_ANA_12_OUT  0x08
#define GAIN_ANA_34_OUT  0x0c
#define GAIN_ANA_12_IN   0x10
#define GAIN_ANA_34_IN   0x14
#define GAIN_ANA_56_IN   0x18
#define GAIN_ANA_78_IN   0x1c
#define GAIN_SPDIF_12_IN 0x20
#define GAIN_ADAT_12_IN  0x24
#define GAIN_ADAT_34_IN  0x28
#define GAIN_ADAT_56_IN  0x2c
#define GAIN_ADAT_78_IN  0x30
#define GAIN_AUX_12_OUT  0x34
#define GAIN_HP_12_OUT   0x38
#define GAIN_HP_34_OUT   0x3c
/*
 * LR balance:
 * Write 32 bit, upper 16bit for left channel and lower 16bit for right.
 * The value is between 0x800(L) to 0x7FFE(R) as the same as '10.3.3 LR Balance
 * Control' in 'AV/C Audio Subunit Specification 1.0 (1394TA 1999008)'.
 */
#define LR_ANA_12_IN   0x40
#define LR_ANA_34_IN   0x44
#define LR_ANA_56_IN   0x48
#define LR_ANA_78_IN   0x4c
#define LR_SPDIF_12_IN 0x50
#define LR_ADAT_12_IN  0x54
#define LR_ADAT_34_IN  0x58
#define LR_ADAT_56_IN  0x5c
#define LR_ADAT_78_IN  0x60
/*
 * AUX inputs:
 * This is the same as 'gain' control above.
 */
#define AUX_STM_12_IN   0x64
#define AUX_STM_34_IN   0x68
#define AUX_ANA_12_IN   0x6c
#define AUX_ANA_34_IN   0x70
#define AUX_ANA_56_IN   0x74
#define AUX_ANA_78_IN   0x78
#define AUX_SPDIF_12_IN 0x7c
#define AUX_ADAT_12_IN  0x80
#define AUX_ADAT_34_IN  0x84
#define AUX_ADAT_56_IN  0x88
#define AUX_ADAT_78_IN  0x8c
/*
 * MIXER inputs:
 * There are bit flags. If flag is 0x01, it means on.
 *
 *  MIX_ANA_DIG_IN:
 *  Write 32bits, upper 16bit for digital inputs and lowe 16bit for analog inputs.
 *   Digital inputs:
 *    Lower 2bits are used. upper for 'to Mix3/4' and lower for 'to Mix1/2'.
 *   Analog inputs:
 *    Lower 8bits are used. upper 4bits for 'to Mix3/4' and lower for 'to
 *    Mix1/2'. Inner the 4bit, for 'from Ana7/8', for 'from Ana5/6', for 'from
 *    Ana3/4', for 'from Ana1/2'.
 *
 *  MIX_STM_IN:
 *   Write 32bits, lower 4bits are used. upper 2bits for 'from Stm1/2' and lower
 *   for 'from Stm3/4'. Inner the 2bits, for 'to Mix3/4' and for 'to
 *   Mix1/2'.
 */
#define MIX_ANA_DIG_IN 0x90
#define MIX_STM_IN     0x94
/*
 * SRC for output:
 * Write 32bit. There are bit flags. If the flag is 0x01, it means on.
 *
 *  SRC_HP_OUT:
 *  Lower 3bits are used, 'from Aux12', 'from Mix34', 'from
 *  Mix12'.
 *
 *  SRC_ANA_OUT:
 *  Lower 2 bits are used, 'to Ana34', 'to Ana12'. If bit is 0x01, it
 *  means 'from Aux12' else 'From Mix12 (or Mix34)'.
 */
#define SRC_HP_OUT   0x98
#define SRC_ANA_OUT  0x9c

namespace BeBoB {
namespace MAudio {
namespace Special {

class Device : public BeBoB::Device
{
public:
    Device(DeviceManager& d, std::auto_ptr<ConfigRom>(configRom));
    virtual ~Device() {};
    virtual void showDevice();

    bool lock();
    bool Unlock();
    bool loadFromCache();
    bool saveCache();

    virtual bool discover();
    virtual bool buildMixer();
    virtual bool destroyMixer();

    std::vector<int> getSupportedSamplingFrequencies();
    bool supportsSamplingFrequency( int s ); int getSamplingFrequency();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual enum FFADODevice::eStreamingState getStreamingState();

    virtual bool readReg(uint64_t addr, uint32_t *data);
    virtual bool writeReg(uint64_t addr, uint32_t data);
    bool writeBlk(uint64_t offset, unsigned int length, uint32_t *data);
protected:
    virtual uint8_t getConfigurationIdSampleRate();
    virtual uint8_t getConfigurationIdNumberOfChannel(AVC::PlugAddress::EPlugDirection ePlugDirection);
    virtual uint16_t getConfigurationIdSyncMode();

private:
    void updateClockSources();
    ClockSource m_fixed_clksrc;

    Mixer *m_special_mixer;
    bool is1814;

    /* cache for mixer settings */
    uint32_t m_regs[(0x9c + 4) / 4];
};

} // namespace Special
} // namespace MAudio
} // namespace BeBoB

#endif /* BEBOB_MAUDIO_SPECIAL_DEVICE_H */
