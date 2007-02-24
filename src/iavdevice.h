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
@brief Interface that is to be implemented to support a device.

 This interface should be used to implement freebob support 
 for a specific device.

*/
class IAvDevice {
public:
	virtual ~IAvDevice() {}
	
	/// Returns the ConfigRom object of the device node.
	virtual ConfigRom& getConfigRom() const = 0;
	
	/**
	 * @brief This is called by the DeviceManager to discover & configure the device
	 * 
	 * @return true if the device was discovered successfuly
	 */
	virtual bool discover() = 0;
	
	/**
	 * @brief Set the samping frequency
	 * @param samplingFrequency 
	 * @return true if successful
	 */
	virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency ) = 0;
	/**
	 * @brief get the samplingfrequency as an integer
	 * @return the sampling frequency as integer
	 */
	virtual int getSamplingFrequency( ) = 0;
	
    /**
     * @brief This is called by the device manager to give the device a unique ID.
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
     * @note Currently this is a simple integer that is equal to
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
     * @return true if successful
     */
    virtual bool setId(unsigned int id) = 0;
	
	/**
	 * @brief Constructs an XML description of the device [obsolete]
	 *
	 * this is a leftover from v1.0 and isn't used
	 * just provide an empty implementation that returns true
	 * untill it is removed
	 *
	 * @param deviceNode 
	 * @return true if successful, false if not
	 */
	virtual bool addXmlDescription( xmlNodePtr deviceNode ) = 0;
	
	/**
	 * @brief Outputs the device configuration to stderr/stdout [debug helper]
	 *
	 * This function prints out a (detailed) description of the 
	 * device detected, and its configuration.
	 */
	virtual void showDevice() const = 0;

	/** 
	 * @brief Lock the device
	 *
	 * This is called by the streaming layer before we start manipulating
	 * and/or using the device.
	 *
	 * It should implement the mechanisms provided by the device to 
	 * make sure that no other controller claims control of the device.
	 *
	 * @return true if successful, false if not
	 */
	virtual bool lock() = 0;
	
	/** 
	 * @brief Unlock the device
	 *
	 * This is called by the streaming layer after we finish manipulating
	 * and/or using the device.
	 *
	 * It should implement the mechanisms provided by the device to 
	 * give up exclusive control of the device.
	 *
	 * @return true if successful, false if not
	 */
	virtual bool unlock() = 0;

	/** 
	 * @brief Prepare the device
	 *
	 * This is called by the streaming layer after the configuration 
	 * parameters (e.g. sample rate) are set, and before 
	 * getStreamProcessor[*] functions are called.
	 *
	 * It should be used to prepare the device's streamprocessors
	 * based upon the device's current configuration. Normally
	 * the streaming layer will not change the device's configuration
	 * after calling this function.
	 *
	 * @return true if successful, false if not
	 */
	virtual bool prepare() = 0;
	
	/**
	 * @brief Returns the number of ISO streams implemented/used by this device
	 *
	 * Most likely this is 2 streams, i.e. one transmit stream and one
	 * receive stream. However there are devices that implement more, for 
	 * example BeBoB's implement 4 streams: 
	 * - 2 audio streams (1 xmit/1 recv)
	 * - 2 sync streams (1 xmit/1 recv), which are an optional sync source
	 *   for the device.
	 *
	 * @note you have to have a StreamProcessor for every stream. I.e.
	 *       getStreamProcessorByIndex(i) should return a valid StreamProcessor
	 *       for i=0 to i=getStreamCount()-1
	 *
	 * @return number of streams available (both transmit and receive)
	 */
	virtual int getStreamCount() = 0;
	
	/** 
	 * @brief Returns the StreamProcessor object for the stream with index i
	 *
	 * @note a streamprocessor returned by getStreamProcessorByIndex(i)
	 *       cannot be the same object as one returned by
	 *       getStreamProcessorByIndex(j) if i isn't equal to j
	 * @note you cannot have two streamprocessors handling the same ISO
	 *       channel (on the same port)
	 *
	 * @param i : Stream index
	 * @pre @ref i smaller than getStreamCount()
	 * @return a StreamProcessor object if successful, NULL otherwise
	 */
	virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i) = 0;
	
	/** 
	 * @brief starts the stream with index i
	 *
	 * This function is called by the streaming layer when this stream should
	 * be started, i.e. the device should start sending data or should be prepared to 
	 * be ready to receive data.
	 *
	 * It returns the channel number that was assigned for this stream.
	 * Channel allocation should be done using the allocation functions provided by the
	 * Ieee1394Service object that is passed in the constructor.
	 *
	 * @param i : Stream index
	 * @pre @ref i smaller than getStreamCount()
	 * @return true if successful, false if not
	 */
	virtual bool startStreamByIndex(int i) = 0;
	
	/** 
	 * @brief stops the stream with index @ref i
	 *
	 * @param i : Stream index
	 * @pre @ref i smaller than getStreamCount()
	 * @return true if successful, false if not
	 */
	virtual bool stopStreamByIndex(int i) = 0;

};

#endif
