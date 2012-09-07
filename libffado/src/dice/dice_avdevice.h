/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef DICEDEVICE_H
#define DICEDEVICE_H

#include "dice_firmware_loader.h"

#include "ffadotypes.h"
#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libstreaming/amdtp/AmdtpReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"

#include "libieee1394/ieee1394service.h"

#include "libcontrol/Element.h"
#include "libcontrol/MatrixMixer.h"
#include "libcontrol/CrossbarRouter.h"

#include <string>
#include <vector>

class ConfigRom;
class Ieee1394Service;

namespace Util {
    class Configuration;
}

namespace Dice {

class EAP;


/**
  @brief Devices based on the DICE-platform

  This class is the basic implementation for devices using the DICE-chip.
  */
class Device : public FFADODevice {
private:
    friend class EAP;

public:
    /// constructor
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    /// destructor
    ~Device();

    static bool probe( Util::Configuration& c, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    static int getConfigurationId( );

    virtual void showDevice();
    bool canChangeNickname() { return true; }

    virtual bool deleteImgFL(const char*, bool v = true);
    virtual bool flashDiceFL(const char*, const char* image = "dice");
    virtual bool dumpFirmwareFL(const char*);
    virtual bool showDiceInfoFL();
    virtual bool showImgInfoFL();
    virtual bool testDiceFL(int);
    virtual DICE_FL_INFO_PARAM* showFlashInfoFL(bool v = true);
    virtual bool showAppInfoFL();

    virtual bool setSamplingFrequency( int samplingFrequency );
    virtual int getSamplingFrequency( );
    virtual std::vector<int> getSupportedSamplingFrequencies();

    virtual ClockSourceVector getSupportedClockSources();
    virtual bool setActiveClockSource(ClockSource);
    virtual ClockSource getActiveClockSource();

    virtual int getStreamCount();
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);
    virtual enum FFADODevice::eStreamingState getStreamingState();

    virtual bool prepare();

    virtual bool lock();
    virtual bool unlock();

    virtual bool startStreamByIndex(int i);
    virtual bool stopStreamByIndex(int i);

    virtual bool enableStreaming();
    virtual bool disableStreaming();

    virtual std::string getNickname();
    virtual bool setNickname(std::string name);

protected:
    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    typedef std::vector< Streaming::StreamProcessor * >::iterator StreamProcessorVectorIterator;
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

private: // active config
    enum eDiceConfig {
        eDC_Unknown,
        eDC_Low,
        eDC_Mid,
        eDC_High,
    };

    enum eDiceConfig getCurrentConfig();

private: // helper functions
    bool enableIsoStreaming();
    bool disableIsoStreaming();
    bool isIsoStreamingEnabled();

    bool maskedCheckZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask);
    bool maskedCheckNotZeroGlobalReg(fb_nodeaddr_t offset, fb_quadlet_t mask);

    stringlist splitNameString(std::string in);
    stringlist getTxNameString(unsigned int i);
    stringlist getRxNameString(unsigned int i);
    stringlist getCptrNameString(unsigned int);
    stringlist getPbckNameString(unsigned int);
    stringlist getClockSourceNameString();

    enum eClockSourceType  clockIdToType(unsigned int id);
    bool isClockSourceIdLocked(unsigned int id, quadlet_t ext_status_reg);
    bool isClockSourceIdSlipping(unsigned int id, quadlet_t ext_status_reg);

// EAP stuff
private:
    EAP*         m_eap;
protected:
    virtual EAP* createEAP();
public:
    EAP* getEAP() {return m_eap;};

private: // register I/O routines
    bool initIoFunctions();
    // functions used for RX/TX abstraction
    bool startstopStreamByIndex(int i, const bool start);
    bool prepareSP (unsigned int, const Streaming::Port::E_Direction direction_requested);
    void setRXTXfuncs (const Streaming::Port::E_Direction direction);

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

    fb_quadlet_t audio_base_register;
    fb_quadlet_t midi_base_register;
    char dir[3];

    // Function pointers to call readTxReg/readRxReg or writeTxReg/writeRxReg respectively
    bool (Device::*writeFunc) (unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t data);
    bool (Device::*readFunc) (unsigned int i, fb_nodeaddr_t offset, fb_quadlet_t *result);

// private:
public:
    /**
     * this class reacts on the DICE device writing to the
     * hosts notify address
     */
    #define DICE_NOTIFIER_BASE_ADDRESS 0x0000FFFFE0000000ULL
    #define DICE_NOTIFIER_BLOCK_LENGTH 4

    class Notifier : public Ieee1394Service::ARMHandler
    {
    public:
        Notifier(Device &, nodeaddr_t start);
        virtual ~Notifier();

    private:
        Device &m_device;
    };

    // notification
    Notifier *m_notifier;



};

}

#endif
