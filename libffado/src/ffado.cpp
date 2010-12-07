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

#include "version.h"

#include "../libffado/ffado.h"

#include "debugmodule/debugmodule.h"
#include "fbtypes.h"
#include "devicemanager.h"
#include "ffadodevice.h"

typedef uint32_t __u32; // FIXME
typedef uint64_t __u64; // FIXME

// NOTE: connections.h is only for the callback definition
#include "libstreaming/connections.h"

#include "libstreaming/streamer.h"

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

#define MAX_NB_CONNECTIONS 16
struct _ffado_device
{
    DeviceManager * m_deviceManager;

    streamer_t m_streamer;
    __u32 conn_ids[MAX_NB_CONNECTIONS]; // FIXME

    ffado_options_t options;
    ffado_device_info_t device_info;

    // buffer info
    // this will be filled in by prepare()
    // and will contain the pointers to the
    // corresponding memory locations

    unsigned int stream_count;

    // this allows to find the internal port 
    // an external port belongs to
    std::vector<struct stream_settings *>capture_port_stream;
    std::vector<unsigned int>capture_port_source_index;
    std::vector<struct stream_settings *>playback_port_stream;
    std::vector<unsigned int>playback_port_source_index;

    // this gives direct access to the buffers
    // required for data I/O
    std::vector<void **>capture_port_buffers;
    std::vector<void **>playback_port_buffers;

    // direct access to the port state
    std::vector<enum stream_port_state *> capture_port_states;
    std::vector<enum stream_port_state *> playback_port_states;
};


/*
 *
 *
 * STREAMING
 *
 *
 */

ffado_device_t *ffado_streaming_init (ffado_device_info_t device_info, ffado_options_t options) {
    unsigned int i = 0;
    setDebugLevel(options.verbose);
    printMessage("%s built %s %s\n", ffado_get_version(), __DATE__, __TIME__);

#if DEBUG_USE_MESSAGE_BUFFER
    // ok
#else
    printMessage("FFADO built without realtime-safe message buffer support. This can cause xruns and is not recommended.\n");
#endif

    struct _ffado_device *dev = new struct _ffado_device;
    if(!dev) {
        debugFatal( "Could not allocate streaming device\n" );
        return NULL;
    }

    memcpy((void *)&dev->options, (void *)&options, sizeof(dev->options));

    dev->m_deviceManager = new DeviceManager();
    if ( !dev->m_deviceManager ) {
        debugFatal( "Could not allocate device manager\n" );
        delete dev;
        return NULL;
    }

    dev->m_deviceManager->setVerboseLevel(dev->options.verbose);

    if(dev->options.realtime) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "Starting with realtime scheduling, base priority %d\n", 
                    dev->options.packetizer_priority);
    } else {
        debugWarning("Realtime scheduling is not enabled. This will cause significant reliability issues.\n");
    }
    dev->m_deviceManager->setThreadParameters(dev->options.realtime, dev->options.packetizer_priority);

    for (i = 0; i < device_info.nb_device_spec_strings; i++) {
        char *s = device_info.device_spec_strings[i];
        if ( !dev->m_deviceManager->addSpecString(s) ) { 
            debugFatal( "Could not add spec string %s to device manager\n", s );
            delete dev->m_deviceManager;
            delete dev;
            return NULL;
        }
    }

    // create a processor manager to manage the actual stream
    // processors

    // setup the streamer
    dev->m_streamer = streamer_new();
    if(dev->m_streamer == NULL) {
        debugError("Could not allocate streamer\n");
        return NULL;
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
        streamer_free(dev->m_streamer);
        delete dev->m_deviceManager;
        delete dev;
        return NULL;
    }

    // discover the devices on the bus
    if(!dev->m_deviceManager->discover()) {
        debugFatal("Could not discover devices\n");
        streamer_free(dev->m_streamer);
        delete dev->m_deviceManager;
        delete dev;
        return NULL;
    }

    // are there devices on the bus?
    if(dev->m_deviceManager->getAvDeviceCount() == 0) {
        debugFatal("There are no devices on the bus\n");
        streamer_free(dev->m_streamer);
        delete dev->m_deviceManager;
        delete dev;
        return NULL;
    }

    // FIXME: assumes that all devices are on the same bus
    FFADODevice *deviceX = dev->m_deviceManager->getAvDeviceByIndex(0);
    int util_port   = deviceX->get1394Service().getNewStackPort();
    int frame_slack = 80; // FIXME: get from an option
    int iso_slack   = 100; // FIXME: get from an option

    if(streamer_init(dev->m_streamer, util_port,
                     dev->options.period_size,
                     dev->options.nb_buffers,
                     frame_slack,
                     iso_slack,
                     dev->options.sample_rate) < 0) {
        debugError("Failed to init streamer\n");
        streamer_free(dev->m_streamer);
        delete dev->m_deviceManager;
        delete dev;
        return NULL;
    }

    // locks the devices and sets their samplerate.
    if(!dev->m_deviceManager->initStreaming(dev->options.sample_rate)) {
        debugFatal("Could not init the streaming system\n");
        streamer_free(dev->m_streamer);
        delete dev->m_deviceManager;
        delete dev;
        return NULL;
    }

    /* iterate over all devices and create the 
       required data structures for the streamer
    */

    // clear the port info vectors
    dev->capture_port_stream.clear();
    dev->capture_port_source_index.clear();
    dev->capture_port_buffers.clear();
    dev->capture_port_states.clear();

    dev->playback_port_stream.clear();
    dev->playback_port_source_index.clear();
    dev->playback_port_buffers.clear();
    dev->playback_port_states.clear();

    // fetch the stream configurations for all devices and 
    // configure the streamer to tune in
    unsigned int nb_devices = dev->m_deviceManager->getAvDeviceCount();
    dev->stream_count = 0;
    for ( i = 0; i < nb_devices; i++)
    {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "Configuring streams for device %d/%d\n",
                    i+1, nb_devices);
        FFADODevice *device = dev->m_deviceManager->getAvDeviceByIndex(i);
        assert(device);

        unsigned int nb_streams = device->getStreamCount();
        unsigned int j;
        for(j = 0; j < nb_streams; j++) {
            debugOutput(DEBUG_LEVEL_VERBOSE, 
                        " Configuring stream %d of device %d/%d\n", 
                        j, i+1, nb_devices);
            struct stream_settings *settings;
            settings = device->getSettingsForStream(j);
            assert(settings);
            if(settings->type == STREAM_TYPE_TRANSMIT) {
                int p;
                for(p = 0; p < settings->nb_substreams; p++) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, 
                                "  -> transmit substream %d\n", 
                                p);
                    dev->playback_port_stream.push_back(settings);
                    dev->playback_port_source_index.push_back(p);
                    dev->playback_port_buffers.push_back(settings->substream_buffers + p);
                    dev->playback_port_states.push_back(settings->substream_states + p);
                }
            } else {
                int p;
                for(p = 0; p < settings->nb_substreams; p++) {
                    debugOutput(DEBUG_LEVEL_VERBOSE, 
                                "  -> receive substream %d\n", 
                                p);
                    dev->capture_port_stream.push_back(settings);
                    dev->capture_port_source_index.push_back(p);
                    dev->capture_port_buffers.push_back(settings->substream_buffers + p);
                    dev->capture_port_states.push_back(settings->substream_states + p);
                }
            }

            int status = streamer_add_stream(dev->m_streamer, settings);
            if(status < 0) {
                debugError("Failed to add stream\n");
                streamer_free(dev->m_streamer);
                delete dev->m_deviceManager;
                delete dev;
                return 0;
            }
            assert(dev->stream_count < MAX_NB_CONNECTIONS);
            dev->conn_ids[dev->stream_count] = status; // FIXME

            dev->stream_count++;
        }
    }

    if(dev->stream_count == 0) {
        // NOTE: at one point this might not be an error
        // e.g. no devices attached, jack still running, picks up devices
        // dynamically when attached
        debugError("No streams to listen to?\n");
        streamer_free(dev->m_streamer);
        delete dev->m_deviceManager;
        delete dev;
        return 0;
    }

    // we are ready!
    return dev;
}

int ffado_streaming_prepare(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "Preparing...\n");


    return 0;
}

void ffado_streaming_finish(ffado_device_t *dev) {
    assert(dev);
    // unlocks the devices
    if(!dev->m_deviceManager->finishStreaming()) {
        debugError("Could not finish the streaming\n");
    }
    streamer_free(dev->m_streamer);

    delete dev->m_deviceManager;
    delete dev;
    return;
}

int ffado_streaming_start(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Start -------------\n");

    bool device_start_failed = false;
    unsigned int i = 0;

    // create the connections for all devices
    // iterate over the found devices
    unsigned int nb_devices = dev->m_deviceManager->getAvDeviceCount();
    for ( i = 0; i < nb_devices; i++)
    {
        if (!dev->m_deviceManager->startStreamingOnDevice(i)) {
            debugWarning("Could not start streaming on device %d!\n", i);
            device_start_failed = true;
            break;
        }
    }

    // if one of the devices failed to start,
    // the previous routine should have cleaned up the failing one.
    // we still have to stop all devices that were started before this one.
    if(device_start_failed) {
        for (i--; i >= 0; i--)
        {
            if (!dev->m_deviceManager->stopStreamingOnDevice(i)) {
                debugWarning("Could not stop streaming on device %d!\n", i);
            }
        }
        return -1;
    }

    // start all connections of the streamer
    for(i = 0; i < dev->stream_count; i++) {
        if(streamer_start_connection(dev->m_streamer, dev->conn_ids[i], -1) < 0)
        {
            debugError("Failed to start stream %d\n", i);
            return -1;
        }
    }

    // elect a sync source
#warning TODO: proper sync source election
    // FIXME
    streamer_set_sync_connection(dev->m_streamer, dev->conn_ids[0]);

    // start the streamer to tune in to the channels
    if(streamer_start(dev->m_streamer) < 0) {
        debugError("Could not start streamer\n");
        for ( i = 0; i < nb_devices; i++)
        {
            if (!dev->m_deviceManager->stopStreamingOnDevice(i)) {
                debugWarning("Could not stop streaming on device %d!\n", i);
            }
        }
        return -1;
    }

    return 0;
}

int ffado_streaming_stop(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Stop -------------\n");
    int retval = 0;

    if(streamer_stop(dev->m_streamer) < 0) {
        debugFatal("Could not stop the streaming system\n");
        retval = -1;
    }

//     // stop all connections of the streamer
//     for(i = 0; i < dev->stream_count; i++) {
//         if(streamer_stop_connection(dev->m_streamer, dev->conn_ids[i]) < 0)
//         {
//             debugError("Failed to stop stream %d\n", i);
//             return -1;
//         }
//     }

    unsigned int nb_devices = dev->m_deviceManager->getAvDeviceCount();
    unsigned int i;
    for ( i = 0; i < nb_devices; i++)
    {
        if (!dev->m_deviceManager->stopStreamingOnDevice(i)) {
            debugWarning("Could not stop streaming on device %d!\n", i);
            retval = -1;
        }
    }

    return retval;
}

int ffado_streaming_reset(ffado_device_t *dev) {
    debugOutput(DEBUG_LEVEL_VERBOSE,"------------- Reset -------------\n");
#warning TODO
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
        streamer_print_stream_state(dev->m_streamer);
        debugOutputShort(DEBUG_LEVEL_NORMAL, "\n");
        periods_print += 1;
    }

#warning todo: xrun handling
    if(streamer_wait_for_period(dev->m_streamer) < 0) {
        debugError("Failed to wait for period\n");
        return ffado_wait_error;
    }
#warning REMOVE!!!
//     if(periods > 10) return ffado_wait_shutdown;
    return ffado_wait_ok;

//     enum DeviceManager::eWaitResult result;
//     result = dev->m_deviceManager->waitForPeriod();
//     if(result == DeviceManager::eWR_OK) {
//         return ffado_wait_ok;
//     } else if (result == DeviceManager::eWR_Xrun) {
//         debugOutput(DEBUG_LEVEL_NORMAL, "Handled XRUN\n");
//         xruns++;
//         return ffado_wait_xrun;
//     } else if (result == DeviceManager::eWR_Shutdown) {
//         debugWarning("Streaming system requests shutdown.\n");
//         return ffado_wait_shutdown;
//     } else {
//         debugError("Error condition while waiting (Unhandled XRUN)\n");
//         xruns++;
//         return ffado_wait_error;
//     }
}

int ffado_streaming_transfer_capture_buffers(ffado_device_t *dev) {
    return streamer_read_frames(dev->m_streamer);
}

int ffado_streaming_transfer_playback_buffers(ffado_device_t *dev) {
    if(streamer_write_frames(dev->m_streamer) < 0) {
        debugError("write failed\n");
        return -1;
    }
#warning: doesnt this mess up jackmp async
    if(streamer_queue_next_period(dev->m_streamer) < 0) {
        debugError("queue failed\n");
        return -1;
    }
    return 0;
}

int ffado_streaming_transfer_buffers(ffado_device_t *dev) {
    int retval = 0;
    if(ffado_streaming_transfer_capture_buffers(dev) < 0) {
        retval = -1;
    }
    if(ffado_streaming_transfer_playback_buffers(dev) < 0) {
        retval = -1;
    }
    return retval;
}

int ffado_streaming_get_nb_capture_streams(ffado_device_t *dev) {
    return dev->capture_port_stream.size();
}

int ffado_streaming_get_nb_playback_streams(ffado_device_t *dev) {
    return dev->playback_port_stream.size();
}

int ffado_streaming_get_capture_stream_name(ffado_device_t *dev, int i, char* buffer, size_t buffersize) {
    assert(i < dev->capture_port_stream.size());
    struct stream_settings * strm = dev->capture_port_stream.at(i);
    unsigned int portidx = dev->capture_port_source_index.at(i);
    if(!strncpy(buffer, strm->substream_names[portidx], buffersize)) {
        debugWarning("Could not copy name\n");
        return -1;
    } else return 0;
}

int ffado_streaming_get_playback_stream_name(ffado_device_t *dev, int i, char* buffer, size_t buffersize) {
    assert(i < dev->playback_port_stream.size());
    struct stream_settings * strm = dev->playback_port_stream.at(i);
    unsigned int portidx = dev->playback_port_source_index.at(i);
    if(!strncpy(buffer, strm->substream_names[portidx], buffersize)) {
        debugWarning("Could not copy name\n");
        return -1;
    } else return 0;
}

ffado_streaming_stream_type ffado_streaming_get_capture_stream_type(ffado_device_t *dev, int i) {
    assert(i < dev->capture_port_stream.size());
    struct stream_settings * strm = dev->capture_port_stream.at(i);
    unsigned int portidx = dev->capture_port_source_index.at(i);
    return strm->substream_types[portidx];
}

ffado_streaming_stream_type ffado_streaming_get_playback_stream_type(ffado_device_t *dev, int i) {
    assert(i < dev->playback_port_stream.size());
    struct stream_settings * strm = dev->playback_port_stream.at(i);
    unsigned int portidx = dev->playback_port_source_index.at(i);
    return strm->substream_types[portidx];
}

int ffado_streaming_set_audio_datatype(ffado_device_t *dev,
    ffado_streaming_audio_datatype t) {
    if(t == ffado_audio_datatype_float) return 0;
    else {
        debugError("Only float streams supported\n");
        return -1;
    }
}

ffado_streaming_audio_datatype ffado_streaming_get_audio_datatype(ffado_device_t *dev) {
    return ffado_audio_datatype_float; // FIXME: make this flexible?
}

int ffado_streaming_playback_stream_onoff(ffado_device_t *dev, int number, int on) {
    enum stream_port_state * port_state = dev->playback_port_states.at(number);
    *port_state = on ? STREAM_PORT_STATE_ON : STREAM_PORT_STATE_OFF;
    return 0; // FIXME: make void
}

int ffado_streaming_capture_stream_onoff(ffado_device_t *dev, int number, int on) {
    enum stream_port_state * port_state = dev->capture_port_states.at(number);
    *port_state = on ? STREAM_PORT_STATE_ON : STREAM_PORT_STATE_OFF;
    return 0; // FIXME: make void
}

int ffado_streaming_set_capture_stream_buffer(ffado_device_t *dev, int i, char *buff) {
    void ** buff_ptr = dev->capture_port_buffers.at(i);
    *buff_ptr = (void *)buff;
//     debugOutput(DEBUG_LEVEL_VERBOSE, "capture port %d gets %p stored at %p\n", i, buff, buff_ptr);
    return 0; // FIXME: make void
}

int ffado_streaming_set_playback_stream_buffer(ffado_device_t *dev, int i, char *buff) {
    void ** buff_ptr = dev->playback_port_buffers.at(i);
    *buff_ptr = (void *)buff;
//     debugOutput(DEBUG_LEVEL_VERBOSE, "playback port %d gets %p stored at %p\n", i, buff, buff_ptr);
    return 0; // FIXME: make void
}
