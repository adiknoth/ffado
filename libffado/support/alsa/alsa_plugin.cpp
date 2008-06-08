/*
 *  ALSA FireWire Plugin
 *
 *  Copyright (c) 2008 Pieter Palmers <pieter.palmers@ffado.org>
 *
 *   Based upon the JACK <-> ALSA plugin
 *    Copyright (c) 2003 by Maarten de Boer <mdeboer@iua.upf.es>
 *                  2005 Takashi Iwai <tiwai@suse.de>
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <src/debugmodule/debugmodule.h>
DECLARE_GLOBAL_DEBUG_MODULE;

#include "libutil/IpcRingBuffer.h"
#include "libutil/SystemTimeSource.h"
using namespace Util;

extern "C" {

#include "version.h"

#include <byteswap.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <inttypes.h>

#include <pthread.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#define FFADO_PLUGIN_VERSION "0.0.1"

// #define PRINT_FUNCTION_ENTRY (printMessage("entering %s\n",__FUNCTION__))
#define PRINT_FUNCTION_ENTRY

typedef struct {
    snd_pcm_ioplug_t io;

    int fd;
    int activated;

    unsigned int hw_ptr;
    unsigned int period_size;
    unsigned int channels;
    snd_pcm_channel_area_t *areas;

    snd_pcm_stream_t stream;

    // thread for polling
    pthread_t thread;

    // IPC stuff
    Util::IpcRingBuffer* buffer;

    // options
    long int verbose;
    long int period;
    long int nb_buffers;

} snd_pcm_ffado_t;

static int snd_pcm_ffado_hw_params(snd_pcm_ioplug_t *io, snd_pcm_hw_params_t *params) {
    PRINT_FUNCTION_ENTRY;
    snd_pcm_ffado_t *ffado = (snd_pcm_ffado_t *)io->private_data;

    ffado->channels=0;
    if (ffado->stream == SND_PCM_STREAM_PLAYBACK) {
        // HACK
        ffado->channels=2;
    } else {
        // HACK
        ffado->channels=2;
    }

    return 0;
}

static int
snd_pcm_ffado_pollfunction(snd_pcm_ffado_t *ffado)
{

//      PRINT_FUNCTION_ENTRY;
    static char buf[1];
    snd_pcm_ioplug_t *io = &ffado->io;
    const snd_pcm_channel_area_t *areas;
    unsigned int i;

    assert(ffado);
    assert(ffado->buffer);

    // wait for data to become available
    // FIXME

    // get the data address everything should go to/come from
    areas = snd_pcm_ioplug_mmap_areas(io);

    // get the current offset
    snd_pcm_uframes_t offset = ffado->hw_ptr;

    IpcRingBuffer::eResult res;
    if (ffado->stream == SND_PCM_STREAM_PLAYBACK) {
        do {
            uint32_t *audiobuffers_raw;
            res = ffado->buffer->requestBlockForWrite((void**) &audiobuffers_raw); // pointer voodoo
            if(res == IpcRingBuffer::eR_OK) {
                memset(audiobuffers_raw, 0, ffado->channels * ffado->period * 4);
                for (i = 0; i < ffado->channels; i++) {
                    uint32_t *alsa_data_ptr = (uint32_t *)((char *)areas[i].addr + (areas[i].step * offset / 8));
                    uint32_t *ffado_data_ptr = audiobuffers_raw + i*ffado->period;
                    memcpy(ffado_data_ptr, alsa_data_ptr, ffado->period * 4);
                }
                // release the block
                res = ffado->buffer->releaseBlockForWrite();
                if(res != IpcRingBuffer::eR_OK) {
                    debugOutput(DEBUG_LEVEL_NORMAL, "PBK: error committing memory block\n");
                    break;
                }
            } else if(res != IpcRingBuffer::eR_Again) {
                debugOutput(DEBUG_LEVEL_NORMAL, "PBK: error getting memory block\n");
            }
        } while (res == IpcRingBuffer::eR_Again);
    } else {
        do {
            uint32_t *audiobuffers_raw;
            res = ffado->buffer->requestBlockForRead((void**) &audiobuffers_raw); // pointer voodoo
            if(res == IpcRingBuffer::eR_OK) {
                for (i = 0; i < ffado->channels; i++) {
                    uint32_t *alsa_data_ptr = (uint32_t *)((char *)areas[i].addr + (areas[i].step * offset / 8));
                    uint32_t *ffado_data_ptr = audiobuffers_raw + i*ffado->period;
                    memcpy(alsa_data_ptr, ffado_data_ptr, ffado->period * 4);
                }
                // release the block
                res = ffado->buffer->releaseBlockForRead();
                if(res != IpcRingBuffer::eR_OK) {
                    debugOutput(DEBUG_LEVEL_NORMAL, "CAP: error committing memory block\n");
                    break;
                }
            } else if(res != IpcRingBuffer::eR_Again) {
                debugOutput(DEBUG_LEVEL_NORMAL, "CAP: error getting memory block\n");
            }
        } while (res == IpcRingBuffer::eR_Again);
    }

    ffado->hw_ptr += ffado->period;
    ffado->hw_ptr %= io->buffer_size;

    write(ffado->fd, buf, 1); /* for polling */

    if(res == IpcRingBuffer::eR_OK) {
        return 0;
    } else {
        debugOutput(DEBUG_LEVEL_NORMAL, "error happened\n");
        return -1;
    }
}

static void * ffado_workthread(void *arg)
{
    PRINT_FUNCTION_ENTRY;
    snd_pcm_ffado_t *ffado = (snd_pcm_ffado_t *)arg;

    int oldstate;

    pthread_setcancelstate (PTHREAD_CANCEL_DEFERRED, &oldstate);

    while (1) {
        snd_pcm_ffado_pollfunction(ffado);
        pthread_testcancel();
    }

}

static int snd_pcm_ffado_poll_revents(snd_pcm_ioplug_t *io,
                     struct pollfd *pfds, unsigned int nfds,
                     unsigned short *revents)
{
    static char buf[1];
    PRINT_FUNCTION_ENTRY;
    assert(pfds && nfds == 1 && revents);
    read(pfds[0].fd, buf, 1);
    *revents = pfds[0].revents;
    return 0;
}

static void snd_pcm_ffado_free(snd_pcm_ffado_t *ffado)
{
    PRINT_FUNCTION_ENTRY;
    if (ffado) {
        close(ffado->fd);
        close(ffado->io.poll_fd);
        free(ffado);
    }
}

static int snd_pcm_ffado_close(snd_pcm_ioplug_t *io)
{
    PRINT_FUNCTION_ENTRY;
    snd_pcm_ffado_t *ffado = (snd_pcm_ffado_t *)io->private_data;

    // cleanup the SHM structures here
    delete ffado->buffer;
    ffado->buffer = NULL;

    snd_pcm_ffado_free(ffado);
    return 0;
}

static snd_pcm_sframes_t snd_pcm_ffado_pointer(snd_pcm_ioplug_t *io)
{
    PRINT_FUNCTION_ENTRY;
    snd_pcm_ffado_t *ffado = (snd_pcm_ffado_t *)io->private_data;
    return ffado->hw_ptr;
}

static int snd_pcm_ffado_start(snd_pcm_ioplug_t *io)
{
    int result = 0;
    snd_pcm_ffado_t *ffado = (snd_pcm_ffado_t *)io->private_data;

    PRINT_FUNCTION_ENTRY;

    result = pthread_create (&ffado->thread, 0, ffado_workthread, ffado);
    if(result) return result;

    // FIXME: start the SHM stuff
    return 0;
}

static int snd_pcm_ffado_stop(snd_pcm_ioplug_t *io)
{
    snd_pcm_ffado_t *ffado = (snd_pcm_ffado_t *)io->private_data;

    PRINT_FUNCTION_ENTRY;

    if(pthread_cancel(ffado->thread)) {
        debugError("could not cancel thread!\n");
    }

    if(pthread_join(ffado->thread,NULL)) {
        debugError("could not join thread!\n");
    }

    // FIXME: stop the SHM client
    return 0;
} 

static int snd_pcm_ffado_prepare(snd_pcm_ioplug_t *io)
{
    PRINT_FUNCTION_ENTRY;
    return 0;
}

static snd_pcm_ioplug_callback_t ffado_pcm_callback;

#define ARRAY_SIZE(ary)    (sizeof(ary)/sizeof(ary[0]))

static int ffado_set_hw_constraint(snd_pcm_ffado_t *ffado)
{
    PRINT_FUNCTION_ENTRY;
    unsigned int access_list[] = {
        SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
        SND_PCM_ACCESS_RW_NONINTERLEAVED,
    };

    unsigned int rate_list[1];

    unsigned int format = SND_PCM_FORMAT_S24;
    int err;

    // FIXME: make all of the parameters dynamic instead of static
    rate_list[0] = 48000;

    // setup the plugin capabilities
    if ((err = snd_pcm_ioplug_set_param_list(&ffado->io, SND_PCM_IOPLUG_HW_ACCESS,
                         ARRAY_SIZE(access_list), access_list)) < 0 ||
        (err = snd_pcm_ioplug_set_param_list(&ffado->io, SND_PCM_IOPLUG_HW_RATE,
                         ARRAY_SIZE(rate_list), rate_list)) < 0 ||
        (err = snd_pcm_ioplug_set_param_list(&ffado->io, SND_PCM_IOPLUG_HW_FORMAT,
                         1, &format)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(&ffado->io, SND_PCM_IOPLUG_HW_CHANNELS,
                           ffado->channels, ffado->channels)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(&ffado->io, SND_PCM_IOPLUG_HW_PERIOD_BYTES,
                           ffado->period, ffado->period)) < 0 ||
        (err = snd_pcm_ioplug_set_param_minmax(&ffado->io, SND_PCM_IOPLUG_HW_PERIODS,
                           ffado->nb_buffers, ffado->nb_buffers)) < 0)
        return err;

    return 0;
}

static int snd_pcm_ffado_open(snd_pcm_t **pcmp, const char *name,
                 snd_pcm_stream_t stream, int mode)
{

    PRINT_FUNCTION_ENTRY;
    snd_pcm_ffado_t *ffado;
    int err;
    int fd[2];

    assert(pcmp);

    ffado = (snd_pcm_ffado_t *)calloc(1, sizeof(*ffado));
    if (!ffado)
        return -ENOMEM;

    ffado->stream=stream;

    // discover the devices to discover the capabilities
    // get the SHM structure

    socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);

    ffado->fd = fd[0];

    // initialize callback struct
    ffado_pcm_callback.close = snd_pcm_ffado_close;
    ffado_pcm_callback.start = snd_pcm_ffado_start;
    ffado_pcm_callback.stop = snd_pcm_ffado_stop;
    ffado_pcm_callback.pointer = snd_pcm_ffado_pointer;
    ffado_pcm_callback.hw_params = snd_pcm_ffado_hw_params;
    ffado_pcm_callback.prepare = snd_pcm_ffado_prepare;
    ffado_pcm_callback.poll_revents = snd_pcm_ffado_poll_revents;

    // prepare io struct
    ffado->io.version = SND_PCM_IOPLUG_VERSION;
    ffado->io.name = "FFADO PCM Plugin";
    ffado->io.callback = &ffado_pcm_callback;
    ffado->io.private_data = ffado;
    ffado->io.mmap_rw = 1;
    ffado->io.poll_fd = fd[1];
    ffado->io.poll_events = stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;

    err = snd_pcm_ioplug_create(&ffado->io, name, stream, mode);
    if (err < 0) {
        snd_pcm_ffado_free(ffado);
        return err;
    }

    err = ffado_set_hw_constraint(ffado);
    if (err < 0) {
        snd_pcm_ioplug_delete(&ffado->io);
        return err;
    }

    *pcmp = ffado->io.pcm;

    // these are the params
    ffado->channels = 2;
    ffado->period = 1024;
    ffado->verbose = 6;
    ffado->nb_buffers = 5;

    setDebugLevel(ffado->verbose);

    // prepare the IPC buffer
    unsigned int buffsize = ffado->channels * ffado->period * 4;
    if(stream == SND_PCM_STREAM_PLAYBACK) {
        ffado->buffer = new IpcRingBuffer("playbackbuffer",
                              IpcRingBuffer::eBT_Slave,
                              IpcRingBuffer::eD_Outward,
                              IpcRingBuffer::eB_Blocking,
                              ffado->nb_buffers, buffsize);
        if(ffado->buffer == NULL) {
            debugError("Could not create playbackbuffer\n");
            return -1;
        }
        if(!ffado->buffer->init()) {
            debugError("Could not init playbackbuffer\n");
            delete ffado->buffer;
            ffado->buffer = NULL;
            return -1;
        }
        ffado->buffer->setVerboseLevel(ffado->verbose);
    } else {
        ffado->buffer = new IpcRingBuffer("capturebuffer",
                              IpcRingBuffer::eBT_Slave,
                              IpcRingBuffer::eD_Inward,
                              IpcRingBuffer::eB_Blocking,
                              ffado->nb_buffers, buffsize);
        if(ffado->buffer == NULL) {
            debugError("Could not create capturebuffer\n");
            return -1;
        }
        if(!ffado->buffer->init()) {
            debugError("Could not init capturebuffer\n");
            delete ffado->buffer;
            ffado->buffer = NULL;
            return -1;
        }
        ffado->buffer->setVerboseLevel(ffado->verbose);
    }

    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(ffado)
{
    printMessage("FireWire plugin for ALSA\n  version %s compiled %s %s\n  using %s\n", 
        FFADO_PLUGIN_VERSION, __DATE__, __TIME__, PACKAGE_STRING);

    snd_config_iterator_t i, next;
    int err;

    snd_config_for_each(i, next, conf) {
        snd_config_t *n = snd_config_iterator_entry(i);
        const char *id;
        if (snd_config_get_id(n, &id) < 0)
            continue;
        if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0)
            continue;

        SNDERR("Unknown field %s", id);
        return -EINVAL;
    }

    err = snd_pcm_ffado_open(pcmp, name, stream, mode);

    return err;

}

SND_PCM_PLUGIN_SYMBOL(ffado);

} // extern "C"
