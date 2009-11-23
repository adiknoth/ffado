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

#include "libcontrol/Element.h"
#include "libcontrol/MatrixMixer.h"
#include "libcontrol/CrossbarRouter.h"

#include "dice_eap.h"

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
    typedef std::vector< std::string > diceNameVector;
    typedef std::vector< std::string >::iterator diceNameVectorIterator;

public:
    class Notifier;
    class EAP;

    /**
     * this class represents the EAP interface
     * available on some devices
     */
    class EAP : public Control::Container
    {
    public:
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
            eRT_None,
        };
        enum eRouteSource {
            eRS_AES = 0,
            eRS_ADAT = 1,
            eRS_Mixer = 2,
            eRS_InS0 = 4,
            eRS_InS1 = 5,
            eRS_ARM = 10,
            eRS_ARX0 = 11,
            eRS_ARX1 = 12,
            eRS_Muted = 15,
            eRS_Invalid = 16,
        };
        enum eRouteDestination {
            eRD_AES = 0,
            eRD_ADAT = 1,
            eRD_Mixer0 = 2,
            eRD_Mixer1 = 3,
            eRD_InS0 = 4,
            eRD_InS1 = 5,
            eRD_ARM = 10,
            eRD_ATX0 = 11,
            eRD_ATX1 = 12,
            eRD_Muted = 15,
            eRD_Invalid = 16,
        };

    public:

        // ----------
        class RouterConfig {
        public:
            struct Route
            {
                enum eRouteSource src;
                int srcChannel;
                enum eRouteDestination dst;
                int dstChannel;
                int peak;
            };
            typedef std::vector<RouterConfig::Route> RouteVector;
            typedef std::vector<RouterConfig::Route>::iterator RouteVectorIterator;
            RouterConfig(EAP &);
            RouterConfig(EAP &, enum eRegBase, unsigned int offset);
            virtual ~RouterConfig();

            virtual bool read() {return read(m_base, m_offset);};
            virtual bool write() {return write(m_base, m_offset);};
            virtual bool read(enum eRegBase b, unsigned offset);
            virtual bool write(enum eRegBase b, unsigned offset);
            virtual void show();


            bool insertRoute(struct Route r)
                {return insertRoute(r, m_routes.size());};
            bool insertRoute(struct Route r, unsigned int index);
            bool replaceRoute(unsigned int old_index, struct Route new_route);
            bool replaceRoute(struct Route old_route, struct Route new_route);
            bool removeRoute(struct Route r);
            bool removeRoute(unsigned int index);
            int getRouteIndex(struct Route r);
            struct Route getRoute(unsigned int index);

            unsigned int getNbRoutes() {return m_routes.size();};

            struct Route getRouteForDestination(enum eRouteDestination dst, int channel);
            RouteVector getRoutesForSource(enum eRouteSource src, int channel);

            struct Route decodeRoute(uint32_t val);
            uint32_t encodeRoute(struct Route r);
        public:
            static enum eRouteDestination intToRouteDestination(int);
            static enum eRouteSource intToRouteSource(int);
        protected:
            EAP &m_eap;
            enum eRegBase m_base;
            unsigned int m_offset;
            RouteVector m_routes;
        protected:
            DECLARE_DEBUG_MODULE_REFERENCE;
        };

        // ----------
        // the peak space is a special version of a router config
        class PeakSpace : public RouterConfig {
        public:
            PeakSpace(EAP &p) : RouterConfig(p, eRT_Peak, 0) {};
            virtual ~PeakSpace() {};

            virtual bool read() {return read(m_base, m_offset);};
            virtual bool write() {return write(m_base, m_offset);};
            virtual bool read(enum eRegBase b, unsigned offset);
            virtual bool write(enum eRegBase b, unsigned offset);
            virtual void show();
        };

        // ----------
        class StreamConfig {
        public:
            struct ConfigBlock { // FIXME: somewhere in the DICE avdevice this is present too
                uint32_t nb_audio;
                uint32_t nb_midi;
                uint32_t names[DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS];
                uint32_t ac3_map;
            };
            void showConfigBlock(struct ConfigBlock &);
            diceNameVector getNamesForBlock(struct ConfigBlock &b);
        public:
            StreamConfig(EAP &, enum eRegBase, unsigned int offset);
            ~StreamConfig();

            bool read() {return read(m_base, m_offset);};
            bool write() {return write(m_base, m_offset);};
            bool read(enum eRegBase b, unsigned offset);
            bool write(enum eRegBase b, unsigned offset);

            void show();

        private:
            EAP &m_eap;
            enum eRegBase m_base;
            unsigned int m_offset;

            uint32_t m_nb_tx;
            uint32_t m_nb_rx;

            struct ConfigBlock *m_tx_configs;
            struct ConfigBlock *m_rx_configs;

            DECLARE_DEBUG_MODULE_REFERENCE;
        };

    public: // mixer control subclass
        class Mixer : public Control::MatrixMixer {
        public:
            Mixer(EAP &);
            ~Mixer();

            bool init();
            void show();

            void updateNameCache();
            /**
             * load the coefficients from the device into the local cache
             * @return 
             */
            bool loadCoefficients();
            /**
             * Stores the coefficients from the cache to the device
             * @return 
             */
            bool storeCoefficients();

            virtual std::string getRowName( const int );
            virtual std::string getColName( const int );
            virtual int canWrite( const int, const int );
            virtual double setValue( const int, const int, const double );
            virtual double getValue( const int, const int );
            virtual int getRowCount( );
            virtual int getColCount( );
        
            // full map updates are unsupported
            virtual bool getCoefficientMap(int &);
            virtual bool storeCoefficientMap(int &);

        private:
            EAP &         m_eap;
            fb_quadlet_t *m_coeff;

            std::map<int, RouterConfig::Route> m_input_route_map;
            std::map<int, RouterConfig::RouteVector> m_output_route_map;

            DECLARE_DEBUG_MODULE_REFERENCE;
        };

        // ----------
        class Router : public Control::CrossbarRouter {
        private:
            struct Source {
                std::string name;
                enum eRouteSource src;
                int srcChannel;
            };
            typedef std::vector<Source> SourceVector;
            typedef std::vector<Source>::iterator SourceVectorIterator;

            struct Destination {
                std::string name;
                enum eRouteDestination dst;
                int dstChannel;
            };
            typedef std::vector<Destination> DestinationVector;
            typedef std::vector<Destination>::iterator DestinationVectorIterator;

        public:
            Router(EAP &);
            ~Router();

            void show();

            // to be subclassed by the implementing
            // devices
            virtual void setupSources();
            virtual void setupDestinations();

            void setupDestinationsAddDestination(const char *name, enum eRouteDestination dstid,
                                                 unsigned int base, unsigned int cnt);
            void setupSourcesAddSource(const char *name, enum eRouteSource srcid,
                                       unsigned int base, unsigned int cnt);

            int getDestinationIndex(enum eRouteDestination dstid, int channel);
            int getSourceIndex(enum eRouteSource srcid, int channel);

            // per-coefficient access
            virtual std::string getSourceName(const int);
            virtual std::string getDestinationName(const int);
            virtual int getSourceIndex(std::string);
            virtual int getDestinationIndex(std::string);
            virtual NameVector getSourceNames();
            virtual NameVector getDestinationNames();

            virtual Control::CrossbarRouter::Groups getSources();
            virtual Control::CrossbarRouter::Groups getDestinations();

            virtual IntVector getDestinationsForSource(const int);
            virtual int getSourceForDestination(const int);

            virtual bool canConnect( const int source, const int dest);
            virtual bool setConnectionState( const int source, const int dest, const bool enable);
            virtual bool getConnectionState( const int source, const int dest );

            virtual bool canConnect(std::string, std::string);
            virtual bool setConnectionState(std::string, std::string, const bool enable);
            virtual bool getConnectionState(std::string, std::string);

            virtual bool clearAllConnections();

            virtual int getNbSources();
            virtual int getNbDestinations();

            // functions to access the entire routing map at once
            // idea is that the row/col nodes that are 1 get a routing entry
            virtual bool getConnectionMap(int *);
            virtual bool setConnectionMap(int *);

            // peak metering support
            virtual bool hasPeakMetering();
            virtual bool getPeakValues(double &) {return false;};
            virtual double getPeakValue(const int source, const int dest);
            virtual Control::CrossbarRouter::PeakValues getPeakValues();

        private:
            EAP &m_eap;
            // these contain the sources and destinations available for this
            // router
            SourceVector      m_sources;
            DestinationVector m_destinations;

            PeakSpace &m_peak;

            DECLARE_DEBUG_MODULE_REFERENCE;
        };

    public:
        EAP(Device &);
        virtual ~EAP();

        static bool supportsEAP(Device &);
        bool init();

        void show();
        void showApplication();
        enum eWaitReturn operationBusy();
        enum eWaitReturn waitForOperationEnd(int max_wait_time_ms = 100);

        bool updateConfigurationCache();
        RouterConfig * getActiveRouterConfig();
        StreamConfig * getActiveStreamConfig();

        bool updateRouterConfig(RouterConfig&, bool low, bool mid, bool high);
        bool updateCurrentRouterConfig(RouterConfig&);
        bool updateStreamConfig(StreamConfig&, bool low, bool mid, bool high);
        bool updateStreamConfig(RouterConfig&, StreamConfig&, bool low, bool mid, bool high);

        bool loadFlashConfig();
        bool storeFlashConfig();

    private:
        bool loadRouterConfig(bool low, bool mid, bool high);
        bool loadStreamConfig(bool low, bool mid, bool high);
        bool loadRouterAndStreamConfig(bool low, bool mid, bool high);
    private:
        bool     m_router_exposed;
        bool     m_router_readonly;
        bool     m_router_flashstored;
        uint16_t m_router_nb_entries;

        bool     m_mixer_exposed;
        bool     m_mixer_readonly;
        bool     m_mixer_flashstored;
        uint8_t  m_mixer_tx_id;
        uint8_t  m_mixer_rx_id;
        uint8_t  m_mixer_nb_tx;
        uint8_t  m_mixer_nb_rx;

        bool     m_general_support_dynstream;
        bool     m_general_support_flash;
        bool     m_general_peak_enabled;
        uint8_t  m_general_max_tx;
        uint8_t  m_general_max_rx;
        bool     m_general_stream_cfg_stored;
        uint16_t m_general_chip;

        bool commandHelper(fb_quadlet_t cmd);

    public:
        bool readReg(enum eRegBase, unsigned offset, quadlet_t *);
        bool writeReg(enum eRegBase, unsigned offset, quadlet_t);
        bool readRegBlock(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        bool writeRegBlock(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        bool readRegBlockSwapped(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        bool writeRegBlockSwapped(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
        fb_nodeaddr_t offsetGen(enum eRegBase, unsigned, size_t);

    private:
        DECLARE_DEBUG_MODULE_REFERENCE;

    private:
        Device & m_device;
        Mixer*   m_mixer;
        Router*  m_router;
        RouterConfig m_current_cfg_routing_low;
        RouterConfig m_current_cfg_routing_mid;
        RouterConfig m_current_cfg_routing_high;
        StreamConfig m_current_cfg_stream_low;
        StreamConfig m_current_cfg_stream_mid;
        StreamConfig m_current_cfg_stream_high;
    public:
        Mixer*  getMixer() {return m_mixer;};
        Router* getRouter() {return m_router;};

    private:

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
    };

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

    diceNameVector splitNameString(std::string in);
    diceNameVector getTxNameString(unsigned int i);
    diceNameVector getRxNameString(unsigned int i);
    diceNameVector getClockSourceNameString();
    std::string getDeviceNickName();
    bool setDeviceNickName(std::string name);

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



};

}
#endif
