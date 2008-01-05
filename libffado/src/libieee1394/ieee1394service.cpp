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

#include "config.h"
#include "ieee1394service.h"
#include "ARMHandler.h"
#include "cycletimer.h"
#include "IsoHandlerManager.h"
#include "CycleTimerHelper.h"

#include <libavc1394/avc1394.h>
#include <libraw1394/csr.h>
#include <libiec61883/iec61883.h>

#include "libutil/SystemTimeSource.h"

#include <errno.h>
#include <netinet/in.h>

#include <string.h>

#include <iostream>
#include <iomanip>

IMPL_DEBUG_MODULE( Ieee1394Service, Ieee1394Service, DEBUG_LEVEL_NORMAL );

Ieee1394Service::Ieee1394Service()
    : m_handle( 0 ), m_resetHandle( 0 ), m_util_handle( 0 )
    , m_port( -1 )
    , m_threadRunning( false )
    , m_realtime ( false )
    , m_base_priority ( 0 )
    , m_pIsoManager( new IsoHandlerManager( *this ) )
    , m_pCTRHelper ( new CycleTimerHelper( *this, 10000 ) )
    , m_have_new_ctr_read ( false )
    , m_pTimeSource ( new Util::SystemTimeSource() )
{
    pthread_mutex_init( &m_mutex, 0 );

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
    : m_handle( 0 ), m_resetHandle( 0 ), m_util_handle( 0 )
    , m_port( -1 )
    , m_threadRunning( false )
    , m_realtime ( rt )
    , m_base_priority ( prio )
    , m_pIsoManager( new IsoHandlerManager( *this, rt, prio + IEEE1394SERVICE_ISOMANAGER_PRIO_INCREASE ) )
    , m_pCTRHelper ( new CycleTimerHelper( *this, 1000, rt, prio + IEEE1394SERVICE_CYCLETIMER_HELPER_PRIO_INCREASE ) )
    , m_have_new_ctr_read ( false )
    , m_pTimeSource ( new Util::SystemTimeSource() )
{
    pthread_mutex_init( &m_mutex, 0 );

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
    stopRHThread();
    for ( arm_handler_vec_t::iterator it = m_armHandlers.begin();
          it != m_armHandlers.end();
          ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Unregistering ARM handler for 0x%016llX\n", (*it)->getStart());
        int err=raw1394_arm_unregister(m_resetHandle, (*it)->getStart());
        if (err) {
            debugError(" Failed to unregister ARM handler for 0x%016llX\n", (*it)->getStart());
            debugError(" Error: %s\n", strerror(errno));
        }
    }

    delete m_pTimeSource;
    if ( m_handle ) {
        raw1394_destroy_handle( m_handle );
    }
    if ( m_resetHandle ) {
        raw1394_destroy_handle( m_resetHandle );
    }
    if ( m_util_handle ) {
        raw1394_destroy_handle( m_util_handle );
    }
}

unsigned int
Ieee1394Service::detectNbPorts( )
{
    raw1394handle_t tmp_handle = raw1394_new_handle();
    if ( tmp_handle == NULL ) {
        debugError("Could not get libraw1394 handle.\n");
        return 0;
    }
    struct raw1394_portinfo pinf[IEEE1394SERVICE_MAX_FIREWIRE_PORTS];
    int nb_detected_ports = raw1394_get_port_info(tmp_handle, pinf, IEEE1394SERVICE_MAX_FIREWIRE_PORTS);
    raw1394_destroy_handle(tmp_handle);

    if (nb_detected_ports < 0) {
        debugError("Failed to detect number of ports\n");
        return 0;
    }
    return nb_detected_ports;
}

bool
Ieee1394Service::initialize( int port )
{
    using namespace std;

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

    m_resetHandle = raw1394_new_handle_on_port( port );
    if ( !m_resetHandle ) {
        if ( !errno ) {
            debugFatal("libraw1394 not compatible\n");
        } else {
            debugFatal("Ieee1394Service::initialize: Could not get 1394 handle: %s",
                strerror(errno) );
            debugFatal("Is ieee1394 and raw1394 driver loaded?\n");
        }
        return false;
    }

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
    err=raw1394_read_cycle_timer(m_handle, &cycle_timer, &local_time);
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
        debugOutput(DEBUG_LEVEL_NORMAL, "This system supports the raw1394_read_cycle_timer call, using it.\n");
        m_have_new_ctr_read = true;
    }

    m_port = port;

    // obtain port name
    raw1394handle_t tmp_handle = raw1394_new_handle();
    if ( tmp_handle == NULL ) {
        debugError("Could not get temporaty libraw1394 handle.\n");
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

    raw1394_set_userdata( m_handle, this );
    raw1394_set_userdata( m_resetHandle, this );
    raw1394_set_userdata( m_util_handle, this );
    raw1394_set_bus_reset_handler( m_resetHandle,
                                   this->resetHandlerLowLevel );

    m_default_arm_handler = raw1394_set_arm_tag_handler( m_resetHandle,
                                   this->armHandlerLowLevel );

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

    startRHThread();

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
    m_base_priority = priority;
    m_realtime = rt;
    if (m_pIsoManager) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Switching IsoManager to (rt=%d, prio=%d)\n",
                                         rt, priority + IEEE1394SERVICE_ISOMANAGER_PRIO_INCREASE);
        result &= m_pIsoManager->setThreadParameters(rt, priority + IEEE1394SERVICE_ISOMANAGER_PRIO_INCREASE);
    }
    if (m_pCTRHelper) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Switching CycleTimerHelper to (rt=%d, prio=%d)\n", 
                                         rt, priority + IEEE1394SERVICE_CYCLETIMER_HELPER_PRIO_INCREASE);
        result &= m_pCTRHelper->setThreadParameters(rt, priority + IEEE1394SERVICE_CYCLETIMER_HELPER_PRIO_INCREASE);
    }
    return result;
}

int
Ieee1394Service::getNodeCount()
{
    return raw1394_get_nodecount( m_handle );
}

nodeid_t Ieee1394Service::getLocalNodeId() {
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
            *cycle_timer = ntohl(*cycle_timer);
            return true;
        } else {
            return false;
        }
    }
}

uint64_t
Ieee1394Service::getCurrentTimeAsUsecs() {
    if(m_pTimeSource) {
        return m_pTimeSource->getCurrentTimeAsUsecs();
    } else {
        debugError("No timesource!\n");
        return 0;
    }
}

bool
Ieee1394Service::read( fb_nodeid_t nodeId,
                       fb_nodeaddr_t addr,
                       size_t length,
                       fb_quadlet_t* buffer )
{
    using namespace std;
    if ( raw1394_read( m_handle, nodeId, addr, length*4, buffer ) == 0 ) {

        #ifdef DEBUG
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
            "read: node 0x%X, addr = 0x%016llX, length = %u\n",
            nodeId, addr, length);
        printBuffer( DEBUG_LEVEL_VERY_VERBOSE, length, buffer );
        #endif

        return true;
    } else {
        #ifdef DEBUG
        debugError("raw1394_read failed: node 0x%X, addr = 0x%016llX, length = %u\n",
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
    using namespace std;

    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"write: node 0x%X, addr = 0x%016X, length = %d\n",
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

fb_octlet_t
Ieee1394Service::byteSwap_octlet(fb_octlet_t value) {
    #if __BYTE_ORDER == __BIG_ENDIAN
        return value;
    #elif __BYTE_ORDER == __LITTLE_ENDIAN
        fb_octlet_t value_new;
        fb_quadlet_t *in_ptr=reinterpret_cast<fb_quadlet_t *>(&value);
        fb_quadlet_t *out_ptr=reinterpret_cast<fb_quadlet_t *>(&value_new);
        *(out_ptr+1)=htonl(*(in_ptr));
        *(out_ptr)=htonl(*(in_ptr+1));
        return value_new;
    #else
        #error Unknown endiannes
    #endif
}

bool
Ieee1394Service::lockCompareSwap64(  fb_nodeid_t nodeId,
                        fb_nodeaddr_t addr,
                        fb_octlet_t  compare_value,
                        fb_octlet_t  swap_value,
                        fb_octlet_t* result )
{
    #ifdef DEBUG
    debugOutput(DEBUG_LEVEL_VERBOSE,"lockCompareSwap64: node 0x%X, addr = 0x%016llX\n",
                nodeId, addr);
    debugOutput(DEBUG_LEVEL_VERBOSE,"  if (*(addr)==0x%016llX) *(addr)=0x%016llX\n",
                compare_value, swap_value);
    fb_octlet_t buffer;
    if(!read_octlet( nodeId, addr,&buffer )) {
        debugWarning("Could not read register\n");
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,"before = 0x%016llX\n", buffer);
    }

    #endif

    // do endiannes swapping
    compare_value=byteSwap_octlet(compare_value);
    swap_value=byteSwap_octlet(swap_value);

    int retval=raw1394_lock64(m_handle, nodeId, addr, RAW1394_EXTCODE_COMPARE_SWAP,
                          swap_value, compare_value, result);

    #ifdef DEBUG
    if(!read_octlet( nodeId, addr,&buffer )) {
        debugWarning("Could not read register\n");
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE,"after = 0x%016llX\n", buffer);
    }
    #endif

    *result=byteSwap_octlet(*result);

    return (retval == 0);
}

fb_quadlet_t*
Ieee1394Service::transactionBlock( fb_nodeid_t nodeId,
                                   fb_quadlet_t* buf,
                                   int len,
                                   unsigned int* resp_len )
{
    for (int i = 0; i < len; ++i) {
        buf[i] = ntohl( buf[i] );
    }

    fb_quadlet_t* result =
        avc1394_transaction_block2( m_handle,
                                    nodeId,
                                    buf,
                                    len,
                                    resp_len,
                                    10 );

    for ( unsigned int i = 0; i < *resp_len; ++i ) {
        result[i] = htonl( result[i] );
    }

    return result;
}


bool
Ieee1394Service::transactionBlockClose()
{
    avc1394_transaction_block_close( m_handle );
    return true;
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
    Ieee1394Service* instance
        = (Ieee1394Service*) raw1394_get_userdata( handle );
    instance->resetHandler( generation );

    return 0;
}

bool
Ieee1394Service::resetHandler( unsigned int generation )
{
    quadlet_t buf=0;

    // do a simple read on ourself in order to update the internal structures
    // this avoids failures after a bus reset
    read_quadlet( getLocalNodeId() | 0xFFC0,
                  CSR_REGISTER_BASE | CSR_CYCLE_TIME,
                  &buf );

    for ( reset_handler_vec_t::iterator it = m_busResetHandlers.begin();
          it != m_busResetHandlers.end();
          ++it )
    {
        Functor* func = *it;
        ( *func )();
    }

    return true;
}

bool Ieee1394Service::registerARMHandler(ARMHandler *h) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Registering ARM handler (%p) for 0x%016llX, length %u\n",
        h, h->getStart(), h->getLength());

    int err=raw1394_arm_register(m_resetHandle, h->getStart(),
                         h->getLength(), h->getBuffer(), (octlet_t)h,
                         h->getAccessRights(),
                         h->getNotificationOptions(),
                         h->getClientTransactions());
    if (err) {
        debugError("Failed to register ARM handler for 0x%016llX\n", h->getStart());
        debugError(" Error: %s\n", strerror(errno));
        return false;
    }

    m_armHandlers.push_back( h );

    return true;
}

bool Ieee1394Service::unregisterARMHandler( ARMHandler *h ) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Unregistering ARM handler (%p) for 0x%016llX\n",
        h, h->getStart());

    for ( arm_handler_vec_t::iterator it = m_armHandlers.begin();
          it != m_armHandlers.end();
          ++it )
    {
        if((*it) == h) {
            int err=raw1394_arm_unregister(m_resetHandle, h->getStart());
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
    debugOutput(DEBUG_LEVEL_VERBOSE, "Finding free ARM block of %d bytes, from 0x%016llX in steps of %d bytes\n",
        length, start, step);

    int cnt=0;
    const int maxcnt=10;
    int err=1;
    while(err && cnt++ < maxcnt) {
        // try to register
        err=raw1394_arm_register(m_resetHandle, start, length, 0, 0, 0, 0, 0);

        if (err) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " -> cannot use 0x%016llX\n", start);
            debugError("    Error: %s\n", strerror(errno));
            start += step;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, " -> use 0x%016llX\n", start);
            err=raw1394_arm_unregister(m_resetHandle, start);
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
    Ieee1394Service* instance
        = (Ieee1394Service*) raw1394_get_userdata( handle );
    instance->armHandler( arm_tag, request_type, requested_length, data );

    return 0;
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
            arm_req_resp  = (struct raw1394_arm_request_response *) data;
            raw1394_arm_request_t arm_req=arm_req_resp->request;
            raw1394_arm_response_t arm_resp=arm_req_resp->response;

            debugOutput(DEBUG_LEVEL_VERBOSE,"ARM handler for address 0x%016llX called\n",
                (*it)->getStart());
            debugOutput(DEBUG_LEVEL_VERBOSE," request type   : 0x%02X\n",request_type);
            debugOutput(DEBUG_LEVEL_VERBOSE," request length : %04d\n",requested_length);

            switch(request_type) {
                case RAW1394_ARM_READ:
                    (*it)->handleRead(arm_req);
                    *arm_resp=*((*it)->getResponse());
                    break;
                case RAW1394_ARM_WRITE:
                    (*it)->handleWrite(arm_req);
                    *arm_resp=*((*it)->getResponse());
                    break;
                case RAW1394_ARM_LOCK:
                    (*it)->handleLock(arm_req);
                    *arm_resp=*((*it)->getResponse());
                    break;
                default:
                    debugWarning("Unknown request type received, ignoring...\n");
            }

            return true;
        }
    }

    debugOutput(DEBUG_LEVEL_VERBOSE,"default ARM handler called\n");

    m_default_arm_handler(m_resetHandle, arm_tag, request_type, requested_length, data );
    return true;
}

bool
Ieee1394Service::startRHThread()
{
    int i;

    if ( m_threadRunning ) {
        return true;
    }
    pthread_mutex_lock( &m_mutex );
    i = pthread_create( &m_thread, 0, rHThread, this );
    pthread_mutex_unlock( &m_mutex );
    if (i) {
        debugFatal("Could not start ieee1394 service thread\n");
        return false;
    }
    m_threadRunning = true;

    return true;
}

void
Ieee1394Service::stopRHThread()
{
    if ( m_threadRunning ) {
        pthread_mutex_lock (&m_mutex);
        pthread_cancel (m_thread);
        pthread_join (m_thread, 0);
        pthread_mutex_unlock (&m_mutex);
        m_threadRunning = false;
    }
}

void*
Ieee1394Service::rHThread( void* arg )
{
    Ieee1394Service* pIeee1394Service = (Ieee1394Service*) arg;

    while (true) {
        raw1394_loop_iterate (pIeee1394Service->m_resetHandle);
        pthread_testcancel ();
    }

    return 0;
}

bool
Ieee1394Service::addBusResetHandler( Functor* functor )
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "Adding busreset handler (%p)\n", functor);
    m_busResetHandlers.push_back( functor );
    return true;
}

bool
Ieee1394Service::remBusResetHandler( Functor* functor )
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

    debugOutput(DEBUG_LEVEL_VERBOSE, "Allocating ISO channel using IEC61883 CMP...\n" );

    struct ChannelInfo cinfo;

    int c = -1;
    int bandwidth=1;

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
    signed int result = raw1394_read (m_handle, raw1394_get_irm_id (m_handle),
        CSR_REGISTER_BASE + CSR_BANDWIDTH_AVAILABLE,
        sizeof (quadlet_t), &buffer);

    if (result < 0)
        return -1;
    return ntohl(buffer);
}

void
Ieee1394Service::setVerboseLevel(int l)
{
    if (m_pIsoManager) m_pIsoManager->setVerboseLevel(l);
    if (m_pCTRHelper) m_pCTRHelper->setVerboseLevel(l);
    setDebugLevel(l);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting verbose level to %d...\n", l );
}

void
Ieee1394Service::show()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Port:  %d\n", getPort() );
    debugOutput( DEBUG_LEVEL_VERBOSE, " Name: %s\n", getPortName().c_str() );
    debugOutputShort( DEBUG_LEVEL_NORMAL, "Iso handler info:\n");
    if (m_pIsoManager) m_pIsoManager->dumpInfo();
}
