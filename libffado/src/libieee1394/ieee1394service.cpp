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

#include "config.h"

#include "ieee1394service.h"
#include "cycletimer.h"
#include "IsoHandlerManager.h"
#include "CycleTimerHelper.h"

#include <unistd.h>
#include <libraw1394/csr.h>
#include <libiec61883/iec61883.h>

#include "libutil/SystemTimeSource.h"
#include "libutil/Watchdog.h"
#include "libutil/PosixMutex.h"
#include "libutil/PosixThread.h"
#include "libutil/Configuration.h"

#include <errno.h>
#include "libutil/ByteSwap.h"

#include <string.h>

#include <iostream>
#include <iomanip>

using namespace std;

IMPL_DEBUG_MODULE( Ieee1394Service, Ieee1394Service, DEBUG_LEVEL_NORMAL );

Ieee1394Service::Ieee1394Service()
    : m_configuration( NULL )
    , m_resetHelper( NULL )
    , m_armHelperNormal( NULL )
    , m_armHelperRealtime( NULL )
    , m_handle( 0 )
    , m_handle_lock( new Util::PosixMutex("SRVCHND") )
    , m_util_handle( 0 )
    , m_port( -1 )
    , m_realtime ( false )
    , m_base_priority ( 0 )
    , m_pIsoManager( new IsoHandlerManager( *this ) )
    , m_pCTRHelper ( new CycleTimerHelper( *this, IEEE1394SERVICE_CYCLETIMER_DLL_UPDATE_INTERVAL_USEC ) )
    , m_have_new_ctr_read ( false )
    , m_filterFCPResponse ( false )
    , m_pWatchdog ( new Util::Watchdog() )
{
    for (unsigned int i=0; i<64; i++) {
        m_channels[i].channel=-1;
        m_channels[i].bandwidth=-1;
        m_channels[i].alloctype=AllocFree;
        m_channels[i].xmit_node=0xFFFF;
        m_channels[i].xmit_plug=-1;
        m_channels[i].recv_node=0xFFFF;
        m_channels[i].recv_plug=-1;
    }
}

Ieee1394Service::Ieee1394Service(bool rt, int prio)
    : m_configuration( NULL )
    , m_resetHelper( NULL )
    , m_armHelperNormal( NULL )
    , m_armHelperRealtime( NULL )
    , m_handle( 0 )
    , m_handle_lock( new Util::PosixMutex("SRVCHND") )
    , m_util_handle( 0 )
    , m_port( -1 )
    , m_realtime ( rt )
    , m_base_priority ( prio )
    , m_pIsoManager( new IsoHandlerManager( *this, rt, prio ) )
    , m_pCTRHelper ( new CycleTimerHelper( *this, IEEE1394SERVICE_CYCLETIMER_DLL_UPDATE_INTERVAL_USEC,
                                           rt && IEEE1394SERVICE_CYCLETIMER_HELPER_RUN_REALTIME,
                                           IEEE1394SERVICE_CYCLETIMER_HELPER_PRIO ) )
    , m_have_new_ctr_read ( false )
    , m_filterFCPResponse ( false )
    , m_pWatchdog ( new Util::Watchdog() )
{
    for (unsigned int i=0; i<64; i++) {
        m_channels[i].channel=-1;
        m_channels[i].bandwidth=-1;
        m_channels[i].alloctype=AllocFree;
        m_channels[i].xmit_node=0xFFFF;
        m_channels[i].xmit_plug=-1;
        m_channels[i].recv_node=0xFFFF;
        m_channels[i].recv_plug=-1;
    }
}

Ieee1394Service::~Ieee1394Service()
{
    delete m_pIsoManager;
    delete m_pCTRHelper;

    m_resetHelper->Stop();
    m_armHelperNormal->Stop();
    m_armHelperRealtime->Stop();

    for ( arm_handler_vec_t::iterator it = m_armHandlers.begin();
          it != m_armHandlers.end();
          ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Unregistering ARM handler for 0x%016"PRIX64"\n", (*it)->getStart());
        if(m_armHelperNormal) {
            int err = raw1394_arm_unregister(m_armHelperNormal->get1394Handle(), (*it)->getStart());
            if (err) {
                debugError(" Failed to unregister ARM handler for 0x%016"PRIX64"\n", (*it)->getStart());
                debugError(" Error: %s\n", strerror(errno));
            }
        } else {
            debugWarning("ARM handler registered without valid ARM helper thread\n");
        }
    }

    delete m_pWatchdog;
    if ( m_handle ) {
        raw1394_destroy_handle( m_handle );
    }
    delete m_handle_lock;

    if(m_resetHelper) delete m_resetHelper;
    if(m_armHelperNormal) delete m_armHelperNormal;
    if(m_armHelperRealtime) delete m_armHelperRealtime;

    if ( m_util_handle ) {
        raw1394_destroy_handle( m_util_handle );
    }
}

bool
Ieee1394Service::useConfiguration(Util::Configuration *c)
{
    m_configuration = c;
    return configurationUpdated();
}

bool
Ieee1394Service::configurationUpdated()
{
    if(m_configuration) {
        
    }
    return true;
}

#define DEVICEFAILTEXT "Could not get libraw1394 handle.\n\
This usually means:\n\
 a) The device-node /dev/raw1394 doesn't exists because you don't have a\n\
    (recognized) firewire controller.\n \
 b) The modules needed aren't loaded. This is not in the scope of ffado but of\n\
    your distribution, so if you have a firewire controller that should be\n\
    supported and the modules aren't loaded, file a bug with your distributions\n\
    bug tracker.\n \
 c) You don't have permissions to access /dev/raw1394. 'ls -l /dev/raw1394'\n\
    shows the device-node with its permissions, make sure you belong to the\n\
    right group and the group is allowed to access the device.\n"

int
Ieee1394Service::detectNbPorts()
{
    raw1394handle_t tmp_handle = raw1394_new_handle();
    if ( tmp_handle == NULL ) {
        debugError(DEVICEFAILTEXT);
        return -1;
    }
    struct raw1394_portinfo pinf[IEEE1394SERVICE_MAX_FIREWIRE_PORTS];
    int nb_detected_ports = raw1394_get_port_info(tmp_handle, pinf, IEEE1394SERVICE_MAX_FIREWIRE_PORTS);
    raw1394_destroy_handle(tmp_handle);

    if (nb_detected_ports < 0) {
        debugError("Failed to detect number of ports\n");
        return -1;
    }
    return nb_detected_ports;
}

void
Ieee1394Service::doBusReset() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Issue bus reset on service %p (port %d).\n", this, getPort());
    raw1394_reset_bus(m_handle);
}

/**
 * This function waits until there are no bus resets generated in a sleep_time_ms interval
 * @param nb_tries number of tries to take
 * @param sleep_time_ms sleep between tries
 * @return true if the storm passed
 */
bool
Ieee1394Service::waitForBusResetStormToEnd( int nb_tries, int sleep_time_ms ) {
    unsigned int gen_current;
    do {
        gen_current = getGeneration();
        debugOutput(DEBUG_LEVEL_VERBOSE, "Waiting... (gen: %u)\n", gen_current);

        // wait for a while
        Util::SystemTimeSource::SleepUsecRelative( sleep_time_ms * 1000);
    } while (gen_current != getGeneration() && --nb_tries);

    debugOutput(DEBUG_LEVEL_VERBOSE, "Bus reset storm over at gen: %u\n", gen_current);

    if (!nb_tries) {
        debugError( "Bus reset storm did not stop on time...\n");
        return false;
    }
    return true;
}

bool
Ieee1394Service::initialize( int port )
{
    using namespace std;

    int nb_ports = detectNbPorts();
    if (port + 1 > nb_ports) {
        debugFatal("Requested port (%d) out of range (# ports: %d)\n", port, nb_ports);
    }
    m_port = port;

    if(!m_pWatchdog) {
        debugError("No valid RT watchdog found.\n");
        return false;
    }
    if(!m_pWatchdog->start()) {
        debugError("Could not start RT watchdog.\n");
        return false;
    }

    m_handle = raw1394_new_handle_on_port( port );
    if ( !m_handle ) {
        if ( !errno ) {
            debugFatal("libraw1394 not compatible\n");
        } else {
            debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s\n",
                strerror(errno) );
            debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
        }
        return false;
    }

    // helper threads for all sorts of ASYNC events
    // note: m_port has to be set!
    m_resetHelper = new HelperThread(*this, "BUSRST");
    if ( !m_resetHelper ) {
        debugFatal("Could not allocate busreset handler helper\n");
        return false;
    }
    m_armHelperNormal = new HelperThread(*this, "ARMSTD");
    if ( !m_armHelperNormal ) {
        debugFatal("Could not allocate standard ARM handler helper\n");
        return false;
    }
    m_armHelperRealtime = new HelperThread(*this, "ARMRT", m_realtime, m_base_priority);
    if ( !m_armHelperRealtime ) {
        debugFatal("Could not allocate realtime ARM handler helper\n");
        return false;
    }

    // start helper threads
    if(!m_resetHelper->Start()) {
        debugFatal("Could not start busreset helper thread\n");
        return false;
    }
    if(!m_armHelperNormal->Start()) {
        debugFatal("Could not start standard ARM helper thread\n");
        return false;
    }
    if(!m_armHelperRealtime->Start()) {
        debugFatal("Could not start realtime ARM helper thread\n");
        return false;
    }

    // attach the reset and ARM handlers
    // NOTE: the handlers have to be started first, or there is no 1394handle
    raw1394_set_bus_reset_handler( m_resetHelper->get1394Handle(),
                                   this->resetHandlerLowLevel );

    m_default_arm_handler = raw1394_set_arm_tag_handler( m_armHelperNormal->get1394Handle(),
                                   this->armHandlerLowLevel );

    // utility handle (used to read the CTR register)
    m_util_handle = raw1394_new_handle_on_port( port );
    if ( !m_util_handle ) {
        if ( !errno ) {
            debugFatal("libraw1394 not compatible\n");
        } else {
            debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s",
                strerror(errno) );
            debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
        }
        return false;
    }

    // test the cycle timer read function
    int err;
    uint32_t cycle_timer;
    uint64_t local_time;
    err = raw1394_read_cycle_timer(m_util_handle, &cycle_timer, &local_time);
    if(err) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "raw1394_read_cycle_timer failed.\n");
        debugOutput(DEBUG_LEVEL_VERBOSE, " Error descr: %s\n", strerror(err));
        debugWarning("==================================================================\n");
        debugWarning(" This system doesn't support the raw1394_read_cycle_timer call.   \n");
        debugWarning(" Fallback to indirect CTR read method.                            \n");
        debugWarning(" FFADO should work, but achieving low-latency might be a problem. \n");
        debugWarning(" Upgrade the kernel to version 2.6.21 or higher to solve this.    \n");
        debugWarning("==================================================================\n");
        m_have_new_ctr_read = false;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "This system supports the raw1394_read_cycle_timer call, using it.\n");
        m_have_new_ctr_read = true;
    }

    // obtain port name
    raw1394handle_t tmp_handle = raw1394_new_handle();
    if ( tmp_handle == NULL ) {
        debugError("Could not get temporary libraw1394 handle.\n");
        return false;
    }
    struct raw1394_portinfo pinf[IEEE1394SERVICE_MAX_FIREWIRE_PORTS];
    int nb_detected_ports = raw1394_get_port_info(tmp_handle, pinf, IEEE1394SERVICE_MAX_FIREWIRE_PORTS);
    raw1394_destroy_handle(tmp_handle);

    if (nb_detected_ports < 0) {
        debugError("Failed to detect number of ports\n");
        return false;
    }

    if(nb_detected_ports && port < IEEE1394SERVICE_MAX_FIREWIRE_PORTS) {
        m_portName = pinf[port].name;
    } else {
        m_portName = "Unknown";
    }
    if (m_portName == "") {
        m_portName = "Unknown";
    }

    // set userdata
    raw1394_set_userdata( m_handle, this );
    raw1394_set_userdata( m_util_handle, this );

    // increase the split-transaction timeout if required (e.g. for bebob's)
    int split_timeout = IEEE1394SERVICE_MIN_SPLIT_TIMEOUT_USECS;
    if(m_configuration) {
        m_configuration->getValueForSetting("ieee1394.min_split_timeout_usecs", split_timeout);
    }

    // set SPLIT_TIMEOUT to one second to cope with DM1x00 devices that
    // send responses regardless of the timeout
    int timeout = getSplitTimeoutUsecs(getLocalNodeId());
    debugOutput(DEBUG_LEVEL_VERBOSE, "Minimum SPLIT_TIMEOUT: %d. Current: %d\n", split_timeout, timeout);
    if (timeout < split_timeout) {
        if(!setSplitTimeoutUsecs(getLocalNodeId(), split_timeout+124)) {
            debugWarning("Could not set SPLIT_TIMEOUT to min requested (%d)\n", split_timeout);
        }
        timeout = getSplitTimeoutUsecs(getLocalNodeId());
        if (timeout < split_timeout) {
            debugWarning("Set SPLIT_TIMEOUT to min requested (%d) did not succeed\n", split_timeout);
        }
    }

    // init helpers
    if(!m_pCTRHelper) {
        debugFatal("No CycleTimerHelper available, bad!\n");
        return false;
    }
    m_pCTRHelper->setVerboseLevel(getDebugLevel());
    if(!m_pCTRHelper->Start()) {
        debugFatal("Could not start CycleTimerHelper\n");
        return false;
    }

    if(!m_pIsoManager) {
        debugFatal("No IsoHandlerManager available, bad!\n");
        return false;
    }
    m_pIsoManager->setVerboseLevel(getDebugLevel());

    if(!m_pIsoManager->init()) {
        debugFatal("Could not initialize IsoHandlerManager\n");
        return false;
    }

    // make sure that the thread parameters of all our helper threads are OK
    if(!setThreadParameters(m_realtime, m_base_priority)) {
        debugFatal("Could not set thread parameters\n");
        return false;
    }
    return true;
}

bool
Ieee1394Service::setThreadParameters(bool rt, int priority) {
    bool result = true;
    if (priority > THREAD_MAX_RTPRIO) priority = THREAD_MAX_RTPRIO;
    if (priority < THREAD_MIN_RTPRIO) priority = THREAD_MIN_RTPRIO;
    m_base_priority = priority;
    m_realtime = rt;
    if (m_pIsoManager) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Switching IsoManager to (rt=%d, prio=%d)\n",
                                         rt, priority);
        result &= m_pIsoManager->setThreadParameters(rt, priority);
    } //else debugError("Bogus isomanager\n");
    if (m_pCTRHelper) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Switching CycleTimerHelper to (rt=%d, prio=%d)\n", 
                                         rt && IEEE1394SERVICE_CYCLETIMER_HELPER_RUN_REALTIME,
                                         IEEE1394SERVICE_CYCLETIMER_HELPER_PRIO);
        result &= m_pCTRHelper->setThreadParameters(rt && IEEE1394SERVICE_CYCLETIMER_HELPER_RUN_REALTIME,
                                                    IEEE1394SERVICE_CYCLETIMER_HELPER_PRIO);
    } //else debugError("Bogus CTR helper\n");
    if(m_armHelperRealtime) {
        m_armHelperRealtime->setThreadParameters(rt, priority);
    } //else debugError("Bogus RT ARM helper\n");
    return result;
}

int
Ieee1394Service::getNodeCount()
{
    Util::MutexLockHelper lock(*m_handle_lock);
    return raw1394_get_nodecount( m_handle );
}

nodeid_t Ieee1394Service::getLocalNodeId() {
    Util::MutexLockHelper lock(*m_handle_lock);
    return raw1394_get_local_id(m_handle) & 0x3F;
}

/**
 * Returns the current value of the cycle timer (in ticks)
 *
 * @return the current value of the cycle timer (in ticks)
 */

uint32_t
Ieee1394Service::getCycleTimerTicks() {
    return m_pCTRHelper->getCycleTimerTicks();
}

/**
 * Returns the current value of the cycle timer (as is)
 *
 * @return the current value of the cycle timer (as is)
 */
uint32_t
Ieee1394Service::getCycleTimer() {
    return m_pCTRHelper->getCycleTimer();
}

/**
 * Returns the current value of the cycle timer (in ticks)
 * for a specific time instant (usecs since epoch)
 * @return the current value of the cycle timer (in ticks)
 */

uint32_t
Ieee1394Service::getCycleTimerTicks(uint64_t t) {
    return m_pCTRHelper->getCycleTimerTicks(t);
}

/**
 * Returns the current value of the cycle timer (as is)
 * for a specific time instant (usecs since epoch)
 * @return the current value of the cycle timer (as is)
 */
uint32_t
Ieee1394Service::getCycleTimer(uint64_t t) {
    return m_pCTRHelper->getCycleTimer(t);
}

uint64_t
Ieee1394Service::getSystemTimeForCycleTimerTicks(uint32_t ticks) {
    return m_pCTRHelper->getSystemTimeForCycleTimerTicks(ticks);
}

uint64_t
Ieee1394Service::getSystemTimeForCycleTimer(uint32_t ctr) {
    return m_pCTRHelper->getSystemTimeForCycleTimer(ctr);
}

bool
Ieee1394Service::readCycleTimerReg(uint32_t *cycle_timer, uint64_t *local_time)
{
    if(m_have_new_ctr_read) {
        int err;
        err = raw1394_read_cycle_timer(m_util_handle, cycle_timer, local_time);
        if(err) {
            debugWarning("raw1394_read_cycle_timer: %s\n", strerror(err));
            return false;
        }
        return true;
    } else {
        // do a normal read of the CTR register
        // the disadvantage is that local_time and cycle time are not
        // read at the same time instant (scheduling issues)
        *local_time = getCurrentTimeAsUsecs();
        if ( raw1394_read( m_util_handle,
                getLocalNodeId() | 0xFFC0,
                CSR_REGISTER_BASE | CSR_CYCLE_TIME,
                sizeof(uint32_t), cycle_timer ) == 0 ) {
            *cycle_timer = CondSwapFromBus32(*cycle_timer);
            return true;
        } else {
            return false;
        }
    }
}

uint64_t
Ieee1394Service::getCurrentTimeAsUsecs() {
    return Util::SystemTimeSource::getCurrentTimeAsUsecs();
}

bool
Ieee1394Service::read( fb_nodeid_t nodeId,
                       fb_nodeaddr_t addr,
                       size_t length,
                       fb_quadlet_t* buffer )
{
    Util::MutexLockHelper lock(*m_handle_lock);
    return readNoLock(nodeId, addr, length, buffer);
}

bool
Ieee1394Service::readNoLock( fb_nodeid_t nodeId,
                             fb_nodeaddr_t addr,
                             size_t length,
                             fb_quadlet_t* buffer )
{
    if (nodeId == INVALID_NODE_ID) {
        debugWarning("operation on invalid node\n");
        return false;
    }
    if ( raw1394_read( m_handle, nodeId, addr, length*4, buffer ) == 0 ) {

        #ifdef DEBUG
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
            "read: node 0x%hX, addr = 0x%016"PRIX64", length = %zd\n",
            nodeId, addr, length);
        printBuffer( DEBUG_LEVEL_VERY_VERBOSE, length, buffer );
        #endif

        return true;
    } else {
        #ifdef DEBUG
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "raw1394_read failed: node 0x%hX, addr = 0x%016"PRIX64", length = %zd\n",
                    nodeId, addr, length);
        #endif
        return false;
    }
}

bool
Ieee1394Service::read_quadlet( fb_nodeid_t nodeId,
                               fb_nodeaddr_t addr,
                               fb_quadlet_t* buffer )
{
    return read( nodeId,  addr, sizeof( *buffer )/4, buffer );
}

bool
Ieee1394Service::read_octlet( fb_nodeid_t nodeId,
                              fb_nodeaddr_t addr,
                              fb_octlet_t* buffer )
{
    return read( nodeId, addr, sizeof( *buffer )/4,
                 reinterpret_cast<fb_quadlet_t*>( buffer ) );
}

bool
Ieee1394Service::write( fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        size_t length,
                        fb_quadlet_t* data )
{
    Util::MutexLockHelper lock(*m_handle_lock);
    return writeNoLock(nodeId, addr, length, data);
}

bool
Ieee1394Service::writeNoLock( fb_nodeid_t nodeId,
                              fb_nodeaddr_t addr,
                              size_t length,
                              fb_quadlet_t* data )
{
    if (nodeId == INVALID_NODE_ID) {
        debugWarning("operation on invalid node\n");
        return false;
    }

    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"write: node 0x%hX, addr = 0x%016"PRIX64", length = %zd\n",
                nodeId, addr, length);
    printBuffer( DEBUG_LEVEL_VERY_VERBOSE, length, data );
    #endif

    return raw1394_write( m_handle, nodeId, addr, length*4, data ) == 0;
}

bool
Ieee1394Service::write_quadlet( fb_nodeid_t nodeId,
                                fb_nodeaddr_t addr,
                                fb_quadlet_t data )
{
    return write( nodeId, addr, sizeof( data )/4, &data );
}

bool
Ieee1394Service::write_octlet( fb_nodeid_t nodeId,
                               fb_nodeaddr_t addr,
                               fb_octlet_t data )
{
    return write( nodeId, addr, sizeof( data )/4,
                  reinterpret_cast<fb_quadlet_t*>( &data ) );
}

bool
Ieee1394Service::lockCompareSwap64( fb_nodeid_t nodeId,
                                    fb_nodeaddr_t addr,
                                    fb_octlet_t compare_value,
                                    fb_octlet_t swap_value,
                                    fb_octlet_t* result )
{
    if (nodeId == INVALID_NODE_ID) {
        debugWarning("operation on invalid node\n");
        return false;
    }
    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERBOSE,"lockCompareSwap64: node 0x%X, addr = 0x%016"PRIX64"\n",
                nodeId, addr);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  if (*(addr)==0x%016"PRIX64") *(addr)=0x%016"PRIX64"\n",
                compare_value, swap_value);
    fb_octlet_t buffer;
    if(!read_octlet( nodeId, addr,&buffer )) {
        debugWarning("Could not read register\n");
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,"before = 0x%016"PRIX64"\n", buffer);
    }
    #endif

    // do endiannes swapping
    compare_value = CondSwapToBus64(compare_value);
    swap_value    = CondSwapToBus64(swap_value);

    // do separate locking here (no MutexLockHelper) since 
    // we use read_octlet in the DEBUG code in this function
    m_handle_lock->Lock();
    int retval=raw1394_lock64(m_handle, nodeId, addr,
                              RAW1394_EXTCODE_COMPARE_SWAP,
                              swap_value, compare_value, result);
    m_handle_lock->Unlock();

    if(retval) {
        debugError("raw1394_lock64 failed: %s\n", strerror(errno));
    }

    #ifdef DEBUG
    if(!read_octlet( nodeId, addr,&buffer )) {
        debugWarning("Could not read register\n");
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,"after = 0x%016"PRIX64"\n", buffer);
    }
    #endif

    *result = CondSwapFromBus64(*result);

    return (retval == 0);
}

fb_quadlet_t*
Ieee1394Service::transactionBlock( fb_nodeid_t nodeId,
                                   fb_quadlet_t* buf,
                                   int len,
                                   unsigned int* resp_len )
{
    // FIXME: simplify semantics
    if (nodeId == INVALID_NODE_ID) {
        debugWarning("operation on invalid node\n");
        return false;
    }
    // NOTE: this expects a call to transactionBlockClose to unlock
    m_handle_lock->Lock();

    // clear the request & response memory
    memset(&m_fcp_block, 0, sizeof(m_fcp_block));

    // make a local copy of the request
    if(len < MAX_FCP_BLOCK_SIZE_QUADS) {
        memcpy(m_fcp_block.request, buf, len*sizeof(quadlet_t));
        m_fcp_block.request_length = len;
    } else {
        debugWarning("Truncating FCP request\n");
        memcpy(m_fcp_block.request, buf, MAX_FCP_BLOCK_SIZE_BYTES);
        m_fcp_block.request_length = MAX_FCP_BLOCK_SIZE_QUADS;
    }
    m_fcp_block.target_nodeid = 0xffc0 | nodeId;

    bool success = doFcpTransaction();
    if(success) {
        *resp_len = m_fcp_block.response_length;
        return m_fcp_block.response;
    } else {
        debugWarning("FCP transaction failed\n");
        *resp_len = 0;
        return NULL;
    }
}

bool
Ieee1394Service::transactionBlockClose()
{
    m_handle_lock->Unlock();
    return true;
}

// FCP code
bool
Ieee1394Service::doFcpTransaction()
{
    for(int i=0; i < IEEE1394SERVICE_FCP_MAX_TRIES; i++) {
        if(doFcpTransactionTry()) {
            return true;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "FCP transaction try %d failed\n", i);
            Util::SystemTimeSource::SleepUsecRelative( IEEE1394SERVICE_FCP_SLEEP_BETWEEN_FAILURES_USECS);
        }
    }
    debugError("FCP transaction didn't succeed in %d tries\n", IEEE1394SERVICE_FCP_MAX_TRIES);
    return false;
}

#define FCP_COMMAND_ADDR   0xFFFFF0000B00ULL
#define FCP_RESPONSE_ADDR  0xFFFFF0000D00ULL

/* AV/C FCP response codes */
#define FCP_RESPONSE_NOT_IMPLEMENTED 0x08000000
#define FCP_RESPONSE_ACCEPTED 0x09000000
#define FCP_RESPONSE_REJECTED 0x0A000000
#define FCP_RESPONSE_IN_TRANSITION 0x0B000000
#define FCP_RESPONSE_IMPLEMENTED 0x0C000000
#define FCP_RESPONSE_STABLE 0x0C000000
#define FCP_RESPONSE_CHANGED 0x0D000000
#define FCP_RESPONSE_INTERIM 0x0F000000

/* AV/C FCP mask macros */
#define FCP_MASK_START(x) ((x) & 0xF0000000)
#define FCP_MASK_CTYPE(x) ((x) & 0x0F000000)
#define FCP_MASK_RESPONSE(x) ((x) & 0x0F000000)
#define FCP_MASK_SUBUNIT(x) ((x) & 0x00FF0000)
#define FCP_MASK_SUBUNIT_TYPE(x) ((x) & 0x00F80000)
#define FCP_MASK_SUBUNIT_ID(x) ((x) & 0x00070000)
#define FCP_MASK_OPCODE(x) ((x) & 0x0000FF00)
#define FCP_MASK_SUBUNIT_AND_OPCODE(x) ((x) & 0x00FFFF00)
#define FCP_MASK_OPERAND0(x) ((x) & 0x000000FF)
#define FCP_MASK_OPERAND(x, n) ((x) & (0xFF000000 >> ((((n)-1)%4)*8)))
#define FCP_MASK_RESPONSE_OPERAND(x, n) ((x) & (0xFF000000 >> (((n)%4)*8)))

bool
Ieee1394Service::doFcpTransactionTry()
{
    // NOTE that access to this is protected by the m_handle lock
    int err;
    bool retval = true;
    uint64_t timeout;

    // prepare an fcp response handler
    raw1394_set_fcp_handler(m_handle, _avc_fcp_handler);

    // start listening for FCP requests
    // this fails if some other program is listening for a FCP response
    err = raw1394_start_fcp_listen(m_handle);
    if(err) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "could not start FCP listen (err=%d, errno=%d)\n", err, errno);
        retval = false;
        goto out;
    }

    m_fcp_block.status = eFS_Waiting;

    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"fcp request: node 0x%hX, length = %d bytes\n",
                m_fcp_block.target_nodeid, m_fcp_block.request_length*4);
    printBuffer(DEBUG_LEVEL_VERY_VERBOSE, m_fcp_block.request_length, m_fcp_block.request );
    #endif

    // write the FCP request
    if(!writeNoLock( m_fcp_block.target_nodeid, FCP_COMMAND_ADDR,
                     m_fcp_block.request_length, m_fcp_block.request)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "write of FCP request failed\n");
        retval = false;
        goto out;
    }

    // wait for the response to arrive
    struct pollfd raw1394_poll;
    raw1394_poll.fd = raw1394_get_fd(m_handle);
    raw1394_poll.events = POLLIN;

    timeout = Util::SystemTimeSource::getCurrentTimeAsUsecs() +
              IEEE1394SERVICE_FCP_RESPONSE_TIMEOUT_USEC;

    while(m_fcp_block.status == eFS_Waiting 
          && Util::SystemTimeSource::getCurrentTimeAsUsecs() < timeout) {
        if(poll( &raw1394_poll, 1, IEEE1394SERVICE_FCP_POLL_TIMEOUT_MSEC) > 0) {
            if (raw1394_poll.revents & POLLIN) {
                raw1394_loop_iterate(m_handle);
            }
        }
    }

    // check the request and figure out what happened
    if(m_fcp_block.status == eFS_Waiting) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "FCP response timed out\n");
        retval = false;
        goto out;
    }
    if(m_fcp_block.status == eFS_Error) {
        debugError("FCP request/response error\n");
        retval = false;
        goto out;
    }

out:
    // stop listening for FCP responses
    err = raw1394_stop_fcp_listen(m_handle);
    if(err) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "could not stop FCP listen (err=%d, errno=%d)\n", err, errno);
        retval = false;
    }

    m_fcp_block.status = eFS_Empty;
    return retval;
}

int
Ieee1394Service::_avc_fcp_handler(raw1394handle_t handle, nodeid_t nodeid, 
                                  int response, size_t length,
                                  unsigned char *data)
{
    Ieee1394Service *service = static_cast<Ieee1394Service *>(raw1394_get_userdata(handle));
    if(service) {
        return service->handleFcpResponse(nodeid, response, length, data);
    } else return -1;
}

int
Ieee1394Service::handleFcpResponse(nodeid_t nodeid,
                                   int response, size_t length,
                                   unsigned char *data)
{
    static struct sFcpBlock fcp_block_last;

    fb_quadlet_t *data_quads = (fb_quadlet_t *)data;
    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"fcp response: node 0x%hX, response = %d, length = %zd bytes\n",
                nodeid, response, length);
    printBuffer(DEBUG_LEVEL_VERY_VERBOSE, (length+3)/4, data_quads );
    #endif

    if (response && length > 3) {
        if(length > MAX_FCP_BLOCK_SIZE_BYTES) {
            length = MAX_FCP_BLOCK_SIZE_BYTES;
            debugWarning("Truncated FCP response\n");
        }

        // is it an actual response or is it INTERIM?
        quadlet_t first_quadlet = CondSwapFromBus32(data_quads[0]);
        if(FCP_MASK_RESPONSE(first_quadlet) == FCP_RESPONSE_INTERIM) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "INTERIM\n");
        } else {
            // it's an actual response, check if it matches our request
            if(nodeid != m_fcp_block.target_nodeid) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "FCP response node id's don't match! (%x, %x)\n",
                                                 m_fcp_block.target_nodeid, nodeid);
            } else if (first_quadlet == 0) {
                debugWarning("Bogus FCP response\n");
                printBuffer(DEBUG_LEVEL_WARNING, (length+3)/4, data_quads );
#ifdef DEBUG
            } else if(FCP_MASK_RESPONSE(first_quadlet) < 0x08000000) {
                debugWarning("Bogus AV/C FCP response code\n");
                printBuffer(DEBUG_LEVEL_WARNING, (length+3)/4, data_quads );
#endif
            } else if(FCP_MASK_SUBUNIT_AND_OPCODE(first_quadlet) 
                      != FCP_MASK_SUBUNIT_AND_OPCODE(CondSwapFromBus32(m_fcp_block.request[0]))) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "FCP response not for this request: %08X != %08X\n",
                             FCP_MASK_SUBUNIT_AND_OPCODE(first_quadlet),
                             FCP_MASK_SUBUNIT_AND_OPCODE(CondSwapFromBus32(m_fcp_block.request[0])));
            } else if(m_filterFCPResponse && (memcmp(fcp_block_last.response, data, length) == 0)) {
                // This is workaround for the Edirol FA-101. The device tends to send more than
                // one responde to one request. This seems to happen when discovering 
                // function blocks and looks very likely there is a race condition in the 
                // device. The workaround here compares the just arrived FCP responde
                // to the last one. If it is the same as the previously one then we
                // just ignore it. The downside of this approach is, we cannot issue
                // the same FCP twice.
                debugWarning("Received duplicate FCP response. Ignore it\n");
            } else {
                m_fcp_block.response_length = (length + sizeof(quadlet_t) - 1) / sizeof(quadlet_t);
                memcpy(m_fcp_block.response, data, length);
                if (m_filterFCPResponse) {
                    memcpy(fcp_block_last.response, data, length);
                }
                m_fcp_block.status = eFS_Responded;
            }
       }
    }
    return 0;
}

bool
Ieee1394Service::setSplitTimeoutUsecs(fb_nodeid_t nodeId, unsigned int timeout)
{
    Util::MutexLockHelper lock(*m_handle_lock);
    debugOutput(DEBUG_LEVEL_VERBOSE, "setting SPLIT_TIMEOUT on node 0x%X to %uusecs...\n", nodeId, timeout);
    unsigned int secs = timeout / 1000000;
    unsigned int usecs = timeout % 1000000;

    quadlet_t split_timeout_hi = CondSwapToBus32(secs & 7);
    quadlet_t split_timeout_low = CondSwapToBus32(((usecs / 125) & 0x1FFF) << 19);

    // write the CSR registers
    if(!writeNoLock( 0xffc0 | nodeId, CSR_REGISTER_BASE + CSR_SPLIT_TIMEOUT_HI, 1,
                  &split_timeout_hi)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "write of CSR_SPLIT_TIMEOUT_HI failed\n");
        return false;
    }
    if(!writeNoLock( 0xffc0 | nodeId, CSR_REGISTER_BASE + CSR_SPLIT_TIMEOUT_LO, 1,
                  &split_timeout_low)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "write of CSR_SPLIT_TIMEOUT_LO failed\n");
        return false;
    }
    return true;
}

int
Ieee1394Service::getSplitTimeoutUsecs(fb_nodeid_t nodeId)
{
    Util::MutexLockHelper lock(*m_handle_lock);

    // Keep Valgrind quiet by including explicit assignment
    quadlet_t split_timeout_hi = 0;
    quadlet_t split_timeout_low = 0;

    debugOutput(DEBUG_LEVEL_VERBOSE, "reading SPLIT_TIMEOUT on node 0x%X...\n", nodeId);

    if(!readNoLock( 0xffc0 | nodeId, CSR_REGISTER_BASE + CSR_SPLIT_TIMEOUT_HI, 1,
                  &split_timeout_hi)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "read of CSR_SPLIT_TIMEOUT_HI failed\n");
        return 0;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " READ HI: 0x%08X\n", split_timeout_hi);

    if(!readNoLock( 0xffc0 | nodeId, CSR_REGISTER_BASE + CSR_SPLIT_TIMEOUT_LO, 1,
                  &split_timeout_low)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "read of CSR_SPLIT_TIMEOUT_LO failed\n");
        return 0;
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " READ LO: 0x%08X\n", split_timeout_low);

    split_timeout_hi = CondSwapFromBus32(split_timeout_hi);
    split_timeout_low = CondSwapFromBus32(split_timeout_low);

    return (split_timeout_hi & 7) * 1000000 + (split_timeout_low >> 19) * 125;
}

void 
Ieee1394Service::setFCPResponseFiltering(bool enable)
{
    m_filterFCPResponse = enable;
}

int
Ieee1394Service::getVerboseLevel()
{
    return getDebugLevel();
}

void
Ieee1394Service::printBuffer( unsigned int level, size_t length, fb_quadlet_t* buffer ) const
{

    for ( unsigned int i=0; i < length; ++i ) {
        if ( ( i % 4 ) == 0 ) {
            if ( i > 0 ) {
                debugOutputShort(level,"\n");
            }
            debugOutputShort(level," %4d: ",i*4);
        }
        debugOutputShort(level,"%08X ",buffer[i]);
    }
    debugOutputShort(level,"\n");
}
void
Ieee1394Service::printBufferBytes( unsigned int level, size_t length, byte_t* buffer ) const
{

    for ( unsigned int i=0; i < length; ++i ) {
        if ( ( i % 16 ) == 0 ) {
            if ( i > 0 ) {
                debugOutputShort(level,"\n");
            }
            debugOutputShort(level," %4d: ",i*16);
        }
        debugOutputShort(level,"%02X ",buffer[i]);
    }
    debugOutputShort(level,"\n");
}

int
Ieee1394Service::resetHandlerLowLevel( raw1394handle_t handle,
                                       unsigned int generation )
{
    raw1394_update_generation ( handle, generation );

    Ieee1394Service::HelperThread *thread = reinterpret_cast<Ieee1394Service::HelperThread *>(raw1394_get_userdata( handle ));
    if(thread == NULL) {
        debugFatal("Bogus 1394 handle private data\n");
        return -1;
    }

    Ieee1394Service& service = thread->get1394Service();
    service.resetHandler( generation );

    return 0;
}

bool
Ieee1394Service::resetHandler( unsigned int generation )
{
    quadlet_t buf=0;

    m_handle_lock->Lock();
    raw1394_update_generation(m_handle, generation);
    m_handle_lock->Unlock();

    // do a simple read on ourself in order to update the internal structures
    // this avoids failures after a bus reset
    read_quadlet( getLocalNodeId() | 0xFFC0,
                  CSR_REGISTER_BASE | CSR_CYCLE_TIME,
                  &buf );

    for ( reset_handler_vec_t::iterator it = m_busResetHandlers.begin();
          it != m_busResetHandlers.end();
          ++it )
    {
        Util::Functor* func = *it;
        ( *func )();
    }

    return true;
}

bool Ieee1394Service::registerARMHandler(ARMHandler *h) {
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Registering ARM handler (%p) for 0x%016"PRIX64", length %zu\n",
                h, h->getStart(), h->getLength());

    // FIXME: note that this will result in the ARM handlers not running in a realtime context
    int err = raw1394_arm_register(m_armHelperNormal->get1394Handle(), h->getStart(),
                                   h->getLength(), h->getBuffer(), (octlet_t)h,
                                   h->getAccessRights(),
                                   h->getNotificationOptions(),
                                   h->getClientTransactions());
    if (err) {
        debugError("Failed to register ARM handler for 0x%016"PRIX64"\n", h->getStart());
        debugError(" Error: %s\n", strerror(errno));
        return false;
    }
    m_armHandlers.push_back( h );
    return true;
}

bool Ieee1394Service::unregisterARMHandler( ARMHandler *h ) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Unregistering ARM handler (%p) for 0x%016"PRIX64"\n",
        h, h->getStart());

    for ( arm_handler_vec_t::iterator it = m_armHandlers.begin();
          it != m_armHandlers.end();
          ++it )
    {
        if((*it) == h) {
            int err = raw1394_arm_unregister(m_armHelperNormal->get1394Handle(), h->getStart());
            if (err) {
                debugError("Failed to unregister ARM handler (%p)\n", h);
                debugError(" Error: %s\n", strerror(errno));
            } else {
                m_armHandlers.erase(it);
                return true;
            }
        }
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " handler not found!\n");

    return false;
}
/**
 * @brief Tries to find a free ARM address range
 *
 * @param start  address to start with
 * @param length length of the block needed (bytes)
 * @param step   step to use when searching (bytes)
 * @return The base address that is free, and 0xFFFFFFFFFFFFFFFF when failed
 */
nodeaddr_t Ieee1394Service::findFreeARMBlock( nodeaddr_t start, size_t length, size_t step ) {
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Finding free ARM block of %zd bytes, from 0x%016"PRIX64" in steps of %zd bytes\n",
                length, start, step);

    int cnt=0;
    const int maxcnt=10;
    int err=1;
    Util::MutexLockHelper lock(*m_handle_lock);
    while(err && cnt++ < maxcnt) {
        // try to register
        err = raw1394_arm_register(m_handle, start, length, 0, 0, 0, 0, 0);

        if (err) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " -> cannot use 0x%016"PRIX64"\n", start);
            debugError("    Error: %s\n", strerror(errno));
            start += step;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, " -> use 0x%016"PRIX64"\n", start);
            err = raw1394_arm_unregister(m_handle, start);
            if (err) {
                debugOutput(DEBUG_LEVEL_VERBOSE, " error unregistering test handler\n");
                debugError("    Error: %s\n", strerror(errno));
                return 0xFFFFFFFFFFFFFFFFLLU;
            }
            return start;
        }
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " Could not find free block in %d tries\n",cnt);
    return 0xFFFFFFFFFFFFFFFFLLU;
}

int
Ieee1394Service::armHandlerLowLevel(raw1394handle_t handle,
                                    unsigned long arm_tag,
                                    byte_t request_type, unsigned int requested_length,
                                    void *data)
{
    Ieee1394Service::HelperThread *thread = reinterpret_cast<Ieee1394Service::HelperThread *>(raw1394_get_userdata( handle ));
    if(thread == NULL) {
        debugFatal("Bogus 1394 handle private data\n");
        return -1;
    }

    Ieee1394Service& service = thread->get1394Service();
    if(service.armHandler( arm_tag, request_type, requested_length, data )) {
        return 0;
    } else {
        return -1;
    }
}

bool
Ieee1394Service::armHandler(  unsigned long arm_tag,
                     byte_t request_type, unsigned int requested_length,
                     void *data)
{
    for ( arm_handler_vec_t::iterator it = m_armHandlers.begin();
          it != m_armHandlers.end();
          ++it )
    {
        if((*it) == (ARMHandler *)arm_tag) {
            struct raw1394_arm_request_response *arm_req_resp;
            arm_req_resp = (struct raw1394_arm_request_response *) data;
            raw1394_arm_request_t arm_req = arm_req_resp->request;
            raw1394_arm_response_t arm_resp = arm_req_resp->response;

            debugOutput(DEBUG_LEVEL_VERBOSE,"ARM handler for address 0x%016"PRIX64" called\n",
                (*it)->getStart());
            debugOutput(DEBUG_LEVEL_VERBOSE," request type   : 0x%02X\n", request_type);
            debugOutput(DEBUG_LEVEL_VERBOSE," request length : %04d\n", requested_length);

            switch(request_type) {
                case RAW1394_ARM_READ:
                    (*it)->handleRead(arm_req);
                    *arm_resp = *((*it)->getResponse());
                    break;
                case RAW1394_ARM_WRITE:
                    (*it)->handleWrite(arm_req);
                    *arm_resp = *((*it)->getResponse());
                    break;
                case RAW1394_ARM_LOCK:
                    (*it)->handleLock(arm_req);
                    *arm_resp = *((*it)->getResponse());
                    break;
                default:
                    debugWarning("Unknown request type received, ignoring...\n");
            }
            return true;
        }
    }

    debugOutput(DEBUG_LEVEL_VERBOSE,"default ARM handler called\n");

    m_default_arm_handler(m_armHelperNormal->get1394Handle(), arm_tag, request_type, requested_length, data );
    return true;
}

bool
Ieee1394Service::addBusResetHandler( Util::Functor* functor )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Adding busreset handler (%p)\n", functor);
    m_busResetHandlers.push_back( functor );
    return true;
}

bool
Ieee1394Service::remBusResetHandler( Util::Functor* functor )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Removing busreset handler (%p)\n", functor);

    for ( reset_handler_vec_t::iterator it = m_busResetHandlers.begin();
          it != m_busResetHandlers.end();
          ++it )
    {
        if ( *it == functor ) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " found\n");
            m_busResetHandlers.erase( it );
            return true;
        }
    }
    debugOutput(DEBUG_LEVEL_VERBOSE, " not found\n");
    return false;
}

/**
 * Allocates an iso channel for use by the interface in a similar way to
 * libiec61883.  Returns -1 on error (due to there being no free channels)
 * or an allocated channel number.
 *
 * Does not perform anything other than registering the channel and the
 * bandwidth at the IRM
 *
 * Also allocates the necessary bandwidth (in ISO allocation units).
 *
 * FIXME: As in libiec61883, channel 63 is not requested; this is either a
 * bug or it's omitted since that's the channel preferred by video devices.
 *
 * @param bandwidth the bandwidth to allocate for this channel
 * @return the channel number
 */
signed int Ieee1394Service::allocateIsoChannelGeneric(unsigned int bandwidth) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Allocating ISO channel using generic method...\n" );

    Util::MutexLockHelper lock(*m_handle_lock);
    struct ChannelInfo cinfo;

    int c = -1;
    for (c = 0; c < 63; c++) {
        if (raw1394_channel_modify (m_handle, c, RAW1394_MODIFY_ALLOC) == 0)
            break;
    }
    if (c < 63) {
        if (raw1394_bandwidth_modify(m_handle, bandwidth, RAW1394_MODIFY_ALLOC) < 0) {
            debugFatal("Could not allocate bandwidth of %d\n", bandwidth);

            raw1394_channel_modify (m_handle, c, RAW1394_MODIFY_FREE);
            return -1;
        } else {
            cinfo.channel=c;
            cinfo.bandwidth=bandwidth;
            cinfo.alloctype=AllocGeneric;

            cinfo.xmit_node=-1;
            cinfo.xmit_plug=-1;
            cinfo.recv_node=-1;
            cinfo.recv_plug=-1;

            if (registerIsoChannel(c, cinfo)) {
                return c;
            } else {
                raw1394_bandwidth_modify(m_handle, bandwidth, RAW1394_MODIFY_FREE);
                raw1394_channel_modify (m_handle, c, RAW1394_MODIFY_FREE);
                return -1;
            }
        }
    }
    return -1;
}

/**
 * Allocates a specific fixed iso channel for use by the interface.  Returns 
 * -1 on error (due to the requested channel not being free) or the fixed iso
 * channel number.
 *
 * Does not perform anything other than registering the channel and the
 * bandwidth at the IRM
 *
 * Also allocates the necessary bandwidth (in ISO allocation units).
 *
 * FIXME: As in libiec61883, channel 63 is not requested; this is either a
 * bug or it's omitted since that's the channel preferred by video devices.
 *
 * @chan the channel number being requested
 * @param bandwidth the bandwidth to allocate for this channel
 * @return the channel number
 */
signed int Ieee1394Service::allocateFixedIsoChannelGeneric(
    unsigned int chan, unsigned int bandwidth
    ) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Allocating ISO channel %d using generic method...\n", chan );

    Util::MutexLockHelper lock(*m_handle_lock);
    struct ChannelInfo cinfo;

    if (raw1394_channel_modify (m_handle, chan, RAW1394_MODIFY_ALLOC) == 0) {
        if (raw1394_bandwidth_modify(m_handle, bandwidth, RAW1394_MODIFY_ALLOC) < 0) {
            debugFatal("Could not allocate bandwidth of %d\n", bandwidth);

            raw1394_channel_modify (m_handle, chan, RAW1394_MODIFY_FREE);
            return -1;
        } else {
            cinfo.channel=chan;
            cinfo.bandwidth=bandwidth;
            cinfo.alloctype=AllocGeneric;

            cinfo.xmit_node=-1;
            cinfo.xmit_plug=-1;
            cinfo.recv_node=-1;
            cinfo.recv_plug=-1;

            if (registerIsoChannel(chan, cinfo)) {
                return chan;
            } else {
                raw1394_bandwidth_modify(m_handle, bandwidth, RAW1394_MODIFY_FREE);
                raw1394_channel_modify (m_handle, chan, RAW1394_MODIFY_FREE);
                return -1;
            }
        }
    }
    return -1;
}

/**
 * Allocates an iso channel for use by the interface in a similar way to
 * libiec61883.  Returns -1 on error (due to there being no free channels)
 * or an allocated channel number.
 *
 * Uses IEC61883 Connection Management Procedure to establish the connection.
 *
 * Also allocates the necessary bandwidth (in ISO allocation units).
 *
 * @param xmit_node  node id of the transmitter
 * @param xmit_plug  the output plug to use. If -1, find the first online plug, and
 * upon return, contains the plug number used.
 * @param recv_node  node id of the receiver
 * @param recv_plug the input plug to use. If -1, find the first online plug, and
 * upon return, contains the plug number used.
 *
 * @return the channel number
 */

signed int Ieee1394Service::allocateIsoChannelCMP(
    nodeid_t xmit_node, int xmit_plug,
    nodeid_t recv_node, int recv_plug
    ) {

    if (xmit_node == INVALID_NODE_ID) {
        debugWarning("operation on invalid node (XMIT)\n");
        return -1;
    }
    if (recv_node == INVALID_NODE_ID) {
        debugWarning("operation on invalid node (RECV)\n");
        return -1;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE, "Allocating ISO channel using IEC61883 CMP...\n" );
    Util::MutexLockHelper lock(*m_handle_lock);

    struct ChannelInfo cinfo;

    int c = -1;
    int bandwidth=1;
    #if IEEE1394SERVICE_SKIP_IEC61883_BANDWIDTH_ALLOCATION
    bandwidth=0;
    #endif

    // do connection management: make connection
    c = iec61883_cmp_connect(
        m_handle,
        xmit_node | 0xffc0,
        &xmit_plug,
        recv_node | 0xffc0,
        &recv_plug,
        &bandwidth);

    if((c<0) || (c>63)) {
        debugError("Could not do CMP from %04X:%02d to %04X:%02d\n",
            xmit_node, xmit_plug, recv_node, recv_plug
            );
        return -1;
    }

    cinfo.channel=c;
    cinfo.bandwidth=bandwidth;
    cinfo.alloctype=AllocCMP;

    cinfo.xmit_node=xmit_node;
    cinfo.xmit_plug=xmit_plug;
    cinfo.recv_node=recv_node;
    cinfo.recv_plug=recv_plug;

    if (registerIsoChannel(c, cinfo)) {
        return c;
    }

    return -1;
}

/**
 * Deallocates an iso channel.  Silently ignores a request to deallocate
 * a negative channel number.
 *
 * Figures out the method that was used to allocate the channel (generic, cmp, ...)
 * and uses the appropriate method to deallocate. Also frees the bandwidth
 * that was reserved along with this channel.
 *
 * @param c channel number
 * @return true if successful
 */
bool Ieee1394Service::freeIsoChannel(signed int c) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Freeing ISO channel %d...\n", c );
    Util::MutexLockHelper lock(*m_handle_lock);

    if (c < 0 || c > 63) {
        debugWarning("Invalid channel number: %d\n", c);
        return false;
    }

    switch (m_channels[c].alloctype) {
        default:
            debugError(" BUG: invalid allocation type!\n");
            return false;

        case AllocFree:
            debugWarning(" Channel %d not registered\n", c);
            return false;

        case AllocGeneric:
            debugOutput(DEBUG_LEVEL_VERBOSE, " allocated using generic routine...\n" );
            debugOutput(DEBUG_LEVEL_VERBOSE, " freeing %d bandwidth units...\n", m_channels[c].bandwidth );
            if (raw1394_bandwidth_modify(m_handle, m_channels[c].bandwidth, RAW1394_MODIFY_FREE) !=0) {
                debugWarning("Failed to deallocate bandwidth\n");
            }
            debugOutput(DEBUG_LEVEL_VERBOSE, " freeing channel %d...\n", m_channels[c].channel );
            if (raw1394_channel_modify (m_handle, m_channels[c].channel, RAW1394_MODIFY_FREE) != 0) {
                debugWarning("Failed to free channel\n");
            }
            if (!unregisterIsoChannel(c))
                return false;
            return true;

        case AllocCMP:
            debugOutput(DEBUG_LEVEL_VERBOSE, " allocated using IEC61883 CMP...\n" );
            debugOutput(DEBUG_LEVEL_VERBOSE, " performing IEC61883 CMP disconnect...\n" );
            if(iec61883_cmp_disconnect(
                    m_handle,
                    m_channels[c].xmit_node | 0xffc0,
                    m_channels[c].xmit_plug,
                    m_channels[c].recv_node | 0xffc0,
                    m_channels[c].recv_plug,
                    m_channels[c].channel,
                    m_channels[c].bandwidth) != 0) {
                debugWarning("Could not do CMP disconnect for channel %d!\n",c);
            }
            if (!unregisterIsoChannel(c))
                return false;
            return true;
    }

    // unreachable
    debugError("BUG: unreachable code reached!\n");

    return false;
}

/**
 * Registers a channel as managed by this ieee1394service
 * @param c channel number
 * @param cinfo channel info struct
 * @return true if successful
 */
bool Ieee1394Service::registerIsoChannel(unsigned int c, struct ChannelInfo cinfo) {
    if (c < 63) {
        if (m_channels[c].alloctype != AllocFree) {
            debugWarning("Channel %d already registered with bandwidth %d\n",
                m_channels[c].channel, m_channels[c].bandwidth);
        }

        memcpy(&m_channels[c], &cinfo, sizeof(struct ChannelInfo));

    } else return false;
    return true;
}

/**
 * unegisters a channel from this ieee1394service
 * @param c channel number
 * @return true if successful
 */
bool Ieee1394Service::unregisterIsoChannel(unsigned int c) {
    if (c < 63) {
        if (m_channels[c].alloctype == AllocFree) {
            debugWarning("Channel %d not registered\n", c);
            return false;
        }

        m_channels[c].channel=-1;
        m_channels[c].bandwidth=-1;
        m_channels[c].alloctype=AllocFree;
        m_channels[c].xmit_node=0xFFFF;
        m_channels[c].xmit_plug=-1;
        m_channels[c].recv_node=0xFFFF;
        m_channels[c].recv_plug=-1;

    } else return false;
    return true;
}

/**
 * Returns the current value of the `bandwidth available' register on
 * the IRM, or -1 on error.
 * @return
 */
signed int Ieee1394Service::getAvailableBandwidth() {
    quadlet_t buffer;
    Util::MutexLockHelper lock(*m_handle_lock);
    signed int result = raw1394_read (m_handle, raw1394_get_irm_id (m_handle),
        CSR_REGISTER_BASE + CSR_BANDWIDTH_AVAILABLE,
        sizeof (quadlet_t), &buffer);

    if (result < 0)
        return -1;
    return CondSwapFromBus32(buffer);
}

void
Ieee1394Service::setVerboseLevel(int l)
{
    if (m_pIsoManager) m_pIsoManager->setVerboseLevel(l);
    if (m_pCTRHelper) m_pCTRHelper->setVerboseLevel(l);
    if (m_pWatchdog) m_pWatchdog->setVerboseLevel(l);
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

void
Ieee1394Service::show()
{
    #ifdef DEBUG
    uint32_t cycle_timer;
    uint64_t local_time;
    if(!readCycleTimerReg(&cycle_timer, &local_time)) {
        debugWarning("Could not read cycle timer register\n");
    
    }
    uint64_t ctr = CYCLE_TIMER_TO_TICKS( cycle_timer );

    debugOutput( DEBUG_LEVEL_VERBOSE, "Port:  %d\n", getPort() );
    debugOutput( DEBUG_LEVEL_VERBOSE, " Name: %s\n", getPortName().c_str() );
    debugOutput( DEBUG_LEVEL_VERBOSE, " CycleTimerHelper: %p, IsoManager: %p, WatchDog: %p\n",
                 m_pCTRHelper, m_pIsoManager, m_pWatchdog );
    debugOutput( DEBUG_LEVEL_VERBOSE, " Time: %011"PRIu64" (%03us %04ucy %04uticks)\n",
                ctr,
                (unsigned int)TICKS_TO_SECS( ctr ),
                (unsigned int)TICKS_TO_CYCLES( ctr ),
                (unsigned int)TICKS_TO_OFFSET( ctr ) );
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Iso handler info:\n");
    #endif
    if (m_pIsoManager) m_pIsoManager->dumpInfo();
}

// the helper thread class
Ieee1394Service::HelperThread::HelperThread(Ieee1394Service &parent, std::string name)
: m_parent( parent )
, m_name( name )
, m_handle( NULL )
, m_thread( *(new Util::PosixThread(this, name, false, 0, PTHREAD_CANCEL_DEFERRED)) )
, m_iterate( false )
, m_debugModule(parent.m_debugModule)
{
    m_handle = raw1394_new_handle_on_port( parent.m_port );
    if(!m_handle) {
        debugError("Could not allocate handle\n");
        // FIXME: better error handling required
    }
    raw1394_set_userdata( m_handle, this );
}

Ieee1394Service::HelperThread::HelperThread(Ieee1394Service &parent, std::string name, bool rt, int prio)
: m_parent( parent )
, m_name( name )
, m_handle( NULL )
, m_thread( *(new Util::PosixThread(this, name, rt, prio, PTHREAD_CANCEL_DEFERRED)) )
, m_iterate( false )
, m_debugModule(parent.m_debugModule)
{
    m_handle = raw1394_new_handle_on_port( parent.m_port );
    if(!m_handle) {
        debugError("Could not allocate handle\n");
        // FIXME: better error handling required
    }
    raw1394_set_userdata( m_handle, this );
}

Ieee1394Service::HelperThread::~HelperThread()
{
    m_thread.Stop();
    delete &m_thread;
    if(m_handle) {
        raw1394_destroy_handle(m_handle);
    }
}

bool
Ieee1394Service::HelperThread::Init()
{
    m_iterate = true;
    return true;
}

bool
Ieee1394Service::HelperThread::Execute()
{
    if(m_iterate) {
        int err;
        err = raw1394_loop_iterate (m_handle);
        if(err < 0) {
            debugError("Failed to iterate handler\n");
            return false;
        } else {
            return true;
        }
    } else {
        Util::SystemTimeSource::SleepUsecRelative(1000);
        return true;
    }
}

void
Ieee1394Service::HelperThread::setThreadParameters(bool rt, int priority)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) switch to: (rt=%d, prio=%d)...\n", this, rt, priority);
    if (priority > THREAD_MAX_RTPRIO) priority = THREAD_MAX_RTPRIO; // cap the priority
    if (rt) {
        m_thread.AcquireRealTime(priority);
    } else {
        m_thread.DropRealTime();
    }
}

bool
Ieee1394Service::HelperThread::Start()
{
    return m_thread.Start() == 0;
}

bool
Ieee1394Service::HelperThread::Stop()
{
    // request to stop iterating
    m_iterate = false;
    // poke the handler such that the iterate() returns
    raw1394_wake_up(m_handle);
    // stop the thread
    return m_thread.Stop() == 0;
}
