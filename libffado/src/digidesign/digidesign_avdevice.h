/*
 * Copyright (C) 2005-2011 by Jonathan Woithe
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

#ifndef DIGIDESIGNDEVICE_H
#define DIGIDESIGNDEVICE_H

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libutil/Configuration.h"

#include "libstreaming/digidesign/DigidesignReceiveStreamProcessor.h"
#include "libstreaming/digidesign/DigidesignTransmitStreamProcessor.h"

class ConfigRom;
class Ieee1394Service;

namespace Digidesign {

// The actual values within this enum are arbitary
enum EDigidesignModel {
    DIGIDESIGN_MODEL_NONE          = 0x0000,
    DIGIDESIGN_MODEL_003_RACK,
};

class Device : public FFADODevice {
public:

    Device( DeviceManager& d,
               std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Device();

    virtual bool buildMixer();
    virtual bool destroyMixer();

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

    signed int getNumChannels(void) { return num_channels; };
    signed int getFramesPerPacket(void);

    bool addPort(Streaming::StreamProcessor *s_processor,
             char *name, enum Streaming::Port::E_Direction direction,
             int position, int size);
    bool addDirPorts(enum Streaming::Port::E_Direction direction);

    unsigned int readRegister(fb_nodeaddr_t reg);
    signed int readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads);
    signed int writeRegister(fb_nodeaddr_t reg, quadlet_t data);
    signed int writeBlock(fb_nodeaddr_t reg, quadlet_t *data, unsigned int n_quads);

    /* General information functions */
    signed int getDigidesignModel(void) { return m_digidesign_model; }

protected:
    enum EDigidesignModel m_digidesign_model;

    // The following two members may not be required for Digidesign hardware.
    // It depends on whether the information is required in multiple places.
    // If it turns out that this isn't needed they can be removed.
    signed int num_channels;
    signed int frames_per_packet; // 1 frame includes 1 sample from each channel

    signed int iso_tx_channel, iso_rx_channel;

    Streaming::DigidesignReceiveStreamProcessor *m_receiveProcessor;
    Streaming::DigidesignTransmitStreamProcessor *m_transmitProcessor;

};

}

#endif
