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
#include "motu_mark3_controls.h"

/* Bitmasks and values used when setting MOTU device registers.  Note that
 * the only "generation 1" device presently supported is the original 828.
 * "Generation 2" devices include the original Traveler, the 828Mk2, the
 * original Ultralite, the 896/896HD and so forth.  "Generation 3" devices
 * are all those which carry the "Mark 3" moniker.
 */
#define MOTU_RATE_BASE_44100         (0<<3)
#define MOTU_RATE_BASE_48000         (1<<3)
#define MOTU_RATE_MULTIPLIER_1X      (0<<4)
#define MOTU_RATE_MULTIPLIER_2X      (1<<4)
#define MOTU_RATE_MULTIPLIER_4X      (2<<4)
#define MOTU_RATE_BASE_MASK          (0x00000008)
#define MOTU_RATE_MULTIPLIER_MASK    (0x00000030)

#define MOTU_G2_OPTICAL_MODE_OFF       0x00
#define MOTU_G2_OPTICAL_MODE_ADAT      0x01
#define MOTU_G2_OPTICAL_MODE_TOSLINK   0x02
#define MOTU_G2_OPTICAL_IN_MODE_MASK   (0x00000300)
#define MOTU_G2_OPTICAL_IN_MODE_BIT0   8
#define MOTU_G2_OPTICAL_OUT_MODE_MASK  (0x00000c00)
#define MOTU_G2_OPTICAL_OUT_MODE_BIT0  10
#define MOTU_G2_OPTICAL_MODE_MASK      (MOTU_G2_OPTICAL_IN_MODE_MASK|MOTU_G2_OPTICAL_MODE_MASK)

#define MOTU_G2_CLKSRC_MASK             0x00000007
#define MOTU_G2_CLKSRC_INTERNAL         0
#define MOTU_G2_CLKSRC_ADAT_OPTICAL     1
#define MOTU_G2_CLKSRC_SPDIF_TOSLINK    2
#define MOTU_G2_CLKSRC_SMPTE            3
#define MOTU_G2_CLKSRC_WORDCLOCK        4
#define MOTU_G2_CLKSRC_ADAT_9PIN        5
#define MOTU_G2_CLKSRC_AES_EBU          7

#define MOTU_METER_PEAKHOLD_MASK     0x3800
#define MOTU_METER_PEAKHOLD_SHIFT    11
#define MOTU_METER_CLIPHOLD_MASK     0x0700
#define MOTU_METER_CLIPHOLD_SHIFT    8
#define MOTU_METER_AESEBU_SRC_MASK   0x0004
#define MOTU_METER_AESEBU_SRC_SHIFT  2
#define MOTU_METER_PROG_SRC_MASK     0x0003
#define MOTU_METER_PROG_SRC_SHIFT    0

/* Device registers.  The "base" registers (that is, those shared
 * between the G1, G2 and G3 interfaces) are specified and assumed to
 * be relative to MOTU_REG_BASE_ADDR.  The following block is notionally
 * for G2 devices, although some of these are shared with G1 and G3 
 * units too.
 */
#define MOTU_REG_BASE_ADDR         0xfffff0000000ULL
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
#define MOTU_REG_INPUT_GAIN_PHINV0 0x0c70
#define MOTU_REG_INPUT_GAIN_PHINV1 0x0c74
#define MOTU_REG_INPUT_GAIN_PHINV2 0x0c78

/* Device register definitions for the earliest generation devices */
#define MOTU_G1_REG_CONFIG         0x0b00
#define MOTU_G1_REG_ISOCTRL        0x0b10

/* Values written to registers in G1 devices.
 *
 * There's an unknown subtlety regarding the optical mode of the "generation
 * 1" devices such as the 828Mk1.  It seems that the same configuration
 * register setting is used for "off" and "adat" modes.  There must be more
 * to this though because the number of audio channels sent presumedly
 * changes when adat mode is selected; there must be some way that the
 * device deduces the mode.
 */
#define MOTU_G1_OPT_IN_MODE_MASK   0x8000
#define MOTU_G1_OPT_IN_MODE_BIT0       15
#define MOTU_G1_OPT_OUT_MODE_MASK  0x4000
#define MOTU_G1_OPT_OUT_MODE_BIT0      14
#define MOTU_G1_OPTICAL_OFF        0x0000
#define MOTU_G1_OPTICAL_TOSLINK    0x0001
#define MOTU_G1_OPTICAL_ADAT       0x0000

#define MOTU_G1_RATE_MASK          0x0004
#define MOTU_G1_RATE_44100         0x0000
#define MOTU_G1_RATE_48000         0x0004

#define MOTU_G1_CLKSRC_MASK        0x0003
#define MOTU_G1_CLKSRC_INTERNAL    0x0000
#define MOTU_G1_CLKSRC_ADAT_9PIN   0x0001
#define MOTU_G1_CLKSRC_SPDIF       0x0002
#define MOTU_G1_CLKSRC_UNCHANGED   MOTU_CLKSRC_UNCHANGED

#define MOTU_G1_MONIN_MASK         0x3f00
#define MOTU_G1_MONIN_L_SRC_MASK   0x0600
#define MOTU_G1_MONIN_R_SRC_MASK   0x3000
#define MOTU_G1_MONIN_L_MUTE_MASK  0x0100  // Yes, the sense of these 2 bits
#define MOTU_G1_MONIN_R_EN_MASK    0x0800  //   really are reversed
#define MOTU_G1_MONIN_L_MUTE       0x0100
#define MOTU_G1_MONIN_L_ENABLE     0x0000
#define MOTU_G1_MONIN_R_MUTE       0x0000
#define MOTU_G1_MONIN_R_ENABLE     0x0800
#define MOTU_G1_MONIN_L_CH1        0x0000
#define MOTU_G1_MONIN_L_CH3        0x0020
#define MOTU_G1_MONIN_L_CH5        0x0040
#define MOTU_G1_MONIN_L_CH7        0x0060
#define MOTU_G1_MONIN_R_CH2        0x0000
#define MOTU_G1_MONIN_R_CH4        0x1000
#define MOTU_G1_MONIN_R_CH6        0x2000
#define MOTU_G1_MONIN_R_CH8        0x3000

/* Mark3 device registers - these don't always have MOTU_BASE_ADDR as the
 * base address so for now we'll define them as absolute addresses.  The
 * "Mark 3" (aka G3) devices share a number of control registers with the G2
 * devices.  Where this occurs we just use the G2 definitions since there's
 * little to be gained by adding duplicate defines.
 */
#define MOTU_G3_REG_MIXER        0xffff00010000ULL
#define MOTU_G3_REG_OPTICAL_CTRL 0xfffff0000c94ULL

/* Mark3 (aka G3) register constants for cases where the G3 devices differ
 * from the G2s.
 */
#define MOTU_G3_CLKSRC_MASK      0x0000001b
#define MOTU_G3_CLKSRC_INTERNAL  0x00000000
#define MOTU_G3_CLKSRC_WORDCLOCK 0x00000001
#define MOTU_G3_CLKSRC_SMPTE     0x00000002
#define MOTU_G3_CLKSRC_SPDIF     0x00000010
#define MOTU_G3_CLKSRC_OPTICAL_A 0x00000018
#define MOTU_G3_CLKSRC_OPTICAL_B 0x00000019
#define MOTU_G3_CLKSRC_ADAT_A    MOTU_G3_CLKSRC_OPTICAL_A
#define MOTU_G3_CLKSRC_ADAT_B    MOTU_G3_CLKSRC_OPTICAL_B
#define MOTU_G3_CLKSRC_TOSLINK_A MOTU_G3_CLKSRC_OPTICAL_A
#define MOTU_G3_CLKSRC_TOSLINK_B MOTU_G3_CLKSRC_OPTICAL_A

#define MOTU_G3_RATE_BASE_44100       (0<<8)
#define MOTU_G3_RATE_BASE_48000       (1<<8)
#define MOTU_G3_RATE_MULTIPLIER_1X    (0<<9)
#define MOTU_G3_RATE_MULTIPLIER_2X    (1<<9)
#define MOTU_G3_RATE_MULTIPLIER_4X    (2<<9)
#define MOTU_G3_RATE_BASE_MASK        (0x00000100)
#define MOTU_G3_RATE_MULTIPLIER_MASK  (0x00000600)

#define MOTU_G3_OPT_A_IN_ENABLE   0x00000001
#define MOTU_G3_OPT_B_IN_ENABLE   0x00000002
#define MOTU_G3_OPT_A_OUT_ENABLE  0x00000100
#define MOTU_G3_OPT_B_OUT_ENABLE  0x00000200
#define MOTU_G3_OPT_A_IN_TOSLINK  0x00010000  // If these mode bits are not 
#define MOTU_G3_OPT_A_OUT_TOSLINK 0x00040000  //   set the mode is ADAT if
#define MOTU_G3_OPT_B_IN_TOSLINK  0x00100000  //   the port is enabled
#define MOTU_G3_OPT_B_OUT_TOSLINK 0x00400000
#define MOTU_G3_OPT_A_IN_MASK     (MOTU_G3_OPT_A_IN_ENABLE|MOTU_G3_OPT_A_IN_TOSLINK)
#define MOTU_G3_OPT_A_OUT_MASK    (MOTU_G3_OPT_A_OUT_ENABLE|MOTU_G3_OPT_A_OUT_TOSLINK)
#define MOTU_G3_OPT_B_IN_MASK     (MOTU_G3_OPT_B_IN_ENABLE|MOTU_G3_OPT_B_IN_TOSLINK)
#define MOTU_G3_OPT_B_OUT_MASK    (MOTU_G3_OPT_B_OUT_ENABLE|MOTU_G3_OPT_B_OUT_TOSLINK)

/* The following values are used when defining configuration structures and
 * calling driver functions.  They are generally not raw values written to
 * registers.
 */

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
#define MOTU_PA_IN               0x0200
#define MOTU_PA_OUT              0x0400
#define MOTU_PA_INOUT            (MOTU_PA_IN | MOTU_PA_OUT)
/* The Mark3 devices have two optical ports */
#define MOTU_PA_MK3_OPT_A_OFF     MOTU_PA_OPTICAL_OFF
#define MOTU_PA_MK3_OPT_A_ADAT    MOTU_PA_OPTICAL_ADAT
#define MOTU_PA_MK3_OPT_A_TOSLINK MOTU_PA_OPTICAL_TOSLINK
#define MOTU_PA_MK3_OPT_A_ON      (MOTU_PA_MK3_OPT_A_ADAT|MOTU_PA_MK3_OPT_A_TOSLINK)
#define MOTU_PA_MK3_OPT_A_ANY     (MOTU_PA_MK3_OPT_A_OFF|MOTU_PA_MK3_OPT_A_ON)
#define MOTU_PA_MK3_OPT_A_MASK    MOTU_PA_MK3_OPT_A_ANY
#define MOTU_PA_MK3_OPT_B_OFF     0x1000
#define MOTU_PA_MK3_OPT_B_ADAT    0x2000
#define MOTU_PA_MK3_OPT_B_TOSLINK 0x4000
#define MOTU_PA_MK3_OPT_B_ON      (MOTU_PA_MK3_OPT_B_ADAT|MOTU_PA_MK3_OPT_B_TOSLINK)
#define MOTU_PA_MK3_OPT_B_ANY     (MOTU_PA_MK3_OPT_B_OFF|MOTU_PA_MK3_OPT_B_ON)
#define MOTU_PA_MK3_OPT_B_MASK    MOTU_PA_MK3_OPT_B_ANY
#define MOTU_PA_MK3_OPT_ANY       (MOTU_PA_MK3_OPT_A_ANY|MOTU_PA_MK3_OPT_B_ANY)

/* Generic "direction" flags */
#define MOTU_DIR_IN          1
#define MOTU_DIR_OUT         2
#define MOTU_DIR_INOUT       (MOTU_DIR_IN | MOTU_DIR_OUT)

/* Optical port mode settings/flags */
#define MOTU_OPTICAL_MODE_OFF     0x0000
#define MOTU_OPTICAL_MODE_ADAT    0x0001
#define MOTU_OPTICAL_MODE_TOSLINK 0x0002
#define MOTU_OPTICAL_MODE_KEEP    0xffff
#define MOTU_OPTICAL_MODE_NONE    0xffffffff

/* Clock source settings/flags */
#define MOTU_CLKSRC_INTERNAL         0
#define MOTU_CLKSRC_ADAT_OPTICAL     1
#define MOTU_CLKSRC_SPDIF_TOSLINK    2
#define MOTU_CLKSRC_SMPTE            3
#define MOTU_CLKSRC_WORDCLOCK        4
#define MOTU_CLKSRC_ADAT_9PIN        5
#define MOTU_CLKSRC_AES_EBU          6
#define MOTU_CLKSRC_OPTICAL_A        7
#define MOTU_CLKSRC_OPTICAL_B        8
#define MOTU_CLKSRC_LAST             8
#define MOTU_CLKSRC_NONE             0xffff
#define MOTU_CLKSRC_UNCHANGED        MOTU_CLKSRC_NONE

/* Device generation identifiers */
#define MOTU_DEVICE_G1            0x0001
#define MOTU_DEVICE_G2            0x0002
#define MOTU_DEVICE_G3            0x0003

class ConfigRom;
class Ieee1394Service;

namespace Util {
    class Configuration;
}

namespace Motu {

enum EMotuModel {
    MOTU_MODEL_NONE             = 0x0000,
    MOTU_MODEL_828mkII          = 0x0001,
    MOTU_MODEL_TRAVELER         = 0x0002,
    MOTU_MODEL_ULTRALITE        = 0x0003,
    MOTU_MODEL_8PRE             = 0x0004,
    MOTU_MODEL_828MkI           = 0x0005,
    MOTU_MODEL_896HD            = 0x0006,
    MOTU_MODEL_828mk3           = 0x0007,
    MOTU_MODEL_ULTRALITEmk3     = 0x0008,
    MOTU_MODEL_ULTRALITEmk3_HYB = 0x0009,
    MOTU_MODEL_TRAVELERmk3      = 0x000a, // Placeholder only at present
    MOTU_MODEL_896HDmk3         = 0x000b, // Placeholder only at present
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
    unsigned int port_flags;
    unsigned int port_offset;
};

struct PortGroupEntry {
    const char *group_name_format;
    signed int n_channels;
    unsigned int flags;
    signed int port_order;
    signed int port_num_offset;
    signed int group_pkt_offset;
};

// Structures used for pre-Mark3 device mixer definitions
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

// Structures used for devices which use the "Mark3" mixer protocol
struct MotuMark3Mixer {
};

struct DevicePropertyEntry {
    PortGroupEntry *portgroup_entry;
    unsigned int n_portgroup_entries;
    const PortEntry* port_entry;
    unsigned int n_port_entries;
    signed int MaxSampleRate;
    // A device will set at most one of the *mixer fields
    const struct MotuMixer *mixer;
    const struct MotuMark3Mixer *m3mixer;
    // Others features can be added here like MIDI port presence.
};

extern const DevicePropertyEntry DevicesProperty[];

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

    unsigned int getHwClockSource();
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
    enum FFADODevice::eStreamingState getStreamingState();

    virtual bool prepare();
    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    signed int getDeviceGeneration(void);

    signed int getIsoRecvChannel(void);
    signed int getIsoSendChannel(void);
    unsigned int getOpticalMode(unsigned int dir, unsigned int *port_a_mode, 
        unsigned int *port_b_mode);
    signed int setOpticalMode(unsigned int dir, 
        unsigned int port_a_mode, unsigned int port_b_mode);

    signed int getEventSize(unsigned int dir);

    signed int       m_motu_model;
protected:
    struct VendorModelEntry * m_model;
    signed int m_iso_recv_channel, m_iso_send_channel;
    signed int m_rx_bandwidth, m_tx_bandwidth;

    Streaming::MotuReceiveStreamProcessor *m_receiveProcessor;
    Streaming::MotuTransmitStreamProcessor *m_transmitProcessor;

private:
    bool buildMixerAudioControls(void);
    bool buildMark3MixerAudioControls(void);
    bool addPort(Streaming::StreamProcessor *s_processor,
        char *name,
        enum Streaming::Port::E_Direction direction,
        int position, int size);
    bool addDirPorts(
        enum Streaming::Port::E_Direction direction, unsigned int sample_rate, 
        unsigned int optical_a_mode, unsigned int optical_b_mode);
    bool addDirPortGroups(
        enum Streaming::Port::E_Direction direction, unsigned int sample_rate, 
        unsigned int optical_a_mode, unsigned int optical_b_mode);

public:
    unsigned int ReadRegister(fb_nodeaddr_t reg);
    signed int WriteRegister(fb_nodeaddr_t reg, quadlet_t data);

private:
    Control::Container *m_MixerContainer;
    Control::Container *m_ControlContainer;
};

}

#endif
