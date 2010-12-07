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

#include "libffado/ffado.h"

#include "ffadodevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"

#include "libieee1394/ieee1394service.h"

#include "libcontrol/Element.h"
#include "libcontrol/MatrixMixer.h"
#include "libcontrol/CrossbarRouter.h"

#include <string>
#include <vector>

class ConfigRom;
class Ieee1394Service;

// from the streaming library
struct am824_settings;
struct stream_settings;

namespace Util {
    class Configuration;
}

namespace Dice {

class EAP;

typedef std::vector< std::string > diceNameVector;
typedef std::vector< std::string >::iterator diceNameVectorIterator;


class Device : public FFADODevice {
// private:
    friend class EAP;

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
    virtual struct stream_settings *getSettingsForStream(unsigned int i);

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
    size_t m_nb_stream_settings;
    struct stream_settings *m_stream_settings;
    struct am824_settings *m_amdtp_settings;

private: // streaming & port helpers
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
// vim: et
#endif
