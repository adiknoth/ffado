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
  @brief Sources for audio hitting the router
  */
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
/**
  @brief Destinations for audio exiting the router
  */
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

/**
  @brief represents the EAP interface available on some devices

  When available, the mixer and router are visible. This class is also the base for custom
  implementations of EAP extensions.
  */
class EAP : public Control::Container
{
public:
    /**
      @brief Command status
      */
    enum eWaitReturn {
        eWR_Error,
        eWR_Timeout,
        eWR_Busy,
        eWR_Done,
    };
    /**
      @brief Constants for the EAP spaces

      @see offsetGen for the calculation of the real offsets.
      */
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

private:

    /**
      @brief Description of the routing in the hardware
      */
    class RouterConfig {
    private:
        friend class Dice::EAP;
        RouterConfig(EAP &);
        RouterConfig(EAP &, enum eRegBase, unsigned int offset);
        ~RouterConfig();

    public:

        bool read() {return read(m_base, m_offset);};
        bool write() {return write(m_base, m_offset);};
        bool read(enum eRegBase b, unsigned offset);
        bool write(enum eRegBase b, unsigned offset);
        void show();

        /**
          @brief Set up a route between src and dest

          If a route with that destination exists, it will be replaced. If no route to that
          destination exists, a new route will be established.
          */
        bool setupRoute(unsigned char src, unsigned char dest);

        /**
          @brief Mute a route for dest

          All EAP devices will support muting.
          Not all of them support (at least full support) removing a dest (see comment below)
          */
        bool muteRoute(unsigned char dest);

        /**
          @brief Remove a route

          @todo is this really necessary?
          */
        bool removeRoute(unsigned char src, unsigned char dest);
        /**
          @brief Remove the destinations route
          */
        bool removeRoute(unsigned char dest);

        /**
          @brief Return the source for the given destination

          Returns -1 if the destination is not connected.
          */
        unsigned char getSourceForDestination(unsigned char dest);
        /**
          @brief Return a list of destinations for a given source

          Returns an empty list if no destination is connected to this source.
          */
        std::vector<unsigned char> getDestinationsForSource(unsigned char src);

        unsigned int getNbRoutes() {return m_routes2.size();};

    private:
        EAP &m_eap;
        enum eRegBase m_base;
        unsigned int m_offset;

        /**
          @brief map for the routes

          The key is the destination as each destination can only have audio from one source.
          Sources can be routed to several destinations though.
          */
//        typedef std::map<unsigned char,unsigned char> RouteVectorV2;
        typedef std::vector< std::pair<unsigned char,unsigned char> > RouteVectorV2;
        RouteVectorV2 m_routes2;
    private:
        DECLARE_DEBUG_MODULE_REFERENCE;
    };

    /**
      @brief Description of the peak space in hardware
      */
    class PeakSpace {
    private:
        friend class Dice::EAP;
        PeakSpace(EAP &p) : m_eap(p), m_base(eRT_Peak), m_offset(0), m_debugModule(p.m_debugModule) {};
        ~PeakSpace() {};

    public:
        bool read() {return read(m_base, m_offset);};
        bool read(enum eRegBase b, unsigned offset);
        void show();

        std::map<unsigned char, int> getPeaks();
        int getPeak(unsigned char dest);

    private:
        EAP &m_eap;
        enum eRegBase m_base;
        unsigned int m_offset;

        /**
          @brief maps peaks to destinations
          */
        std::map<unsigned char, int> m_peaks;

        DECLARE_DEBUG_MODULE_REFERENCE;
    };

    /**
      @brief Description of the streams in the hardware
      */
    class StreamConfig {
    private:
        friend class Dice::EAP;
        StreamConfig(EAP &, enum eRegBase, unsigned int offset);
        ~StreamConfig();

    public:
        struct ConfigBlock { // FIXME: somewhere in the DICE avdevice this is present too
            uint32_t nb_audio;
            uint32_t nb_midi;
            uint32_t names[DICE_EAP_CHANNEL_CONFIG_NAMESTR_LEN_QUADS];
            uint32_t ac3_map;
        };
        void showConfigBlock(struct ConfigBlock &);
        stringlist getNamesForBlock(struct ConfigBlock &b);
        stringlist getTxNamesString(unsigned int);
        stringlist getRxNamesString(unsigned int);

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

public:

    /**
      @brief The matrixmixer exposed
      */
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

        //
        bool hasNames() const { return false; }
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

        //std::map<int, RouterConfig::Route> m_input_route_map;
        //std::map<int, RouterConfig::RouteVector> m_output_route_map;

        DECLARE_DEBUG_MODULE_REFERENCE;
    };

    /**
      @brief The router exposed
      */
    class Router : public Control::CrossbarRouter {
    public:
        Router(EAP &);
        ~Router();

        void update();
        void show();

        void addDestination(const std::string& name, enum eRouteDestination dstid,
                            unsigned int base, unsigned int cnt, unsigned int offset=0);
        void addSource(const std::string& name, enum eRouteSource srcid,
                       unsigned int base, unsigned int cnt, unsigned int offset=0);

        // per-coefficient access
        virtual std::string getSourceName(const int);
        virtual std::string getDestinationName(const int);
        virtual int getSourceIndex(std::string);
        virtual int getDestinationIndex(std::string);
        virtual stringlist getSourceNames();
        virtual stringlist getDestinationNames();

        std::string getSourceForDestination(const std::string& dstname);
        stringlist getDestinationsForSource(const std::string& srcname);

        virtual bool canConnect(const int srcidx, const int dstidx);
        virtual bool setConnectionState(const int srcidx, const int dstidx, const bool enable);
        virtual bool getConnectionState(const int srcidx, const int dstidx);

        virtual bool canConnect(const std::string& srcname, const std::string& dstname);
        virtual bool setConnectionState(const std::string& srcname, const std::string& dstname, const bool enable);
        virtual bool getConnectionState(const std::string& srcname, const std::string& dstname);

        virtual bool clearAllConnections();

        // peak metering support
        virtual bool hasPeakMetering();
        virtual double getPeakValue(const std::string& dest);
        virtual std::map<std::string, double> getPeakValues();

    private:
        EAP &m_eap;
        /**
          @{
          @brief Name-Index pairs for the sources and destinations

          The index is 'artificial' and is the block/channel combination used in the dice.
          */
        std::map<std::string, int> m_sources;
        std::map<std::string, int> m_destinations;
        // @}

        PeakSpace &m_peak;

        DECLARE_DEBUG_MODULE_REFERENCE;
    };

public:
    /** constructor */
    EAP(Device &);
    /** destructor */
    virtual ~EAP();

    /**
      @brief Does this device support the EAP?

      When subclassing EAP, return true only on devices that actually have an EAP.

      @todo Shouldn't this be inside Dice::Device?
      */
    static bool supportsEAP(Device &);

    /**
      @brief Initialize the EAP
      */
    bool init();

    /// update EAP
    void update();

    /// Show information about the EAP
    void show();
    /// Dump the first parts of the application space
    void showApplication();
    /// Show the full router content
    void showFullRouter();
    /// Show the full peak space content
    void showFullPeakSpace();

    /// Restore from flash
    bool loadFlashConfig();
    /// Store to flash
    bool storeFlashConfig();

    /// Is the current operation still busy?
    enum eWaitReturn operationBusy();
    /// Block until the current operation is done
    enum eWaitReturn waitForOperationEnd(int max_wait_time_ms = 100);

    /// Update all configurations from the device
    bool updateConfigurationCache();

    /**
      @{
      @brief Read and write registers on the device
      */
    bool readReg(enum eRegBase, unsigned offset, quadlet_t *);
    bool writeReg(enum eRegBase, unsigned offset, quadlet_t);
    bool readRegBlock(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
    bool writeRegBlock(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
    bool readRegBlockSwapped(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
    bool writeRegBlockSwapped(enum eRegBase, unsigned, fb_quadlet_t *, size_t);
    //@}

    /** @brief Get access to the mixer */
    Mixer*  getMixer() {return m_mixer;};
    /** @brief Get access to the router */
    Router* getRouter() {return m_router;};

    /** @brief Get capture and playback names */
    stringlist getCptrNameString(unsigned int);
    stringlist getPbckNameString(unsigned int);

protected:
    /**
      @brief Setup all the available sources

      This adds the needed entries for sources to the router. The default implementation decides on
      the chip which sources to add, subclasses should only add the sources actually usable for the
      device.

      To ease custom device support, this function is not in EAP::Router but here.
      */
    void setupSources();
    virtual void setupSources_low();
    virtual void setupSources_mid();
    virtual void setupSources_high();
    virtual unsigned int getSMuteId();
    /**
      @brief Setup all the available destinations

      This adds the needed entries for destinations to the router. The default implementation
      decides on the chip which destinations to add, subclasses should only add the destinations
      actually usable for the device.

      To ease custom device support, this function is not in EAP::Router but here.
      */
    void setupDestinations();
    virtual void setupDestinations_low();
    virtual void setupDestinations_mid();
    virtual void setupDestinations_high();

    /**
      @brief Actually add the source
      */
    void addSource(const std::string name, unsigned int base, unsigned int count,
                   enum eRouteSource srcid, unsigned int offset=0);
    /**
      @brief Actually add the destination
      */
    void addDestination(const std::string name, unsigned int base, unsigned int count,
                        enum eRouteDestination destid, unsigned int offset=0);

    uint16_t getMaxNbRouterEntries() {return m_router_nb_entries;};

private:
    /// Return the router configuration for the current rate
    RouterConfig * getActiveRouterConfig();
    /// Return the stream configuration for the current rate
    StreamConfig * getActiveStreamConfig();

    /// Write a new router configuration to the device
    bool updateRouterConfig(RouterConfig&, bool low, bool mid, bool high);
    /// Write a new router configuration to the device
    bool updateCurrentRouterConfig(RouterConfig&);
    /// Write a new stream configuration to the device
    bool updateStreamConfig(StreamConfig&, bool low, bool mid, bool high);
    /// Write a new stream configuration to the device
    bool updateStreamConfig(RouterConfig&, StreamConfig&, bool low, bool mid, bool high);

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

    /// Calculate the real offset for the different spaces
    fb_nodeaddr_t offsetGen(enum eRegBase, unsigned, size_t);

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

protected:
    DECLARE_DEBUG_MODULE;
};

};

#endif // __DICE_EAP_H
