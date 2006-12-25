/* iavdevice.h
 * Copyright (C) 2006 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef IAVDEVICE_H
#define IAVDEVICE_H

#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

class ConfigRom;
class Ieee1394Service;

namespace FreebobStreaming {
	class StreamProcessor;
}
/*!
\brief Interface that is to be implemented to support a device.

 This interface should be used to implement freebob support for a specific device.

*/
class IAvDevice {
public:
	virtual ~IAvDevice() {}
	
	/// Returns the ConfigRom object of the device node.
	virtual ConfigRom& getConfigRom() const = 0;
	
	/**
	 * This is called by the probe function to discover & configure the device
	 *
	 * 
	 * @return true if the device was discovered successfully
	 */
	virtual bool discover() = 0;
	
	/**
	 * Set the samping frequency
	 * @param samplingFrequency 
	 * @return true if successfull
	 */
	virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency ) = 0;
	/**
	 * get the samplingfrequency as an integer
	 * @return the sampling frequency as integer
	 */
	virtual int getSamplingFrequency( ) = 0;
	
    /**
     * This is called by the device manager to give the device
     * a unique ID.
     *
     * The purpose of this is to allow for unique port naming
     * in case there are multiple identical devices on the bus.
     * Some audio API's (e.g. jack) don't work properly when the
     * port names are not unique.
     *
     * Say you have two devices having a port named OutputLeft.
     * This can cause the streaming
     * part to present two OutputLeft ports to the audio API,
     * which won't work. This ID will allow you to construct
     * the port names as 'dev1_OutputLeft' and 'dev2_OutputLeft'
     *
     * \note Currently this is a simple integer that is equal to
     *       the position of the device in the devicemanager's 
     *       device list. Therefore it is dependant on the order
     *       in which the devices are detected. The side-effect
     *       of this is that it is dependant on the bus topology
     *       and history (e.g. busresets etc). This makes that
     *       these ID's are not fixed to a specific physical device.
     *       At some point, we will replaced this with a GUID based 
     *       approach, which is tied to a physiscal device and is
     *       bus & time independant. 
     *
     * @param id 
     * @return true if successfull
     */
    virtual bool setId(unsigned int id) = 0;
	
	/**
	 * Constructs an XML description of the device [obsolete]
	 *
	 * this is a leftover from v1.0 and isn't used
	 * just provide an empty implementation that returns true
	 * untill it is removed
	 *
	 * @param deviceNode 
	 * @return true if successfull
	 */
	virtual bool addXmlDescription( xmlNodePtr deviceNode ) = 0;
	
	/**
	 * Outputs the device configuration to stderr/stdout [debug helper]
	 *
	 * This function prints out a (detailed) description of the 
	 * device detected, and its configuration.
	 */
	virtual void showDevice() const = 0;
	
	/** 
	 * Prepare the device
	 *  \todo when exactly is this called? and what should it do?
	 */
	virtual bool prepare() = 0;
	
	/// 
	/**
	 * Returns the number of ISO streams implemented/used by this device
	 *
	 * Most likely this is 2 streams, i.e. one transmit stream and one
	 * receive stream. However there are devices that implement more, for 
	 * example BeBoB's implement 4 streams: 
	 * - 2 audio streams (1 xmit/1 recv)
	 * - 2 sync streams (1 xmit/1 recv), which are an optional sync source
	 *   for the device.
	 *
	 * \note you have to have a StreamProcessor for every stream. I.e.
	 *       getStreamProcessorByIndex(i) should return a valid StreamProcessor
	 *       for i=0 to i=getStreamCount()-1
	 * \note
	 *
	 * @return 
	 */
	virtual int getStreamCount() = 0;
	
	/** 
	 * Returns the StreamProcessor object for the stream with index i
	 * \note a streamprocessor returned by getStreamProcessorByIndex(i)
	 *       cannot be the same object as one returned by
	 *       getStreamProcessorByIndex(j) if i isn't equal to j
	 * \note you could have two streamprocessors handling the same ISO
	 *       channel if that's needed
	 * \param i : Stream index
	 * \pre \ref i smaller than getStreamCount()
	 * @return a StreamProcessor object if successfull, NULL otherwise
	 */
	virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i) = 0;
	
	/** 
	 * starts the stream with index i
	 *
	 * This function is called by the streaming layer when this stream should
	 * be started, i.e. the device should start sending data or should be prepared to 
	 * be ready to receive data.
	 *
	 * It returns the channel number that was assigned for this stream.
	 * 
	 * For BeBoB's this implements the IEC61883 connection management procedure (CMP).
	 * This CMP is a way to negotiate the channel that should be used, the bandwidth 
	 * consumed and to start the device's streaming mode.
	 *
	 * Note that this IEC61883 CMP is nescessary to allow multiple ISO devices on the bus,
	 * because otherwise there is no way to know what ISO channels are already in use.
	 * or is there?
	 * \note I wouldn't know how to assign channels for devices that don't implement
	 *       IEC61883 CMP (like the Motu?). As long as there is only one device on the
	 *       bus this is no problem and can be solved with static assignments. 
	 *
	 * \param i : Stream index
	 * \pre \ref i smaller than getStreamCount()
	 * \return the ISO channel number assigned to this stream
	 */
	virtual int startStreamByIndex(int i) = 0;
	
	/** 
	 * stops the stream with index \ref i
	 *
	 * \param i : Stream index
	 * \pre \ref i smaller than getStreamCount()
	 * \return 0 on success, -1 else
	 */
	virtual int stopStreamByIndex(int i) = 0;

};

#endif
