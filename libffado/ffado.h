/* ffado.h
 *
 * Copyright (C) 2005-2007 by Pieter Palmers
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#ifndef FFADO_H
#define FFADO_H

#define FFADO_MAX_NAME_LEN 256

#define FFADO_BOUNCE_SERVER_VENDORNAME  "FFADO Server"
#define FFADO_BOUNCE_SERVER_MODELNAME   "ffado-server"

#define FFADO_BOUNCE_SERVER_GETXMLDESCRIPTION_CMD
#define AVC1394_SUBUNIT_TYPE_FFADO_BOUNCE_SERVER     0x0D

#define FFADO_API_VERSION 2

enum ffado_direction {
    FFADO_CAPTURE  = 0,
    FFADO_PLAYBACK = 1,
};

typedef struct ffado_handle* ffado_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

ffado_handle_t
ffado_new_handle( int port );

int
ffado_destroy_handle( ffado_handle_t ffado_handle );

int
ffado_discover_devices( ffado_handle_t ffado_handle, int verbose_level );

int ffado_node_is_valid_ffado_device(ffado_handle_t fb_handle, int node_id);
int ffado_get_nb_devices_on_bus(ffado_handle_t fb_handle);

int ffado_get_device_node_id(ffado_handle_t fb_handle, int device_nr);
int ffado_set_samplerate(ffado_handle_t ffado_handle, int node_id, int samplerate);

/* ABI stuff */
const char*
ffado_get_version();

int
ffado_get_api_version();

/* various function */

/* workaround: wait usec after each AVC command.
   will disapear as soon bug is fixed */    
void ffado_sleep_after_avc_command( int time );

#ifdef __cplusplus
}
#endif

#endif /* FFADO_H */

/* ffado_streaming.h
 *
 * Specification for the FFADO Streaming API
 *
 */
#ifndef __FFADO_STREAMING_H__
#define __FFADO_STREAMING_H__

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FFADO_STREAMING_MAX_URL_LENGTH 2048

#define FFADO_IGNORE_CAPTURE         (1<<0)
#define FFADO_IGNORE_PLAYBACK     (1<<1)


/* The basic operation of the API is as follows:
 * 
 * ffado_streaming_init()
 * ffado_streaming_start()
 * while(running) {
 *   retval = ffado_streaming_wait();
 *   if (retval == -1) {
 *     ffado_streaming_reset();
 *     continue;
 *   }
 *
 *   ffado_streaming_transfer_buffers(dev);
 *
 *   for(all channels) {
 *     switch (channel_type) {
 *     case audio:
 *       bytesread=ffado_streaming_read(audioinbuffer[channel]);
 *       byteswritten=ffado_streaming_write(audiooutbuffer[channel]);
 *     case midi:
 *       bytesread=ffado_streaming_read(midiinbuffer[channel]);
 *       byteswritten=ffado_streaming_write(midioutbuffer[channel]);
 *     }
 *   }
 * }
 * ffado_streaming_stop();
 * ffado_streaming_finish();
 *
 */

typedef struct _ffado_device ffado_device_t;

/**
 * The sample format used by the ffado streaming API
 */

typedef unsigned int ffado_sample_t;


typedef unsigned int ffado_nframes_t;


typedef struct ffado_device_info {
    /* TODO: How is the device specification done? */
//    ffado_device_info_location_type location_type;
} ffado_device_info_t;

/**
 * Structure to pass the options to the ffado streaming code.
 */
typedef struct ffado_options {
    /* driver related setup */
    int sample_rate;         /* this is acutally dictated by the device
                             * you can specify a value here or -1 to autodetect
                              */

    /* buffer setup */
    int period_size;     /* one period is the amount of frames that
                 * has to be sent or received in order for
                 * a period boundary to be signalled.
                 * (unit: frames)
                 */
    int nb_buffers;    /* the size of the frame buffer (in periods) */

    /* packetizer thread options */
    int realtime;
    int packetizer_priority;
    
    /* libffado related setup */
    int node_id;
    int port;
    
    /* direction map */
    int directions;
    
    /* verbosity */
    int verbose;
    
    /* slave mode */
    int slave_mode;
    
    /* snoop mode */
    int snoop_mode;

} ffado_options_t;

/**
 * The types of streams supported by the API
 *
 * A ffado_audio type stream is a stream that consists of successive samples.
 * The format is a 24bit UINT in host byte order, aligned as the 24LSB's of the 
 * 32bit UINT of the read/write buffer.
 * The wait operation looks at this type of streams only.
 *
 * A ffado_midi type stream is a stream of midi bytes. The bytes are 8bit UINT,
 * aligned as the first 8LSB's of the 32bit UINT of the read/write buffer.
 *
 */
typedef enum {
    ffado_stream_type_invalid                      =   -1,
      ffado_stream_type_unknown                      =   0,
      ffado_stream_type_audio                        =   1,
      ffado_stream_type_midi                         =   2,
      ffado_stream_type_control                      =   3,
} ffado_streaming_stream_type;

/**
 * 
 * Buffer types known to the API
 * 
 */
typedef enum {
    ffado_buffer_type_per_stream          =   -1, // use this to use the per-stream read functions
    ffado_buffer_type_int24           =   0,
    ffado_buffer_type_float            =   1,
    ffado_buffer_type_midi            =   2,
//     ffado_buffer_type_uint32           =   2,
//     ffado_buffer_type_double           =   3,
//     ...
} ffado_streaming_buffer_type;

/**
 * Initializes the streaming from/to a FFADO device. A FFADO device
 * is a virtual device composed of several BeBoB or compatible devices,
 * linked together in one sync domain.
 *
 * This prepares all IEEE1394 related stuff and sets up all buffering.
 * It elects a sync master if nescessary.
 * 
 * @param device_info provides a way to specify the virtual device
 * @param options options regarding buffers, ieee1394 setup, ...
 *
 * @return Opaque device handle if successful.  If this is NULL, the
 * init operation failed.
 *
 */
ffado_device_t *ffado_streaming_init (ffado_device_info_t *device_info,
                     ffado_options_t options);

/**
 * preparation should be done after setting all per-stream parameters
 * the way you want them. being buffer data type etc...
 *
 * @param dev the ffado device
 * @return 
 */
int ffado_streaming_prepare(ffado_device_t *dev);


/**
 * Finishes the FFADO streaming. Cleans up all internal data structures
 * and terminates connections.
 *
 * @param dev the ffado device to be closed.
 */
void ffado_streaming_finish(ffado_device_t *dev);

/**
 * Returns the amount of capture channels available
 *
 * @param dev the ffado device
 *
 * @return the number of capture streams present & active on the device. 
 *         can be 0. returns -1 upon error.
 */
int ffado_streaming_get_nb_capture_streams(ffado_device_t *dev);

/**
 * Returns the amount of playack channels available
 *
 * @param dev the ffado device
 *
 * @return the number of playback streams present & active on the device. 
 *         can be 0. returns -1 upon error.
 */
int ffado_streaming_get_nb_playback_streams(ffado_device_t *dev);

/**
 * Copies the capture channel name into the specified buffer
 *
 * @param dev the ffado device
 * @param number the stream number
 * @param buffer the buffer to copy the name into. has to be allocated.
 * @param buffersize the size of the buffer
 *
 * @return the number of characters copied into the buffer
 */
int ffado_streaming_get_capture_stream_name(ffado_device_t *dev, int number, char* buffer, size_t buffersize);

/**
 * Copies the playback channel name into the specified buffer
 *
 * @param dev the ffado device
 * @param number the stream number
 * @param buffer the buffer to copy the name into. has to be allocated.
 * @param buffersize the size of the buffer
 *
 * @return the number of characters copied into the buffer
 */
int ffado_streaming_get_playback_stream_name(ffado_device_t *dev, int number, char* buffer, size_t buffersize);

/**
 * Returns the type of a capture channel
 *
 * @param dev the ffado device
 * @param number the stream number
 *
 * @return the channel type 
 */
ffado_streaming_stream_type ffado_streaming_get_capture_stream_type(ffado_device_t *dev, int number);

/**
 * Returns the type of a playback channel
 *
 * @param dev the ffado device
 * @param number the stream number
 *
 * @return the channel type 
 */
ffado_streaming_stream_type ffado_streaming_get_playback_stream_type(ffado_device_t *dev, int number);
/*
 *
 * Note: buffer handling will change in order to allow setting the sample type for *_read and *_write
 * and separately indicate if you want to use a user buffer or a managed buffer.
 *
 */
 
 
 
/**
 * Sets the decode buffer for the stream. This allows for zero-copy decoding.
 * The call to ffado_streaming_transfer_buffers will decode one period of the stream to
 * this buffer. Make sure it is large enough. 
 * 
 * @param dev the ffado device
 * @param number the stream number
 * @param buff a pointer to the sample buffer, make sure it is large enough 
 *             i.e. sizeof(your_sample_type)*period_size
 * @param t   the type of the buffer. this determines sample type and the decode function used.
 *
 * @return -1 on error, 0 on success
 */

int ffado_streaming_set_capture_stream_buffer(ffado_device_t *dev, int number, char *buff);
int ffado_streaming_set_capture_buffer_type(ffado_device_t *dev, int i, ffado_streaming_buffer_type t);
int ffado_streaming_capture_stream_onoff(ffado_device_t *dev, int number, int on);

/**
 * Sets the encode buffer for the stream. This allows for zero-copy encoding (directly to the events).
 * The call to ffado_streaming_transfer_buffers will encode one period of the stream from
 * this buffer to the event buffer.
 * 
 * @param dev the ffado device
 * @param number the stream number
 * @param buff a pointer to the sample buffer
 * @param t   the type of the buffer. this determines sample type and the decode function used.
 *
 * @return -1 on error, 0 on success
 */
int ffado_streaming_set_playback_stream_buffer(ffado_device_t *dev, int number, char *buff);
int ffado_streaming_set_playback_buffer_type(ffado_device_t *dev, int i, ffado_streaming_buffer_type t);
int ffado_streaming_playback_stream_onoff(ffado_device_t *dev, int number, int on);


/**
 * preparation should be done after setting all per-stream parameters
 * the way you want them. being buffer data type etc...
 *
 * @param dev 
 * @return 
 */
 
int ffado_streaming_prepare(ffado_device_t *dev);

/**
 * Starts the streaming operation. This initiates the connections to the FFADO devices and
 * starts the packet handling thread(s). This has to be called before any I/O can occur.
 *
 * @param dev the ffado device
 *
 * @return 0 on success, -1 on failure.
 */
int ffado_streaming_start(ffado_device_t *dev);

/**
 * Stops the streaming operation. This closes the connections to the FFADO devices and
 * stops the packet handling thread(s). 
 *
 * @param dev the ffado device
 *
 * @return 0 on success, -1 on failure.
 */
int ffado_streaming_stop(ffado_device_t *dev);

/**
 * Resets the streaming as if it was stopped and restarted. The difference is that the connections
 * are not nescessarily broken and restored.
 *
 * All buffers are reset in the initial state and all data in them is lost.
 *
 * @param dev the ffado device
 *
 * @return 0 on success, -1 on failure.
 */
int ffado_streaming_reset(ffado_device_t *dev);

/**
 * Waits until there is at least one period of data available on all capture connections and
 * room for one period of data on all playback connections
 *
 * @param dev the ffado device
 *
 * @return The number of frames ready. -1 when a problem occurred.
 */
int ffado_streaming_wait(ffado_device_t *dev);

/**
 * Reads from a specific channel to a supplied buffer.
 * 
 * @param dev the ffado device
 * @param number the stream number
 * @param buffer the buffer to copy the samples into
 * @param nsamples the number of samples to be read. the buffer has to be big enough for this amount of samples.
 *
 * @return the amount of samples actually read. -1 on error (xrun).
 */
int ffado_streaming_read(ffado_device_t *dev, int number, ffado_sample_t *buffer, int nsamples);

/**
 * Write to a specific channel from a supplied buffer.
 * 
 * @param dev the ffado device
 * @param number the stream number
 * @param buffer the buffer to copy the samples from
 * @param nsamples the number of samples to be written.
 *
 * @return the amount of samples actually written. -1 on error.
 */
int ffado_streaming_write(ffado_device_t *dev, int number, ffado_sample_t *buffer, int nsamples);

/**
 * Transfer & decode the events from the packet buffer to the sample buffers
 * 
 * This should be called after the wait call returns, before reading/writing the sample buffers
 * with ffado_streaming_[read|write].
 * 
 * The purpose is to allow more precise timing information. ffado_streaming_wait returns as soon as the 
 * period boundary is crossed, and can therefore be used to determine the time instant of this crossing (e.g. jack DLL).
 *
 * The actual decoding work is done in this function and can therefore be omitted in this timing calculation.
 * Note that you HAVE to call this function in order for the buffers not to overflow, and only call it when
 * ffado_streaming_wait doesn't indicate a buffer xrun (xrun handler resets buffer).
 * 
 * If user supplied playback buffers are specified with ffado_streaming_set_playback_buffers
 * their contents should be valid before calling this function.
 * If user supplied capture buffers are specified with ffado_streaming_set_capture_buffers
 * their contents are updated in this function.
 * 
 * Use either ffado_streaming_transfer_buffers to transfer all buffers at once, or use 
 * ffado_streaming_transfer_playback_buffers and ffado_streaming_transfer_capture_buffers 
 * to have more control. Don't use both.
 * 
 * @param dev the ffado device
 * @return  -1 on error.
 */
 
int ffado_streaming_transfer_buffers(ffado_device_t *dev);

/**
 * Transfer & encode the events from the sample buffers to the packet buffer
 * 
 * This should be called after the wait call returns, after writing the sample buffers
 * with ffado_streaming_write.
 * 
 * If user supplied playback buffers are specified with ffado_streaming_set_playback_buffers
 * their contents should be valid before calling this function.
 * 
 * Use either ffado_streaming_transfer_buffers to transfer all buffers at once, or use 
 * ffado_streaming_transfer_playback_buffers and ffado_streaming_transfer_capture_buffers 
 * to have more control. Don't use both.
 * 
 * @param dev the ffado device
 * @return  -1 on error.
 */
 
int ffado_streaming_transfer_playback_buffers(ffado_device_t *dev);

/**
 * Transfer & decode the events from the packet buffer to the sample buffers
 * 
 * This should be called after the wait call returns, before reading the sample buffers
 * with ffado_streaming_read.
 * 
 * If user supplied capture buffers are specified with ffado_streaming_set_capture_buffers
 * their contents are updated in this function.
 * 
 * Use either ffado_streaming_transfer_buffers to transfer all buffers at once, or use 
 * ffado_streaming_transfer_playback_buffers and ffado_streaming_transfer_capture_buffers 
 * to have more control. Don't use both.
 * 
 * @param dev the ffado device
 * @return  -1 on error.
 */

int ffado_streaming_transfer_capture_buffers(ffado_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* FFADO_STREAMING */
