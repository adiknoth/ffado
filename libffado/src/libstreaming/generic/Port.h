/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef __FFADO_PORT__
#define __FFADO_PORT__

#include "libutil/ringbuffer.h"

#include "debugmodule/debugmodule.h"

#include <string>
#include <stdint.h>

namespace Streaming {

/*!
\brief The Base Class for Ports

 Ports are the entities that provide the interface between the ISO streaming
 layer and the datatype-specific layer. You can define port types by subclassing
 the base port class.

 After creating a port, you have to set its parameters and then call the init() function.
 This is because a port needs information from two sources to operate:
 1) the stream composition information from the AvDevice
 2) the streaming API setup (buffer type, data type, ...)

 \note There are not much virtual functions here because of the high frequency of
       calling. We try to do everything with a base class getter, and a child class
       setter. If this isn't possible, we do a static_cast. This however can only be
       done inside the streamprocessor that handles the specific sub-class types of
       the ports. i.e. by design you should make sure that the static_cast will be
       OK.

 \todo rework the implementation into something more beautifull
*/
class Port {

public:
    friend class PortManager;

    /*
     * IMPORTANT: if you add something to any of these enum's, be sure to
     *            check the code where they are used.
     */

    /*!
    \brief Specifies the buffer type for ports

    A PointerBuffer uses the getBufferAddress() and setBufferAddres() interface
    A Ringbuffer uses the read/write interface
    */
    enum E_BufferType {
        E_PointerBuffer,
        E_RingBuffer
    };

    /*!
    \brief Specifies the signalling type for ports
    */
    enum E_SignalType {
        E_PacketSignalled, ///< the port is to be processed for every packet
        E_PeriodSignalled, ///< the port is to be processed after a period of frames
//         E_SampleSignalled ///< the port is to be processed after each frame (sample)
    };

    /*!
    \brief The datatype of the port buffer
    */
    enum E_DataType {
        E_Float,
        E_Int24,
        E_MidiEvent,
        E_Default,
    };

    /*!
    \brief The port type
    */
    enum E_PortType {
        E_Audio,
        E_Midi,
        E_Control,
    };

    /*!
    \brief The port direction
    */
    enum E_Direction {
        E_Playback,
        E_Capture,
    };

    Port(std::string name, enum E_PortType porttype, enum E_Direction direction);

    virtual ~Port()
      {};


    /// Enable the port. (this can be called anytime)
    void enable();
    /// Disable the port. (this can be called anytime)
    void disable();
    /// is the port disabled? (this can be called anytime)
    bool isDisabled() {return m_disabled;};

    /*!
    \brief Initialize the port
    */
    bool init();

    bool prepare() {return true;};
    bool reset();

    std::string getName() {return m_Name;};
    bool setName(std::string name);

    /**
     * \brief returns the size of the events in the port buffer, in bytes
     *
     */
    unsigned int getEventSize();

    /**
     * \brief sets the event type for the port buffer
     *
     * \note use before calling init()
     */
    virtual bool setDataType(enum E_DataType);

    enum E_DataType getDataType() {return m_DataType;};

    /**
     * \brief sets the event type for the port buffer
     *
     * \note use before calling init()
     */
    virtual bool setSignalType(enum E_SignalType );

    enum E_SignalType getSignalType() {return m_SignalType;}; ///< returns the signalling type of the port

    /**
     * \brief sets the buffer type for the port
     *
     * \note use before calling init()
     */
    virtual bool setBufferType(enum E_BufferType );

    enum E_BufferType getBufferType() {return m_BufferType;}; ///< returns the buffer type of the port

    enum E_PortType getPortType() {return m_PortType;}; ///< returns the port type (is fixed)
    enum E_Direction getDirection() {return m_Direction;}; ///< returns the direction (is fixed)

    /**
     * \brief returns the size of the port buffer
     *
     * counted in number of E_DataType units (events), not in bytes
     *
     */
    unsigned int getBufferSize() {return m_buffersize;};

    /**
     * \brief sets the size of the port buffer
     *
     * counted in number of E_DataType units, not in bytes
     *
     * if there is an external buffer assigned, it should
     * be large enough
     * if there is an internal buffer, it will be resized
     *
     * \note use before calling init()
     */
    virtual bool setBufferSize(unsigned int);

    /**
     * \brief use an external buffer (or not)
     *
     * \note use before calling init()
     */
    virtual bool useExternalBuffer(bool b);

    void setExternalBufferAddress(void *buff);


    /**
     * \brief enable/disable ratecontrol
     *
     * Rate control is nescessary for some types of ports (most notably
     * midi). The StreamProcessor that handles the port should call canRead()
     * everytime a 'slot' that could be filled with an event passes. The canRead
     * function will return true if
     *  (1) there is an event ready in the buffer
     *  (2) we are allowed to send an event in this slot
     *
     * Setting the rate works is done with the slot_interval and the event_interval
     * parameters. On every call to canRead(), a counter is decremented with
     * slot_interval. If the counter drops below 0, canRead() returns true and resets
     * the counter to event_interval.
     *
     * e.g. for AMDTP midi, we are only allowed to send a midi byte every 320us
     *      if the SYT interval is 8, there is exactly one midi slot every packet.
     *    therefore the slot_interval is 1/8000s (=125us), and the event_interval
     *      is 320us.
     *
     *      Note that the interval parameters are unitless, so you can adapt them
     *      to your needs. In the AMDTP case for example, when the SYT interval is 32
     *      (when the samplerate is 192kHz for example) there are 4 midi slots in
     *      each packet, making the slot time interval 125us/4 = 31.25us.
     *      The event time interval stays the same (320us). We can however set the
     *    slot_interval to 3125 and the event_interval to 32000, as we can choose
     *    the unit of the counter time step (chosen to be 10ns in this case).
     *
     * The average argument deserves some attention too. If average is true, we use
     * average rate control. This means that on average there will be a delay of
     * event_interval between two events, but that sometimes there can be a smaller
     * delay. This mode fixes the average rate of the stream.
     * If average is false, there will always be a minimal delay of event_interval
     * between two events. This means that the maximum rate of the stream is fixed,
     * and that the average rate will be lower than (or at max equal to) the rate in
     * average mode.
     *
     *
     * \note only works for the E_RingBuffer ports
     * \note use before calling init()
     *
     * @param use set this to true to use rate control
     * @param slot_interval the interval between slots
     * @param event_interval the interval between events
     * @param average use average rate control
     * @return true if rate control was enabled/disabled successfully
     */
    virtual bool useRateControl(bool use, unsigned int slot_interval,
                                unsigned int event_interval, bool average);

    bool usingRateControl() { return m_do_ratecontrol;}; ///< are we using rate control?

    /**
     * Can we send an event in this slot. subject to rate control and
     * byte availability.
     * @return true if we can send an event on this slot
     */
    bool canRead();

    // FIXME: this is not really OO, but for performance???
    void *getBufferAddress();

    // TODO: extend this with a blocking interface
    bool writeEvent(void *event); ///< write one event
    bool readEvent(void *event); ///< read one event
    int writeEvents(void *event, unsigned int nevents); ///< write multiple events
    int readEvents(void *event, unsigned int nevents); ///< read multiple events

    virtual void setVerboseLevel(int l);
    virtual void show();
    
protected:
    std::string m_Name; ///< Port name, [at construction]

    enum E_SignalType m_SignalType; ///< Signalling type, [at construction]
    enum E_BufferType m_BufferType; ///< Buffer type, [at construction]

    bool m_disabled; ///< is the port disabled?, [anytime]

    unsigned int m_buffersize;
    unsigned int m_eventsize;

    enum E_DataType m_DataType;
    enum E_PortType m_PortType;
    enum E_Direction m_Direction;

    void *m_buffer;
    ffado_ringbuffer_t *m_ringbuffer;
    bool m_use_external_buffer;

    bool m_do_ratecontrol;
    int m_event_interval;
    int m_slot_interval;
    int m_rate_counter;
    int m_rate_counter_minimum;
    bool m_average_ratecontrol;

    bool allocateInternalBuffer();
    void freeInternalBuffer();

    bool allocateInternalRingBuffer();
    void freeInternalRingBuffer();

    DECLARE_DEBUG_MODULE;
    
    // the state machine
    protected:
        enum EStates {
            E_Created,
            E_Initialized,
            E_Prepared,
            E_Running,
            E_Error
        };

        enum EStates m_State;
};

/*!
\brief The Base Class for an Audio Port


*/
class AudioPort : public Port {

public:

    AudioPort(std::string name, enum E_Direction direction)
      : Port(name, E_Audio, direction)
    {};

    virtual ~AudioPort() {};

protected:


};

/*!
\brief The Base Class for a Midi Port


*/
class MidiPort : public Port {

public:

    MidiPort(std::string name, enum E_Direction direction)
      : Port(name, E_Midi, direction)
    {};
    virtual ~MidiPort() {};


protected:


};

/*!
\brief The Base Class for a control port


*/
class ControlPort : public Port {

public:

    ControlPort(std::string name, enum E_Direction direction)
      : Port(name, E_Control, direction)
    {};
    virtual ~ControlPort() {};


protected:


};

}

#endif /* __FFADO_PORT__ */


