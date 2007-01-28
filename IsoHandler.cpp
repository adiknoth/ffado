/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

#include "IsoHandler.h"
#include "IsoStream.h"
#include "cyclecounter.h"

#include "libutil/Time.h"
#include "libutil/TimeSource.h"
#include "libutil/SystemTimeSource.h"

#include <errno.h>
#include <netinet/in.h>
#include <assert.h>
#include <unistd.h>

#include <iostream>
using namespace std;


#define CC_SLEEP_TIME_AFTER_UPDATE     100
#define CC_SLEEP_TIME_AFTER_FAILURE     10
#define CC_DLL_COEFF     ((0.01)*((float)(CC_SLEEP_TIME_AFTER_UPDATE/1000.0)))

#define CC_MAX_RATE_ERROR           (2/100.0)
#define CC_INIT_MAX_TRIES 10


namespace FreebobStreaming
{

IMPL_DEBUG_MODULE( IsoHandler, IsoHandler, DEBUG_LEVEL_NORMAL );

/* the C callbacks */
enum raw1394_iso_disposition 
IsoXmitHandler::iso_transmit_handler(raw1394handle_t handle,
		unsigned char *data, unsigned int *length,
		unsigned char *tag, unsigned char *sy,
		int cycle, unsigned int dropped) {

	IsoXmitHandler *xmitHandler=static_cast<IsoXmitHandler *>(raw1394_get_userdata(handle));
	assert(xmitHandler);

	return xmitHandler->getPacket(data, length, tag, sy, cycle, dropped);
}

enum raw1394_iso_disposition 
IsoRecvHandler::iso_receive_handler(raw1394handle_t handle, unsigned char *data, 
						unsigned int length, unsigned char channel,
						unsigned char tag, unsigned char sy, unsigned int cycle, 
						unsigned int dropped) {

	IsoRecvHandler *recvHandler=static_cast<IsoRecvHandler *>(raw1394_get_userdata(handle));
	assert(recvHandler);

	return recvHandler->putPacket(data, length, channel, tag, sy, cycle, dropped);
}

int IsoHandler::busreset_handler(raw1394handle_t handle, unsigned int generation)
{	
	debugOutput( DEBUG_LEVEL_VERBOSE, "Busreset happened, generation %d...\n", generation);

	IsoHandler *handler=static_cast<IsoHandler *>(raw1394_get_userdata(handle));
	assert(handler);
	return handler->handleBusReset(generation);
}


/* Base class implementation */
IsoHandler::IsoHandler(int port)
   : TimeSource(), m_handle(0), m_handle_util(0), m_port(port), 
   m_buf_packets(400), m_max_packet_size(1024), m_irq_interval(-1),
   m_cyclecounter_ticks(0), m_lastmeas_usecs(0), m_ticks_per_usec(24.576), 
   m_ticks_per_usec_dll_err2(0),
   m_packetcount(0), m_dropped(0), m_Client(0)
{
    InitTime();
    m_TimeSource=new FreebobUtil::SystemTimeSource();
}

IsoHandler::IsoHandler(int port, unsigned int buf_packets, unsigned int max_packet_size, int irq)
   : TimeSource(), m_handle(0), m_port(port), 
   m_buf_packets(buf_packets), m_max_packet_size( max_packet_size), 
   m_irq_interval(irq), 
   m_cyclecounter_ticks(0), m_lastmeas_usecs(0), m_ticks_per_usec(24.576),
   m_ticks_per_usec_dll_err2(0),
   m_packetcount(0), m_dropped(0), m_Client(0)
{
    InitTime();
    m_TimeSource=new FreebobUtil::SystemTimeSource();
}

IsoHandler::~IsoHandler() {
    if(m_handle) {
            stop();
        raw1394_destroy_handle(m_handle);
    }
    if(m_handle_util) raw1394_destroy_handle(m_handle_util);
    
    delete m_TimeSource;
}

bool
IsoHandler::init()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "IsoHandler (%p) enter...\n",this);

    m_handle = raw1394_new_handle_on_port( m_port );
    if ( !m_handle ) {
        if ( !errno ) {
			cerr << "libraw1394 not compatible" << endl;
        } else {
			perror( "IsoHandler::Initialize: Could not get 1394 handle" );
			cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
        }
        return false;
    }
    raw1394_set_userdata(m_handle, static_cast<void *>(this));
    
    // a second handle for utility stuff
    m_handle_util = raw1394_new_handle_on_port( m_port );
    if ( !m_handle_util ) {
        if ( !errno ) {
			cerr << "libraw1394 not compatible" << endl;
        } else {
			perror( "IsoHandler::Initialize: Could not get 1394 handle" );
			cerr << "Is ieee1394 and raw1394 driver loaded?" << endl;
        }
        return false;
    }
	
    raw1394_set_userdata(m_handle_util, static_cast<void *>(this));
	
    if(raw1394_busreset_notify (m_handle, RAW1394_NOTIFY_ON)) {
        debugWarning("Could not enable busreset notification.\n");
        debugWarning(" Error message: %s\n",strerror(errno));
	}
	
        raw1394_set_bus_reset_handler(m_handle, busreset_handler);

    // initialize the local timesource
    m_TimeSource_NbCycleWraps=0;
    quadlet_t buf=0;
    unsigned int new_counter;
    
    raw1394_read(m_handle_util, raw1394_get_local_id(m_handle_util), 
        CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);

    new_counter= ntohl(buf) & 0xFFFFFFFF;
    m_TimeSource_LastSecs=CYCLE_COUNTER_GET_SECS(new_counter);

    // update the cycle counter value for initial value
    initCycleCounter();
    
    return true;
}

bool
IsoHandler::setSyncMaster(FreebobUtil::TimeSource *t)
{
    m_TimeSource=t;

    // update the cycle counter value for initial value
    initCycleCounter();

    return true;
}

bool IsoHandler::stop()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
    raw1394_iso_stop(m_handle);
    return true;
}

/**
 * Bus reset handler
 *
 * @return ?
 */
 
int IsoHandler::handleBusReset(unsigned int generation) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "bus reset...\n");
    
    // as busreset can elect a new cycle master,
    // we need to re-initialize our timing code
    initCycleCounter();
    
    return 0;
}

/**
 * Returns the current value of the cycle counter (in ticks)
 *
 * @return the current value of the cycle counter (in ticks)
 */

unsigned int IsoHandler::getCycleCounter() {
    // calculate the cycle counter based upon the current time
    // and the estimated tick rate
    freebob_microsecs_t now=m_TimeSource->getCurrentTimeAsUsecs();

    // linear interpolation
    int delta_usecs=now-m_lastmeas_usecs;

    float offset=m_ticks_per_usec * ((float)delta_usecs);

    unsigned int pred_ticks=m_cyclecounter_ticks+(unsigned int)offset;

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Get CC: d_usecs=%d, offset=%f, cc_ticks=%lu, pred_ticks=%lu\n",
      delta_usecs, offset, m_cyclecounter_ticks,pred_ticks
      );

    // if we need to wrap, do it
    if (pred_ticks > TICKS_PER_SECOND * 128) {
        pred_ticks -= TICKS_PER_SECOND * 128;
    }
    
    return pred_ticks;
}

bool IsoHandler::updateCycleCounter() {
    quadlet_t buf=0;

    freebob_microsecs_t prev_usecs=m_lastmeas_usecs;
    unsigned int prev_ticks=m_cyclecounter_ticks;
    
    freebob_microsecs_t new_usecs;
    unsigned int new_ticks;
    unsigned int new_counter;
    
    /* To estimate the cycle counter, we implement a 
       DLL based routine, that maps the cycle counter
       on the system clock.
       
       For more info, refer to:
        "Using a DLL to filter time"
        Fons Adriaensen
        
        Can be found at:
        http://users.skynet.be/solaris/linuxaudio/downloads/usingdll.pdf
        or maybe at:
        http://www.kokkinizita.net/linuxaudio
    
        Basically what we do is estimate the next point (T1,CC1_est)
        based upon the previous point (T0, CC0) and the estimated rate (R).
        Then we compare our estimation with the measured cycle counter
        at T1 (=CC1_meas). We then calculate the estimation error on R:
        err=(CC1_meas-CC0)/(T1-T2) - (CC1_est-CC0)/(T1-T2)
        and try to minimize this on average (DLL)
        
        Note that in order to have a contignous mapping, we should
        update CC0<=CC1_est instead of CC0<=CC1_meas. The measurement 
        serves only to correct the error 'on average'.
        
        In the code, the following variable names are used:
        T0=prev_usecs
        T1=next_usecs
        
        CC0=prev_ticks
        CC1_est=est_ticks
        CC1_meas=meas_ticks
        
     */
    
    // normally we should be able to use the same handle
    // because it is not iterated on by any other stuff
    // but I'm not sure
    raw1394_read(m_handle_util, raw1394_get_local_id(m_handle_util), 
        CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);
    new_usecs=m_TimeSource->getCurrentTimeAsUsecs();

    new_counter= ntohl(buf) & 0xFFFFFFFF;
    new_ticks=CYCLE_COUNTER_TO_TICKS(new_counter);

    // the difference in system time
    int delta_usecs=new_usecs-prev_usecs;
    // this cannot be 0, because m_TimeSource->getCurrentTimeAsUsecs should 
    // never return the same value (maybe in future terrahz processors?)
    assert(delta_usecs);
    
    // the measured cycle counter difference
    unsigned int delta_ticks_meas;
    if (new_ticks > prev_ticks) {
        delta_ticks_meas=new_ticks - prev_ticks;
    } else { // wraparound
        delta_ticks_meas=CYCLE_COUNTER_UNWRAP_TICKS(new_ticks) - prev_ticks;
    }
    
    // the estimated cycle counter difference
    unsigned int delta_ticks_est=(unsigned int)(m_ticks_per_usec * ((float)delta_usecs));
    
    // the measured & estimated rate
    float rate_meas=((float)delta_ticks_meas/(float)delta_usecs);
    float rate_est=((float)m_ticks_per_usec);
    
    // these make sure we don't update when the measurement is
    // bad. We know the nominal rate, and it can't be that far
    // off. The thing is that there is a problem in measuring
    // both usecs and ticks at the same time (no provision in
    // the kernel.
    // We know that there are some tolerances on both
    // the system clock and the firewire clock such that the 
    // actual difference is rather small. So we discard values 
    // that are too far from the nominal rate. 
    // Otherwise the DLL has to have a very low bandwidth, in 
    // order not to be desturbed too much by these bad measurements
    // resulting in very slow locking.
    
    if (   (rate_meas < 24.576*(1.0+CC_MAX_RATE_ERROR)) 
        && (rate_meas > 24.576*(1.0-CC_MAX_RATE_ERROR))) {

#ifdef DEBUG

        int diff=(int)delta_ticks_est;
        
        // calculate the difference in predicted ticks and
        // measured ticks
        diff -= delta_ticks_meas;
        
        
        if (diff > 24000 || diff < -24000) { // approx +/-1 msec error
            debugOutput(DEBUG_LEVEL_VERBOSE,"Bad pred (%p): diff=%d, dt_est=%u, dt_meas=%u, d=%dus, err=%fus\n", this,
                diff, delta_ticks_est, delta_ticks_meas, delta_usecs, (((float)diff)/24.576)
                );
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Good pred: diff=%d, dt_est=%u, dt_meas=%u, d=%dus, err=%fus\n",
                diff, delta_ticks_est, delta_ticks_meas, delta_usecs, (((float)diff)/24.576)
                );
        }
#endif
        // DLL the error to obtain the rate.
        // (note: the DLL makes the error=0)
        // only update the DLL if the rate is within 10% of the expected
        // rate
        float err=rate_meas-rate_est;
        
        // 2nd order DLL update
//         const float w=6.28*0.0001;
//         const float b=w*1.45;
//         const float c=w*w;
//         
//         m_ticks_per_usec += b*err + m_ticks_per_usec_dll_err2;
//         m_ticks_per_usec_dll_err2 += c * err;

        // first order DLL update
         m_ticks_per_usec += CC_DLL_COEFF*err;
    
        if (   (m_ticks_per_usec > 24.576*(1.0+CC_MAX_RATE_ERROR)) 
            || (m_ticks_per_usec < 24.576*(1.0-CC_MAX_RATE_ERROR))) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Warning: DLL ticks/usec near clipping (%8.4f)\n",
                        m_ticks_per_usec);
        }
        
        // update the internal values
        // note: the next cyclecounter point is
        //       the estimated one, not the measured one!
        m_cyclecounter_ticks += delta_ticks_est;
        // if we need to wrap, do it
        if (m_cyclecounter_ticks > TICKS_PER_SECOND * 128) {
            m_cyclecounter_ticks -= TICKS_PER_SECOND * 128;
        }

        m_lastmeas_usecs = new_usecs;

        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"U TS: %10u -> %10u, d=%7uus, dt_est=%7u,  dt_meas=%7u, erate=%6.4f, mrate=%6f\n",
              prev_ticks, m_cyclecounter_ticks, delta_usecs,
              delta_ticks_est, delta_ticks_meas, m_ticks_per_usec, rate_meas
              );

        // the estimate is good
        return true;
    } else {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"U TS: Not updating, rate out of range (%6.4f)\n",
              rate_meas
              );
        return false;

    }
}

void IsoHandler::initCycleCounter() {
    quadlet_t buf=0;

    freebob_microsecs_t prev_usecs;
    unsigned int prev_ticks;
    unsigned int prev_counter;
    
    freebob_microsecs_t new_usecs;
    unsigned int new_ticks;
    unsigned int new_counter;
    
    float rate=0.0;
    
    unsigned int try_cnt=0;
    
    // make sure that we start with a decent rate,
    // meaning that we want two successive (usecs,ticks)
    // points that make sense.
    
    while ( (try_cnt++ < CC_INIT_MAX_TRIES) &&
           (   (rate > 24.576*(1.0+CC_MAX_RATE_ERROR)) 
           || (rate < 24.576*(1.0-CC_MAX_RATE_ERROR)))) {
           
        // normally we should be able to use the same handle
        // because it is not iterated on by any other stuff
        // but I'm not sure
        raw1394_read(m_handle_util, raw1394_get_local_id(m_handle_util), 
            CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);
        prev_usecs=m_TimeSource->getCurrentTimeAsUsecs();
        
        prev_counter= ntohl(buf) & 0xFFFFFFFF;
        prev_ticks=CYCLE_COUNTER_TO_TICKS(prev_counter);
        
        usleep(CC_SLEEP_TIME_AFTER_UPDATE);
        
        // normally we should be able to use the same handle
        // because it is not iterated on by any other stuff
        // but I'm not sure
        raw1394_read(m_handle_util, raw1394_get_local_id(m_handle_util), 
            CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);
        new_usecs=m_TimeSource->getCurrentTimeAsUsecs();

        new_counter= ntohl(buf) & 0xFFFFFFFF;
        new_ticks=CYCLE_COUNTER_TO_TICKS(new_counter);
        
        unsigned int delta_ticks;
        
        if (new_ticks > prev_ticks) {
            delta_ticks=new_ticks - prev_ticks;
        } else { // wraparound
            delta_ticks=CYCLE_COUNTER_UNWRAP_TICKS(new_ticks) - prev_ticks;
        }
        
        int delta_usecs=new_usecs-prev_usecs;
        
        // this cannot be 0, because m_TimeSource->getCurrentTimeAsUsecs should 
        // never return the same value (maybe in future terrahz processors?)
        assert(delta_usecs);
        
        rate=((float)delta_ticks/(float)delta_usecs);
        
        // update the internal values
        m_cyclecounter_ticks=new_ticks;
        m_lastmeas_usecs=new_usecs;
        
        debugOutput(DEBUG_LEVEL_VERBOSE,"Try %d: rate=%6.4f\n",
            try_cnt,rate
            );

    }
    
    // this is not fatal, the DLL will eventually correct this
    if(try_cnt == CC_INIT_MAX_TRIES) {
        debugWarning("Failed to properly initialize cycle counter...\n");
    }
    
    // initialize this to the nominal value
    m_ticks_per_usec = 24.576;
    m_ticks_per_usec_dll_err2 = 0;
    
}

void IsoHandler::dumpInfo()
{

    int channel=-1;
    if (m_Client) channel=m_Client->getChannel();

    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Handler type    : %s\n",
            (this->getType()==EHT_Receive ? "Receive" : "Transmit"));
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Port, Channel  : %2d, %2d\n",
            m_port, channel);
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Packet count   : %10d (%5d dropped)\n",
            this->getPacketCount(), this->getDroppedCount());
    #ifdef DEBUG
    unsigned int cc=this->getCycleCounter();
	debugOutputShort( DEBUG_LEVEL_NORMAL, "  Cycle counter  : %10lu (%03us, %04ucycles, %04uticks)\n",
            cc,TICKS_TO_SECS(cc),TICKS_TO_CYCLES(cc),TICKS_TO_OFFSET(cc));
    #endif
    debugOutputShort( DEBUG_LEVEL_NORMAL, "  Ticks/usec     : %8.6f (dll2: %8.6e)\n\n",
            this->getTicksPerUsec(), m_ticks_per_usec_dll_err2);

};

void IsoHandler::setVerboseLevel(int l)
{
    setDebugLevel(l);
}

bool IsoHandler::registerStream(IsoStream *stream)
{
    assert(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, "registering stream (%p)\n", stream);

    if (m_Client) {
            debugFatal( "Generic IsoHandlers can have only one client\n");	
            return false;
    }

    m_Client=stream;

    m_Client->setHandler(this);

    return true;

}

bool IsoHandler::unregisterStream(IsoStream *stream)
{
    assert(stream);
    debugOutput( DEBUG_LEVEL_VERBOSE, "unregistering stream (%p)\n", stream);

    if(stream != m_Client) {
            debugFatal( "no client registered\n");	
            return false;
    }

    m_Client->clearHandler();
    
    m_Client=0;
    return true;

}

/* The timesource interface */
freebob_microsecs_t IsoHandler::getCurrentTime() {
    quadlet_t buf=0;
    unsigned int new_counter;
    
    raw1394_read(m_handle_util, raw1394_get_local_id(m_handle_util), 
        CSR_REGISTER_BASE | CSR_CYCLE_TIME, 4, &buf);
    
    new_counter= ntohl(buf) & 0xFFFFFFFF;
        
    // this assumes that it never happens that there are more than 2
    // minutes between calls
    if (CYCLE_COUNTER_GET_SECS(new_counter) < m_TimeSource_LastSecs) {
        m_TimeSource_NbCycleWraps++;
    }
    
    freebob_microsecs_t ticks=m_TimeSource_NbCycleWraps * 128 * TICKS_PER_SECOND
            + CYCLE_COUNTER_TO_TICKS(new_counter);
    
    m_TimeSource_LastSecs=CYCLE_COUNTER_GET_SECS(new_counter);
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Wraps=%4u, LastSecs=%3u, nowSecs=%3u, ticks=%10u\n",
              m_TimeSource_NbCycleWraps, m_TimeSource_LastSecs,
              CYCLE_COUNTER_GET_SECS(new_counter), ticks
              );
              
    return  ticks;
}

freebob_microsecs_t IsoHandler::getCurrentTimeAsUsecs() {
    float tmp=getCurrentTime();
    float tmp2 = tmp * USECS_PER_TICK;
    freebob_microsecs_t retval=(freebob_microsecs_t)tmp2;
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"tmp=%f, tmp2=%f, retval=%u\n",
              tmp, tmp2,retval
              );
    
    return retval;
}



/* Child class implementations */

IsoRecvHandler::IsoRecvHandler(int port)
                : IsoHandler(port)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
}
IsoRecvHandler::IsoRecvHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
                : IsoHandler(port, buf_packets,max_packet_size,irq)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}
IsoRecvHandler::~IsoRecvHandler()
{
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() will do any iso system shutdown required.
//	raw1394_iso_shutdown(m_handle);
	raw1394_destroy_handle(m_handle);
	m_handle = NULL;
}

bool
IsoRecvHandler::init() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "init recv handler %p\n",this);

    if(!(IsoHandler::init())) {
        return false;
    }
    return true;

}

enum raw1394_iso_disposition IsoRecvHandler::putPacket(unsigned char *data, unsigned int length, 
                    unsigned char channel, unsigned char tag, unsigned char sy, 
                    unsigned int cycle, unsigned int dropped) {

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
                 "received packet: length=%d, channel=%d, cycle=%d\n",
                 length, channel, cycle );
    m_packetcount++;
    m_dropped+=dropped;

    if(m_Client) {
        return m_Client->putPacket(data, length, channel, tag, sy, cycle, dropped);
    }
    
    return RAW1394_ISO_OK;
}

bool IsoRecvHandler::prepare()
{
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.
//	raw1394_iso_shutdown(m_handle);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso receive handler (%p)\n",this);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Buffers         : %d \n",m_buf_packets);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Max Packet size : %d \n",m_max_packet_size);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Channel         : %d \n",m_Client->getChannel());
    debugOutput( DEBUG_LEVEL_VERBOSE, " Irq interval    : %d \n",m_irq_interval);

	if(raw1394_iso_recv_init(m_handle,   iso_receive_handler,
                             m_buf_packets,
                             m_max_packet_size,
                             m_Client->getChannel(),
                             RAW1394_DMA_BUFFERFILL,
                             m_irq_interval)) {
        debugFatal("Could not do receive initialisation!\n" );
        debugFatal("  %s\n",strerror(errno));

        return false;
    }
    return true;
}

bool IsoRecvHandler::start(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);
    
    if(raw1394_iso_recv_start(m_handle, cycle, -1, 0)) {
        debugFatal("Could not start receive handler (%s)\n",strerror(errno));
        return false;
    }
    return true;
}

int IsoRecvHandler::handleBusReset(unsigned int generation) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "handle bus reset...\n");
    
    //TODO: implement busreset
    
    // pass on the busreset signal
    if(IsoHandler::handleBusReset(generation)) {
        return -1;
    }
    return 0;
}

/* ----------------- XMIT --------------- */

IsoXmitHandler::IsoXmitHandler(int port)
                : IsoHandler(port), m_prebuffers(0)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "IsoXmitHandler enter...\n");

}
IsoXmitHandler::IsoXmitHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
                : IsoHandler(port, buf_packets, max_packet_size,irq),
                  m_speed(RAW1394_ISO_SPEED_400), m_prebuffers(0)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "IsoXmitHandler enter...\n");

}
IsoXmitHandler::IsoXmitHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq,
                               enum raw1394_iso_speed speed)
                : IsoHandler(port, buf_packets,max_packet_size,irq),
                  m_speed(speed), m_prebuffers(0)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "IsoXmitHandler enter...\n");

}

IsoXmitHandler::~IsoXmitHandler()
{
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() will do any iso system shutdown required.
//	raw1394_iso_shutdown(m_handle);
	raw1394_destroy_handle(m_handle);
	m_handle = NULL;
}

bool
IsoXmitHandler::init() {

    debugOutput( DEBUG_LEVEL_VERBOSE, "init xmit handler %p\n",this);

    if(!(IsoHandler::init())) {
        return false;
    }

    return true;

}

enum raw1394_iso_disposition IsoXmitHandler::getPacket(unsigned char *data, unsigned int *length,
		              unsigned char *tag, unsigned char *sy,
		              int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "sending packet: length=%d, cycle=%d\n",
	             *length, cycle );
	m_packetcount++;
	m_dropped+=dropped;

	if(m_Client) {
    	return m_Client->getPacket(data, length, tag, sy, cycle, dropped, m_max_packet_size);
	}
	
	return RAW1394_ISO_OK;
}

bool IsoXmitHandler::prepare()
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing iso transmit handler (%p, client=%p)\n",this,m_Client);

// 	raw1394_iso_shutdown(m_handle);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Buffers         : %d \n",m_buf_packets);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Max Packet size : %d \n",m_max_packet_size);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Channel         : %d \n",m_Client->getChannel());
    debugOutput( DEBUG_LEVEL_VERBOSE, " Speed           : %d \n",m_speed);
    debugOutput( DEBUG_LEVEL_VERBOSE, " Irq interval    : %d \n",m_irq_interval);

    if(raw1394_iso_xmit_init(m_handle,
                             iso_transmit_handler,
                             m_buf_packets,
                             m_max_packet_size,
                             m_Client->getChannel(),
                             m_speed,
                             m_irq_interval)) {
        debugFatal("Could not do xmit initialisation!\n" );

        return false;
    }

    return true;
}

bool IsoXmitHandler::start(int cycle)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "start on cycle %d\n", cycle);
    if(raw1394_iso_xmit_start(m_handle, cycle, m_prebuffers)) {
        debugFatal("Could not start xmit handler (%s)\n",strerror(errno));
        return false;
    }
    return true;
}

int IsoXmitHandler::handleBusReset(unsigned int generation) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "bus reset...\n");
    //TODO: implement busreset
    
    // pass on the busreset signal
    if(IsoHandler::handleBusReset(generation)) {
            return -1;
    }
    
    return 0;
}

}

/* multichannel receive  */
#if 0
IsoRecvHandler::IsoRecvHandler(int port)
		: IsoHandler(port)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
}
IsoRecvHandler::IsoRecvHandler(int port, unsigned int buf_packets, 
                               unsigned int max_packet_size, int irq)
		: IsoHandler(port, buf_packets,max_packet_size,irq)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

}
IsoRecvHandler::~IsoRecvHandler()
{
// Don't call until libraw1394's raw1394_new_handle() function has been
// fixed to correctly initialise the iso_packet_infos field.  Bug is
// confirmed present in libraw1394 1.2.1.  In any case,
// raw1394_destroy_handle() (in the base class destructor) will do any iso
// system shutdown required.
	raw1394_iso_shutdown(m_handle);

}

bool
IsoRecvHandler::initialize() {
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	IsoHandler *base=static_cast<IsoHandler *>(this);

	if(!(base->initialize())) {
		return false;
	}

	raw1394_set_userdata(m_handle, static_cast<void *>(this));

	if(raw1394_iso_multichannel_recv_init(m_handle,
                                         iso_receive_handler,
                                         m_buf_packets,
                                         m_max_packet_size,
                                         m_irq_interval)) {
		debugFatal("Could not do multichannel receive initialisation!\n" );

		return false;
	}

	return true;

}

enum raw1394_iso_disposition IsoRecvHandler::putPacket(unsigned char *data, unsigned int length, 
		              unsigned char channel, unsigned char tag, unsigned char sy, 
			          unsigned int cycle, unsigned int dropped) {

	debugOutput( DEBUG_LEVEL_VERY_VERBOSE,
	             "received packet: length=%d, channel=%d, cycle=%d\n",
	             length, channel, cycle );
	
	return RAW1394_ISO_OK;
}

// an recv handler can have multiple destination IsoStreams
// NOTE: this implementation even allows for already registered
// streams to be registered again.
int IsoRecvHandler::registerStream(IsoRecvStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	m_Clients.push_back(stream);

	listen(stream->getChannel());
	return 0;

}

int IsoRecvHandler::unregisterStream(IsoRecvStream *stream)
{
	assert(stream);
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

    for ( IsoRecvStreamVectorIterator it = m_Clients.begin();
          it != m_Clients.end();
          ++it )
    {
        IsoRecvStream* s = *it;
        if ( s == stream ) { 
			unListen(s->getChannel());
            m_Clients.erase(it);
			return 0;
        }
    }

	return -1; //not found

}

void IsoRecvHandler::listen(int channel) {
	int retval;
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	retval=raw1394_iso_recv_listen_channel(m_handle, channel);

}

void IsoRecvHandler::unListen(int channel) {
	int retval;
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");

	retval=raw1394_iso_recv_unlisten_channel(m_handle, channel);

}

int IsoRecvHandler::start(int cycle)
{
	debugOutput( DEBUG_LEVEL_VERBOSE, "enter...\n");
	return raw1394_iso_recv_start(m_handle, cycle, -1, 0);
}
#endif
