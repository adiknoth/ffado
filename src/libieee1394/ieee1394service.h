/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef FFADO_IEEE1394SERVICE_H
#define FFADO_IEEE1394SERVICE_H

#include "fbtypes.h"
#include "libutil/Functors.h"
#include "libutil/Mutex.h"

#include "debugmodule/debugmodule.h"

#include "IEC61883.h"

#include <libraw1394/raw1394.h>
#include <pthread.h>

#include <vector>
#include <string>

#define MAX_FCP_BLOCK_SIZE_BYTES (512)
#define MAX_FCP_BLOCK_SIZE_QUADS (MAX_FCP_BLOCK_SIZE_BYTES / 4)

class IsoHandlerManager;
class CycleTimerHelper;

namespace Util {
    class Watchdog;
    class Configuration;
}

class Ieee1394Service : public IEC61883 {
public:
    Ieee1394Service();
    Ieee1394Service(bool rt, int prio);
    ~Ieee1394Service();

    bool initialize( int port );
    bool setThreadParameters(bool rt, int priority);
    Util::Watchdog *getWatchdog() {return m_pWatchdog;};

   /**
    * @brief get number of ports (firewire adapters) in this machine
    *
    * @return the number of ports
    */
    static int detectNbPorts();

   /**
    * @brief get port (adapter) id
    *
    * @return get port (adapter) id
    */
    int getPort()
        { return m_port; }

   /**
    * @brief get port (adapter) name
    *
    * @return get port (adapter) name
    */
    std::string getPortName()
        { return m_portName; };

   /**
    * @brief get number of nodes on the bus
    *
    * Since the root node always has
    * the highest node ID, this number can be used to determine that ID (it's
    * LOCAL_BUS|(count-1)).
    *
    * @return the number of nodes on the bus to which the port is connected.
    * This value can change with every bus reset.
    */
    int getNodeCount();

   /**
    * @brief get the node id of the local node
    *
    * @note does not include the bus part (0xFFC0)
    *
    * @return the node id of the local node
    * This value can change with every bus reset.
    */
    nodeid_t getLocalNodeId();

    /**
     * @brief get the most recent cycle timer value (in ticks)
     *
     * @note Uses the most appropriate method for getting the cycle timer
     *       which is not necessarily a direct read (could be DLL)
     */
    uint32_t getCycleTimerTicks();

    /**
     * @brief get the most recent cycle timer value (in CTR format)
     *
     * @note Uses the most appropriate method for getting the cycle timer
     *       which is not necessarily a direct read (could be DLL)
     */
    uint32_t getCycleTimer();

    /**
     * @brief get the cycle timer value for a specific time instant (in ticks)
     *
     * @note Uses the most appropriate method for getting the cycle timer
     *       which is not necessarily a direct read (could be DLL)
     */
    uint32_t getCycleTimerTicks(uint64_t t);

    /**
     * @brief get the cycle timer value for a specific time instant (in CTR format)
     *
     * @note Uses the most appropriate method for getting the cycle timer
     *       which is not necessarily a direct read (could be DLL)
     */
    uint32_t getCycleTimer(uint64_t t);

    /**
     * @brief get the system time for a specific cycle timer value (in ticks)
     * @note thread safe
     */
    uint64_t getSystemTimeForCycleTimerTicks(uint32_t ticks);

    /**
     * @brief get the system time for a specific cycle timer value (in CTR format)
     * @note thread safe
     */
    uint64_t getSystemTimeForCycleTimer(uint32_t ctr);

    /**
     * @brief read the cycle timer value from the controller (in CTR format)
     *
     * @note Uses a direct method to read the value from the controller
     * @return true if successful
     */
    bool readCycleTimerReg(uint32_t *cycle_timer, uint64_t *local_time);

    /**
     * @brief provide the current system time
     * @return 
     */
    uint64_t getCurrentTimeAsUsecs();

    /**
     * @brief send async read request to a node and wait for response.
     *
     * This does the complete transaction and will return when it's finished.
     *
     * @param node target node (\todo needs 0xffc0 stuff)
     * @param addr address to read from
     * @param length amount of data to read in quadlets
     * @param buffer pointer to buffer where data will be saved
     *
     * @return true on success or false on failure (sets errno)
     */
    bool read( fb_nodeid_t nodeId,
           fb_nodeaddr_t addr,
           size_t length,
           fb_quadlet_t* buffer );

    bool read_quadlet( fb_nodeid_t nodeId,
                       fb_nodeaddr_t addr,
                       fb_quadlet_t* buffer );

    bool read_octlet( fb_nodeid_t nodeId,
                      fb_nodeaddr_t addr,
                      fb_octlet_t* buffer );

    /**
    * @brief send async write request to a node and wait for response.
    *
    * This does the complete transaction and will return when it's finished.
    *
    * @param node target node (\XXX needs 0xffc0 stuff)
    * @param addr address to write to
    * @param length amount of data to write in quadlets
    * @param data pointer to data to be sent
    *
    * @return true on success or false on failure (sets errno)
    */
    bool write( fb_nodeid_t nodeId,
        fb_nodeaddr_t addr,
        size_t length,
        fb_quadlet_t* data );

    bool write_quadlet( fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        fb_quadlet_t data );

    bool write_octlet(  fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        fb_octlet_t data );

    /**
     * @brief send 64-bit compare-swap lock request and wait for response.
     *
     * swaps the content of \ref addr with \ref swap_value , but only if
     * the content of \ref addr equals \ref compare_with
     *
     * @note takes care of endiannes
     *
     * @param nodeId target node ID
     * @param addr address within target node address space
     * @param compare_with value to compare \ref addr with
     * @param swap_value new value to put in \ref addr
     * @param result the value (originally) in \ref addr
     *
     * @return true if succesful, false otherwise
     */
    bool lockCompareSwap64( fb_nodeid_t nodeId,
                            fb_nodeaddr_t addr,
                            fb_octlet_t  compare_value,
                            fb_octlet_t  swap_value,
                            fb_octlet_t* result );

    /**
     * initiate AV/C transaction
     * @param nodeId 
     * @param buf 
     * @param len 
     * @param resp_len 
     * @return 
     */
    fb_quadlet_t* transactionBlock( fb_nodeid_t nodeId,
                                    fb_quadlet_t* buf,
                                    int len,
                                    unsigned int* resp_len );

    /**
     * close AV/C transaction.
     * @param nodeId 
     * @param buf 
     * @param len 
     * @param resp_len 
     * @return 
     */
    bool transactionBlockClose();

    int getVerboseLevel();

    bool addBusResetHandler( Util::Functor* functor );
    bool remBusResetHandler( Util::Functor* functor );

    void doBusReset();
    bool waitForBusResetStormToEnd( int nb_tries, int sleep_time_ms );

    /**
     * @brief get the current generation
     *
     * @return the current generation
     **/
    unsigned int getGeneration() {
        Util::MutexLockHelper lock(*m_handle_lock);
        return raw1394_get_generation( m_handle );
    }

    /**
     * @brief sets the SPLIT_TIMEOUT_HI and SPLIT_TIMEOUT_LO CSR registers
     *
     * sets the SPLIT_TIMEOUT_HI and SPLIT_TIMEOUT_LO CSR registers on node
     * nodeId such that the timeout is equal to timeout
     *
     * @param nodeId node to set CSR registers on
     * @param timeout timeout in usecs
     * @return true if successful
     */
    bool setSplitTimeoutUsecs(fb_nodeid_t nodeId, unsigned int timeout);

    /**
     * @brief gets the SPLIT_TIMEOUT_X timeout value
     *
     * gets the SPLIT_TIMEOUT_HI and SPLIT_TIMEOUT_LO CSR registers on node
     * nodeId and recombine them into one usec value
     *
     * @param nodeId node to get CSR registers from
     * @return timeout in usecs if successful, 0 else
     */
    int getSplitTimeoutUsecs(fb_nodeid_t nodeId);

    /**
     * @brief use the provided configuration for this service
     *
     * only update the config once, before init. not thread safe,
     * and no effect when things are already running.
     *
     * @param c configuration to use
     * @return bool if this config is ok.
     */
    bool useConfiguration(Util::Configuration *c);

    Util::Configuration *getConfiguration() {return m_configuration;};

// ISO channel stuff
public:
    signed int getAvailableBandwidth();
    signed int allocateIsoChannelGeneric(unsigned int bandwidth);
    signed int allocateIsoChannelCMP(nodeid_t xmit_node, int xmit_plug,
                                     nodeid_t recv_node, int recv_plug);
    bool freeIsoChannel(signed int channel);

    IsoHandlerManager& getIsoHandlerManager() {return *m_pIsoManager;};
private:
    enum EAllocType {
        AllocFree = 0, // not allocated (by us)
        AllocGeneric = 1, // allocated with generic functions
        AllocCMP=2 // allocated with CMP
    };

    struct ChannelInfo {
        int channel;
        int bandwidth;
        enum EAllocType alloctype;
        nodeid_t xmit_node;
        int xmit_plug;
        nodeid_t recv_node;
        int recv_plug;
    };

    // the info for the channels we manage
    struct ChannelInfo m_channels[64];

    bool unregisterIsoChannel(unsigned int c);
    bool registerIsoChannel(unsigned int c, struct ChannelInfo cinfo);

public:
// FIXME: should be private, but is used to do the PCR control in GenericAVC::AvDevice
    raw1394handle_t getHandle() {return m_handle;};

protected:
    Util::Configuration     *m_configuration;

private:
    bool configurationUpdated();

    bool startRHThread();
    void stopRHThread();
    static void* rHThread( void* arg );

    void printBuffer( unsigned int level, size_t length, fb_quadlet_t* buffer ) const;
    void printBufferBytes( unsigned int level, size_t length, byte_t* buffer ) const;

    static int resetHandlerLowLevel( raw1394handle_t handle,
                                     unsigned int generation );
    bool resetHandler( unsigned int generation );

    raw1394handle_t m_handle;
    Util::Mutex*    m_handle_lock;
    raw1394handle_t m_resetHandle;
    raw1394handle_t m_util_handle; // a handle for operations from the rt thread
    int             m_port;
    std::string     m_portName;

    pthread_t       m_thread;
    Util::Mutex*    m_RHThread_lock;
    bool            m_threadRunning;

    bool            m_realtime;
    int             m_base_priority;

    IsoHandlerManager*      m_pIsoManager;
    CycleTimerHelper*       m_pCTRHelper;
    bool                    m_have_new_ctr_read;

    // the RT watchdog
    Util::Watchdog*     m_pWatchdog;

    typedef std::vector< Util::Functor* > reset_handler_vec_t;
    reset_handler_vec_t m_busResetHandlers;

    // unprotected variants
    bool writeNoLock( fb_nodeid_t nodeId,
        fb_nodeaddr_t addr,
        size_t length,
        fb_quadlet_t* data );
    bool readNoLock( fb_nodeid_t nodeId,
           fb_nodeaddr_t addr,
           size_t length,
           fb_quadlet_t* buffer );

    // FCP transaction support
    static int _avc_fcp_handler(raw1394handle_t handle, nodeid_t nodeid, 
                                int response, size_t length,
                                unsigned char *data);
    int handleFcpResponse(nodeid_t nodeid,
                          int response, size_t length,
                          unsigned char *data);

    enum eFcpStatus {
        eFS_Empty,
        eFS_Waiting,
        eFS_Responded,
        eFS_Error,
    };

    struct sFcpBlock {
        enum eFcpStatus status;
        nodeid_t target_nodeid;
        unsigned int request_length;
        quadlet_t request[MAX_FCP_BLOCK_SIZE_QUADS];
        unsigned int response_length;
        quadlet_t response[MAX_FCP_BLOCK_SIZE_QUADS];
    };
    struct sFcpBlock m_fcp_block;

    bool doFcpTransaction();
    bool doFcpTransactionTry();

public:
    void setVerboseLevel(int l);
    void show();
private:
    DECLARE_DEBUG_MODULE;
};

#endif // FFADO_IEEE1394SERVICE_H
