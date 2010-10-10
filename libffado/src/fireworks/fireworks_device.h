/*
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

#ifndef FIREWORKS_DEVICE_H
#define FIREWORKS_DEVICE_H

#include "debugmodule/debugmodule.h"

#include "genericavc/avc_avdevice.h"

#include "efc/efc_cmd.h"
#include "efc/efc_cmds_hardware.h"
#include "fireworks_session_block.h"

#include <pthread.h>
#include "libutil/Mutex.h"

class ConfigRom;
class Ieee1394Service;

namespace FireWorks {

class Device : public GenericAVC::Device {
    friend class MonitorControl;
    friend class SimpleControl;
    friend class BinaryControl;
    friend class IOConfigControl;
    
public:
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ) );
    virtual ~Device();
    
    static bool probe( Util::Configuration&, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    virtual void showDevice();
    
    virtual bool buildMixer();
    virtual bool destroyMixer();

    virtual std::vector<int> getSupportedSamplingFrequencies();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();
    virtual int getSamplingFrequency();
    virtual bool setSamplingFrequency( int );

    const EfcHardwareInfoCmd getHwInfo()
        {return m_HwInfo;};

// protected: //?
    bool doEfcOverAVC(EfcCmd& c);
    
    /**
     * @brief Read flash
     * @param start start address
     * @param len length in quadlets (4 bytes)
     * @param buffer target buffer (should be 'len*4' bytes long)
     * @return true if successful
     */
    bool readFlash(uint32_t start, uint32_t len, uint32_t* buffer);

    /**
     * @brief Write flash
     * @param start start address
     * @param len length in quadlets (4 bytes)
     * @param buffer target buffer (should be 'len*4' bytes long)
     * @return true if successful
     */
    bool writeFlash(uint32_t start, uint32_t len, uint32_t* buffer);

    /**
     * @brief (un)lock the flash
     * @param lock true=locked, false=unlocked
     * @return true if successful
     */
    bool lockFlash(bool lock);
    /**
     * @brief erase flash block
     * @param addr address of block to erase
     * @return true if successful
     */
    bool eraseFlash(uint32_t addr);
    bool eraseFlashBlocks(uint32_t start_address, unsigned int nb_quads);

    /**
     * @brief wait until the device indicates the flash memory is ready
     * @param msecs time to wait before timeout
     * @return true if the flash is ready, false if timeout
     */
    bool waitForFlash(unsigned int msecs);

    /**
     * @brief get the flash address of the session block
     * @return session block base address
     */
    uint32_t getSessionBase();

    /**
     * load the session block from the device
     * @return true if successful
     */
    bool loadSession();
    /**
     * save the session block to the device
     * @return true if successful
     */
    bool saveSession();

// Echo specific stuff
private:
    
    bool discoverUsingEFC();

    FFADODevice::ClockSource clockIdToClockSource(uint32_t clockflag);
    bool isClockValid(uint32_t id);
    uint32_t getClock();
    bool setClock(uint32_t);

    uint32_t            m_efc_version;

    EfcHardwareInfoCmd  m_HwInfo;

    bool updatePolledValues();
    Util::Mutex*        m_poll_lock;
    EfcPolledValuesCmd  m_Polled;

    bool                m_efc_discovery_done;

protected:
    Session             m_session;
private:
    Control::Container *m_MixerContainer;
    Control::Container *m_HwInfoContainer;

};


} // namespace FireWorks

#endif
