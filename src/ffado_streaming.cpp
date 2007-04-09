/*
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

/*
 * Implementation of the FFADO Streaming API
 */

#include "../libffado/ffado.h"
#include "devicemanager.h"
#include "iavdevice.h"

#include "libstreaming/StreamProcessorManager.h"

#include <assert.h>

#include <string.h>
#include <string>

/**
* Device structure
*/

DECLARE_GLOBAL_DEBUG_MODULE;

using namespace Streaming;

struct _ffado_device
{
    DeviceManager * m_deviceManager;
    StreamProcessorManager *processorManager;

    ffado_options_t options;
    ffado_device_info_t device_info;
};

ffado_device_t *ffado_streaming_init (ffado_device_info_t *device_info, ffado_options_t options) {
    unsigned int i=0;

    struct _ffado_device *dev = new struct _ffado_device;

    debugFatal("%s built %s %s\n", ffado_get_version(), __DATE__, __TIME__);

    if(!dev) {
            debugFatal( "Could not allocate streaming device\n" );
            return 0;
    }

    memcpy((void *)&dev->options, (void *)&options, sizeof(dev->options));
    memcpy((void *)&dev->device_info, (void *)device_info, sizeof(dev->device_info));

    dev->m_deviceManager = new DeviceManager();
    if ( !dev->m_deviceManager ) {
            debugFatal( "Could not allocate device manager\n" );
            delete dev;
            return 0;
    }

    dev->m_deviceManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
    if ( !dev->m_deviceManager->initialize( dev->options.port ) ) {
            debugFatal( "Could not initialize device manager\n" );
            delete dev->m_deviceManager;
            delete dev;
            return 0;
    }

    // create a processor manager to manage the actual stream
    // processors
    dev->processorManager = new StreamProcessorManager(dev->options.period_size,dev->options.nb_buffers);
    if(!dev->processorManager) {
            debugFatal("Could not create StreamProcessorManager\n");
            delete dev->m_deviceManager;
            delete dev;
            return 0;
    }

    dev->processorManager->setThreadParameters(dev->options.realtime, dev->options.packetizer_priority);

    dev->processorManager->setVerboseLevel(DEBUG_LEVEL_VERBOSE);
    if(!dev->processorManager->init()) {
            debugFatal("Could not init StreamProcessorManager\n");
            delete dev->processorManager;
            delete dev->m_deviceManager;
            delete dev;
            return 0;
    }

    // set slave mode option
    bool slaveMode=(dev->options.slave_mode != 0);
    debugOutput(DEBUG_LEVEL_VERBOSE, "setting slave mode to %d\n", slaveMode);
    if(!dev->m_deviceManager->setOption("slaveMode", slaveMode)) {
            debugWarning("Failed to set slave mode option\n");
    }
    // set snoop mode option
    bool snoopMode=(dev->options.snoop_mode != 0);
    debugOutput(DEBUG_LEVEL_VERBOSE, "setting snoop mode to %d\n", snoopMode);
    if(!dev->m_deviceManager->setOption("snoopMode", snoopMode)) {
            debugWarning("Failed to set snoop mode option\n");
    }

    // discover the devices on the bus
    if(!dev->m_deviceManager->discover()) {
            debugFatal("Could not discover devices\n");
            delete dev->processorManager;
            delete dev->m_deviceManager;
            delete dev;
            return 0;
    }

    // are there devices on the bus?
    if(dev->m_deviceManager->getAvDeviceCount()==0) {
            debugFatal("There are no devices on the bus\n");
            delete dev->processorManager;
            delete dev->m_deviceManager;
            delete dev;
            return 0;
    }

    // iterate over the found devices
    // add the stream processors of the devices to the managers
    for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
        IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
        assert(device);

        debugOutput(DEBUG_LEVEL_VERBOSE, "Locking device (%p)\n", device);

        if (!device->lock()) {
            debugWarning("Could not lock device, skipping device (%p)!\n", device);
            continue;
        }

        debugOutput(DEBUG_LEVEL_VERBOSE, "Setting samplerate to %d for (%p)\n",
                    dev->options.sample_rate, device);

        // Set the device's sampling rate to that requested
        // FIXME: does this really belong here?  If so we need to handle errors.
        if (!device->setSamplingFrequency(parseSampleRate(dev->options.sample_rate))) {
            debugOutput(DEBUG_LEVEL_VERBOSE, " => Retry setting samplerate to %d for (%p)\n",
                        dev->options.sample_rate, device);

            // try again:
            if (!device->setSamplingFrequency(parseSampleRate(dev->options.sample_rate))) {
                delete dev->processorManager;
                delete dev->m_deviceManager;
                delete dev;
                debugFatal("Could not set sampling frequency to %d\n",dev->options.sample_rate);
                return 0;
            }
        }

        // prepare the device
        device->prepare();

        int j=0;
        for(j=0; j<device->getStreamCount();j++) {
            StreamProcessor *streamproc=device->getStreamProcessorByIndex(j);
            debugOutput(DEBUG_LEVEL_VERBOSE, "Registering stream processor %d of device %d with processormanager\n",j,i);
            if (!dev->processorManager->registerProcessor(streamproc)) {
                debugWarning("Could not register stream processor (%p) with the Processor manager\n",streamproc);
            }
        }
    }

    // set the sync source
    if (!dev->processorManager->setSyncSource(dev->m_deviceManager->getSyncSource())) {
        debugWarning("Could not set processorManager sync source (%p)\n",
            dev->m_deviceManager->getSyncSource());
    }

    // we are ready!
    debugOutputShort(DEBUG_LEVEL_VERBOSE, "\n\n");
    return dev;

}

int ffado_streaming_prepare(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Preparing...\n");

    if (!dev->processorManager->prepare()) {
        debugFatal("Could not prepare streaming...\n");
        return false;
    }

    return true;
}

void ffado_streaming_finish(ffado_device_t *dev) {
    unsigned int i=0;

    assert(dev);

    // iterate over the found devices
    for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
        IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
        assert(device);

        debugOutput(DEBUG_LEVEL_VERBOSE, "Unlocking device (%p)\n", device);

        if (!device->unlock()) {
            debugWarning("Could not unlock device (%p)!\n", device);
        }
    }

    delete dev->processorManager;
    delete dev->m_deviceManager;
    delete dev;

    return;
}

int ffado_streaming_start(ffado_device_t *dev) {
    unsigned int i=0;
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Start -------------\n");

    // create the connections for all devices
    // iterate over the found devices
    // add the stream processors of the devices to the managers
    for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
        IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
        assert(device);

        int j=0;
        for(j=0; j<device->getStreamCount();j++) {
        debugOutput(DEBUG_LEVEL_VERBOSE,"Starting stream %d of device %d\n",j,i);
            // start the stream
            if (!device->startStreamByIndex(j)) {
                debugWarning("Could not start stream %d of device %d\n",j,i);
                continue;
            }
        }

        if (!device->enableStreaming()) {
            debugWarning("Could not enable streaming on device %d!\n",i);
        }
    }

    if(dev->processorManager->start()) {
        return 0;
    } else {
        ffado_streaming_stop(dev);
        return -1;
    }
}

int ffado_streaming_stop(ffado_device_t *dev) {
    unsigned int i;
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Stop -------------\n");

    dev->processorManager->stop();

    // create the connections for all devices
    // iterate over the found devices
    // add the stream processors of the devices to the managers
    for(i=0;i<dev->m_deviceManager->getAvDeviceCount();i++) {
        IAvDevice *device=dev->m_deviceManager->getAvDeviceByIndex(i);
        assert(device);

        if (!device->disableStreaming()) {
            debugWarning("Could not disable streaming on device %d!\n",i);
        }

        int j=0;
        for(j=0; j<device->getStreamCount();j++) {
            debugOutput(DEBUG_LEVEL_VERBOSE,"Stopping stream %d of device %d\n",j,i);
            // stop the stream
            // start the stream
            if (!device->stopStreamByIndex(j)) {
                debugWarning("Could not stop stream %d of device %d\n",j,i);
                continue;
            }
        }
    }

    return 0;
}

int ffado_streaming_reset(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Reset -------------\n");

    // dev->processorManager->reset();

    return 0;
}

int ffado_streaming_wait(ffado_device_t *dev) {
    static int periods=0;
    static int periods_print=0;
    static int xruns=0;

    periods++;
    if(periods>periods_print) {
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "\nffado_streaming_wait\n");
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "============================================\n");
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "Xruns: %d\n",xruns);
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "============================================\n");
        dev->processorManager->dumpInfo();
        debugOutputShort(DEBUG_LEVEL_VERBOSE, "\n");
        periods_print+=100;
    }

    if(dev->processorManager->waitForPeriod()) {
        return dev->options.period_size;
    } else {
        debugWarning("XRUN detected\n");

        // do xrun recovery
        dev->processorManager->handleXrun();
        xruns++;
        return -1;
    }
}

int ffado_streaming_transfer_capture_buffers(ffado_device_t *dev) {
    return dev->processorManager->transfer(StreamProcessor::E_Receive);
}

int ffado_streaming_transfer_playback_buffers(ffado_device_t *dev) {
    return dev->processorManager->transfer(StreamProcessor::E_Transmit);
}

int ffado_streaming_transfer_buffers(ffado_device_t *dev) {
    return dev->processorManager->transfer();
}


int ffado_streaming_write(ffado_device_t *dev, int i, ffado_sample_t *buffer, int nsamples) {
    Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
    // use an assert here performancewise,
    // it should already have failed before, if not correct
    assert(p);

    return p->writeEvents((void *)buffer, nsamples);
}

int ffado_streaming_read(ffado_device_t *dev, int i, ffado_sample_t *buffer, int nsamples) {
    Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);
    // use an assert here performancewise,
    // it should already have failed before, if not correct
    assert(p);

    return p->readEvents((void *)buffer, nsamples);
}

int ffado_streaming_get_nb_capture_streams(ffado_device_t *dev) {
    return dev->processorManager->getPortCount(Port::E_Capture);
}

int ffado_streaming_get_nb_playback_streams(ffado_device_t *dev) {
    return dev->processorManager->getPortCount(Port::E_Playback);
}

int ffado_streaming_get_capture_stream_name(ffado_device_t *dev, int i, char* buffer, size_t buffersize) {
    Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);
    if(!p) {
        debugWarning("Could not get capture port at index %d\n",i);
        return -1;
    }

    std::string name=p->getName();
    if (!strncpy(buffer, name.c_str(), buffersize)) {
        debugWarning("Could not copy name\n");
        return -1;
    } else return 0;
}

int ffado_streaming_get_playback_stream_name(ffado_device_t *dev, int i, char* buffer, size_t buffersize) {
    Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
    if(!p) {
        debugWarning("Could not get playback port at index %d\n",i);
        return -1;
    }

    std::string name=p->getName();
    if (!strncpy(buffer, name.c_str(), buffersize)) {
        debugWarning("Could not copy name\n");
        return -1;
    } else return 0;
}

ffado_streaming_stream_type ffado_streaming_get_capture_stream_type(ffado_device_t *dev, int i) {
    Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);
    if(!p) {
        debugWarning("Could not get capture port at index %d\n",i);
        return ffado_stream_type_invalid;
    }
    switch(p->getPortType()) {
    case Port::E_Audio:
        return ffado_stream_type_audio;
    case Port::E_Midi:
        return ffado_stream_type_midi;
    case Port::E_Control:
        return ffado_stream_type_control;
    default:
        return ffado_stream_type_unknown;
    }
}

ffado_streaming_stream_type ffado_streaming_get_playback_stream_type(ffado_device_t *dev, int i) {
    Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
    if(!p) {
        debugWarning("Could not get playback port at index %d\n",i);
        return ffado_stream_type_invalid;
    }
    switch(p->getPortType()) {
    case Port::E_Audio:
        return ffado_stream_type_audio;
    case Port::E_Midi:
        return ffado_stream_type_midi;
    case Port::E_Control:
        return ffado_stream_type_control;
    default:
        return ffado_stream_type_unknown;
    }
}

int ffado_streaming_set_stream_buffer_type(ffado_device_t *dev, int i,
    ffado_streaming_buffer_type t, enum Port::E_Direction direction) {

    Port *p=dev->processorManager->getPortByIndex(i, direction);
    if(!p) {
        debugWarning("Could not get %s port at index %d\n",
            (direction==Port::E_Playback?"Playback":"Capture"),i);
        return -1;
    }

    switch(t) {
    case ffado_buffer_type_int24:
        if (!p->setDataType(Port::E_Int24)) {
            debugWarning("%s: Could not set data type to Int24\n",p->getName().c_str());
            return -1;
        }
        if (!p->setBufferType(Port::E_PointerBuffer)) {
            debugWarning("%s: Could not set buffer type to Pointerbuffer\n",p->getName().c_str());
            return -1;
        }
        break;
    case ffado_buffer_type_float:
        if (!p->setDataType(Port::E_Float)) {
            debugWarning("%s: Could not set data type to Float\n",p->getName().c_str());
            return -1;
        }
        if (!p->setBufferType(Port::E_PointerBuffer)) {
            debugWarning("%s: Could not set buffer type to Pointerbuffer\n",p->getName().c_str());
            return -1;
        }
        break;
    case ffado_buffer_type_midi:
        if (!p->setDataType(Port::E_MidiEvent)) {
            debugWarning("%s: Could not set data type to MidiEvent\n",p->getName().c_str());
            return -1;
        }
        if (!p->setBufferType(Port::E_RingBuffer)) {
            debugWarning("%s: Could not set buffer type to Ringbuffer\n",p->getName().c_str());
            return -1;
        }
        break;
    default:
        debugWarning("%s: Unsupported buffer type\n",p->getName().c_str());
        return -1;
    }
    return 0;

}

int ffado_streaming_set_playback_buffer_type(ffado_device_t *dev, int i, ffado_streaming_buffer_type t) {
    return ffado_streaming_set_stream_buffer_type(dev, i, t, Port::E_Playback);
}

int ffado_streaming_set_capture_buffer_type(ffado_device_t *dev, int i, ffado_streaming_buffer_type t) {
    return ffado_streaming_set_stream_buffer_type(dev, i, t, Port::E_Capture);
}

int ffado_streaming_stream_onoff(ffado_device_t *dev, int i,
    int on, enum Port::E_Direction direction) {
    Port *p=dev->processorManager->getPortByIndex(i, direction);
    if(!p) {
        debugWarning("Could not get %s port at index %d\n",
            (direction==Port::E_Playback?"Playback":"Capture"),i);
        return -1;
    }
    if(on) {
        p->enable();
    } else {
        p->disable();
    }
    return 0;
}

int ffado_streaming_playback_stream_onoff(ffado_device_t *dev, int number, int on) {
    return ffado_streaming_stream_onoff(dev, number, on, Port::E_Playback);
}

int ffado_streaming_capture_stream_onoff(ffado_device_t *dev, int number, int on) {
    return ffado_streaming_stream_onoff(dev, number, on, Port::E_Capture);
}

// TODO: the way port buffers are set in the C api doesn't satisfy me
int ffado_streaming_set_capture_stream_buffer(ffado_device_t *dev, int i, char *buff) {
        Port *p=dev->processorManager->getPortByIndex(i, Port::E_Capture);

        // use an assert here performancewise,
        // it should already have failed before, if not correct
        assert(p);

        p->useExternalBuffer(true);
        p->setExternalBufferAddress((void *)buff);

        return 0;

}

int ffado_streaming_set_playback_stream_buffer(ffado_device_t *dev, int i, char *buff) {
        Port *p=dev->processorManager->getPortByIndex(i, Port::E_Playback);
        // use an assert here performancewise,
        // it should already have failed before, if not correct
        assert(p);

        p->useExternalBuffer(true);
        p->setExternalBufferAddress((void *)buff);

        return 0;
}

