/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef BOUNCEDEVICE_H
#define BOUNCEDEVICE_H

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_extended_cmd_generic.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "libieee1394/ARMHandler.h"

#include "ffadodevice.h"

#include <vector>

#define BOUNCE_REGISTER_BASE 0x0000FFFFE0000000ULL
#define BOUNCE_REGISTER_LENGTH (4*256)
#define BOUNCE_REGISTER_TX_ISOCHANNEL 0x10
#define BOUNCE_REGISTER_RX_ISOCHANNEL 0x14
#define BOUNCE_INVALID_OFFSET 0xFFFFF00000000000ULL

#define BOUNCE_NB_AUDIO_CHANNELS 4

// don't define more than 8 midi channels!
#define BOUNCE_NB_MIDI_CHANNELS  2

class ConfigRom;
class Ieee1394Service;

namespace Bounce {

// struct to define the supported devices
struct VendorModelEntry {
    uint32_t vendor_id;
    uint32_t model_id;
    uint32_t unit_specifier_id;
    char *vendor_name;
    char *model_name;
};

class BounceDevice : public FFADODevice {
private:
    class BounceNotifier;
public:
    BounceDevice( std::auto_ptr<ConfigRom>( configRom ),
          Ieee1394Service& ieee1394Service,
          int nodeId );
    virtual ~BounceDevice();

    static bool probe( ConfigRom& configRom );
    static int getConfigurationId( );
    virtual bool discover();

    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );

    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual int getStreamCount();

    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    virtual void showDevice();

protected:
    unsigned int m_samplerate;
    struct VendorModelEntry* m_model;

    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;

    bool addPortsToProcessor(
       Streaming::StreamProcessor *processor,
       Streaming::Port::E_Direction direction);

private: // generic helpers
    int allocateIsoChannel(unsigned int packet_size);
    bool deallocateIsoChannel(int channel);

protected: // I/O helpers
    // quadlet read/write routines
    bool readReg(fb_nodeaddr_t, fb_quadlet_t *);
    bool writeReg(fb_nodeaddr_t, fb_quadlet_t);
    bool readRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);
    bool writeRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);

private:
    BounceNotifier *m_Notifier;
    /**
     * this class reacts on the other side writing to the
     * hosts address space
     */
    #define BOUNCE_NOTIFIER_BASE_ADDRESS 0x0000FFFFE0000000ULL
    #define BOUNCE_NOTIFIER_BLOCK_LENGTH 4
    class BounceNotifier : public ARMHandler
    {
    public:
        BounceNotifier(BounceDevice *, nodeaddr_t start);
        virtual ~BounceNotifier();

    private:
        BounceDevice *m_bouncedevice;
    };
};

}

#endif
