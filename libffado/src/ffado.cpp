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

/*
 * Implementation of the FFADO external C API
 */

#include "config.h"

#include "../libffado/ffado.h"
#include "libstreaming/generic/StreamProcessor.h"
#include "libstreaming/generic/Port.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "ffadodevice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string>

DECLARE_GLOBAL_DEBUG_MODULE;
IMPL_GLOBAL_DEBUG_MODULE( FFADO, DEBUG_LEVEL_VERBOSE );

#ifdef __cplusplus
extern "C" {
#endif

// this is very much nescessary, as otherwise the
// message buffer thread doesn't get killed when the
// library is dlclose()'d

static void exitfunc(void) __attribute__((destructor));

static void exitfunc(void)
{
    delete DebugModuleManager::instance();

}
#ifdef __cplusplus
}
#endif

const char*
ffado_get_version() {
    return PACKAGE_STRING;
}

int
ffado_get_api_version() {
    return FFADO_API_VERSION;
}

#warning this should be cleaned up
#include "libavc/general/avc_generic.h"
void ffado_sleep_after_avc_command( int time )
{
    AVC::AVCCommand::setSleepAfterAVCCommand( time );
}

struct _ffado_device
{
    DeviceManager * m_deviceManager;

    ffado_options_t options;
    ffado_device_info_t device_info;
};

ffado_device_t *ffado_streaming_init (ffado_device_info_t device_info, ffado_options_t options) {
    unsigned int i=0;
    setDebugLevel(options.verbose);

    struct _ffado_device *dev = new struct _ffado_device;

    debugWarning("%s built %s %s\n", ffado_get_version(), __DATE__, __TIME__);

    if(!dev) {
        debugFatal( "Could not allocate streaming device\n" );
        return 0;
    }

    memcpy((void *)&dev->options, (void *)&options, sizeof(dev->options));

    dev->m_deviceManager = new DeviceManager();
    if ( !dev->m_deviceManager ) {
        debugFatal( "Could not allocate device manager\n" );
        delete dev;
        return 0;
    }

    dev->m_deviceManager->setVerboseLevel(dev->options.verbose);
    dev->m_deviceManager->setThreadParameters(dev->options.realtime, dev->options.packetizer_priority);

    for (i = 0; i < device_info.nb_device_spec_strings; i++) {
        char *s = device_info.device_spec_strings[i];
        if ( !dev->m_deviceManager->addSpecString(s) ) { 
            debugFatal( "Could not add spec string %s to device manager\n", s );
            delete dev->m_deviceManager;
            delete dev;
            return 0;
        }
    }
    // create a processor manager to manage the actual stream
    // processors
    if ( !dev->m_deviceManager->setStreamingParams(dev->options.period_size, 
                                                   dev->options.sample_rate,
                                                   dev->options.nb_buffers))
    {
        debugFatal( "Could not set streaming parameters of device manager\n" );
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

    if ( !dev->m_deviceManager->initialize() ) {
        debugFatal( "Could not initialize device manager\n" );
        delete dev->m_deviceManager;
        delete dev;
        return 0;
    }
    // discover the devices on the bus
    if(!dev->m_deviceManager->discover()) {
        debugFatal("Could not discover devices\n");
        delete dev->m_deviceManager;
        delete dev;
        return 0;
    }
    // are there devices on the bus?
    if(dev->m_deviceManager->getAvDeviceCount() == 0) {
        debugFatal("There are no devices on the bus\n");
        delete dev->m_deviceManager;
        delete dev;
        return 0;
    }
    // prepare here or there are no ports for jack
    if(!dev->m_deviceManager->initStreaming()) {
        debugFatal("Could not init the streaming system\n");
        return 0;
    }
    // we are ready!
    return dev;
}

int ffado_streaming_prepare(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Preparing...\n");
    // prepare here or there are no ports for jack
    if(!dev->m_deviceManager->prepareStreaming()) {
        debugFatal("Could not prepare the streaming system\n");
        return -1;
    }
    return 0;
}

void ffado_streaming_finish(ffado_device_t *dev) {
    assert(dev);
    if(!dev->m_deviceManager->finishStreaming()) {
        debugError("Could not finish the streaming\n");
    }
    delete dev->m_deviceManager;
    delete dev;
    return;
}

int ffado_streaming_start(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Start -------------\n");
    if(!dev->m_deviceManager->startStreaming()) {
        debugFatal("Could not start the streaming system\n");
        return -1;
    }
    return 0;
}

int ffado_streaming_stop(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Stop -------------\n");
    if(!dev->m_deviceManager->stopStreaming()) {
        debugFatal("Could not stop the streaming system\n");
        return -1;
    }
    return 0;
}

int ffado_streaming_reset(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Reset -------------\n");
    if(!dev->m_deviceManager->resetStreaming()) {
        debugFatal("Could not reset the streaming system\n");
        return -1;
    }
    return 0;
}

ffado_wait_response
ffado_streaming_wait(ffado_device_t *dev) {
    static int periods=0;
    static int periods_print=0;
    static int xruns=0;

    periods++;
    if(periods>periods_print) {
        debugOutputShort(DEBUG_LEVEL_NORMAL, "\nffado_streaming_wait\n");
        debugOutputShort(DEBUG_LEVEL_NORMAL, "============================================\n");
        debugOutputShort(DEBUG_LEVEL_NORMAL, "Xruns: %d\n", xruns);
        debugOutputShort(DEBUG_LEVEL_NORMAL, "============================================\n");
        dev->m_deviceManager->showStreamingInfo();
        debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");
        periods_print+=100;
    }

    enum DeviceManager::eWaitResult result;
    result = dev->m_deviceManager->waitForPeriod();
    if(result == DeviceManager::eWR_OK) {
        return ffado_wait_ok;
    } else if (result == DeviceManager::eWR_Xrun) {
        debugWarning("Handled XRUN\n");
        xruns++;
        return ffado_wait_xrun;
    } else if (result == DeviceManager::eWR_Shutdown) {
        debugWarning("Streaming system requests shutdown.\n");
        return ffado_wait_shutdown;
    } else {
        debugError("Error condition while waiting (Unhandled XRUN)\n");
        xruns++;
        return ffado_wait_error;
    }
}

int ffado_streaming_transfer_capture_buffers(ffado_device_t *dev) {
    return dev->m_deviceManager->getStreamProcessorManager().transfer(Streaming::StreamProcessor::ePT_Receive);
}

int ffado_streaming_transfer_playback_buffers(ffado_device_t *dev) {
    return dev->m_deviceManager->getStreamProcessorManager().transfer(Streaming::StreamProcessor::ePT_Transmit);
}

int ffado_streaming_transfer_buffers(ffado_device_t *dev) {
    return dev->m_deviceManager->getStreamProcessorManager().transfer();
}

int ffado_streaming_get_nb_capture_streams(ffado_device_t *dev) {
    return dev->m_deviceManager->getStreamProcessorManager().getPortCount(Streaming::Port::E_Capture);
}

int ffado_streaming_get_nb_playback_streams(ffado_device_t *dev) {
    return dev->m_deviceManager->getStreamProcessorManager().getPortCount(Streaming::Port::E_Playback);
}

int ffado_streaming_get_capture_stream_name(ffado_device_t *dev, int i, char* buffer, size_t buffersize) {
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, Streaming::Port::E_Capture);
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
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, Streaming::Port::E_Playback);
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
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, Streaming::Port::E_Capture);
    if(!p) {
        debugWarning("Could not get capture port at index %d\n",i);
        return ffado_stream_type_invalid;
    }
    switch(p->getPortType()) {
    case Streaming::Port::E_Audio:
        return ffado_stream_type_audio;
    case Streaming::Port::E_Midi:
        return ffado_stream_type_midi;
    case Streaming::Port::E_Control:
        return ffado_stream_type_control;
    default:
        return ffado_stream_type_unknown;
    }
}

ffado_streaming_stream_type ffado_streaming_get_playback_stream_type(ffado_device_t *dev, int i) {
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, Streaming::Port::E_Playback);
    if(!p) {
        debugWarning("Could not get playback port at index %d\n",i);
        return ffado_stream_type_invalid;
    }
    switch(p->getPortType()) {
    case Streaming::Port::E_Audio:
        return ffado_stream_type_audio;
    case Streaming::Port::E_Midi:
        return ffado_stream_type_midi;
    case Streaming::Port::E_Control:
        return ffado_stream_type_control;
    default:
        return ffado_stream_type_unknown;
    }
}

int ffado_streaming_set_audio_datatype(ffado_device_t *dev,
    ffado_streaming_audio_datatype t) {
    switch(t) {
        case ffado_audio_datatype_int24:
            if(!dev->m_deviceManager->getStreamProcessorManager().setAudioDataType(
               Streaming::StreamProcessorManager::eADT_Int24)) {
                debugError("Could not set datatype\n");
                return -1;
            }
            break;
        case ffado_audio_datatype_float:
            if(!dev->m_deviceManager->getStreamProcessorManager().setAudioDataType(
               Streaming::StreamProcessorManager::eADT_Float)) {
                debugError("Could not set datatype\n");
                return -1;
            }
            break;
        default:
            debugError("Invalid audio datatype\n");
            return -1;
    }
    return 0;
}

ffado_streaming_audio_datatype ffado_streaming_get_audio_datatype(ffado_device_t *dev) {
    switch(dev->m_deviceManager->getStreamProcessorManager().getAudioDataType()) {
        case Streaming::StreamProcessorManager::eADT_Int24:
            return ffado_audio_datatype_int24;
        case Streaming::StreamProcessorManager::eADT_Float:
            return ffado_audio_datatype_float;
        default:
            debugError("Invalid audio datatype\n");
            return ffado_audio_datatype_error;
    }
    #warning FIXME
}

int ffado_streaming_stream_onoff(ffado_device_t *dev, int i,
    int on, enum Streaming::Port::E_Direction direction) {
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, direction);
    if(!p) {
        debugWarning("Could not get %s port at index %d\n",
            (direction==Streaming::Port::E_Playback?"Playback":"Capture"),i);
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
    return ffado_streaming_stream_onoff(dev, number, on, Streaming::Port::E_Playback);
}

int ffado_streaming_capture_stream_onoff(ffado_device_t *dev, int number, int on) {
    return ffado_streaming_stream_onoff(dev, number, on, Streaming::Port::E_Capture);
}

int ffado_streaming_set_capture_stream_buffer(ffado_device_t *dev, int i, char *buff) {
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, Streaming::Port::E_Capture);
    // use an assert here performancewise,
    // it should already have failed before, if not correct
    assert(p);
    p->setBufferAddress((void *)buff);
    return 0;
}

int ffado_streaming_set_playback_stream_buffer(ffado_device_t *dev, int i, char *buff) {
    Streaming::Port *p = dev->m_deviceManager->getStreamProcessorManager().getPortByIndex(i, Streaming::Port::E_Playback);
    // use an assert here performancewise,
    // it should already have failed before, if not correct
    assert(p);
    p->setBufferAddress((void *)buff);
    return 0;
}
