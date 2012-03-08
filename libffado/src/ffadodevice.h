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

#ifndef FFADODEVICE_H
#define FFADODEVICE_H

#include "libutil/OptionContainer.h"
#include "libutil/PosixMutex.h"

#include "libcontrol/BasicElements.h"

#include "libieee1394/vendor_model_ids.h"

#include <vector>
#include <string>

class DeviceManager;
class ConfigRom;
class Ieee1394Service;

namespace Streaming {
    class StreamProcessor;
}

namespace Control {
    class Container;
}

/*!
@brief Base class for device support

 This class should be subclassed to implement ffado support
 for a specific device.

*/
class FFADODevice
    : public Util::OptionContainer,
      public Control::Container
{
public:
    FFADODevice( DeviceManager&, std::auto_ptr< ConfigRom >( configRom ) );

    virtual ~FFADODevice();

    /**
     * @brief Compares the GUID of two FFADODevices
     *
     * This function compares the GUID of two FFADODevices and returns true
     * if the GUID of a is larger than the GUID of b . This is intended to be
     * used with the STL sort() algorithm.
     * 
     * Note that GUID's are converted to integers for this.
     * 
     * @param a pointer to first FFADODevice
     * @param b pointer to second FFADODevice
     * 
     * @returns true if the GUID of a is larger than the GUID of b .
     */
    static bool compareGUID( FFADODevice *a, FFADODevice *b );

    /// Returns the 1394 service of the FFADO device
    virtual Ieee1394Service& get1394Service();
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
     * @brief Called by DeviceManager to check whether a device requires rediscovery
     *
     * This function is called to figure out if the device has to be rediscovered
     * e.g. after a bus reset where the device internal structure changed.
     *
     * @returns true if device requires rediscovery
     */
    virtual bool needsRediscovery();

    /**
     * @brief This is called by the DeviceManager to create an instance of the device
     *
     * This function enables the FFADODevice to return a subclass of itself should that
     * be needed. If we don't do this we'd need to know about the subclasses in the 
     * devicemanager, whilst now we don't.
     * 
     * The function should return an instance of either the class itself or a subclass
     * of itself.
     *
     * This should be overridden in any subclass.
     *
     * @return a new instance of the AvDevice type, NULL when unsuccessful
     */
    static FFADODevice * createDevice( std::auto_ptr<ConfigRom>( x ));

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
     * @brief get the supported sampling frequencies
     * @return a vector containing the supported sampling frequencies
     */
    virtual std::vector<int> getSupportedSamplingFrequencies( ) = 0;

    /**
     * @brief sync state enum
     */
    enum eSyncState {
        eSS_Unknown=0,
        eSS_Locked=1,
        eSS_Unlocked=2,
    };

    /**
     * @brief gets the devices current synchronization state
     * @return the device's sync state
     */
    virtual enum eSyncState getSyncState( );

    /**
     * @brief clock source types
     */
    enum eClockSourceType {
        eCT_Invalid,   ///> invalid entry (e.g. on error)
        eCT_Auto,      ///> automatically select clock source
        eCT_Internal,  ///> internal sync (unspecified)
        eCT_1394Bus,   ///> Sync on the 1394 bus clock (e.g. CSP)
        eCT_SytMatch,  ///> SYT match on incoming audio stream
        eCT_SytStream, ///> SYT match on incoming sync stream
        eCT_WordClock, ///> SYT on WordClock input
        eCT_SPDIF,     ///> SYT on SPDIF input
        eCT_ADAT,      ///> SYT on ADAT input
        eCT_TDIF,      ///> SYT on TDIF input
        eCT_AES,       ///> SYT on AES input
        eCT_SMPTE,     ///> SMPTE clock
    };

    /**
     * @brief convert the clock source type to a C string
     * @return a C string describing the clock source type
     */
    static const char *ClockSourceTypeToString(enum eClockSourceType);

    /**
     * @brief Clock source identification struct
     */
    class ClockSource {
        public:
        ClockSource()
            : type( eCT_Invalid )
            , id( 0 )
            , valid( false )
            , active( false )
            , locked( true )
            , slipping( false )
            , description( "" )
        {};
        /// indicates the type of the clock source (e.g. eCT_ADAT)
        enum eClockSourceType type;
        /// indicated the id of the clock source (e.g. id=1 => clocksource is ADAT_1)
        unsigned int id;
        /// is the clock source valid (i.e. can be selected) at this moment?
        bool valid;
        /// is the clock source active at this moment?
        bool active;
        /// is the clock source locked?
        bool locked;
        /// is the clock source slipping?
        bool slipping;
        /// description of the clock struct (optional)
        std::string description;
        
        bool operator==(const ClockSource& x) {
            return (type == x.type) && (id == x.id);
        }
    };

    typedef std::vector< ClockSource > ClockSourceVector;
    typedef std::vector< ClockSource >::iterator ClockSourceVectorIterator;

    /**
     * @brief Get the clocksources supported by this device
     *
     * This function returns a vector of ClockSource structures that contains
     * one entry for every clock source supported by the device.
     *
     * @returns a vector of ClockSource structures
     */
    virtual ClockSourceVector getSupportedClockSources() = 0;


    /**
     * @brief Sets the active clock source of this device
     *
     * This function sets the clock source of the device.
     *
     * @returns true upon success. false upon failure.
     */
    virtual bool setActiveClockSource(ClockSource) = 0;

    /**
     * @brief Returns the active clock source of this device
     *
     * This function returns the active clock source of the device.
     *
     * @returns the active ClockSource
     */
    virtual ClockSource getActiveClockSource() = 0;

    /**
     * @brief stream states
     */
    enum eStreamingState {
        eSS_Idle = 0,        ///> not streaming
        eSS_Sending = 1,     ///> the device is sending a stream
        eSS_Receiving = 2,   ///> the device is receiving a stream
        eSS_Both = 3,        ///> the device is sending and receiving a stream
    };

    /**
     * @brief gets the devices current synchronization state
     * @return the device's sync state
     */
    virtual enum eStreamingState getStreamingState();

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
     * @brief Performs operations needed to prepare for a stream start
     *
     * This is called by the streaming layer just before streaming is
     * started.  It provides a place where reset activity can be done which
     * ensures the object is ready to restart streaming even if streaming
     * has been previously started and stopped.
     *
     * @return true if successful, false if not
     */
    virtual bool resetForStreaming() { return true; }

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
     * @param i : Stream index (i has to be smaller than getStreamCount())
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
     * @param i : Stream index (i has to be smaller than getStreamCount())
     * @return true if successful, false if not
     */
    virtual bool startStreamByIndex(int i) = 0;

    /**
     * @brief stops the stream with index i
     *
     * @param i : Stream index (i has to be smaller than getStreamCount())
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
    int getNodeId();

    /**
     * @brief return the nick name of this device
     *
     * @return string containing the name
     */
    virtual std::string getNickname();

    /**
     * @brief set the nick name of this device
     * @param name new nickname
     * @return true if successful
     */
    virtual bool setNickname(std::string name);

    /**
     * @brief return whether the nick name of this device can be changed
     *
     * @return true if the nick can be changed
     */
    virtual bool canChangeNickname();

    /**
     * @brief handle a bus reset
     *
     * Called whenever a bus reset is detected. Handle everything
     * that has to be done to cope with a bus reset.
     *
     */
    // FIXME: not virtual?
    void handleBusReset();

    // the Control::Container functions
    virtual std::string getName();
    virtual bool setName( std::string n )
        { return false; };

    DeviceManager& getDeviceManager()
        {return m_pDeviceManager;};
private:
    std::auto_ptr<ConfigRom>( m_pConfigRom );
    DeviceManager& m_pDeviceManager;
    Control::Container* m_genericContainer;
protected:
    DECLARE_DEBUG_MODULE;
    Util::PosixMutex m_DeviceMutex;
};

#endif
