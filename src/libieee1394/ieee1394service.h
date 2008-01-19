/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include "threads.h"

#include "debugmodule/debugmodule.h"

#include "IEC61883.h"

#include <libraw1394/raw1394.h>
#include <pthread.h>

#include <vector>
#include <string>

class ARMHandler;
class IsoHandlerManager;
class CycleTimerHelper;

namespace Util {
    class TimeSource;
}

class Ieee1394Service : public IEC61883 {
public:
    Ieee1394Service();
    Ieee1394Service(bool rt, int prio);
    ~Ieee1394Service();

    bool initialize( int port );
    bool setThreadParameters(bool rt, int priority);
   /**
    * @brief get number of ports (firewire adapters) in this machine
    *
    * @return the number of ports
    */
    static unsigned int detectNbPorts();

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
    bool lockCompareSwap64(  fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        fb_octlet_t  compare_value,
                        fb_octlet_t  swap_value,
                        fb_octlet_t* result );

    fb_quadlet_t* transactionBlock( fb_nodeid_t nodeId,
                                    fb_quadlet_t* buf,
                                    int len,
                    unsigned int* resp_len );

    bool transactionBlockClose();

    raw1394handle_t getHandle() {return m_handle;};

    int getVerboseLevel();

    bool addBusResetHandler( Functor* functor );
    bool remBusResetHandler( Functor* functor );

    /**
     * @brief get the current generation
     *
     * @return the current generation
     **/
    unsigned int getGeneration() {
        return raw1394_get_generation( m_handle );
    }

    /**
     * @brief register an AddressRangeMapping Handler
     * @param h pointer to the handler to register
     *
     * @return true on success or false on failure
     **/

    bool registerARMHandler( ARMHandler *h );

    /**
     * @brief unregister ARM range
     * @param h pointer to the handler to unregister
     * @return true if successful, false otherwise
     */
    bool unregisterARMHandler( ARMHandler *h );

    nodeaddr_t findFreeARMBlock( nodeaddr_t start, size_t length, size_t step );

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

private:

    bool startRHThread();
    void stopRHThread();
    static void* rHThread( void* arg );

    void printBuffer( unsigned int level, size_t length, fb_quadlet_t* buffer ) const;
    void printBufferBytes( unsigned int level, size_t length, byte_t* buffer ) const;

    static int resetHandlerLowLevel( raw1394handle_t handle,
                    unsigned int generation );
    bool resetHandler( unsigned int generation );

    static int armHandlerLowLevel(raw1394handle_t handle, unsigned long arm_tag,
                     byte_t request_type, unsigned int requested_length,
                     void *data);
    bool armHandler(  unsigned long arm_tag,
                     byte_t request_type, unsigned int requested_length,
                     void *data);

    raw1394handle_t m_handle;
    raw1394handle_t m_resetHandle;
    raw1394handle_t m_util_handle; // a handle for operations from the rt thread
    int             m_port;
    std::string     m_portName;

    pthread_t       m_thread;
    pthread_mutex_t m_mutex;
    bool            m_threadRunning;

    bool            m_realtime;
    int             m_base_priority;

    IsoHandlerManager*      m_pIsoManager;
    CycleTimerHelper*       m_pCTRHelper;
    bool                    m_have_new_ctr_read;

    // the time source
    Util::TimeSource*   m_pTimeSource;

    typedef std::vector< Functor* > reset_handler_vec_t;
    reset_handler_vec_t m_busResetHandlers;

    // ARM stuff
    arm_tag_handler_t m_default_arm_handler;

    typedef std::vector< ARMHandler * > arm_handler_vec_t;
    arm_handler_vec_t m_armHandlers;

    fb_octlet_t byteSwap_octlet(fb_octlet_t value);

public:
    void setVerboseLevel(int l);
    void show();
private:
    DECLARE_DEBUG_MODULE;
};

#endif // FFADO_IEEE1394SERVICE_H
