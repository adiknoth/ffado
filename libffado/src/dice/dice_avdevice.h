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

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libstreaming/amdtp/AmdtpReceiveStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpTransmitStreamProcessor.h"
#include "libstreaming/amdtp/AmdtpPort.h"

#include "libieee1394/ieee1394service.h"

#include <string>
#include <vector>

class ConfigRom;
class Ieee1394Service;

namespace Util {
    class Configuration;
}

namespace Dice {

class Notifier;

class Device : public FFADODevice {
// private:
public:
    class Notifier;
    class EAP;

public:
    Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    ~Device();

    static bool probe( Util::Configuration& c, ConfigRom& configRom, bool generic = false );
    static FFADODevice * createDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual bool discover();

    static int getConfigurationId( );

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
    bool setDeviceNickName(std::string name);

    enum eClockSourceType  clockIdToType(unsigned int id);
    bool isClockSourceIdLocked(unsigned int id, quadlet_t ext_status_reg);
    bool isClockSourceIdSlipping(unsigned int id, quadlet_t ext_status_reg);

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

// private:
public:
    // notification
    Notifier *m_notifier;

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

    /**
     * this class represents the EAP interface
     * available on some devices
     */
    class EAP
    {
    public:
        class Router;
        class Mixer;

    private:
        enum eWaitReturn {
            eWR_Error,
            eWR_Timeout,
            eWR_Busy,
            eWR_Done,
        };
        enum eRegBase {
            eRT_Base,
            eRT_Capability,
            eRT_Command,
            eRT_Mixer,
            eRT_Peak,
            eRT_NewRouting,
            eRT_NewStreamCfg,
            eRT_CurrentCfg,
            eRT_Standalone,
            eRT_Application,
        };

    public:
        EAP(Device &);
        virtual ~EAP();

        static bool supportsEAP(Device &);
        bool init();

        void show();
        enum eWaitReturn operationBusy();
        enum eWaitReturn waitForOperationEnd(int max_wait_time_ms = 100);
        bool loadRouterConfig(bool low, bool mid, bool high);
        bool loadStreamConfig(bool low, bool mid, bool high);
        bool loadRouterAndStreamConfig(bool low, bool mid, bool high);
        bool loadFlashConfig();
        bool storeFlashConfig();
        
    private:
        bool     m_router_exposed;
        bool     m_router_readonly;
        bool     m_router_flashstored;
        uint16_t m_router_nb_entries;

        bool     m_mixer_exposed;
        bool     m_mixer_readonly;
        bool     m_mixer_flashstored;
        uint8_t  m_mixer_input_id;
        uint8_t  m_mixer_output_id;
        uint8_t  m_mixer_nb_inputs;
        uint8_t  m_mixer_nb_outputs;

        bool     m_general_support_dynstream;
        bool     m_general_support_flash;
        bool     m_general_peak_enabled;
        uint8_t  m_general_max_tx;
        uint8_t  m_general_max_rx;
        bool     m_general_stream_cfg_stored;
        uint16_t m_general_chip;

        bool commandHelper(fb_quadlet_t cmd);

        bool readReg(enum eRegBase, unsigned offset, quadlet_t *);
        bool writeReg(enum eRegBase, unsigned offset, quadlet_t);
        bool readRegBlock(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        bool writeRegBlock(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        bool readRegBlockSwapped(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        bool writeRegBlockSwapped(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        fb_nodeaddr_t offsetGen(enum eRegBase, unsigned, size_t);

        Device &m_device;
        DECLARE_DEBUG_MODULE_REFERENCE;

        fb_quadlet_t m_capability_offset;
        fb_quadlet_t m_capability_size;
        fb_quadlet_t m_cmd_offset;
        fb_quadlet_t m_cmd_size;
        fb_quadlet_t m_mixer_offset;
        fb_quadlet_t m_mixer_size;
        fb_quadlet_t m_peak_offset;
        fb_quadlet_t m_peak_size;
        fb_quadlet_t m_new_routing_offset;
        fb_quadlet_t m_new_routing_size;
        fb_quadlet_t m_new_stream_cfg_offset;
        fb_quadlet_t m_new_stream_cfg_size;
        fb_quadlet_t m_curr_cfg_offset;
        fb_quadlet_t m_curr_cfg_size;
        fb_quadlet_t m_standalone_offset;
        fb_quadlet_t m_standalone_size;
        fb_quadlet_t m_app_offset;
        fb_quadlet_t m_app_size;

    public: // mixer subclass
        class Mixer {
        public:
            Mixer(EAP &);
            ~Mixer();

            bool init();
            void show();
            bool updateCoefficients();

        private:
            EAP &m_parent;
            fb_quadlet_t *m_coeff;

            DECLARE_DEBUG_MODULE_REFERENCE;
        };

        class Router {
        public:
            Router(EAP &);
            ~Router();

            bool init();
            void show();

        private:
            EAP &m_parent;

            DECLARE_DEBUG_MODULE_REFERENCE;
        };

    };

};

}
#endif
