/*
 * Copyright (C) 2005-2008 by Jonathan Woithe
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

#ifndef RMEDEVICE_H
#define RMEDEVICE_H

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libutil/Configuration.h"

#include "fireface_def.h"
// #include "libstreaming/rme/RmeStreamProcessor.h"

class ConfigRom;
class Ieee1394Service;

namespace Rme {

// Note: the values in this enum do not have to correspond to the unit
// version reported by the respective devices.  It just so happens that they
// currently do for the Fireface-800 and Fireface-400.
enum ERmeModel {
    RME_MODEL_NONE          = 0x0000,
    RME_MODEL_FIREFACE800   = 0x0001,
    RME_MODEL_FIREFACE400   = 0x0002,
};

class Device : public FFADODevice {
public:

    Device( DeviceManager& d,
               std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Device();

    static bool probe( Util::Configuration& c, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d,
                                        std::auto_ptr<ConfigRom>( configRom ));
    static int getConfigurationId( );
    virtual bool discover();

    virtual void showDevice();

    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );
    virtual std::vector<int> getSupportedSamplingFrequencies();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    unsigned int readRegister(fb_nodeaddr_t reg);
    signed int readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads);
    signed int writeRegister(fb_nodeaddr_t reg, quadlet_t data);
    signed int writeBlock(fb_nodeaddr_t reg, quadlet_t *data, unsigned int n_quads);

protected:
    enum ERmeModel m_rme_model;

    signed int m_ddsFreq;

    FF_software_settings_t settings;

private:
    unsigned long long int cmd_buffer_addr();
    unsigned long long int stream_init_reg();
    unsigned long long int stream_start_reg();
    unsigned long long int stream_end_reg();
    unsigned long long int flash_settings_addr();
    unsigned long long int flash_mixer_vol_addr();
    unsigned long long int flash_mixer_pan_addr();
    unsigned long long int flash_mixer_hw_addr();

    /* Low-level flash memory functions */
    signed int wait_while_busy(unsigned int init_delay);
    signed int get_revision(unsigned int *revision);
    signed int read_flash(fb_nodeaddr_t addr, quadlet_t *buf, unsigned int n_quads);

    /* Upper level flash memory functions */
    signed int read_device_settings(void);

    /* Hardware functions */
    signed int init_hardware(void);
    signed int set_hardware_params(FF_software_settings_t *sw_settings);
    signed int read_tco(quadlet_t *tco_data, signed int size);
    signed int write_tco(quadlet_t *tco_data, signed int size);
    
};

}

#endif
