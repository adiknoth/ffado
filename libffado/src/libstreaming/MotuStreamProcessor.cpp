/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *   Copyright (C) 2006 Jonathan Woithe <jwoithe@physics.adelaide.edu.au>
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
#include "MotuStreamProcessor.h"
#include "Port.h"
#include "MotuPort.h"

#include <math.h>

#include <netinet/in.h>

#include "cycletimer.h"

// in ticks
#define TRANSMIT_TRANSFER_DELAY 6000U
// the number of cycles to send a packet in advance of it's timestamp
#define TRANSMIT_ADVANCE_CYCLES 1U

namespace Streaming {

IMPL_DEBUG_MODULE( MotuTransmitStreamProcessor, MotuTransmitStreamProcessor, DEBUG_LEVEL_NORMAL );
IMPL_DEBUG_MODULE( MotuReceiveStreamProcessor, MotuReceiveStreamProcessor, DEBUG_LEVEL_NORMAL );

// Set to 1 to enable the generation of a 1 kHz test tone in analog output 1
#define TESTTONE 1

// A macro to extract specific bits from a native endian quadlet
#define get_bits(_d,_start,_len) (((_d)>>((_start)-(_len)+1)) & ((1<<(_len))-1))

// Convert an SPH timestamp as received from the MOTU to a full timestamp in ticks.
static inline uint32_t sphRecvToFullTicks(uint32_t sph, uint32_t ct_now) {

uint32_t timestamp = CYCLE_TIMER_TO_TICKS(sph & 0x1ffffff);
uint32_t now_cycles = CYCLE_TIMER_GET_CYCLES(ct_now);

uint32_t ts_sec = CYCLE_TIMER_GET_SECS(ct_now);
  // If the cycles have wrapped, correct ts_sec so it represents when timestamp
  // was received.  The timestamps sent by the MOTU are always 1 or two cycles 
  // in advance of the cycle timer (reasons unknown at this stage).  In addition,
  // iso buffering can delay the arrival of packets for quite a number of cycles
  // (have seen a delay >12 cycles).
  if (abs(CYCLE_TIMER_GET_CYCLES(sph) - now_cycles) > 1000) {
debugOutput(DEBUG_LEVEL_VERBOSE, "now=%d, ct=%d\n", now_cycles, CYCLE_TIMER_GET_CYCLES(sph));
    if (ts_sec)
      ts_sec--;
    else
      ts_sec = 127;
  }
  return timestamp + ts_sec*TICKS_PER_SECOND;
}

// Convert a full timestamp into an SPH timestamp as required by the MOTU
static inline uint32_t fullTicksToSph(uint32_t timestamp) {
  return timestamp & 0x1ffffff;
}

/* transmit */
MotuTransmitStreamProcessor::MotuTransmitStreamProcessor(int port, int framerate,
		unsigned int event_size)
	: TransmitStreamProcessor(port, framerate), m_event_size(event_size),
	m_tx_dbc(0),
	m_closedown_count(-1), m_streaming_active(0) {
}

MotuTransmitStreamProcessor::~MotuTransmitStreamProcessor() {

}

bool MotuTransmitStreamProcessor::init() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Initializing (%p)...\n");
	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if(!TransmitStreamProcessor::init()) {
		debugFatal("Could not do base class init (%p)\n",this);
		return false;
	}

	return true;
}

void MotuTransmitStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l); // sets the debug level of the current object
	TransmitStreamProcessor::setVerboseLevel(l); // also set the level of the base class
}


enum raw1394_iso_disposition
MotuTransmitStreamProcessor::getPacket(unsigned char *data, unsigned int *length,
	              unsigned char *tag, unsigned char *sy,
	              int cycle, unsigned int dropped, unsigned int max_length) {

// FIXME: the actual delays in the system need to be worked out so
// we can get this thing synchronised.  For now this seems to work.
    uint64_t ts_head, fc;

	quadlet_t *quadlet = (quadlet_t *)data;
	signed int i;
	
    m_last_cycle=cycle;
    
    // determine if we want to send a packet or not
    // note that we can't use getCycleTimer directly here,
    // because packets are queued in advance. This means that
    // we the packet we are constructing will be sent out 
    // on 'cycle', not 'now'.
    unsigned int ctr=m_handler->getCycleTimer();
    int now_cycles = (int)CYCLE_TIMER_GET_CYCLES(ctr);
    
    // the difference between the cycle this
    // packet is intended for and 'now'
    int cycle_diff = diffCycles(cycle, now_cycles);
    
	// Signal that streaming is still active
	m_streaming_active = 1;

    // as long as the cycle parameter is not in sync with
    // the current time, the stream is considered not
    // to be 'running'
    // NOTE: this works only at startup
    if (!m_running && cycle_diff >= 0 && cycle >= 0) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Xmit StreamProcessor %p started running at cycle %d\n",this, cycle);
            m_running=true;
    }

    if (!m_disabled && m_is_disabled) {
        // this means that we are trying to enable
        if ((unsigned int)cycle == m_cycle_to_enable_at) {
            m_is_disabled=false;
            
            debugOutput(DEBUG_LEVEL_VERBOSE,"Enabling StreamProcessor %p at %u\n", this, cycle);
            
            // initialize the buffer head & tail
            m_SyncSource->m_data_buffer->getBufferHeadTimestamp(&ts_head, &fc); // thread safe
            
            // the number of cycles the sync source lags (> 0)
            // or leads (< 0)
            int sync_lag_cycles=diffCycles(cycle, m_SyncSource->getLastCycle());
            
            // account for the cycle lag between sync SP and this SP
            // the last update of the sync source's timestamps was sync_lag_cycles
            // cycles before the cycle we are calculating the timestamp for.
            // if we were to use one-frame buffers, you would expect the 
            // frame that is sent on cycle CT to have a timestamp T1.
            // ts_head however is for cycle CT-sync_lag_cycles, and lies
            // therefore sync_lag_cycles * TICKS_PER_CYCLE earlier than
            // T1.
            ts_head = addTicks(ts_head, sync_lag_cycles * TICKS_PER_CYCLE);
            
            m_data_buffer->setBufferTailTimestamp(ts_head);
            
            #ifdef DEBUG
            if ((unsigned int)m_data_buffer->getFrameCounter() != m_data_buffer->getBufferSize()) {
                debugWarning("m_data_buffer->getFrameCounter() != m_data_buffer->getBufferSize()\n");
            }
            #endif
            debugOutput(DEBUG_LEVEL_VERBOSE,"XMIT TS SET: TS=%10lld, LAG=%03d, FC=%4d\n",
                            ts_head, sync_lag_cycles, m_data_buffer->getFrameCounter());
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "will enable StreamProcessor %p at %u, now is %d\n",
                        this, m_cycle_to_enable_at, cycle);
        }
    } else if (m_disabled && !m_is_disabled) {
        // trying to disable
        debugOutput(DEBUG_LEVEL_VERBOSE,"disabling StreamProcessor %p at %u\n", 
                    this, cycle);
        m_is_disabled=true;
    }

	// Do housekeeping expected for all packets sent to the MOTU, even
	// for packets containing no audio data.
	*sy = 0x00;
	*tag = 1;      // All MOTU packets have a CIP-like header


    // the base timestamp is the one of the next sample in the buffer
    m_data_buffer->getBufferHeadTimestamp(&ts_head, &fc); // thread safe
    
    int64_t timestamp = ts_head;

    // we send a packet some cycles in advance, to avoid the
    // following situation:
    // suppose we are only a few ticks away from 
    // the moment to send this packet. therefore we decide
    // not to send the packet, but send it in the next cycle.
    // This means that the next time point will be 3072 ticks
    // later, making that the timestamp will be expired when the 
    // packet is sent, unless TRANSFER_DELAY > 3072.
    // this means that we need at least one cycle of extra buffering.
    uint64_t ticks_to_advance = TICKS_PER_CYCLE * TRANSMIT_ADVANCE_CYCLES;
    
    // if cycle lies cycle_diff cycles in the future, we should
    // queue this packet cycle_diff * TICKS_PER_CYCLE earlier than
    // we would if it were to be sent immediately.
    ticks_to_advance += cycle_diff * TICKS_PER_CYCLE;

    // determine the 'now' time in ticks
    uint64_t cycle_timer=CYCLE_TIMER_TO_TICKS(ctr);
    
    // time until the packet is to be sent (if > 0: send packet)
    int32_t until_next=diffTicks(timestamp, cycle_timer + ticks_to_advance);
    
	// Size of a single data frame in quadlets
	unsigned dbs = m_event_size / 4;
	
    // don't process the stream when it is not enabled, not running
    // or when the next sample is not due yet.
    if((until_next>0) || m_is_disabled || !m_running) {
        // send dummy packet
        
        // construct the packet CIP-like header.  Even if this is a data-less 
        // packet the dbs field is still set as if there were data blocks 
        // present.  For data-less packets the dbc is the same as the previously
        // transmitted block.
        *quadlet = htonl(0x00000400 | ((getNodeId()&0x3f)<<24) | m_tx_dbc | (dbs<<16));
        quadlet++;
        *quadlet = htonl(0x8222ffff);
        quadlet++;
        *length = 8;
        
        #warning high-pitched sound protection removed!
        // In the disabled state simply zero all data sent to the MOTU.  If
        // a stream of empty packets are sent once iso streaming is enabled
        // the MOTU tends to emit high-pitched audio (approx 10 kHz) for
        // some reason.  This is not completely sufficient, however (zeroed
        // packets must also be sent on iso closedown).
    
        // FIXME: Currently we simply send empty packets to the MOTU when
        // the stream is disabled so the "m_disabled == 0" code is never
        // executed.  However, this may change in future so it's left in 
        // for the moment for reference.
        // FIXME: Currently we don't read the buffer at all during closedown.
        // We could (and silently junk the contents) if it turned out to be
        // more helpful.

//if (!m_is_disabled && m_running)
//  return RAW1394_ISO_OK;
//else	
        return RAW1394_ISO_DEFER;
    }

	// The number of events expected by the MOTU is solely dependent on
	// the current sample rate.  An 'event' is one sample from all channels
	// plus possibly other midi and control data.
	signed n_events = m_framerate<=48000?8:(m_framerate<=96000?16:32);

    // add the transmit transfer delay to construct the playout time
    uint64_t ts=addTicks(timestamp, TRANSMIT_TRANSFER_DELAY);
    
    if (m_data_buffer->readFrames(n_events, (char *)(data + 8))) {
    
        // Increment the dbc (data block count).  This is only done if the
        // packet will contain events - that is, we are due to send some
        // data.  Otherwise a pad packet is sent which contains the DBC of
        // the previously sent packet.  This regime also means that the very
        // first packet containing data will have a DBC of n_events, which
        // matches what is observed from other systems.
        m_tx_dbc += n_events;
        if (m_tx_dbc > 0xff)
            m_tx_dbc -= 0x100;
    
        // construct the packet CIP-like header.  Even if this is a data-less 
        // packet the dbs field is still set as if there were data blocks 
        // present.  For data-less packets the dbc is the same as the previously
        // transmitted block.
        *quadlet = htonl(0x00000400 | ((getNodeId()&0x3f)<<24) | m_tx_dbc | (dbs<<16));
        quadlet++;
        *quadlet = htonl(0x8222ffff);
        quadlet++;

        *length = n_events*m_event_size + 8;

        // convert the timestamp to Motu format
        // I assume that it is in ticks, and wraps as the cycle timer
        #warning syt conversion to be done
        
		// FIXME: if we choose to read the buffer even during closedown,
		// here is where the data is silenced.
		//   if (m_closedown_count >= 0)
		//     memset(data+8, 0, read_size);
		if (m_closedown_count > 0)
			m_closedown_count--;

		// Set up each frames's SPH.  Note that the (int) typecast
		// appears to do rounding.

float ticks_per_frame = m_SyncSource->m_data_buffer->getRate();
		for (i=0; i<n_events; i++, quadlet += dbs) {
//			unsigned int ts_frame = ts;
			unsigned int ts_frame = timestamp;
			ts_frame += (unsigned int)((float)i * ticks_per_frame);
			*quadlet = htonl( TICKS_TO_CYCLE_TIMER(ts_frame) & 0x1ffffff);
if (cycle==0) {
  debugOutput(DEBUG_LEVEL_VERBOSE,"%d %d %d\n",
    TICKS_TO_SECS(ts_frame),
    TICKS_TO_CYCLES(ts_frame),
    TICKS_TO_OFFSET(ts_frame));
}
#if TESTTONE
			// FIXME: remove this hacked in 1 kHz test signal to
			// analog-1 when testing is complete.  Note that the tone is
			// *never* added during closedown.
			if (m_closedown_count<0) {
				static signed int a_cx = 0;
				signed int val;
				val = (int)(0x7fffff*sin(1000.0*2.0*M_PI*(a_cx/24576000.0)));
 				if ((a_cx+=512) >= 24576000) {
					a_cx -= 24576000;
				}
				*(data+8+i*m_event_size+16) = (val >> 16) & 0xff;
				*(data+8+i*m_event_size+17) = (val >> 8) & 0xff;
				*(data+8+i*m_event_size+18) = val & 0xff;
			}
#endif

		}

		// Process all ports that should be handled on a per-packet base
		// this is MIDI for AMDTP (due to the need of DBC, which is lost 
		// when putting the events in the ringbuffer)
		// for motu this might also be control data, however as control
		// data isn't time specific I would also include it in the period
		// based processing
        
		// FIXME: m_tx_dbc probably needs to be initialised to a non-zero
		// value somehow so MIDI sync is possible.  For now we ignore
		// this issue.
		if (!encodePacketPorts((quadlet_t *)(data+8), n_events, m_tx_dbc)) {
			debugWarning("Problem encoding Packet Ports\n");
		}

        return RAW1394_ISO_OK;
        
    } else if (now_cycles<cycle) {
        // we can still postpone the queueing of the packets
        return RAW1394_ISO_AGAIN;
    } else { // there is no more data in the ringbuffer

        debugWarning("Transmit buffer underrun (now %d, queue %d, target %d)\n", 
                 now_cycles, cycle, TICKS_TO_CYCLES(ts));

        // signal underrun
        m_xruns++;

        // disable the processing, will be re-enabled when
        // the xrun is handled
        m_disabled=true;
        m_is_disabled=true;

        // compose a no-data packet, we should always
        // send a valid packet
        
        // send dummy packet
        
        // construct the packet CIP-like header.  Even if this is a data-less 
        // packet the dbs field is still set as if there were data blocks 
        // present.  For data-less packets the dbc is the same as the previously
        // transmitted block.
        *quadlet = htonl(0x00000400 | ((getNodeId()&0x3f)<<24) | m_tx_dbc | (dbs<<16));
        quadlet++;
        *quadlet = htonl(0x8222ffff);
        quadlet++;
        *length = 8;

        return RAW1394_ISO_DEFER;
    }

    // we shouldn't get here
    return RAW1394_ISO_ERROR;

}

int MotuTransmitStreamProcessor::getMinimalSyncDelay() {
    return 0;
}

bool MotuTransmitStreamProcessor::prefill() {
	// this is needed because otherwise there is no data to be 
	// sent when the streaming starts
    
	int i = m_nb_buffers;
	while (i--) {
		if(!transferSilence(m_period)) {
			debugFatal("Could not prefill transmit stream\n");
			return false;
		}
	}
	return true;
}

bool MotuTransmitStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    // we have to make sure that the buffer HEAD timestamp
    // lies in the future for every possible buffer fill case.
    int offset=(int)(m_data_buffer->getBufferSize()*m_ticks_per_frame);
    
    m_data_buffer->setTickOffset(offset);
    
	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if (!TransmitStreamProcessor::reset()) {
		debugFatal("Could not do base class reset\n");
		return false;
	}

	// we should prefill the event buffer
	if (!prefill()) {
		debugFatal("Could not prefill buffers\n");
		return false;    
	}

	return true;
}

bool MotuTransmitStreamProcessor::prepare() {
    
	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");
    
	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if (!TransmitStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}

	m_PeriodStat.setName("XMT PERIOD");
	m_PacketStat.setName("XMT PACKET");
	m_WakeupStat.setName("XMT WAKEUP");

	debugOutput( DEBUG_LEVEL_VERBOSE, "Event size: %d\n", m_event_size);
    
	// allocate the event buffer
	unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;

    // allocate the internal buffer
    m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);
	unsigned int events_per_frame = m_framerate<=48000?8:(m_framerate<=96000?16:32);

    assert(m_data_buffer);    
    m_data_buffer->setBufferSize(ringbuffer_size_frames);
    m_data_buffer->setEventSize(m_event_size);
    m_data_buffer->setEventsPerFrame(1);
    
    m_data_buffer->setUpdatePeriod(m_period);
    m_data_buffer->setNominalRate(m_ticks_per_frame);
    
    // FIXME: check if the timestamp wraps at one second
    m_data_buffer->setWrapValue(128L*TICKS_PER_SECOND);
    
    m_data_buffer->prepare();

	// Set the parameters of ports we can: we want the audio ports to be
	// period buffered, and the midi ports to be packet buffered.
	for ( PortVectorIterator it = m_Ports.begin();
	  it != m_Ports.end();
	  ++it ) {
		debugOutput(DEBUG_LEVEL_VERBOSE, "Setting up port %s\n",(*it)->getName().c_str());
		if(!(*it)->setBufferSize(m_period)) {
			debugFatal("Could not set buffer size to %d\n",m_period);
			return false;
        	}

		switch ((*it)->getPortType()) {
		case Port::E_Audio:
			if (!(*it)->setSignalType(Port::E_PeriodSignalled)) {
				debugFatal("Could not set signal type to PeriodSignalling");
				return false;
			}
			break;

		case Port::E_Midi:
			if (!(*it)->setSignalType(Port::E_PacketSignalled)) {
				debugFatal("Could not set signal type to PacketSignalling");
				return false;
			}
			if (!(*it)->setBufferType(Port::E_RingBuffer)) {
				debugFatal("Could not set buffer type");
				return false;
			}
			if (!(*it)->setDataType(Port::E_MidiEvent)) {
				debugFatal("Could not set data type");
				return false;
			}
			// FIXME: probably need rate control too.  See
			// Port::useRateControl() and AmdtpStreamProcessor.
			break;
                
		case Port::E_Control:
			if (!(*it)->setSignalType(Port::E_PeriodSignalled)) {
				debugFatal("Could not set signal type to PeriodSignalling");
				return false;
			}
			break;

		default:
			debugWarning("Unsupported port type specified\n");
			break;
		}
	}

	// The API specific settings of the ports are already set before
	// this routine is called, therefore we can init&prepare the ports
	if (!initPorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	if(!preparePorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	return true;
}

bool MotuTransmitStreamProcessor::prepareForStop() {

	// If the stream is disabled or isn't running there's no need to
	// wait since the MOTU *should* still be in a "zero data" state.
	//
	// If the m_streaming_active flag is 0 it indicates that the
	// transmit callback hasn't been called since a closedown was
	// requested when this function was last called.  This effectively
	// signifies that the streaming thread has been exitted due to an
	// xrun in either the receive or transmit handlers.  In this case
	// there's no point in waiting for the closedown count to hit zero
	// because it never will; the zero data will never get to the MOTU. 
	// It's best to allow an immediate stop and let the xrun handler
	// proceed as best it can.
	//
	// The ability to detect the lack of streaming also prevents the
	// "wait for stop" in the stream processor manager's stop() method
	// from hitting its timeout which in turn seems to increase the
	// probability of a successful recovery.
	if (m_is_disabled || !isRunning() || !m_streaming_active)
		return true;

	if (m_closedown_count < 0) {
		// No closedown has been initiated, so start one now.  Set
		// the closedown count to the number of zero packets which
		// will be sent to the MOTU before closing off the iso
		// streams.  FIXME: 128 packets (each containing 8 frames at
		// 48 kHz) is the experimentally-determined figure for 48
		// kHz with a period size of 1024.  It seems that at least
		// one period of zero samples need to be sent to allow for
		// inter-thread communication occuring on period boundaries. 
		// This needs to be confirmed for other rates and period
		// sizes.
		signed n_events = m_framerate<=48000?8:(m_framerate<=96000?16:32);
		m_closedown_count = m_period / n_events;

		// Set up a test to confirm that streaming is still active.
		// If the streaming function hasn't been called by the next
		// iteration through this function there's no point in
		// continuing since it means the zero data will never get to 
		// the MOTU.
		m_streaming_active = 0;
		return false;
	}

	// We are "go" for closedown once all requested zero packets
	// (initiated by a previous call to this function) have been sent to
	// the MOTU.
	return m_closedown_count == 0;
}

bool MotuTransmitStreamProcessor::prepareForStart() {
// Reset some critical variables required so the stream starts cleanly. This
// method is called once on every stream restart. Initialisations which should 
// be done once should be placed in the init() method instead.
	m_running = 0;
	m_closedown_count = -1;
	m_streaming_active = 0;

	// At this point we'll also disable the stream processor here.
	// At this stage stream processors are always explicitly re-enabled
	// after being started, so by starting in the disabled state we 
	// ensure that every start will be exactly the same.
	disable();

	return true;
}

bool MotuTransmitStreamProcessor::prepareForEnable(uint64_t time_to_enable_at) {

    debugOutput(DEBUG_LEVEL_VERBOSE,"Preparing to enable...\n");

    // for the transmit SP, we have to initialize the 
    // buffer timestamp to something sane, because this timestamp
    // is used when it is SyncSource
    
    // the time we initialize to will determine the time at which
    // the first sample in the buffer will be sent, so we should
    // make it at least 'time_to_enable_at'
    
    uint64_t now=m_handler->getCycleTimer();
    unsigned int now_secs=CYCLE_TIMER_GET_SECS(now);
    
    // check if a wraparound on the secs will happen between
    // now and the time we start
    if (CYCLE_TIMER_GET_CYCLES(now)>time_to_enable_at) {
        // the start will happen in the next second
        now_secs++;
        if (now_secs>=128) now_secs=0;
    }
    
//    uint64_t ts_head= now_secs*TICKS_PER_SECOND;
    uint64_t ts_head = time_to_enable_at*TICKS_PER_CYCLE;
    
    // we also add the nb of cycles we transmit in advance
    ts_head=addTicks(ts_head, TRANSMIT_ADVANCE_CYCLES*TICKS_PER_CYCLE);
    
    m_data_buffer->setBufferTailTimestamp(ts_head);

    if (!StreamProcessor::prepareForEnable(time_to_enable_at)) {
        debugError("StreamProcessor::prepareForEnable failed\n");
        return false;
    }

    return true;
}

bool MotuTransmitStreamProcessor::transferSilence(unsigned int size) {
    bool retval;
    
	// This function should tranfer 'size' frames of 'silence' to the event buffer
	char *dummybuffer=(char *)calloc(size,m_event_size);

	transmitSilenceBlock(dummybuffer, size, 0);

    // add the silence data to the ringbuffer
    if(m_data_buffer->writeFrames(size, dummybuffer, 0)) { 
        retval=true;
    } else {
        debugWarning("Could not write to event buffer\n");
        retval=false;
    }

	free(dummybuffer);

	return retval;
}

bool MotuTransmitStreamProcessor::putFrames(unsigned int nbframes, int64_t ts) {
    m_PeriodStat.mark(m_data_buffer->getBufferFill());
    
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "MotuTransmitStreamProcessor::putFrames(%d, %llu)\n", nbframes, ts);
    
    // transfer the data
    m_data_buffer->blockProcessWriteFrames(nbframes, ts);

    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, " New timestamp: %llu\n", ts);

    return true;
}

/* 
 * write received events to the stream ringbuffers.
 */

bool MotuTransmitStreamProcessor::processWriteBlock(char *data, 
                       unsigned int nevents, unsigned int offset) {
	bool no_problem=true;
	unsigned int i;

	// FIXME: ensure the MIDI and control streams are all zeroed until
	// such time as they are fully implemented.
	for (i=0; i<nevents; i++) {
		memset(data+4+i*m_event_size, 0x00, 6);
	}

	for ( PortVectorIterator it = m_PeriodPorts.begin();
	  it != m_PeriodPorts.end();
	  ++it ) {
		// If this port is disabled, don't process it
		if((*it)->isDisabled()) {continue;};
        
		//FIXME: make this into a static_cast when not DEBUG?
		Port *port=dynamic_cast<Port *>(*it);
		
		switch(port->getPortType()) {
		
		case Port::E_Audio:
			if (encodePortToMotuEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
				no_problem=false;
			}
			break;
		// midi is a packet based port, don't process
		//	case MotuPortInfo::E_Midi:
		//		break;

		default: // ignore
			break;
		}
	}
	return no_problem;
}

int MotuTransmitStreamProcessor::transmitSilenceBlock(char *data, 
                       unsigned int nevents, unsigned int offset) {
	// This is the same as the non-silence version, except that is
	// doesn't read from the port buffers.

	int problem=0;

	for ( PortVectorIterator it = m_PeriodPorts.begin();
	  it != m_PeriodPorts.end();
	  ++it ) {
		//FIXME: make this into a static_cast when not DEBUG?
		Port *port=dynamic_cast<Port *>(*it);
		
		switch(port->getPortType()) {
		
		case Port::E_Audio:
			if (encodeSilencePortToMotuEvents(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not encode port %s to MBLA events",(*it)->getName().c_str());
				problem=1;
			}
			break;
		// midi is a packet based port, don't process
		//	case MotuPortInfo::E_Midi:
		//		break;

		default: // ignore
			break;
		}
	}
	return problem;
}

/**
 * @brief decode a packet for the packet-based ports
 *
 * @param data Packet data
 * @param nevents number of events in data (including events of other ports & port types)
 * @param dbc DataBlockCount value for this packet
 * @return true if all successfull
 */
bool MotuTransmitStreamProcessor::encodePacketPorts(quadlet_t *data, unsigned int nevents, 
		unsigned int dbc) {
	bool ok=true;
	char byte;

	// Use char here since the target address won't necessarily be
	// aligned; use of an unaligned quadlet_t may cause issues on
	// certain architectures.  Besides, the target for MIDI data going
	// directly to the MOTU isn't structured in quadlets anyway; it is a
	// sequence of 3 unaligned bytes.
	unsigned char *target = NULL;

	for ( PortVectorIterator it = m_PacketPorts.begin();
		it != m_PacketPorts.end();
		++it ) {

		Port *port=static_cast<Port *>(*it);
 		assert(port); // this should not fail!!

		// Currently the only packet type of events for MOTU 
		// is MIDI in mbla.  However in future control data
		// might also be sent via "packet" events.
		// assert(pinfo->getFormat()==MotuPortInfo::E_Midi);

		// FIXME: MIDI output is completely untested at present.
		switch (port->getPortType()) {
			case Port::E_Midi: {
				MotuMidiPort *mp=static_cast<MotuMidiPort *>(*it);

				// Send a byte if we can. MOTU MIDI data is
				// sent using a 3-byte sequence starting at
				// the port's position.  For now we'll
				// always send in the first event of a
				// packet, but this might need refinement
				// later.
				if (mp->canRead()) {
					mp->readEvent(&byte);
					target = (unsigned char *)data + mp->getPosition();
					*(target++) = 0x01;
					*(target++) = 0x00;
					*(target++) = byte;
				}
				break;
			}
			default:
				debugOutput(DEBUG_LEVEL_VERBOSE, "Unknown packet-type port type %d\n",port->getPortType());
				return ok;        
      		}
	}

	return ok;
}

int MotuTransmitStreamProcessor::encodePortToMotuEvents(MotuAudioPort *p, quadlet_t *data, 
                       unsigned int offset, unsigned int nevents) {
// Encodes nevents worth of data from the given port into the given buffer.  The
// format of the buffer is precisely that which will be sent to the MOTU.
// The basic idea:
//   iterate over the ports
//     * get port buffer address
//     * loop over events
//         - pick right sample in event based upon PortInfo
//         - convert sample from Port format (E_Int24, E_Float, ..) to MOTU
//           native format
//
// We include the ability to start the transfer from the given offset within
// the port (expressed in frames) so the 'efficient' transfer method can be
// utilised.

	unsigned int j=0;

	// Use char here since the target address won't necessarily be 
	// aligned; use of an unaligned quadlet_t may cause issues on certain
	// architectures.  Besides, the target (data going directly to the MOTU)
	// isn't structured in quadlets anyway; it mainly consists of packed
	// 24-bit integers.
	unsigned char *target;
	target = (unsigned char *)data + p->getPosition();

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				// Offset is in frames, but each port is only a single
				// channel, so the number of frames is the same as the
				// number of quadlets to offset (assuming the port buffer
				// uses one quadlet per sample, which is the case currently).
				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // Decode nsamples
					*target = (*buffer >> 16) & 0xff;
					*(target+1) = (*buffer >> 8) & 0xff;
					*(target+2) = (*buffer) & 0xff;

					buffer++;
					target+=m_event_size;
				}
			}
			break;
		case Port::E_Float:
			{
				const float multiplier = (float)(0x7FFFFF);
				float *buffer=(float *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples		
					unsigned int v = (int)(*buffer * multiplier);
					*target = (v >> 16) & 0xff;
					*(target+1) = (v >> 8) & 0xff;
					*(target+2) = v & 0xff;

					buffer++;
					target+=m_event_size;
				}
			}
			break;
	}

	return 0;
}

int MotuTransmitStreamProcessor::encodeSilencePortToMotuEvents(MotuAudioPort *p, quadlet_t *data, 
                       unsigned int offset, unsigned int nevents) {
	unsigned int j=0;
	unsigned char *target = (unsigned char *)data + p->getPosition();

	switch (p->getDataType()) {
	default:
        case Port::E_Int24:
        case Port::E_Float:
		for (j = 0; j < nevents; j++) {
			*target = *(target+1) = *(target+2) = 0;
			target += m_event_size;
		}
		break;
	}

	return 0;
}

/* --------------------- RECEIVE ----------------------- */

MotuReceiveStreamProcessor::MotuReceiveStreamProcessor(int port, int framerate, 
	unsigned int event_size)
    : ReceiveStreamProcessor(port, framerate), m_event_size(event_size),
	m_closedown_active(0) {

}

MotuReceiveStreamProcessor::~MotuReceiveStreamProcessor() {

}

bool MotuReceiveStreamProcessor::init() {

	// call the parent init
	// this has to be done before allocating the buffers, 
	// because this sets the buffersizes from the processormanager
	if(!ReceiveStreamProcessor::init()) {
		debugFatal("Could not do base class init (%d)\n",this);
		return false;
	}

	return true;
}

	// NOTE by PP: timestamp based sync fixes this automagically by
	//             enforcing that the roundtrip latency is constant:
	// Detect missed receive cycles
	// FIXME: it would be nice to advance the rx buffer by the amount of
	// frames missed.  However, since the MOTU transmits more frames per
	// cycle than the average and "catches up" with periodic empty
	// cycles it's not trivial to work out precisely how many frames
	// were missed.  Ultimately I think we need to do so if sync is to
	// be maintained across a transient receive failure.
	
enum raw1394_iso_disposition 
MotuReceiveStreamProcessor::putPacket(unsigned char *data, unsigned int length, 
                  unsigned char channel, unsigned char tag, unsigned char sy, 
                  unsigned int cycle, unsigned int dropped) {
    
	enum raw1394_iso_disposition retval=RAW1394_ISO_OK;
	// this is needed for the base class getLastCycle() to work.
	// this avoids a function call like StreamProcessor::updateLastCycle()
    m_last_cycle=cycle;

    // check our enable status
    if (!m_disabled && m_is_disabled) {
        // this means that we are trying to enable
        if (cycle == m_cycle_to_enable_at) {
            m_is_disabled=false;
            debugOutput(DEBUG_LEVEL_VERBOSE,"Enabling StreamProcessor %p at %d\n", 
                this, cycle);
                
            // the previous timestamp is the one we need to start with
            // because we're going to update the buffer again this loop
            // using writeframes
//            m_data_buffer->setBufferTailTimestamp(m_last_timestamp2);
            m_data_buffer->setBufferTailTimestamp(m_last_timestamp);

debugOutput(DEBUG_LEVEL_VERBOSE,"On enable: last ts=%lld, ts2=%lld = %lld (%p)\n",
  m_last_timestamp, m_last_timestamp2, m_last_timestamp-m_last_timestamp2,
  m_data_buffer);

        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                "will enable StreamProcessor %p at %u, now is %d\n",
                    this, m_cycle_to_enable_at, cycle);
        }
    } else if (m_disabled && !m_is_disabled) {
        // trying to disable
        debugOutput(DEBUG_LEVEL_VERBOSE,"disabling StreamProcessor %p at %u\n", this, cycle);
        m_is_disabled=true;
    }
	
	// If the packet length is 8 bytes (ie: just a CIP-like header)
	// there is no isodata.
	if (length > 8) {
		// The iso data blocks from the MOTUs comprise a CIP-like
		// header followed by a number of events (8 for 1x rates, 16
		// for 2x rates, 32 for 4x rates).
		quadlet_t *quadlet = (quadlet_t *)data;
		unsigned int dbs = get_bits(ntohl(quadlet[0]), 23, 8);  // Size of one event in terms of fdf_size
		unsigned int fdf_size = get_bits(ntohl(quadlet[1]), 23, 8) == 0x22 ? 32:0; // Event unit size in bits

		// Don't even attempt to process a packet if it isn't what
		// we expect from a MOTU.  Yes, an FDF value of 32 bears
		// little relationship to the actual data (24 bit integer)
		// sent by the MOTU - it's one of those areas where MOTU
		// have taken a curious detour around the standards.
		if (tag!=1 || fdf_size!=32) {
			return RAW1394_ISO_OK;
		}
		
		// put this after the check because event_length can become 0 on invalid packets
		unsigned int event_length = (fdf_size * dbs) / 8;       // Event size in bytes
		unsigned int n_events = (length-8) / event_length;
		
        //=> store the previous timestamp
        m_last_timestamp2=m_last_timestamp;

        //=> convert the SYT to a full timestamp in ticks
//        m_last_timestamp=sytRecvToFullTicks((uint32_t)ntohl(*(quadlet_t *)(data+8)), 
//                                        cycle, m_handler->getCycleTimer());
//***
// FIXME: it given that we later advance this to be the timestamp of the sample immediately following
// this packet, it perhaps makes more sense to acquire the timestamp of the last frame in the packet.
// Then it's just a matter of adding m_ticks_per_frame rather than frame_size times this.
uint32_t first_sph = ntohl(*(quadlet_t *)(data+8));
//uint32_t first_sph = ntohl(*(quadlet_t *)(data+8+(event_length*(n_events-1))));
//        m_last_timestamp = ((first_sph & 0x1fff000)>>12)*3072 + (first_sph & 0xfff);
//        m_last_timestamp = CYCLE_TIMER_TO_TICKS(first_sph & 0x1ffffff);
m_last_timestamp = sphRecvToFullTicks(first_sph, m_handler->getCycleTimer());

		// Signal that we're running
        if(!m_running && n_events && m_last_timestamp2 && m_last_timestamp) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Receive StreamProcessor %p started running at %d\n", this, cycle);
            m_running=true;
        }

        //=> don't process the stream samples when it is not enabled.
        if(m_is_disabled) {

            // we keep track of the timestamp here
            // this makes sure that we will have a somewhat accurate
            // estimate as to when a period might be ready. i.e. it will not
            // be ready earlier than this timestamp + period time
            
            // the next (possible) sample is not this one, but lies 
            // SYT_INTERVAL * rate later
            float frame_size=m_framerate<=48000?8:(m_framerate<=96000?16:32);
            uint64_t ts=addTicks(m_last_timestamp,
                                 (uint64_t)(frame_size * m_ticks_per_frame));
//            uint64_t ts=addTicks(m_last_timestamp,
//                                 (uint64_t)(m_ticks_per_frame));

            // set the timestamp as if there will be a sample put into
            // the buffer by the next packet.
if (ts >= 128L* TICKS_PER_SECOND)
  ts -= 128L*TICKS_PER_SECOND;
            m_data_buffer->setBufferTailTimestamp(ts);
//debugOutput(DEBUG_LEVEL_VERBOSE,"%p, last ts=%lld, ts=%lld, lts2=%lld\n", m_data_buffer, m_last_timestamp, ts, m_last_timestamp2);
            
            return RAW1394_ISO_DEFER;
        }

		debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "put packet...\n");
//debugOutput(DEBUG_LEVEL_VERBOSE,"enabled: %p, last ts=%lld, ts2=%lld\n",m_data_buffer, m_last_timestamp, m_last_timestamp2);

        //=> process the packet
        // add the data payload to the ringbuffer
        if(m_data_buffer->writeFrames(n_events, (char *)(data+8), m_last_timestamp)) { 
            retval=RAW1394_ISO_OK;
            
            int dbc = get_bits(ntohl(quadlet[0]), 8, 8);
            
            // process all ports that should be handled on a per-packet base
            // this is MIDI for AMDTP (due to the need of DBC)
            if (!decodePacketPorts((quadlet_t *)(data+8), n_events, dbc)) {
                debugWarning("Problem decoding Packet Ports\n");
                retval=RAW1394_ISO_DEFER;
            }
            
        } else {
        
            debugWarning("Receive buffer overrun (cycle %d, FC=%d, PC=%d)\n", 
                 cycle, m_data_buffer->getFrameCounter(), m_handler->getPacketCount());
            
            m_xruns++;
            
            // disable the processing, will be re-enabled when
            // the xrun is handled
            m_disabled=true;
            m_is_disabled=true;

            retval=RAW1394_ISO_DEFER;
        }
    }

	return retval;
}

// returns the delay between the actual (real) time of a timestamp as received,
// and the timestamp that is passed on for the same event. This is to cope with
// ISO buffering
int MotuReceiveStreamProcessor::getMinimalSyncDelay() {
	unsigned int n_events = m_framerate<=48000?8:(m_framerate<=96000?16:32);
    
    return (int)(m_handler->getWakeupInterval() * n_events * m_ticks_per_frame);
}

bool MotuReceiveStreamProcessor::reset() {

	debugOutput( DEBUG_LEVEL_VERBOSE, "Resetting...\n");

    m_data_buffer->setTickOffset(0);

	// reset all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::reset()) {
		debugFatal("Could not do base class reset\n");
		return false;
	}

	return true;
}

bool MotuReceiveStreamProcessor::prepare() {

	// prepare all non-device specific stuff
	// i.e. the iso stream and the associated ports
	if(!ReceiveStreamProcessor::prepare()) {
		debugFatal("Could not prepare base class\n");
		return false;
	}

	debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing...\n");

	m_PeriodStat.setName("RCV PERIOD");
	m_PacketStat.setName("RCV PACKET");
	m_WakeupStat.setName("RCV WAKEUP");

    // setup any specific stuff here
    // FIXME: m_frame_size would be a better name
	debugOutput( DEBUG_LEVEL_VERBOSE, "Event size: %d\n", m_event_size);
    
    // prepare the framerate estimate
    m_ticks_per_frame = (TICKS_PER_SECOND*1.0) / ((float)m_framerate);
	
	// initialize internal buffer
	unsigned int ringbuffer_size_frames=m_nb_buffers * m_period;
	
	unsigned int events_per_frame = m_framerate<=48000?8:(m_framerate<=96000?16:32);

    assert(m_data_buffer);    
    m_data_buffer->setBufferSize(ringbuffer_size_frames);
    m_data_buffer->setEventSize(m_event_size);
    m_data_buffer->setEventsPerFrame(1);	
    
// JMW: The rx buffer receives a new timestamp once per received frame so I think the
// buffer update period is events_per_frame, not events per period.
//    m_data_buffer->setUpdatePeriod(m_period);
    m_data_buffer->setUpdatePeriod(events_per_frame);
    m_data_buffer->setNominalRate(m_ticks_per_frame);
    
    m_data_buffer->setWrapValue(128L*TICKS_PER_SECOND);
    
    m_data_buffer->prepare();

	// set the parameters of ports we can:
	// we want the audio ports to be period buffered,
	// and the midi ports to be packet buffered
	for ( PortVectorIterator it = m_Ports.begin();
		  it != m_Ports.end();
		  ++it )
	{
		debugOutput(DEBUG_LEVEL_VERBOSE, "Setting up port %s\n",(*it)->getName().c_str());
		
		if(!(*it)->setBufferSize(m_period)) {
			debugFatal("Could not set buffer size to %d\n",m_period);
			return false;
		}

		switch ((*it)->getPortType()) {
			case Port::E_Audio:
				if(!(*it)->setSignalType(Port::E_PeriodSignalled)) {
					debugFatal("Could not set signal type to PeriodSignalling");
					return false;
				}
				break;
			case Port::E_Midi:
				if(!(*it)->setSignalType(Port::E_PacketSignalled)) {
					debugFatal("Could not set signal type to PacketSignalling");
					return false;
				}
				if (!(*it)->setBufferType(Port::E_RingBuffer)) {
					debugFatal("Could not set buffer type");
					return false;
				}
				if (!(*it)->setDataType(Port::E_MidiEvent)) {
					debugFatal("Could not set data type");
					return false;
				}
				// FIXME: probably need rate control too.  See
				// Port::useRateControl() and AmdtpStreamProcessor.
				break;
			case Port::E_Control:
				if(!(*it)->setSignalType(Port::E_PeriodSignalled)) {
					debugFatal("Could not set signal type to PeriodSignalling");
					return false;
				}
				break;
			default:
				debugWarning("Unsupported port type specified\n");
				break;
		}

	}

	// The API specific settings of the ports are already set before
	// this routine is called, therefore we can init&prepare the ports
	if(!initPorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}

	if(!preparePorts()) {
		debugFatal("Could not initialize ports!\n");
		return false;
	}
	
	return true;

}


bool MotuReceiveStreamProcessor::prepareForStop() {

	// A MOTU receive stream can stop at any time.  However, signify
	// that stopping is in progress because other streams (notably the
	// transmit stream) may keep going for some time and cause an
	// overflow in the receive buffers.  If a closedown is in progress
	// the receive handler simply throws all incoming data away so
	// no buffer overflow can occur.
	m_closedown_active = 1;
	return true;
}

bool MotuReceiveStreamProcessor::prepareForStart() {
// Reset some critical variables required so the stream starts cleanly. This
// method is called once on every stream restart, including those during
// xrun recovery.  Initialisations which should be done once should be
// placed in the init() method instead.
	m_running = 0;
	m_closedown_active = 0;

	// At this point we'll also disable the stream processor here.
	// At this stage stream processors are always explicitly re-enabled
	// after being started, so by starting in the disabled state we 
	// ensure that every start will be exactly the same.
	disable();

	return true;
}

bool MotuReceiveStreamProcessor::getFrames(unsigned int nbframes) {

    m_PeriodStat.mark(m_data_buffer->getBufferFill());

    // ask the buffer to process nbframes of frames
    // using it's registered client's processReadBlock(),
    // which should be ours
    m_data_buffer->blockProcessReadFrames(nbframes);

    return true;
}

/**
 * \brief write received events to the port ringbuffers.
 */
bool MotuReceiveStreamProcessor::processReadBlock(char *data, 
					   unsigned int nevents, unsigned int offset)
{
	bool no_problem=true;
	for ( PortVectorIterator it = m_PeriodPorts.begin();
          it != m_PeriodPorts.end();
          ++it ) {
		if((*it)->isDisabled()) {continue;};

		//FIXME: make this into a static_cast when not DEBUG?
		Port *port=dynamic_cast<Port *>(*it);
		
		switch(port->getPortType()) {
		
		case Port::E_Audio:
			if(decodeMotuEventsToPort(static_cast<MotuAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
				debugWarning("Could not decode packet MBLA to port %s",(*it)->getName().c_str());
				no_problem=false;
			}
			break;
		// midi is a packet based port, don't process
		//	case MotuPortInfo::E_Midi:
		//		break;

		default: // ignore
			break;
		}
	}
	return no_problem;
}

/**
 * @brief decode a packet for the packet-based ports
 *
 * @param data Packet data
 * @param nevents number of events in data (including events of other ports & port types)
 * @param dbc DataBlockCount value for this packet
 * @return true if all successfull
 */
bool MotuReceiveStreamProcessor::decodePacketPorts(quadlet_t *data, unsigned int nevents, 
		unsigned int dbc) {
	bool ok=true;

	// Use char here since the source address won't necessarily be
	// aligned; use of an unaligned quadlet_t may cause issues on
	// certain architectures.  Besides, the source for MIDI data going
	// directly to the MOTU isn't structured in quadlets anyway; it is a
	// sequence of 3 unaligned bytes.
	unsigned char *src = NULL;

	for ( PortVectorIterator it = m_PacketPorts.begin();
		it != m_PacketPorts.end();
		++it ) {

		Port *port=dynamic_cast<Port *>(*it);
		assert(port); // this should not fail!!

		// Currently the only packet type of events for MOTU 
		// is MIDI in mbla.  However in future control data
		// might also be sent via "packet" events, so allow 
		// for this possible expansion.

		// FIXME: MIDI input is completely untested at present.
		switch (port->getPortType()) {
			case Port::E_Midi: {
				MotuMidiPort *mp=static_cast<MotuMidiPort *>(*it);
				signed int sample;
				unsigned int j = 0;
				// Get MIDI bytes if present anywhere in the
				// packet.  MOTU MIDI data is sent using a
				// 3-byte sequence starting at the port's
				// position.  It's thought that there can never
				// be more than one MIDI byte per packet, but
				// for completeness we'll check the entire packet
				// anyway.
				src = (unsigned char *)data + mp->getPosition();
				while (j < nevents) {
					if (*src==0x01 && *(src+1)==0x00) {
						sample = *(src+2);
						if (!mp->writeEvent(&sample)) {
							debugWarning("MIDI packet port events lost\n");
							ok = false;
						}
					}
					j++;
					src += m_event_size;
				}
				break;
			}
			default:
				debugOutput(DEBUG_LEVEL_VERBOSE, "Unknown packet-type port format %d\n",port->getPortType());
				return ok;        
      		}
	}

	return ok;
}

signed int MotuReceiveStreamProcessor::decodeMotuEventsToPort(MotuAudioPort *p, 
		quadlet_t *data, unsigned int offset, unsigned int nevents)
{
	unsigned int j=0;

	// Use char here since a port's source address won't necessarily be
	// aligned; use of an unaligned quadlet_t may cause issues on
	// certain architectures.  Besides, the source (data coming directly
	// from the MOTU) isn't structured in quadlets anyway; it mainly
	// consists of packed 24-bit integers.

	unsigned char *src_data;
	src_data = (unsigned char *)data + p->getPosition();

	switch(p->getDataType()) {
		default:
		case Port::E_Int24:
			{
				quadlet_t *buffer=(quadlet_t *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				// Offset is in frames, but each port is only a single
				// channel, so the number of frames is the same as the
				// number of quadlets to offset (assuming the port buffer
				// uses one quadlet per sample, which is the case currently).
				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // Decode nsamples
					*buffer = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);
					// Sign-extend highest bit of 24-bit int.
					// FIXME: this isn't strictly needed since E_Int24 is a 24-bit,
					// but doing so shouldn't break anything and makes the data
					// easier to deal with during debugging.
					if (*src_data & 0x80)
						*buffer |= 0xff000000;

					buffer++;
					src_data+=m_event_size;
				}
			}
			break;
		case Port::E_Float:
			{
				const float multiplier = 1.0f / (float)(0x7FFFFF);
				float *buffer=(float *)(p->getBufferAddress());

				assert(nevents + offset <= p->getBufferSize());

				buffer+=offset;

				for(j = 0; j < nevents; j += 1) { // decode max nsamples		
	
					unsigned int v = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);

					// sign-extend highest bit of 24-bit int
					int tmp = (int)(v << 8) / 256;
		
					*buffer = tmp * multiplier;
				
					buffer++;
					src_data+=m_event_size;
				}
			}
			break;
	}

	return 0;
}

signed int MotuReceiveStreamProcessor::setEventSize(unsigned int size) {
	m_event_size = size;
	return 0;
}

unsigned int MotuReceiveStreamProcessor::getEventSize(void) {
//
// Return the size of a single event sent by the MOTU as part of an iso
// data packet in bytes.
//
	return m_event_size;
}

void MotuReceiveStreamProcessor::setVerboseLevel(int l) {
	setDebugLevel(l);
	ReceiveStreamProcessor::setVerboseLevel(l);
}

} // end of namespace Streaming
