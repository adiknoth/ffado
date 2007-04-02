/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "Port.h"
#include <stdlib.h>
#include <assert.h>

namespace Streaming {

IMPL_DEBUG_MODULE( Port, Port, DEBUG_LEVEL_NORMAL );

Port::Port(std::string name, enum E_PortType porttype, enum E_Direction direction)
      : m_Name(name),
    m_SignalType(E_PeriodSignalled),
    m_BufferType(E_PointerBuffer),
    m_disabled(true),
    m_initialized(false),
    m_buffersize(0),
    m_eventsize(0),
    m_DataType(E_Int24),
    m_PortType(porttype),
    m_Direction(direction),
    m_buffer(0),
    m_ringbuffer(0),
    m_use_external_buffer(false),
    m_do_ratecontrol(false),
    m_event_interval(0),
    m_slot_interval(0),
    m_rate_counter(0),
    m_rate_counter_minimum(0),
    m_average_ratecontrol(false)

{

}

/**
 * The idea is that you set all port parameters, and then initialize the port.
 * This allocates all resources and makes the port usable. However, you can't
 * change the port properties anymore after this.
 *
 * @return true if successfull. false if not (all resources are freed).
 */
bool Port::init() {
    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    if (m_buffersize==0) {
        debugFatal("Cannot initialize a port with buffersize=0\n");
        return false;
    }

    switch (m_BufferType) {
        case E_PointerBuffer:
            if (m_use_external_buffer) {
                // don't do anything
            } else if (!allocateInternalBuffer()) {
                debugFatal("Could not allocate internal buffer!\n");
                return false;
            }
            break;

        case E_RingBuffer:
            if (m_use_external_buffer) {
                debugFatal("Cannot use an external ringbuffer! \n");
                return false;
            } else if (!allocateInternalRingBuffer()) {
                debugFatal("Could not allocate internal ringbuffer!\n");
                return false;
            }
            break;
        default:
            debugFatal("Unsupported buffer type! (%d)\n",(int)m_BufferType);
            return false;
            break;
    }

    m_initialized=true;

    m_eventsize=getEventSize(); // this won't change, so cache it

    return m_initialized;
}

bool Port::reset() {
    if (m_BufferType==E_RingBuffer) {
        ffado_ringbuffer_reset(m_ringbuffer);
    }
    return true;
};


void Port::setVerboseLevel(int l) {
    setDebugLevel(l);
}

bool Port::setName(std::string name) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting name to %s for port %s\n",name.c_str(),m_Name.c_str());

    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    m_Name=name;

    return true;
}

bool Port::setBufferSize(unsigned int newsize) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting buffersize to %d for port %s\n",newsize,m_Name.c_str());
    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    m_buffersize=newsize;
    return true;

}

unsigned int Port::getEventSize() {
    switch (m_DataType) {
        case E_Float:
            return sizeof(float);
        case E_Int24: // 24 bit 2's complement, packed in a 32bit integer (LSB's)
            return sizeof(uint32_t);
        case E_MidiEvent:
            return sizeof(uint32_t);
        default:
            return 0;
    }
}

bool Port::setDataType(enum E_DataType d) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting datatype to %d for port %s\n",(int) d,m_Name.c_str());
    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    // do some sanity checks
    bool type_is_ok=false;
    switch (m_PortType) {
        case E_Audio:
            if(d == E_Int24) type_is_ok=true;
            if(d == E_Float) type_is_ok=true;
            break;
        case E_Midi:
            if(d == E_MidiEvent) type_is_ok=true;
            break;
        case E_Control:
            if(d == E_Default) type_is_ok=true;
            break;
        default:
            break;
    }

    if(!type_is_ok) {
        debugFatal("Datatype not supported by this type of port!\n");
        return false;
    }

    m_DataType=d;
    return true;
}

bool Port::setSignalType(enum E_SignalType s) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting signaltype to %d for port %s\n",(int)s,m_Name.c_str());
    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    // do some sanity checks
    bool type_is_ok=false;
    switch (m_PortType) {
        case E_Audio:
            if(s == E_PeriodSignalled) type_is_ok=true;
            break;
        case E_Midi:
            if(s == E_PacketSignalled) type_is_ok=true;
            break;
        case E_Control:
            if(s == E_PeriodSignalled) type_is_ok=true;
            break;
        default:
            break;
    }

    if(!type_is_ok) {
        debugFatal("Signalling type not supported by this type of port!\n");
        return false;
    }

    m_SignalType=s;
    return true;

}

bool Port::setBufferType(enum E_BufferType b) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting buffer type to %d for port %s\n",(int)b,m_Name.c_str());
    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    // do some sanity checks
    bool type_is_ok=false;
    switch (m_PortType) {
        case E_Audio:
            if(b == E_PointerBuffer) type_is_ok=true;
            break;
        case E_Midi:
            if(b == E_RingBuffer) type_is_ok=true;
            break;
        case E_Control:
            break;
        default:
            break;
    }

    if(!type_is_ok) {
        debugFatal("Buffer type not supported by this type of port!\n");
        return false;
    }

    m_BufferType=b;
    return true;

}

bool Port::useExternalBuffer(bool b) {

    // If called on an initialised stream but the request isn't for a change silently
    // allow it (relied on by C API as used by jack backend driver)
    if (m_initialized && m_use_external_buffer==b)
        return true;

    debugOutput( DEBUG_LEVEL_VERBOSE, "Setting external buffer use to %d for port %s\n",(int)b,m_Name.c_str());

    if (m_initialized) {
        debugFatal("Port already initialized... (%s)\n",m_Name.c_str());
        return false;
    }

    m_use_external_buffer=b;
    return true;
}

// buffer handling api's for pointer buffers
/**
 * Get the buffer address (being the external or the internal one).
 *
 * @param buff
 */
void *Port::getBufferAddress() {
    assert(m_BufferType==E_PointerBuffer);
    return m_buffer;
};

/**
 * Set the external buffer address.
 * only call this when specifying an external buffer before init()
 *
 * @param buff
 */
void Port::setExternalBufferAddress(void *buff) {
    assert(m_BufferType==E_PointerBuffer);
    assert(m_use_external_buffer); // don't call this with an internal buffer!
    m_buffer=buff;
};

// buffer handling api's for ringbuffers
bool Port::writeEvent(void *event) {
    assert(m_BufferType==E_RingBuffer);
    assert(m_ringbuffer);

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Writing event %08X with size %d to port %s\n",*((quadlet_t *)event),m_eventsize, m_Name.c_str());

    return (ffado_ringbuffer_write(m_ringbuffer, (char *)event, m_eventsize)==m_eventsize);
}

bool Port::readEvent(void *event) {
    assert(m_ringbuffer);

    unsigned int read=ffado_ringbuffer_read(m_ringbuffer, (char *)event, m_eventsize);

    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Reading event %X with size %d from port %s\n",*((quadlet_t *)event),m_eventsize,m_Name.c_str());
    return (read==m_eventsize);
}

int Port::writeEvents(void *event, unsigned int nevents) {
    assert(m_BufferType==E_RingBuffer);
    assert(m_ringbuffer);

    unsigned int bytes2write=m_eventsize*nevents;

    unsigned int written=ffado_ringbuffer_write(m_ringbuffer, (char *)event,bytes2write)/m_eventsize;

#ifdef DEBUG
    if(written) {
        unsigned int i=0;
        quadlet_t * tmp=(quadlet_t *)event;
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Written %d events (",written);
        for (i=0;i<written;i++) {
            debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "%X ", *(tmp+i));
        }
        debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, ") to port %s\n",m_Name.c_str());
    }
#endif

    return written;

}

int Port::readEvents(void *event, unsigned int nevents) {
    assert(m_ringbuffer);

    unsigned int bytes2read=m_eventsize*nevents;

    unsigned int read=ffado_ringbuffer_read(m_ringbuffer, (char *)event, bytes2read)/m_eventsize;

#ifdef DEBUG
    if(read) {
        unsigned int i=0;
        quadlet_t * tmp=(quadlet_t *)event;
        debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Read %d events (",read);
        for (i=0;i<read;i++) {
            debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, "%X ", *(tmp+i));
        }
        debugOutputShort(DEBUG_LEVEL_VERY_VERBOSE, ") from port %s\n",m_Name.c_str());
    }
#endif

    return read;
}

/* rate control */
bool Port::canRead() {
    bool byte_present_in_buffer;

    bool retval=false;

    assert(m_ringbuffer);

    byte_present_in_buffer=(ffado_ringbuffer_read_space(m_ringbuffer) >= m_eventsize);

    if(byte_present_in_buffer) {

        if(!m_do_ratecontrol) {
            return true;
        }

        if(m_rate_counter <= 0) {
            // update the counter
            if(m_average_ratecontrol) {
                m_rate_counter += m_event_interval;
                assert(m_rate_counter<m_event_interval);
            } else {
                m_rate_counter = m_event_interval;
            }

            retval=true;
        } else {
            debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Rate limit (%s)! rate_counter=%d \n",m_Name.c_str(),m_rate_counter);

        }
    }


    m_rate_counter -= m_slot_interval;

    // we have to limit the decrement of the ratecounter somehow.
    // m_rate_counter_minimum is initialized when enabling ratecontrol
    if(m_rate_counter < m_rate_counter_minimum) {
        m_rate_counter = m_rate_counter_minimum;
    }

    return retval;
}

bool Port::useRateControl(bool use, unsigned int slot_interval,
                                unsigned int event_interval, bool average) {

    if (use) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Enabling rate control for port %s...\n",m_Name.c_str());
        if(slot_interval>event_interval) {
            debugWarning("Rate control not needed!\n",m_Name.c_str());
            m_do_ratecontrol=false;
            return false;
        }
        if(slot_interval==0) {
            debugFatal("Cannot have slot interval == 0!\n");
            m_do_ratecontrol=false;
            return false;
        }
        if(event_interval==0) {
            debugFatal("Cannot have event interval == 0!\n");
            m_do_ratecontrol=false;
            return false;
        }
        m_do_ratecontrol=use;
        m_event_interval=event_interval;
        m_slot_interval=slot_interval;
        m_rate_counter=0;

        // NOTE: pretty arbitrary, but in average mode this limits the peak stream rate
        m_rate_counter_minimum=-(2*event_interval);

        m_average_ratecontrol=average;

    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Disabling rate control for port %s...\n",m_Name.c_str());
        m_do_ratecontrol=use;
    }
    return true;
}

/// Enable the port. (this can be called anytime)
void
Port::enable()  {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Enabling port %s...\n",m_Name.c_str());
    m_disabled=false;
};

/// Disable the port. (this can be called anytime)
void
Port::disable() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Disabling port %s...\n",m_Name.c_str());
    m_disabled=false;
};


/* Private functions */

bool Port::allocateInternalBuffer() {
    int event_size=getEventSize();

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Allocating internal buffer of %d events with size %d (%s)\n",
                m_buffersize, event_size, m_Name.c_str());

    if(m_buffer) {
        debugWarning("already has an internal buffer attached, re-allocating\n");
        freeInternalBuffer();
    }

    m_buffer=calloc(m_buffersize,event_size);
    if (!m_buffer) {
        debugFatal("could not allocate internal buffer\n");
        m_buffersize=0;
        return false;
    }

    return true;
}

void Port::freeInternalBuffer() {
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Freeing internal buffer (%s)\n",m_Name.c_str());

    if(m_buffer) {
        free(m_buffer);
        m_buffer=0;
    }
}

bool Port::allocateInternalRingBuffer() {
    int event_size=getEventSize();

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Allocating internal buffer of %d events with size %d (%s)\n",
                m_buffersize, event_size, m_Name.c_str());

    if(m_ringbuffer) {
        debugWarning("already has an internal ringbuffer attached, re-allocating\n");
        freeInternalRingBuffer();
    }

    m_ringbuffer=ffado_ringbuffer_create(m_buffersize * event_size);
    if (!m_ringbuffer) {
        debugFatal("could not allocate internal ringbuffer\n");
        m_buffersize=0;
        return false;
    }

    return true;
}

void Port::freeInternalRingBuffer() {
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Freeing internal ringbuffer (%s)\n",m_Name.c_str());

    if(m_ringbuffer) {
        ffado_ringbuffer_free(m_ringbuffer);
        m_ringbuffer=0;
    }
}

}
