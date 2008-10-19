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

#define MOTU_BASE_ADDR               0xfffff0000000ULL

#define MOTU_RATE_BASE_44100         (0<<3)
#define MOTU_RATE_BASE_48000         (1<<3)
#define MOTU_RATE_MULTIPLIER_1X      (0<<4)
#define MOTU_RATE_MULTIPLIER_2X      (1<<4)
#define MOTU_RATE_MULTIPLIER_4X      (2<<4)
#define MOTU_RATE_BASE_MASK          (0x00000008)
#define MOTU_RATE_MULTIPLIER_MASK    (0x00000030)

#define MOTU_OPTICAL_MODE_OFF        0x00
#define MOTU_OPTICAL_MODE_ADAT       0x01
#define MOTU_OPTICAL_MODE_TOSLINK    0x02
#define MOTU_OPTICAL_IN_MODE_MASK    (0x00000300)
#define MOTU_OPTICAL_OUT_MODE_MASK   (0x00000c00)
#define MOTU_OPTICAL_MODE_MASK       (MOTU_OPTICAL_IN_MODE_MASK|MOTU_OPTICAL_MODE_MASK)

#define MOTU_CLKSRC_MASK             0x00000007
#define MOTU_CLKSRC_INTERNAL         0
#define MOTU_CLKSRC_ADAT_OPTICAL     1
#define MOTU_CLKSRC_SPDIF_TOSLINK    2
#define MOTU_CLKSRC_SMPTE            3
#define MOTU_CLKSRC_WORDCLOCK        4
#define MOTU_CLKSRC_ADAT_9PIN        5
#define MOTU_CLKSRC_AES_EBU          7
#define MOTU_CLKSRC_NONE             0xffff
#define MOTU_CLKSRC_UNCHANGED        MOTU_CLKSRC_NONE

#define MOTU_DIR_IN          1
#define MOTU_DIR_OUT         2
#define MOTU_DIR_INOUT       (MOTU_DIR_IN | MOTU_DIR_OUT)

#define MOTU_METER_PEAKHOLD_MASK     0x3800
#define MOTU_METER_PEAKHOLD_SHIFT    11
#define MOTU_METER_CLIPHOLD_MASK     0x0700
#define MOTU_METER_CLIPHOLD_SHIFT    8
#define MOTU_METER_AESEBU_SRC_MASK   0x0004
#define MOTU_METER_AESEBU_SRC_SHIFT  2
#define MOTU_METER_PROG_SRC_MASK     0x0003
#define MOTU_METER_PROG_SRC_SHIFT    0

/* Device registers */
#define MOTU_REG_ISOCTRL           0x0b00
#define MOTU_REG_OPTICAL_CTRL      0x0b10
#define MOTU_REG_CLK_CTRL          0x0b14
#define MOTU_REG_896HD_METER_REG   0x0b1c
#define MOTU_REG_896HD_METER_CONF  0x0b24
#define MOTU_REG_ROUTE_PORT_CONF   0x0c04
#define MOTU_REG_INPUT_LEVEL       0x0c08
#define MOTU_REG_INPUT_BOOST       0x0c14
#define MOTU_REG_INPUT_GAIN_PAD_0  0x0c1c
#define MOTU_REG_CLKSRC_NAME0      0x0c60

/* Port Active Flags (ports declaration) */
#define MOTU_PA_RATE_1x          0x0001    /* 44k1 or 48k */
#define MOTU_PA_RATE_2x          0x0002    /* 88k2 or 96k */
#define MOTU_PA_RATE_4x          0x0004    /* 176k4 or 192k */
#define MOTU_PA_RATE_1x2x        (MOTU_PA_RATE_1x|MOTU_PA_RATE_2x)
#define MOTU_PA_RATE_ANY         (MOTU_PA_RATE_1x|MOTU_PA_RATE_2x|MOTU_PA_RATE_4x)
#define MOTU_PA_RATE_MASK        MOTU_PA_RATE_ANY
#define MOTU_PA_OPTICAL_OFF      0x0010    /* Optical port off */
#define MOTU_PA_OPTICAL_ADAT     0x0020    /* Optical port in ADAT mode */
#define MOTU_PA_OPTICAL_TOSLINK  0x0040    /* Optical port in SPDIF/Toslink mode */
#define MOTU_PA_OPTICAL_ON       (MOTU_PA_OPTICAL_ADAT|MOTU_PA_OPTICAL_TOSLINK)
#define MOTU_PA_OPTICAL_ANY      (MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ON)
#define MOTU_PA_OPTICAL_MASK     MOTU_PA_OPTICAL_ANY
#define MOTU_PA_PADDING          0x0100

class ConfigRom;
class Ieee1394Service;

namespace Util {
    class Configuration;
}

namespace Motu {

enum EMotuModel {
    MOTU_MODEL_NONE     = 0x0000,
    MOTU_MODEL_828mkII  = 0x0001,
    MOTU_MODEL_TRAVELER = 0x0002,
    MOTU_MODEL_ULTRALITE= 0x0003,
    MOTU_MODEL_8PRE     = 0x0004,
    MOTU_MODEL_828MkI   = 0x0005,
    MOTU_MODEL_896HD    = 0x0006,
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

struct MatrixMixBus {
    const char *name;
    unsigned int address;
};

struct MatrixMixChannel {
    const char *name;
    unsigned int flags;
    unsigned int addr_ofs;
};

struct MotuMixer {
    const MixerCtrl *mixer_ctrl;
    unsigned int n_mixer_ctrls;
    const MatrixMixBus *mixer_buses;
    unsigned int n_mixer_buses;
    const MatrixMixChannel *mixer_channels;
    unsigned int n_mixer_channels;
};

struct DevicePropertyEntry {
    const PortEntry* port_entry;
    unsigned int n_port_entries;
    signed int MaxSampleRate;
    const struct MotuMixer *mixer;
    // Others features can be added here like MIDI port presence.
};

/* Macro to calculate the size of an array */
#define N_ELEMENTS(_array) (sizeof(_array) / sizeof((_array)[0]))

/* Macro to define a MotuMixer structure succintly */
#define MOTUMIXER(_ctrls, _buses, _channels) \
    { _ctrls, N_ELEMENTS(_ctrls), _buses, N_ELEMENTS(_buses), _channels, N_ELEMENTS(_channels), }

class MotuDevice : public FFADODevice {
public:

    MotuDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ) );
    virtual ~MotuDevice();

    virtual bool buildMixer();
    virtual bool destroyMixer();

    static bool probe( Util::Configuration&, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    static int getConfigurationId( );
    virtual bool discover();

    virtual void showDevice();

    bool setClockCtrlRegister(signed int samplingFrequency, unsigned int clock_source);
    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );
    virtual std::vector<int> getSupportedSamplingFrequencies();

    FFADODevice::ClockSource clockIdToClockSource(unsigned int id);
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

    signed int       m_motu_model;
protected:
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

public:
    unsigned int ReadRegister(unsigned int reg);
    signed int WriteRegister(unsigned int reg, quadlet_t data);

private:
    Control::Container *m_MixerContainer;
    Control::Container *m_ControlContainer;
};

}

#endif
