/*
 * Copyright (C) 2005-2009 by Pieter Palmers
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
#ifndef __DICE_EAP_H
#define __DICE_EAP_H

#include "dice_avdevice.h"

#define DICE_EAP_BASE                  0x0000000000200000ULL
#define DICE_EAP_MAX_SIZE              0x0000000000F00000ULL

#define DICE_EAP_CAPABILITY_SPACE_OFF      0x0000
#define DICE_EAP_CAPABILITY_SPACE_SZ       0x0004
#define DICE_EAP_CMD_SPACE_OFF             0x0008
#define DICE_EAP_CMD_SPACE_SZ              0x000C
#define DICE_EAP_MIXER_SPACE_OFF           0x0010
#define DICE_EAP_MIXER_SPACE_SZ            0x0014
#define DICE_EAP_PEAK_SPACE_OFF            0x0018
#define DICE_EAP_PEAK_SPACE_SZ             0x001C
#define DICE_EAP_NEW_ROUTING_SPACE_OFF     0x0020
#define DICE_EAP_NEW_ROUTING_SPACE_SZ      0x0024
#define DICE_EAP_NEW_STREAM_CFG_SPACE_OFF  0x0028
#define DICE_EAP_NEW_STREAM_CFG_SPACE_SZ   0x002C
#define DICE_EAP_CURR_CFG_SPACE_OFF        0x0030
#define DICE_EAP_CURR_CFG_SPACE_SZ         0x0034
#define DICE_EAP_STAND_ALONE_CFG_SPACE_OFF 0x0038
#define DICE_EAP_STAND_ALONE_CFG_SPACE_SZ  0x003C
#define DICE_EAP_APP_SPACE_OFF             0x0040
#define DICE_EAP_APP_SPACE_SZ              0x0044
#define DICE_EAP_ZERO_MARKER_1             0x0048

// CAPABILITY registers
#define DICE_EAP_CAPABILITY_ROUTER         0x0000
#define DICE_EAP_CAPABILITY_MIXER          0x0004
#define DICE_EAP_CAPABILITY_GENERAL        0x0008
#define DICE_EAP_CAPABILITY_RESERVED       0x000C

// CAPABILITY bit definitions
#define DICE_EAP_CAP_ROUTER_EXPOSED         0
#define DICE_EAP_CAP_ROUTER_READONLY        1
#define DICE_EAP_CAP_ROUTER_FLASHSTORED     2
#define DICE_EAP_CAP_ROUTER_MAXROUTES      16

#define DICE_EAP_CAP_MIXER_EXPOSED          0
#define DICE_EAP_CAP_MIXER_READONLY         1
#define DICE_EAP_CAP_MIXER_FLASHSTORED      2
#define DICE_EAP_CAP_MIXER_IN_DEV           4
#define DICE_EAP_CAP_MIXER_OUT_DEV          8
#define DICE_EAP_CAP_MIXER_INPUTS          16
#define DICE_EAP_CAP_MIXER_OUTPUTS         24

#define DICE_EAP_CAP_GENERAL_STRM_CFG_EN    0
#define DICE_EAP_CAP_GENERAL_FLASH_EN       1
#define DICE_EAP_CAP_GENERAL_PEAK_EN        2
#define DICE_EAP_CAP_GENERAL_MAX_TX_STREAM  4
#define DICE_EAP_CAP_GENERAL_MAX_RX_STREAM  8
#define DICE_EAP_CAP_GENERAL_STRM_CFG_FLS  12
#define DICE_EAP_CAP_GENERAL_CHIP          16

#define DICE_EAP_CAP_GENERAL_CHIP_DICEII    0
#define DICE_EAP_CAP_GENERAL_CHIP_DICEMINI  1
#define DICE_EAP_CAP_GENERAL_CHIP_DICEJR    2

// COMMAND registers
#define DICE_EAP_COMMAND_OPCODE         0x0000
#define DICE_EAP_COMMAND_RETVAL         0x0004

// opcodes
#define DICE_EAP_CMD_OPCODE_NO_OP            0x0000
#define DICE_EAP_CMD_OPCODE_LD_ROUTER        0x0001
#define DICE_EAP_CMD_OPCODE_LD_STRM_CFG      0x0002
#define DICE_EAP_CMD_OPCODE_LD_RTR_STRM_CFG  0x0003
#define DICE_EAP_CMD_OPCODE_LD_FLASH_CFG     0x0004
#define DICE_EAP_CMD_OPCODE_ST_FLASH_CFG     0x0005

#define DICE_EAP_CMD_OPCODE_FLAG_LD_LOW      (1U<<16)
#define DICE_EAP_CMD_OPCODE_FLAG_LD_MID      (1U<<17)
#define DICE_EAP_CMD_OPCODE_FLAG_LD_HIGH     (1U<<18)
#define DICE_EAP_CMD_OPCODE_FLAG_LD_EXECUTE  (1U<<31)


// MIXER registers
// TODO

// PEAK registers
// TODO

// NEW ROUTER registers
// TODO

// NEW STREAM CFG registers
// TODO

// CURRENT CFG registers
#define DICE_EAP_CURRCFG_LOW_ROUTER         0x0000
#define DICE_EAP_CURRCFG_LOW_STREAM         0x1000
#define DICE_EAP_CURRCFG_MID_ROUTER         0x2000
#define DICE_EAP_CURRCFG_MID_STREAM         0x3000
#define DICE_EAP_CURRCFG_HIGH_ROUTER        0x4000
#define DICE_EAP_CURRCFG_HIGH_STREAM        0x5000

#define DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS  (64)
#define DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_BYTES  (4*DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS)

namespace Dice {

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

        virtual int getRowCount( );
        virtual int getColCount( );

        virtual int canWrite( const int, const int );
        virtual double setValue( const int, const int, const double );
        virtual double getValue( const int, const int );

        bool hasNames() const { return true; }
        std::string getRowName( const int );
        std::string getColName( const int );

        // TODO: implement connections.
        bool canConnect() const { return false; }

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

protected:
    DECLARE_DEBUG_MODULE; //_REFERENCE;

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

};

#endif // __DICE_EAP_H
