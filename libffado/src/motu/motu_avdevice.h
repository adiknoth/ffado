/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Jonathan Woithe
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef MOTUDEVICE_H
#define MOTUDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libstreaming/MotuStreamProcessor.h"

#define MOTUFW_BASE_ADDR                0xfffff0000000ULL

#define MOTUFW_RATE_BASE_44100          (0<<3)
#define MOTUFW_RATE_BASE_48000          (1<<3)
#define MOTUFW_RATE_MULTIPLIER_1X       (0<<4)
#define MOTUFW_RATE_MULTIPLIER_2X       (1<<4)
#define MOTUFW_RATE_MULTIPLIER_4X       (2<<4)
#define MOTUFW_RATE_BASE_MASK           (0x00000008)
#define MOTUFW_RATE_MULTIPLIER_MASK     (0x00000030)

#define MOTUFW_OPTICAL_MODE_OFF        0x00
#define MOTUFW_OPTICAL_MODE_ADAT    0x01
#define MOTUFW_OPTICAL_MODE_TOSLINK    0x02
#define MOTUFW_OPTICAL_IN_MODE_MASK    (0x00000300)
#define MOTUFW_OPTICAL_OUT_MODE_MASK    (0x00000c00)
#define MOTUFW_OPTICAL_MODE_MASK    (MOTUFW_OPTICAL_IN_MODE_MASK|MOTUFW_OPTICAL_MODE_MASK)

#define MOTUFW_CLKSRC_MASK        0x00000007
#define MOTUFW_CLKSRC_INTERNAL        0
#define MOTUFW_CLKSRC_ADAT_OPTICAL    1
#define MOTUFW_CLKSRC_SPDIF_TOSLINK    2
#define MOTUFW_CLKSRC_SMTPE        3
#define MOTUFW_CLKSRC_WORDCLOCK        4
#define MOTUFW_CLKSRC_ADAT_9PIN        5
#define MOTUFW_CLKSRC_AES_EBU        7

#define MOTUFW_DIR_IN            1
#define MOTUFW_DIR_OUT            2
#define MOTUFW_DIR_INOUT        (MOTUFW_DIR_IN | MOTUFW_DIR_OUT)

/* Device registers */
#define MOTUFW_REG_ISOCTRL        0x0b00
#define MOTUFW_REG_OPTICAL_CTRL        0x0b10
#define MOTUFW_REG_CLK_CTRL        0x0b14
#define MOTUFW_REG_ROUTE_PORT_CONF      0x0c04
#define MOTUFW_REG_CLKSRC_NAME0        0x0c60

class ConfigRom;
class Ieee1394Service;

namespace Motu {

enum EMotuModel {
      MOTUFW_MODEL_NONE     = 0x0000,
      MOTUFW_MODEL_828mkII  = 0x0001,
      MOTUFW_MODEL_TRAVELER = 0x0002,
};

struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    unsigned int unit_version;
    unsigned int unit_specifier_id;
    enum EMotuModel model;
    char *vendor_name;
    char *model_name;
};

class MotuDevice : public IAvDevice {
public:

    MotuDevice( std::auto_ptr<ConfigRom>( configRom ),
          Ieee1394Service& ieee1394Service,
          int nodeId );
    virtual ~MotuDevice();

    static bool probe( ConfigRom& configRom );
    virtual bool discover();

    virtual void showDevice();

    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );
    virtual int getConfigurationId( );

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
    signed int m_bandwidth;

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

};

}

#endif
