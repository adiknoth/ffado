/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Jonathan Woithe
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

#ifndef MOTUDEVICE_H
#define MOTUDEVICE_H

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libstreaming/motu/MotuReceiveStreamProcessor.h"
#include "libstreaming/motu/MotuTransmitStreamProcessor.h"

#include "motu_controls.h"

#define MOTUFW_BASE_ADDR               0xfffff0000000ULL

#define MOTUFW_RATE_BASE_44100         (0<<3)
#define MOTUFW_RATE_BASE_48000         (1<<3)
#define MOTUFW_RATE_MULTIPLIER_1X      (0<<4)
#define MOTUFW_RATE_MULTIPLIER_2X      (1<<4)
#define MOTUFW_RATE_MULTIPLIER_4X      (2<<4)
#define MOTUFW_RATE_BASE_MASK          (0x00000008)
#define MOTUFW_RATE_MULTIPLIER_MASK    (0x00000030)

#define MOTUFW_OPTICAL_MODE_OFF        0x00
#define MOTUFW_OPTICAL_MODE_ADAT       0x01
#define MOTUFW_OPTICAL_MODE_TOSLINK    0x02
#define MOTUFW_OPTICAL_IN_MODE_MASK    (0x00000300)
#define MOTUFW_OPTICAL_OUT_MODE_MASK   (0x00000c00)
#define MOTUFW_OPTICAL_MODE_MASK       (MOTUFW_OPTICAL_IN_MODE_MASK|MOTUFW_OPTICAL_MODE_MASK)

#define MOTUFW_CLKSRC_MASK             0x00000007
#define MOTUFW_CLKSRC_INTERNAL         0
#define MOTUFW_CLKSRC_ADAT_OPTICAL     1
#define MOTUFW_CLKSRC_SPDIF_TOSLINK    2
#define MOTUFW_CLKSRC_SMTPE            3
#define MOTUFW_CLKSRC_WORDCLOCK        4
#define MOTUFW_CLKSRC_ADAT_9PIN        5
#define MOTUFW_CLKSRC_AES_EBU          7

#define MOTUFW_DIR_IN          1
#define MOTUFW_DIR_OUT         2
#define MOTUFW_DIR_INOUT       (MOTUFW_DIR_IN | MOTUFW_DIR_OUT)

/* Device registers */
#define MOTUFW_REG_ISOCTRL         0x0b00
#define MOTUFW_REG_OPTICAL_CTRL    0x0b10
#define MOTUFW_REG_CLK_CTRL        0x0b14
#define MOTUFW_REG_ROUTE_PORT_CONF 0x0c04
#define MOTUFW_REG_CLKSRC_NAME0    0x0c60

/* Port Active Flags (ports declaration) */
#define MOTUFW_PA_RATE_1x          0x0001    /* 44k1 or 48k */
#define MOTUFW_PA_RATE_2x          0x0002    /* 88k2 or 96k */
#define MOTUFW_PA_RATE_4x          0x0004    /* 176k4 or 192k */
#define MOTUFW_PA_RATE_1x2x        (MOTUFW_PA_RATE_1x|MOTUFW_PA_RATE_2x)
#define MOTUFW_PA_RATE_ANY         (MOTUFW_PA_RATE_1x|MOTUFW_PA_RATE_2x|MOTUFW_PA_RATE_4x)
#define MOTUFW_PA_RATE_MASK        MOTUFW_PA_RATE_ANY
#define MOTUFW_PA_OPTICAL_OFF      0x0010    /* Optical port off */
#define MOTUFW_PA_OPTICAL_ADAT     0x0020    /* Optical port in ADAT mode */
#define MOTUFW_PA_OPTICAL_TOSLINK  0x0040    /* Optical port in SPDIF/Toslink mode */
#define MOTUFW_PA_OPTICAL_ON       (MOTUFW_PA_OPTICAL_ADAT|MOTUFW_PA_OPTICAL_TOSLINK)
#define MOTUFW_PA_OPTICAL_ANY      (MOTUFW_PA_OPTICAL_OFF|MOTUFW_PA_OPTICAL_ON)
#define MOTUFW_PA_OPTICAL_MASK     MOTUFW_PA_OPTICAL_ANY

class ConfigRom;
class Ieee1394Service;

namespace Motu {

enum EMotuModel {
    MOTUFW_MODEL_NONE     = 0x0000,
    MOTUFW_MODEL_828mkII  = 0x0001,
    MOTUFW_MODEL_TRAVELER = 0x0002,
    MOTUFW_MODEL_ULTRALITE= 0x0003,
    MOTUFW_MODEL_8PRE     = 0x0004,
    MOTUFW_MODEL_828MkI   = 0x0005,
    MOTUFW_MODEL_896HD    = 0x0006,
};

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    unsigned int unit_version;
    unsigned int unit_specifier_id;
    enum EMotuModel model;
    const char *vendor_name;
    const char *model_name;
};

struct PortEntry {
    const char *port_name;
    unsigned int port_dir;
    unsigned int port_flags;
    unsigned int port_offset;
};

struct MixerCtrl {
    const char *name, *label, *desc;
    unsigned int type;
    unsigned int dev_register;
};

struct DevicePropertyEntry {
    const PortEntry* port_entry;
    unsigned int n_port_entries;
    signed int MaxSampleRate;
    const MixerCtrl *mixer_ctrl;
    unsigned int n_mixer_ctrls;
    // Others features can be added here like MIDI port presence.
};

/* Macro to calculate the size of an array */
#define N_ELEMENTS(_array) (sizeof(_array) / sizeof((_array)[0]))

class MotuDevice : public FFADODevice {
// Declare mixer controls as friends so they can access the register 
// transaction functions.
friend class ChannelFader;
friend class ChannelPan;
public:

    MotuDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ) );
    virtual ~MotuDevice();

    virtual bool buildMixer();
    virtual bool destroyMixer();

    static bool probe( ConfigRom& configRom );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    static int getConfigurationId( );
    virtual bool discover();

    virtual void showDevice();

    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );

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

    signed int getIsoRecvChannel(void);
    signed int getIsoSendChannel(void);
    unsigned int getOpticalMode(unsigned int dir);
    signed int setOpticalMode(unsigned int dir, unsigned int mode);

    signed int getEventSize(unsigned int dir);

protected:
    signed int       m_motu_model;
    struct VendorModelEntry * m_model;
    signed int m_iso_recv_channel, m_iso_send_channel;
    signed int m_rx_bandwidth, m_tx_bandwidth;

    Streaming::MotuReceiveStreamProcessor *m_receiveProcessor;
    Streaming::MotuTransmitStreamProcessor *m_transmitProcessor;

private:
    bool addPort(Streaming::StreamProcessor *s_processor,
        char *name,
        enum Streaming::Port::E_Direction direction,
        int position, int size);
    bool addDirPorts(
        enum Streaming::Port::E_Direction direction,
        unsigned int sample_rate, unsigned int optical_mode);

    unsigned int ReadRegister(unsigned int reg);
    signed int WriteRegister(unsigned int reg, quadlet_t data);

    Control::Container *m_MixerContainer;
    Control::Container *m_ControlContainer;
};

}

#endif
