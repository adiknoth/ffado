/*
 * Copyright (C) 2005-2008, 2011 by Jonathan Woithe
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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


#include "libutil/float_cast.h"

#include "DigidesignReceiveStreamProcessor.h"
#include "DigidesignPort.h"
#include "../StreamProcessorManager.h"
#include "devicemanager.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"
#include "libieee1394/cycletimer.h"

#include "libutil/ByteSwap.h"

#include <cstring>
#include <math.h>
#include <assert.h>


namespace Streaming {

DigidesignReceiveStreamProcessor::DigidesignReceiveStreamProcessor(FFADODevice &parent, unsigned int event_size)
    : StreamProcessor(parent, ePT_Receive)
    , m_event_size( event_size )
{
    // Add whatever else needs to be initialised.
}

unsigned int
DigidesignReceiveStreamProcessor::getMaxPacketSize() {

    // Frame rate is accessible with something like this:
    //   int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();

    // What's returned here is the maximum packet size seen at the current
    // frame rate.  This depends both on the device and its configuration,
    // and is futher complicated by the IN/OUT channel asymmetry in some
    // devices.  To avoid most of these complications, just return the
    // largest packet sizes seen by any supported Digidesign device.

    // Fill in the requsite details.
    return 0;
}

unsigned int
DigidesignReceiveStreamProcessor::getNominalFramesPerPacket() {
    // Return the number of frames per firewire iso packet.  A "frame" here is a collection
    // of a single audio sample from all active audio channels.  If this depends on the
    // sample rate, that can be obtained using something like this:
    //   int framerate = m_Parent.getDeviceManager().getStreamProcessorManager().getNominalRate();
    return 0;
}

bool
DigidesignReceiveStreamProcessor::prepareChild() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Preparing (%p)...\n", this);

    // If the receive stream processor requires that things be set up which
    // could not be done in the constructor, here is where they should be
    // done.  Return true on success, or false if the setup failed for some
    // reason.  In most cases, this method will do nothing.
    
    return true;
}

enum StreamProcessor::eChildReturnValue
DigidesignReceiveStreamProcessor::processPacketHeader(unsigned char *data, unsigned int length, 
                                                unsigned char tag, unsigned char sy,
                                                uint32_t pkt_ctr)
{
    // This function will be called once for each incoming packet.  It
    // should check to ensure the packet contains valid data and (if it is)
    // extract a timestamp from it.  "data" points to the iso packet's
    // contents - no assumption is made about what constitutes a "header"
    // because each device's protocol is different.  Note that the firewire
    // ISO header is not included in "data".
    //
    // The return value should be one of the eCRV_* constants.  The two
    // most commonly used here are:
    //   eCRV_Invalid = unrecognised packet.  No further processing needed.
    //   eCRV_OK = packet contains audio data and requires processing.
    //
    // The decision as to what constitutes a valid data packet depends on
    // the format of the iso packets sent by the Digidesign hardware.
    //
    // Other parameters to this function contain selected information from
    // the firewire ISO header which came with this packet:
    //   - length = length in bytes of the content pointed to by "data".
    //   - tag = the iso packet header's "tag" field.
    //   - sy = the sy field from the iso packet header.
    //   - pkt_ctr = the value of the iso timer at the time the packet was
    //     received by the PC.
    //
    // If a valid packet has been received from the Digidesign device, 
    // this method should set the object's m_last_timestamp field to the
    // timestamp of the last frame in the packet.  Determining this is
    // device specific.  Some devices embed this in the stream, while
    // others require that it be synthesised.  FFADO uses this timestamp
    // to keep the transmit stream in sync with the receive stream.

    // An example implementation:
    //
    // if (packet is valid) {
    //   m_last_timestamp = something useful
    //   return eCRV_OK;
    // } else {
    //   return eCRV_Invalid;
    // }

    return eCRV_Invalid;
}

enum StreamProcessor::eChildReturnValue
DigidesignReceiveStreamProcessor::processPacketData(unsigned char *data, unsigned int length) {

    // This method is called once per ISO packet which has been flagged as 
    // valid by processPacketHeader().  "data" points to "length" bytes
    // of data.  "data" will in general have the same content as was
    // presented to processPacketHeader().
    //
    // Conceptually this method is quite simple.  Once you know the number
    // of "events" (aka frames) in the packet - either through prior
    // knowledge or by calculation based on the packet length - one simply
    // calls the associated data buffer's writeFrames() method to get the
    // data from the iso packet and into the internal buffer.  The details
    // as to how this is done are encapsulated in later methods.

    // First we either calculate or obtain the number of events in the packet.
    unsigned int n_events = 0;

    // Call writeFrames() to process the data.  m_last_timestamp should have
    // been set up by processPacketHeader().  The pointer passed to
    // writeFrames (data in this case) should for convenience be the pointer
    // to the start of the first frame's data.  If for example the packet
    // starts with its own 8-byte packet header (as opposed to the standard
    // iso packet header which is stripped off before this method is
    // called), (data+8) would be supplied instead.
    if(m_data_buffer->writeFrames(n_events, (char *)(data), m_last_timestamp)) {
        return eCRV_OK;
    } else {
        return eCRV_XRun;
    }
}

/***********************************************
 * Encoding/Decoding API                       *
 ***********************************************/
/**
 * \brief write received events to the port ringbuffers.
 */
bool DigidesignReceiveStreamProcessor::processReadBlock(char *data,
                       unsigned int nevents, unsigned int offset)
{
    // This function is called by the Encoding/Decoding engine encapsulated
    // by the call to writeFrames().  It should extract data from the "data"
    // array into the device's port ringbuffers.  A "port" in this context
    // is analogous to a jack port - one has one port for each audio channel
    // provided by the device.
    //
    // Each "port" is processed in turn.  The decodeDigidesign*() methods
    // are called to do the actual work.
    //
    // "offset" is an offset (in frames) from the start of the ring buffer
    // where the incoming data should be copied to.  "nevents" is the 
    // number of events (aka frames) which need to be processed for each 
    // port.

    bool no_problem=true;

    for ( PortVectorIterator it = m_Ports.begin();
          it != m_Ports.end();
          ++it ) {
        if((*it)->isDisabled()) {continue;};

        Port *port=(*it);

        switch(port->getPortType()) {

        case Port::E_Audio:
            if(decodeDigidesignEventsToPort(static_cast<DigidesignAudioPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                debugWarning("Could not decode packet data to port %s\n",(*it)->getName().c_str());
                no_problem=false;
            }
            break;
        case Port::E_Midi:
             if(decodeDigidesignMidiEventsToPort(static_cast<DigidesignMidiPort *>(*it), (quadlet_t *)data, offset, nevents)) {
                 debugWarning("Could not decode packet midi data to port %s\n",(*it)->getName().c_str());
                 no_problem=false;
             }
            break;

        default: // ignore
            break;
        }
    }
    return no_problem;
}

signed int DigidesignReceiveStreamProcessor::decodeDigidesignEventsToPort(DigidesignAudioPort *p,
        quadlet_t *data, unsigned int offset, unsigned int nevents)
{

    // Decode "nevents" samples corresponding to the audio port "p" from the
    // device datastream "data" into each port's ringbuffer at an offset of
    // "offset" samples from the ringbuffer's origin.  Return value should
    // be 0.
    //
    // In theory the type of data handled by the ringbuffers can be
    // any one of the eADT_* settings.  This can be determined by calling
    //   m_StreamProcessorManager.getAudioDataType().
    // For use with JACK (the most common use case of FFADO) this will
    // always be StreamProcessorManager::eADT_Float, but one should at
    // least allow for the possibility of others, even if one chooses not
    // to support them yet.
    //
    // The getPosition() port method returns the "position" field of the
    // port.  This is used to describe where in the frame the channel's data
    // is to be found.  Other fields can be added to DigidesignAudioPort to
    // store other details which might be required to decode the data
    // packets.  In general "position" is about the only one normally
    // needed.  Whether "position" is in bytes, frames, or something else is
    // really determined by the contents of this method.  Choose whatever
    // is convenient.

    unsigned int j=0;

    // The following is an example implementation showing the general idea. 
    // It will need to be changed to suit the protocol of the Digidesign
    // devices.  It assumes that data is supplied by the device in packed
    // 24-bit integers in big endian format, and that the port's "position"
    // is in bytes.  Note that the m_event_size object member is assumed to
    // have been set up previous to be the event size in bytes.  If for a
    // given implementation it is more convenient to use a "signed int *"
    // for src_data one could have m_event_size measured in quadlets (32-bit
    // integers).  Again, it doesn't really matter so long as you're
    // consistent.

    // Use char here since a port's source address won't necessarily be
    // aligned; use of an unaligned quadlet_t may cause issues on
    // certain architectures.
    unsigned char *src_data;
    src_data = (unsigned char *)data + p->getPosition();

    switch(m_StreamProcessorManager.getAudioDataType()) {
        case StreamProcessorManager::eADT_Float:
            {
                const float multiplier = 1.0f / (float)(0x7FFFFF);
                float *buffer=(float *)(p->getBufferAddress());

                assert(nevents + offset <= p->getBufferSize());

                buffer+=offset;

                for (j = 0; j < nevents; j += 1) { // decode max nsamples

                    signed int v = (*src_data<<16)+(*(src_data+1)<<8)+*(src_data+2);
                    /* Sign-extend highest bit of incoming 24-bit integer */
                    if (*src_data & 0x80)
                      v |= 0xff000000;
                    *buffer = v * multiplier;
                    buffer++;
                    src_data += m_event_size;
                }
            }
            break;

        case StreamProcessorManager::eADT_Int24:
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
                    // This isn't strictly needed since E_Int24 is a 24-bit,
                    // but doing so shouldn't break anything and makes the data
                    // easier to deal with during debugging.
                    if (*src_data & 0x80)
                        *buffer |= 0xff000000;

                    buffer++;
                    src_data+=m_event_size;
                }
            }
            break;

        default:
            // Unsupported type.
            break;
    }

    return 0;
}

int
DigidesignReceiveStreamProcessor::decodeDigidesignMidiEventsToPort(
                      DigidesignMidiPort *p, quadlet_t *data,
                      unsigned int offset, unsigned int nevents)
{
    // As for decodeDigidesignEventsToPort() except this method
    // deals with MIDI streams.  Depending on how MIDI is sent by
    // the device, this method may be structured similarly to
    // decodeDigidesignEventsToPort() or it may be completely
    // different (as it is for MOTU devices for example).  Return
    // value should be zero.
    return 0;    
}


} // end of namespace Streaming
