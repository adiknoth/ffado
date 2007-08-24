/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef FFADODEVICE_H
#define FFADODEVICE_H

#include "libutil/OptionContainer.h"
#include "libosc/OscNode.h"

class ConfigRom;
class Ieee1394Service;

namespace Streaming {
    class StreamProcessor;
}

/*!
@brief Base class for device support

 This class should be subclassed to implement ffado support
 for a specific device.

*/
class FFADODevice
    : public Util::OptionContainer,
      public OSC::OscNode
{
public:
    FFADODevice( std::auto_ptr< ConfigRom >( configRom ),
                    Ieee1394Service& ieee1394service,
                    int nodeId );

    virtual ~FFADODevice() {};

    
    /**
     * @brief Compares the GUID of two FFADODevices
     *
     * This function compares the GUID of two FFADODevices and returns true
     * if the GUID of @ref a is larger than the GUID of @ref b . This is intended
     * to be used with the STL sort() algorithm.
     * 
     * Note that GUID's are converted to integers for this.
     * 
     * @param a pointer to first FFADODevice
     * @param b pointer to second FFADODevice
     * 
     * @returns true if the GUID of @ref a is larger than the GUID of @ref b .
     */
    static bool compareGUID( FFADODevice *a, FFADODevice *b );

    /// Returns the 1394 service of the FFADO device
    virtual Ieee1394Service& get1394Service()
        { return *m_p1394Service; };
    /// Returns the ConfigRom object of the device node.
    virtual ConfigRom& getConfigRom() const;
    
    /**
     * @brief Called by DeviceManager to load device model from cache.
     *
     * This function is called before discover in order to speed up
     * system initializing.
     *
     * @returns true if device was cached and successfully loaded from cache
     */
    virtual bool loadFromCache();

    /**
     * @brief Called by DeviceManager to allow device driver to save a cache version
     * of the current configuration.
     *
     * @returns true if caching was successful. False doesn't mean an error just,
     * the driver was unable to store the configuration
     */
    virtual bool saveCache();

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
    virtual bool setSamplingFrequency( int samplingFrequency ) = 0;
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
     *       approach, which is tied to a physical device and is
     *       bus & time independant.
     *
     * @param id
     * @return true if successful
     */
    bool setId(unsigned int id);

    /**
     * @brief Outputs the device configuration to stderr/stdout [debug helper]
     *
     * This function prints out a (detailed) description of the
     * device detected, and its configuration.
     */
    virtual void showDevice();

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
     * @brief Enable streaming on all 'started' streams
     *
     * Enables the ISO streaming on all streams that are 'started'
     * using startStreamByIndex. This is useful to control a 'master enable'
     * function on the device.
     *
     * @return true if successful
     */
    virtual bool enableStreaming();

    /**
     * @brief Disable streaming on all streams
     *
     * Disables ISO streaming on all streams.
     * This is useful to control a 'master enable'
     * function on the device.
     *
     * @return true if successful
     */
    virtual bool disableStreaming();

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
    virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i) = 0;

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

    /**
     * set verbosity level
     */
    virtual void setVerboseLevel(int l);

    /**
     * @brief return the node id of this device
     *
     * @return the node id
     */
    int getNodeId() { return m_nodeId;};

protected:
    std::auto_ptr<ConfigRom>( m_pConfigRom );
    Ieee1394Service*          m_p1394Service;
    int                       m_nodeId;

    DECLARE_DEBUG_MODULE;
};

#endif
