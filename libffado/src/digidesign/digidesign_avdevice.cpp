/*
 * Copyright (C) 2005-2011 by Jonathan Woithe
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

#warning Digidesign support is at an early development stage and is not functional

#include "config.h"

#include "digidesign/digidesign_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"
#include "libieee1394/IsoHandlerManager.h"

#include "debugmodule/debugmodule.h"

#include "libstreaming/digidesign/DigidesignPort.h"

#include "devicemanager.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"

#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace Digidesign {

Device::Device( DeviceManager& d,
                      std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_digidesign_model( DIGIDESIGN_MODEL_NONE )
    , num_channels( 0 )
    , frames_per_packet( 0 )
    , iso_tx_channel( -1 )
    , iso_rx_channel( -1 )
    , m_receiveProcessor( NULL )
    , m_transmitProcessor( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Digidesign::Device (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

Device::~Device()
{
    delete m_receiveProcessor;
    delete m_transmitProcessor;

    if (iso_tx_channel>=0 && !get1394Service().freeIsoChannel(iso_tx_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free tx iso channel %d\n", iso_tx_channel);
    }
    if (iso_rx_channel>=0 && !get1394Service().freeIsoChannel(iso_rx_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free rx iso channel %d\n", iso_rx_channel);
    }

    destroyMixer();

}

bool
Device::buildMixer() {

    destroyMixer();
    debugOutput(DEBUG_LEVEL_VERBOSE, "Building a Digidesign mixer...\n");

    // From the sounds of it there are no software-controllable device
    // settings on the Digidesign interfaces beyond those things directly
    // related to streaming.  Therefore this method probably doesn't need
    // to do anything.  If this situation changes, check out the RME
    // Device::buildMixer() method as an example of how it's done.

    return true;
}

bool
Device::destroyMixer() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "destroy mixer...\n");

    // If buildMixer() doesn't do anything then there's nothing for
    // this function to do either.
    return false;
}

bool
Device::probe( Util::Configuration& c, ConfigRom& configRom, bool generic )
{
    if (generic) {
        return false;
    } else {
        // check if device is in supported devices list.  This is where
        // you insert code which allows you to identify the Digidesign
        // devices on the bus.  For most devices the entries in the vendor
        // model table (which are consulted here) are sufficient.
        unsigned int vendorId = configRom.getNodeVendorId();
        unsigned int modelId = configRom.getModelId();

        Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );
        return c.isValid(vme) && vme.driver == Util::Configuration::eD_Digidesign;
    }
}

FFADODevice *
Device::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new Device(d, configRom );
}

bool
Device::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();
    
    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    if (c.isValid(vme) && vme.driver == Util::Configuration::eD_Digidesign) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
                     vme.vendor_name.c_str(),
                     vme.model_name.c_str());
    } else {
        debugWarning("Device '%s %s' unsupported by Digidesign driver (no generic Digidesign support)\n",
                     getConfigRom().getVendorName().c_str(), getConfigRom().getModelName().c_str());
    }

    // Here you work out what m_digidesign_model should be set to and 
    // set it accordingly.  This can then be used by the rest of the driver
    // whenever one needs to do different things for different interfaces.
    //
    // m_digidesign_model = DIGIDESIGN_MODEL_...
    //
    // This function should return true if the device is a supported device
    // or false if not.  Presently only the 003 Rack is supported.

    if (m_digidesign_model != DIGIDESIGN_MODEL_003_RACK) {
        debugError("Unsupported model\n");
        return false;
    }

    if (!buildMixer()) {
        debugWarning("Could not build mixer\n");
    }

    return true;
}

int
Device::getSamplingFrequency( ) {

    // This function should return the current sampling rate of the device.
    return 0;
}

int
Device::getConfigurationId()
{
    return 0;
}

bool
Device::setSamplingFrequency( int samplingFrequency )
{
    // Do whatever it takes to set the device's sampling rate.  Return false
    // if the requested rate cannot be done, true if it can.
    return false;
}

std::vector<int>
Device::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;

    // Generate a list of sampling rates supported by the device, in Hz.
    // This may be a relatively simple process.  For example:
    //   frequencies.push_back(44100);
    //   frequencies.push_back(48000);

    return frequencies;
}

FFADODevice::ClockSourceVector
Device::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    ClockSource s;

    // Similar to getSupportedSamplingFrequencies(), the available clock
    // sources for the device are configured here.  For example:
    //   s.valid = true;
    //   s.locked = true;
    //   s.active = true;
    //   s.slipping = false;
    //   s.type = eCT_Internal;
    //   s.description = "Internal sync";
    //   r.push_back(s);
    //
    // See src/ffadodevice.h for the full list of possible eCT_* types.
    return r;
}

bool
Device::setActiveClockSource(ClockSource s) {

    // Do whatever is necessary to configure the device to use the given
    // clock source.  Return true on success, false on error.
    return false;
}

FFADODevice::ClockSource
Device::getActiveClockSource() {
    ClockSource s;
    // Return the active clock source
    return s;
}

bool
Device::lock() {

    // Use this method (and the companion unlock() method) if a device lock
    // is required for some reason.
    return true;
}


bool
Device::unlock() {

    return true;
}

void
Device::showDevice()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int modelId = getConfigRom().getModelId();

    // This method is really just to aid debugging and can be used to to
    // print useful identification information to the debug output.
    Util::Configuration &c = getDeviceManager().getConfiguration();
    Util::Configuration::VendorModelEntry vme = c.findDeviceVME( vendorId, modelId );

    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", vme.vendor_name.c_str(), vme.model_name.c_str(), getNodeId());
}

bool
Device::prepare() {

    signed int bandwidth;
    signed int err = 0;

    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing Device...\n" );

    // This method should prepare the device for streaming without actually
    // turning streaming on.  It is mostly used to configure ports which
    // will ultimately show up as JACK ports - one per audio channel.

    // Store the number of frames per firewire packet.  Depending on the
    // device protocol this may not need to be stored in a data field of 
    // the object, in which case frames_per_packet could be come a local
    // variable.
    frames_per_packet = getFramesPerPacket();

    // Similarly, the number of channels might not be needed beyond this
    // method.  In any case, it must be set correctly to permit calculation
    // of required bus bandwidth.
    num_channels = 0;

    // Bus bandwidth is calculated here.  We assume the device is an S400
    // device, so 1 allocation unit is 1 transmitted byte.  There is 25
    // allocation units of protocol overhead per packet.  The following
    // expression is correct if each channel of audio data is sent/received
    // as a 32 bit integer.

    bandwidth = 25 + num_channels*4*frames_per_packet;

    // Depending on the device, the onus on reserving bus bandwidth rests
    // either with the device or this driver.  The following code shows
    // how iso channels can be requested from the IRM and then reserved
    // with the required amount of bandwidth.

    if (iso_tx_channel < 0) {
        iso_tx_channel = get1394Service().allocateIsoChannelGeneric(bandwidth);
    }
    if (iso_tx_channel < 0) {
        debugFatal("Could not allocate iso tx channel\n");
        return false;
    }

    if (iso_rx_channel < 0) {
        iso_rx_channel = get1394Service().allocateIsoChannelGeneric(bandwidth);
    }
    if (iso_rx_channel < 0) {
        debugFatal("Could not allocate iso rx channel\n");
        err = 1;
    }
  
    if (err) {
        if (iso_tx_channel >= 0) 
            get1394Service().freeIsoChannel(iso_tx_channel);
        if (iso_rx_channel >= 0)
            get1394Service().freeIsoChannel(iso_rx_channel);
        return false;
    }

    // get the device specific and/or global SP configuration
    Util::Configuration &config = getDeviceManager().getConfiguration();
    // base value is the config.h value
    float recv_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;
    float xmit_sp_dll_bw = STREAMPROCESSOR_DLL_BW_HZ;

    // we can override that globally
    config.getValueForSetting("streaming.spm.recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForSetting("streaming.spm.xmit_sp_dll_bw", xmit_sp_dll_bw);

    // or override in the device section
    config.getValueForDeviceSetting(getConfigRom().getNodeVendorId(), getConfigRom().getModelId(), "recv_sp_dll_bw", recv_sp_dll_bw);
    config.getValueForDeviceSetting(getConfigRom().getNodeVendorId(), getConfigRom().getModelId(), "xmit_sp_dll_bw", xmit_sp_dll_bw);

    // Calculate the event size.  Each audio channel is allocated 4 bytes in
    // the data stream by the statement which follows.  This will need to be
    // changed to suit the Digidesign interfaces.
    signed int event_size = num_channels * 4;

    // Set up receive stream processor, initialise it and set DLL bw
    m_receiveProcessor = new Streaming::DigidesignReceiveStreamProcessor(*this, 
      event_size);
    m_receiveProcessor->setVerboseLevel(getDebugLevel());
    if (!m_receiveProcessor->init()) {
        debugFatal("Could not initialize receive processor!\n");
        return false;
    }
    if (!m_receiveProcessor->setDllBandwidth(recv_sp_dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete m_receiveProcessor;
        m_receiveProcessor = NULL;
        return false;
    }

    // Add ports to the receive stream processor - one port per audio channel
    // expected from the device.
    std::string id=std::string("dev?");

    // Ports can be added directly in this method.  I (Jonathan Woithe)
    // prefer them in a separate method to keep the prepare() method as
    // clear as possible.
    addDirPorts(Streaming::Port::E_Capture);

    /* Now set up the transmit stream processor */
    m_transmitProcessor = new Streaming::DigidesignTransmitStreamProcessor(*this,
      event_size);
    m_transmitProcessor->setVerboseLevel(getDebugLevel());
    if (!m_transmitProcessor->init()) {
        debugFatal("Could not initialise receive processor!\n");
        return false;
    }
    if (!m_transmitProcessor->setDllBandwidth(xmit_sp_dll_bw)) {
        debugFatal("Could not set DLL bandwidth\n");
        delete m_transmitProcessor;
        m_transmitProcessor = NULL;
        return false;
    }

    // Add ports to the transmit stream processor. This is one port
    // per audio channel to be sent to the device.
    addDirPorts(Streaming::Port::E_Playback);
    
    return true;
}

int
Device::getStreamCount() {
    return 2; // Normally this is 2: one receive, one transmit
}

Streaming::StreamProcessor *
Device::getStreamProcessorByIndex(int i) {
    // A housekeeping method.  This should not need to be changed.
    switch (i) {
        case 0:
            return m_receiveProcessor;
        case 1:
            return m_transmitProcessor;
        default:
            debugWarning("Invalid stream index %d\n", i);
    }
    return NULL;
}

bool
Device::startStreamByIndex(int i) {
    // Do whatever is needed to get the device to start the i'th stream.
    // Some devices don't permit the separate enabling of the different
    // streams, in which case you can do everything when stream 0 is 
    // requested, and nothing for the others.  Below is an example of this.
    if (i == 0) {
        m_receiveProcessor->setChannel(iso_rx_channel);
        m_transmitProcessor->setChannel(iso_tx_channel);

        // Send commands to device to start the streaming
    }

    // Return true on success, false on failure.
    return true;
}

bool
Device::stopStreamByIndex(int i) {
    // This method should stop the streaming for index i, returning true
    // on success or false on error.  Again, if streams can't be stopped
    // separately one can react only when i is 0.
    return true;
}

signed int
Device::getFramesPerPacket(void) {
    // Return the number of frames transmitted in a single firewire packet.
    // For some devices this is fixed, while for others it depends on the
    // current sampling rate.
    //
    // Getting the current sample rate is as simple as:
    //   signed int freq = getSamplingFrequency();

    // This needs to be set as required.
    return 0;
}

bool 
Device::addPort(Streaming::StreamProcessor *s_processor,
    char *name, enum Streaming::Port::E_Direction direction,
    int position, int size) {

    // This is a helper method for addDirPorts() to minimise the amount of
    // boilerplate code needed in the latter.  It creates the Port object
    // as required, which is automatically inserted into the supplied stream
    // processor object.
    Streaming::Port *p;
    p = new Streaming::DigidesignAudioPort(*s_processor, name, direction, position, size);
    if (p == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",name);
    }
    return true;
}

bool 
Device::addDirPorts(enum Streaming::Port::E_Direction direction) {

    const char *mode_str = direction==Streaming::Port::E_Capture?"cap":"pbk";
    Streaming::StreamProcessor *s_processor;
    std::string id;
    char name[128];

    // This method (in conjunction with addPort()) illustrates the port
    // creation process.

    if (direction == Streaming::Port::E_Capture) {
        s_processor = m_receiveProcessor;
    } else {
        s_processor = m_transmitProcessor;
    }

    id = std::string("dev?");
    if (!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defaulting to 'dev?'\n");
    }

    // Add two ports for an example
    snprintf(name, sizeof(name), "%s_%s_%s", id.c_str(), mode_str, "port_0");
    addPort(s_processor, name, direction, 0, 0);
    snprintf(name, sizeof(name), "%s_%s_%s", id.c_str(), mode_str, "port_1");
    addPort(s_processor, name, direction, 4, 0);

    return true;
}

unsigned int 
Device::readRegister(fb_nodeaddr_t reg) {

    quadlet_t quadlet = 0;

    // This function is a convenience wrapper around the read() method
    // from ieee1394service.  It takes care of merging the node ID with
    // the register address and any other address components which are
    // required to construct a complete bus address.  It also deals with
    // byte ordering.
    
    if (get1394Service().read(0xffc0 | getNodeId(), reg, 1, &quadlet) <= 0) {
        debugError("Error doing Digidesign read from register 0x%06llx\n",reg);
    }

    // This return value assumes that the device uses bus byte order (big
    // endian) for async packets.  Most do.
    return CondSwapFromBus32(quadlet);
}

signed int 
Device::readBlock(fb_nodeaddr_t reg, quadlet_t *buf, unsigned int n_quads) {

    unsigned int i;

    // As for readRegister() except that an arbitary block of data is
    // transferred to the device.

    if (get1394Service().read(0xffc0 | getNodeId(), reg, n_quads, buf) <= 0) {
        debugError("Error doing Digidesign block read of %d quadlets from register 0x%06llx\n",
            n_quads, reg);
        return -1;
    }
    for (i=0; i<n_quads; i++) {
       buf[i] = CondSwapFromBus32(buf[i]);
    }

    return 0;
}

signed int
Device::writeRegister(fb_nodeaddr_t reg, quadlet_t data) {

    unsigned int err = 0;

    // Similar to readRegister(), this is a convenience wrapper method
    // around ieee1394service's write() method.  Again, this assumes that
    // bus byte order (big endian) is expected by the device in async
    // packets.

    data = CondSwapToBus32(data);
    if (get1394Service().write(0xffc0 | getNodeId(), reg, 1, &data) <= 0) {
        err = 1;
        debugError("Error doing Digidesign write to register 0x%06llx\n",reg);
    }

    return (err==0)?0:-1;
}

signed int
Device::writeBlock(fb_nodeaddr_t reg, quadlet_t *data, unsigned int n_quads) {

    // Write a block of data to the device starting at address "reg".  Note
    // that the conditional byteswap is done "in place" on data, so the
    // contents of data may be modified by calling this function.

    unsigned int err = 0;
    unsigned int i;

    for (i=0; i<n_quads; i++) {
      data[i] = CondSwapToBus32(data[i]);
    }
    if (get1394Service().write(0xffc0 | getNodeId(), reg, n_quads, data) <= 0) {
        err = 1;
        debugError("Error doing Digidesign block write of %d quadlets to register 0x%06llx\n",
          n_quads, reg);
    }

    return (err==0)?0:-1;
}
                  
}
