/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#ifndef DICEDEVICE_H
#define DICEDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libieee1394/ARMHandler.h"

#include <string>
#include <vector>

class ConfigRom;
class Ieee1394Service;

namespace Dice {

class DiceNotifier;

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
};

class DiceAvDevice : public IAvDevice {
private:
    class DiceNotifier;
public:
    DiceAvDevice( std::auto_ptr<ConfigRom>( configRom ),
                  Ieee1394Service& ieee1394Service,
                  int nodeId);
    ~DiceAvDevice();

    static bool probe( ConfigRom& configRom );
    static int getConfigurationId( );
    virtual bool discover();

    virtual void showDevice();

    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();

    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    virtual bool enableStreaming();
    virtual bool disableStreaming();

protected:
    struct VendorModelEntry *m_model;

    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;

private: // streaming & port helpers
    enum EPortTypes {
        ePT_Analog,
        ePT_MIDI,
    };

    typedef struct {
        std::string name;
        enum EPortTypes portType;
        unsigned int streamPosition;
        unsigned int streamLocation;
    } diceChannelInfo;

    bool addChannelToProcessor( diceChannelInfo *,
                              Streaming::StreamProcessor *,
                              Streaming::Port::E_Direction direction);

    int allocateIsoChannel(unsigned int packet_size);
    bool deallocateIsoChannel(int channel);

private: // helper functions
    bool enableIsoStreaming();
    bool disableIsoStreaming();
    bool isIsoStreamingEnabled();

    bool maskedCheckZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask);
    bool maskedCheckNotZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask);

    typedef std::vector< std::string > diceNameVector;
    typedef std::vector< std::string >::iterator diceNameVectorIterator;
    diceNameVector splitNameString(std::string in);
    diceNameVector getTxNameString(unsigned int i);
    diceNameVector getRxNameString(unsigned int i);
    diceNameVector getClockSourceNameString();
    std::string getDeviceNickName();

private: // register I/O routines
    bool initIoFunctions();
    // quadlet read/write routines
    bool readReg(fb_nodeaddr_t, fb_quadlet_t *);
    bool writeReg(fb_nodeaddr_t, fb_quadlet_t);
    bool readRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);
    bool writeRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);

    bool readGlobalReg(fb_nodeaddr_t, fb_quadlet_t *);
    bool writeGlobalReg(fb_nodeaddr_t, fb_quadlet_t);
    bool readGlobalRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);
    bool writeGlobalRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);
    fb_nodeaddr_t globalOffsetGen(fb_nodeaddr_t, size_t);

    bool readTxReg(unsigned int i, fb_nodeaddr_t, fb_quadlet_t *);
    bool writeTxReg(unsigned int i, fb_nodeaddr_t, fb_quadlet_t);
    bool readTxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length);
    bool writeTxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length);
    fb_nodeaddr_t txOffsetGen(unsigned int, fb_nodeaddr_t, size_t);

    bool readRxReg(unsigned int i, fb_nodeaddr_t, fb_quadlet_t *);
    bool writeRxReg(unsigned int i, fb_nodeaddr_t, fb_quadlet_t);
    bool readRxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length);
    bool writeRxRegBlock(unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *data, size_t length);
    fb_nodeaddr_t rxOffsetGen(unsigned int, fb_nodeaddr_t, size_t);

    fb_quadlet_t m_global_reg_offset;
    fb_quadlet_t m_global_reg_size;
    fb_quadlet_t m_tx_reg_offset;
    fb_quadlet_t m_tx_reg_size;
    fb_quadlet_t m_rx_reg_offset;
    fb_quadlet_t m_rx_reg_size;
    fb_quadlet_t m_unused1_reg_offset;
    fb_quadlet_t m_unused1_reg_size;
    fb_quadlet_t m_unused2_reg_offset;
    fb_quadlet_t m_unused2_reg_size;

    fb_quadlet_t m_nb_tx;
    fb_quadlet_t m_tx_size;
    fb_quadlet_t m_nb_rx;
    fb_quadlet_t m_rx_size;

private:
    // notification
    DiceNotifier *m_notifier;

    /**
     * this class reacts on the DICE device writing to the
     * hosts notify address
     */
    #define DICE_NOTIFIER_BASE_ADDRESS 0x0000FFFFE0000000ULL
    #define DICE_NOTIFIER_BLOCK_LENGTH 4
    class DiceNotifier : public ARMHandler
    {
    public:
        DiceNotifier(DiceAvDevice *, nodeaddr_t start);
        virtual ~DiceNotifier();

    private:
        DiceAvDevice *m_dicedevice;
    };

};

}
#endif
