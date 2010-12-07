/*
 * Copyright (C) 2009-2010 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <string.h>

#include "config.h"

typedef uint32_t __u32;
typedef uint64_t __u64;

#include "connections.h"

#include "libstreaming/util/cip.h"
#include "libieee1394/cycletimer2.h"

#include "libutil/ByteSwap.h"

#include "am824stream.h"
#include "streamer.h"

#include "debugmodule/debugmodule.h"
DECLARE_GLOBAL_DEBUG_MODULE;

#define AMDTP_FLOAT_MULTIPLIER (1.0f * ((1<<23) - 1))

#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

// Set to 1 to enable the generation of a 1 kHz test tone in analog output 1.  Even with
// this defined to 1 the test tone will now only be produced if run with a non-zero 
// debug level.
#define TESTTONE 1

#if TESTTONE
#include <math.h>
#endif

static inline int am824_generate_data_packet(struct stream_settings *settings,
                                             struct am824_settings *amdtp_settings,
                                             struct iec61883_packet *packet,
                                             __u32 tsp, int *length, size_t offset);


static inline int am824_generate_empty_packet(struct stream_settings *settings,
                                              struct am824_settings *amdtp_settings,
                                              struct iec61883_packet *packet,
                                              int *length);

static inline int
am824_demux_audio_ports(struct stream_settings *settings,
                        uint32_t *data, size_t nframes,
                        size_t offset);
static inline int
am824_demux_midi_ports(struct stream_settings *settings,
                       uint32_t *data, size_t nframes,
                       size_t offset);

static inline int
am824_generate_data_payload(struct stream_settings *settings,
                            struct am824_settings *amdtp_settings,
                            uint32_t *data, size_t offset);
static inline int
am824_generate_empty_payload(struct stream_settings *settings,
                             struct am824_settings *amdtp_settings,
                             uint32_t *data);

static inline void
am824_generate_audio_channel(uint32_t *target_event,
                             void* buffer, size_t offset,
                             size_t nframes,
                             size_t dimension);
static inline void
am824_generate_audio_channel_silent(uint32_t *target_event,
                                    size_t nframes,
                                    size_t dimension);

static inline void
am824_generate_midi_channel(uint32_t *target_event,
                            void* fbuff, size_t offset,
                            size_t nframes,
                            size_t dimension);
static inline void
am824_generate_midi_channel_silent(uint32_t *target_event,
                                   size_t nframes,
                                   size_t dimension);

static inline unsigned int
am824_get_syt_interval(unsigned int rate);

static inline unsigned int
am824_get_fdf(unsigned int rate);

#define INCREMENT_AND_WRAP(val, incr, wrapat) \
    assert(incr < wrapat); \
    val += incr; \
    if(val >= wrapat) { \
        val -= wrapat; \
    } \
    assert(val < wrapat);

void am824_dump_packet(uint32_t *data, unsigned int len, unsigned int nbchannels) {
    if(len < 2) {
        debugError("Bogus packet, no header\n");
        return;
    }
    struct iec61883_packet *packet =
            (struct iec61883_packet *) data;

    debugOutput(DEBUG_LEVEL_VERBOSE, "=== AMDTP packet ===\n");
    debugOutput(DEBUG_LEVEL_VERBOSE,
                " H: %08X %08X | FMT: %02X, FDF: %02X, SYT: %04X | PKTLEN: %d\n",
                *data, *(data + 1),
                packet->fmt, packet->fdf, packet->syt, len);
    data += 2;
    len -= 2;

    if(len % nbchannels != 0) {
        debugError("Bogus payload len: %d\n", len);
        return;
    }

    char tmpbuff[1024];
    int n = 0;
    int i = 0;
    while(len) {
        n = 0;
        int j;
        int printed = 0;
        printed = snprintf(tmpbuff + n, 1024-n, "%2d: ", i);
        n += printed;
        for(j = 0; j < nbchannels; j++) {
            printed = snprintf(tmpbuff + n, 1024-n, "%08X ", *data);
            n += printed;
            data++;
        }
        printed = snprintf(tmpbuff + n, 1024-n, "\n");
        n += printed;
        len -= nbchannels;
        i++;
        debugOutput(DEBUG_LEVEL_VERBOSE, "%s", tmpbuff);
    }

}

int am824_init(struct am824_settings *settings, int node_id, 
               unsigned int nb_channels, unsigned int samplerate)
{
    assert(settings);

    settings->samplerate = samplerate;

    settings->send_nodata_payload = AMDTP_SEND_PAYLOAD_IN_NODATA_XMIT_BY_DEFAULT;
    settings->transfer_delay = AMDTP_TRANSMIT_TRANSFER_DELAY;

    settings->syt_interval = am824_get_syt_interval(settings->samplerate);
    settings->packet_length = settings->syt_interval * 4 * nb_channels + 8;

    settings->header.sid = node_id;

    settings->header.dbs = nb_channels;
    settings->header.fn = 0;
    settings->header.qpc = 0;
    settings->header.sph = 0;
    settings->header.reserved = 0;
    settings->header.dbc = 0;
    settings->header.eoh1 = 2;
    settings->header.fmt = IEC61883_FMT_AMDTP;
    settings->header.fdf = am824_get_fdf(settings->samplerate);

    return 0;
}

unsigned int
am824_get_syt_interval(unsigned int rate) {
    switch (rate) {
        case 32000:
        case 44100:
        case 48000:
            return 8;
        case 88200:
        case 96000:
            return 16;
        case 176400:
        case 192000:
            return 32;
        default:
            debugError("Unsupported rate: %d\n", rate);
            return 0;
    }
}

unsigned int
am824_get_fdf(unsigned int rate) {
    switch (rate) {
        case 32000: return IEC61883_FDF_SFC_32KHZ;
        case 44100: return IEC61883_FDF_SFC_44K1HZ;
        case 48000: return IEC61883_FDF_SFC_48KHZ;
        case 88200: return IEC61883_FDF_SFC_88K2HZ;
        case 96000: return IEC61883_FDF_SFC_96KHZ;
        case 176400: return IEC61883_FDF_SFC_176K4HZ;
        case 192000: return IEC61883_FDF_SFC_192KHZ;
        default:
            debugError("Unsupported rate: %d\n", rate);
            return 0;
    }
}

//*****************************************************
//*                                                   *
//* RECEIVE                                           *
//*                                                   *
//*****************************************************

enum conn_callback_status
receive_am824_packet_data(struct stream_info *stream, uint32_t tsp, uint32_t *data, int* len)
{
    assert(stream);
    assert(data);
    assert(len);
    assert(stream->settings);
    assert(stream->settings->client_data);

    struct iec61883_packet *packet =
            (struct iec61883_packet *) data;
    struct am824_settings *settings = 
            (struct am824_settings *)stream->settings->client_data;

    int sec     = (tsp >> 13) & 0x7;
    int cycle   = (tsp >>  0) & 0x1FFF;

    if(stream->last_tsp != INVALID_TIMESTAMP_TICKS) {
        int last_cycle   = (stream->last_recv_tsp >>  0) & 0x1FFF;
        INCREMENT_AND_WRAP(last_cycle, 1, 8000);
        if(cycle != last_cycle) {
            debugError("non-consequtive cycle count: %d => %d\n",
                       last_cycle, cycle);

            // fix up the last_tsp to ensure sync is not confused
            // FIXME: doesn't work!
//             int delta = cycle - last_cycle;
//             if(delta < 0) delta += 8000; // causality ensures that it was a delay
//             delta *= TICKS_PER_CYCLE;
//             delta %= MAX_TICKS;
//             stream->last_tsp = addTicks(stream->last_tsp, delta);

            // TODO set a flag
            stream->last_tsp = INVALID_TIMESTAMP_TICKS;
        }
    }
    stream->last_recv_tsp = tsp;

    bool ok = (packet->fdf != 0xFF) &&
              (packet->fmt == 0x10) &&
              (*len >= 2*sizeof(uint32_t));

    uint32_t this_timestamp = INVALID_TIMESTAMP_TICKS;
    enum conn_callback_status retval = CONN_NEED_MORE;
    if(ok) {
        if(packet->syt != 0xFFFF) {
            assert(packet->dbs > 0);
            this_timestamp = sytRecvToFullTicks2(
                    (uint32_t)CondSwapFromBus16(packet->syt),
                    (tsp << 12));

            if(TICKS_TO_SYT(this_timestamp) != CondSwapFromBus16(packet->syt)) {
                debugError("Bogus TSP %x | %x\n", 
                           (unsigned int)TICKS_TO_SYT(this_timestamp), 
                           (unsigned int)CondSwapFromBus16(packet->syt));
            }

            unsigned int nframes = ((*len / sizeof (uint32_t)) - 2)/packet->dbs;
            assert(nframes == settings->syt_interval);
            assert((nframes & 0x7) == 0);

            // if we are the sync connection, update the rate
            float current_rate = stream->master->tpf;
            if(stream == stream->master) {
                if(stream->last_tsp != INVALID_TIMESTAMP_TICKS) {
                    int delta = diffTicks(this_timestamp, stream->last_tsp);
                    assert(nframes > 0);

                    current_rate = ((float)delta) / ((float)nframes);
                    float err = current_rate - stream->tpf;
                    stream->tpf = 0.01 * err + stream->tpf;
                }
            }
            stream->last_tsp = this_timestamp;

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%p] %03ds %04dc | %4d | %08X %08X | %12d %03ds %04dc %04dt | %f %f\n",
                        stream, (unsigned int)sec, (unsigned int)cycle, *len,
                        (unsigned int)CondSwapFromBus32(*data),
                         (unsigned int)CondSwapFromBus32(*(data+1)),
                        (unsigned int)this_timestamp,
                        (unsigned int)TICKS_TO_SECS(this_timestamp),
                        (unsigned int)TICKS_TO_CYCLES(this_timestamp),
                        (unsigned int)TICKS_TO_OFFSET(this_timestamp),
                        current_rate, stream->tpf);

            int ticks_in_buffer = stream->offset*current_rate;
            uint32_t next_tsp = addTicks(stream->base_tsp, ticks_in_buffer);

            int ticks_too_late = diffTicks(this_timestamp, next_tsp);
            float frames_too_late = ticks_too_late / current_rate;
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        " NEXT: [%1ds %4dc %4dt] THIS: [%1ds %4dc %4dt] (%d / %f)\n", 
                            (int)TICKS_TO_SECS(next_tsp),
                            (int)TICKS_TO_CYCLES(next_tsp),
                            (int)TICKS_TO_OFFSET(next_tsp),
                            (int)TICKS_TO_SECS(this_timestamp),
                            (int)TICKS_TO_CYCLES(this_timestamp),
                            (int)TICKS_TO_OFFSET(this_timestamp),
                            ticks_too_late, frames_too_late
                        );

            if(frames_too_late < -(int)settings->syt_interval/2) {
                // this packet is older than the requested tsp
                // we have to drop it
                debugWarning("not using rcv packet of %d frames, %f frames (%d ticks) before sync.\n",
                             nframes, frames_too_late, ticks_too_late);
            } else {
                // advance the next pointer until we are at the point of the current stream
                // FIXME: can this actually happen?
                while(frames_too_late >= settings->syt_interval/2) {
                    stream->base_tsp = addTicks(stream->base_tsp, settings->syt_interval*current_rate);
                    next_tsp = addTicks(stream->base_tsp, ticks_in_buffer);

                    ticks_too_late = diffTicks(this_timestamp, next_tsp);
                    frames_too_late = ticks_too_late / current_rate;

                    debugOutput(DEBUG_LEVEL_VERBOSE,
                                " catch-up  NEXT: [%1ds %4dc %4dt] THIS: [%1ds %4dc %4dt] (%d / %f)\n", 
                                    (int)TICKS_TO_SECS(next_tsp),
                                    (int)TICKS_TO_CYCLES(next_tsp),
                                    (int)TICKS_TO_OFFSET(next_tsp),
                                    (int)TICKS_TO_SECS(this_timestamp),
                                    (int)TICKS_TO_CYCLES(this_timestamp),
                                    (int)TICKS_TO_OFFSET(this_timestamp),
                                    ticks_too_late, frames_too_late
                                );
                }

                // NOTE: advancing stream->next_tsp could be done using a DLL on the timestamps?

                if(am824_demux_audio_ports(stream->settings,
                                           (uint32_t *)packet->data,
                                           nframes, stream->offset) < 0) {
                    debugError("failed to demux packet data\n");
                }
                if(am824_demux_midi_ports(stream->settings,
                                          (uint32_t *)packet->data,
                                          nframes, stream->offset) < 0) {
                    debugError("failed to demux midi data\n");
                }

                // we need this packet
                stream->todo -= nframes;
                stream->offset += nframes;
            }

            if(stream->todo <= 0) {
                retval = CONN_HAVE_ENOUGH;
            }
        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%p] %03ds %04dc | %4d | %08X %08X | [empty]\n",
                        stream, sec, cycle, *len,
                        CondSwapFromBus32(*data),
                        CondSwapFromBus32(*(data+1)));
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                    "[%p] %03ds %04dc | %4d | %08X %08X | [bogus data]\n",
                    stream, sec, cycle, *len,
                    CondSwapFromBus32(*data), CondSwapFromBus32(*(data+1)));
    }

    return retval;
}

int
am824_demux_audio_ports(struct stream_settings *settings,
                        uint32_t *data, size_t nframes,
                        size_t offset)
{
    const float multiplier = 1.0f / (float)(0x7FFFFF);
    unsigned int i, j;
    uint32_t *target_event;

    for(i=0; i < settings->nb_substreams; i++) {
        if(settings->substream_types[i] != ffado_stream_type_audio) {
            continue;
        }
        float *buffer = (float *)settings->substream_buffers[i];
        buffer += offset;
        target_event = (quadlet_t *)(data + i);
        for(j = 0; j < nframes; j += 1) {
            unsigned int v = CondSwapFromBus32(*target_event) & 0x00FFFFFF;
            // sign-extend highest bit of 24-bit int
            int tmp = (int)(v << 8) / 256;
            *buffer = tmp * multiplier;
            buffer++;
            target_event += settings->nb_substreams;
        }
    }
    return 0;
}

int
am824_demux_midi_ports(struct stream_settings *settings,
                       uint32_t *data, size_t nframes,
                       size_t offset)
{
    unsigned int i, j;
    uint32_t *target_event;

    for(i=0; i < settings->nb_substreams; i++) {
        if(settings->substream_types[i] != ffado_stream_type_midi) {
            continue;
        }
        uint32_t *buffer = (uint32_t *)settings->substream_buffers[i];
        buffer += offset;
        target_event = (quadlet_t *)(data + i);
        for(j = 0; j < nframes; j += 1) {
            *buffer = CondSwapFromBus32(*target_event) & 0x00FFFFFF;
            buffer++;
            target_event += settings->nb_substreams;
        }
    }
    return 0;
}


//*****************************************************
//*                                                   *
//* TRANSMIT                                          *
//*                                                   *
//*****************************************************
void am824_dump_packet_info(int cycle, uint32_t *data, unsigned int len, unsigned int nbchannels) {
  if (cycle < 0) return;

    if(len < 2) {
        debugError("Bogus packet, no header\n");
        return;
    }
    struct iec61883_packet *packet =
            (struct iec61883_packet *) data;
	    
    int syt = CondSwapFromBus16(packet->syt);
    uint32_t tsp = CYCLE_TIMER_TO_TICKS(syt);
    uint32_t now_cy = cycle & 0x0F;
    if (CYCLE_TIMER_GET_CYCLES(syt) < now_cy) {
      tsp = addTicks(tsp, 16*TICKS_PER_CYCLE);
    }
    int diff = diffTicks(tsp, now_cy * TICKS_PER_CYCLE);
    static int prev_tsp = 0;
    static float tpf = 512.0;
  
    if (packet->fdf == 0x02) {
      int delta = diffTicks(tsp, prev_tsp);
      if (delta < 0) delta += TICKS_PER_CYCLE*16;
      prev_tsp = tsp;
    
      float current_rate = ((float)delta) / (8.0);
      float err = current_rate - tpf;
      tpf = 0.01 * err + tpf;
  
      debugOutput(DEBUG_LEVEL_WARNING,
                " %4d: %08X %08X | %02X %02X | SYT: %04X | LEN: %d | TSP: %6d DIFF: %6d DELTA: %6d RATE: %6f\n",
                cycle, *data, *(data + 1),
		packet->fmt, packet->fdf, syt, len,
		tsp, diff, delta, tpf);
    }
    data += 2;
    len -= 2;

    if(len % nbchannels != 0) {
        debugError("Bogus payload len: %d\n", len);
        return;
    }
}

enum conn_callback_status
transmit_am824_packet_data(struct stream_info *stream, uint32_t tsp, uint32_t *data, int* len)
{
    assert(stream);
    assert(data);
    assert(len);
    assert(stream->settings->client_data);

    struct am824_settings *settings = 
            (struct am824_settings *)stream->settings->client_data;

    stream->last_recv_tsp = tsp;

    struct iec61883_packet *packet =
            (struct iec61883_packet *) data;
    assert(packet);

    enum conn_callback_status retval = CONN_NEED_MORE;

    if(tsp == INVALID_TIMESTAMP_TICKS || stream->base_tsp == INVALID_TIMESTAMP_TICKS) {
        *data = tsp;
        *(data + 1) = 0;
        *len = 2*4;
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                    "[%p] %04d | %4d | %08X %08X | [no sync]\n",
                    stream, tsp, *len,
                    CondSwapFromBus32(*data),
                    CondSwapFromBus32(*(data+1)));
    } else {
        uint32_t cycle = tsp & 0x1FFF;

        float current_rate = stream->master->tpf;
        int ticks_in_buffer = stream->offset * current_rate;
        uint32_t next_tsp = addTicks(stream->base_tsp, ticks_in_buffer);

        uint32_t transmit_at_tsp   = substractTicks( next_tsp, settings->transfer_delay );
        uint32_t transmit_at_cycle = TICKS_TO_CYCLES(transmit_at_tsp);

        int cycles_too_late = diffCycles(cycle, transmit_at_cycle);

        if(cycles_too_late >= 0) {

            while(cycles_too_late > 8) {
                stream->base_tsp = addTicks(stream->base_tsp, settings->syt_interval*current_rate);
                next_tsp = addTicks(stream->base_tsp, ticks_in_buffer);

                transmit_at_tsp   = substractTicks( next_tsp, settings->transfer_delay );
                transmit_at_cycle = TICKS_TO_CYCLES(transmit_at_tsp);
                cycles_too_late   = diffCycles(cycle, transmit_at_cycle);

                debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                            " catch-up  NEXT: [%4dc] THIS: [%4dc] (%d / %f)\n", 
                                (int)TICKS_TO_CYCLES(next_tsp),
                                (int)cycle,
                                cycles_too_late, cycles_too_late*TICKS_PER_CYCLE/current_rate
                            );
            }

            if(am824_generate_data_packet(stream->settings, settings,
                                          packet, transmit_at_tsp, len, stream->offset) < 0) {
                debugError("Failed to generate data packet\n");
            }

            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%p] %04dc | %4d | %08X %08X | %12d %03ds %04dc %04dt | %f %f\n",
                        stream, (unsigned int)cycle, *len,
                        (unsigned int)CondSwapFromBus32(*data),
                         (unsigned int)CondSwapFromBus32(*(data+1)),
                        (unsigned int)stream->last_tsp,
                        (unsigned int)TICKS_TO_SECS(stream->last_tsp),
                        (unsigned int)TICKS_TO_CYCLES(stream->last_tsp),
                        (unsigned int)TICKS_TO_OFFSET(stream->last_tsp),
                        current_rate, stream->tpf);

            am824_dump_packet_info(cycle, (uint32_t *)data, (*len)/4, stream->settings->nb_substreams);

            stream->todo -= settings->syt_interval;
            stream->offset += settings->syt_interval;
            stream->last_tsp = next_tsp;

        } else {
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE,
                        "[%p] %04dc | %4d | %08X %08X | [empty]\n",
                        stream, cycle, *len,
                        CondSwapFromBus32(*data),
                        CondSwapFromBus32(*(data+1)));

            if(am824_generate_empty_packet(stream->settings, settings, 
                                           packet, len) < 0) {
                debugError("Failed to generate empty packet\n");
            }
        }

        // determine whether we need more than this
        if(stream->todo <= 0) {
            retval = CONN_HAVE_ENOUGH;
        }
    }
//     static int cnt = 0;
//     cnt++;
//     *(data + 2) = tsp & 0x1FFF;
//     *(data + 3) = cnt;
//     am824_dump_packet(data, (*len)/4, stream->settings->nb_substreams);
    return retval;
}

int am824_generate_data_packet(struct stream_settings *settings,
                               struct am824_settings *amdtp_settings,
                               struct iec61883_packet *packet,
                               __u32 tsp, int *length, size_t offset )
{
    assert(settings);
    assert(packet);

    // copy the 2 quadlets of header
    memcpy(packet, &amdtp_settings->header, 8);

    // convert the timestamp to SYT format
    uint16_t timestamp_syt = TICKS_TO_SYT ( tsp );
    packet->syt = CondSwapToBus16 ( timestamp_syt );

    *length = amdtp_settings->packet_length;

    // update the dbc to account for the frames sent
    amdtp_settings->header.dbc += amdtp_settings->syt_interval;

    return am824_generate_data_payload(settings, amdtp_settings,
                                       (uint32_t *)packet->data,
                                       offset);
}

int am824_generate_empty_packet(struct stream_settings *settings,
                                struct am824_settings *amdtp_settings,
                                struct iec61883_packet *packet,
                                int *length)
{
    assert(settings);
    assert(packet);

    // copy the 2 quadlets of header
    memcpy(packet, &amdtp_settings->header, 8);

    // overwrite the FDF
    packet->fdf = IEC61883_FDF_NODATA;

    // generate the appropriate SYT
    packet->syt = 0xffff;

#if AMDTP_ALLOW_PAYLOAD_IN_NODATA_XMIT
    if ( amdtp_settings->send_nodata_payload )
    { // no-data packets with payload (NOTE: DICE-II doesn't like that)
        *length = amdtp_settings->packet_length;

        // update the dbc to account for the frames sent
        amdtp_settings->header.dbc += amdtp_settings->syt_interval;

        // generate empty payload
        return am824_generate_empty_payload(settings, amdtp_settings, (uint32_t *)packet->data);

    } else { // no-data packets without payload
        *length = 8;
    }
#else
    // no-data packets without payload
    *length = 8;
#endif
    return 0;
}

#if TESTTONE
void
am824_generate_audio_channel_test(uint32_t *target_event,
                                  void* buffer, size_t offset,
                                  size_t nframes,
                                  size_t dimension)
{
    static int cnt = 0;
    unsigned int j;
    float *frame_buffer = (float *)buffer;
    frame_buffer += offset;
    for (j = 0;j < nframes; j += 1)
    {
        float sinval = sin(2.0*M_PI*1000.0/48000.0*cnt);
        cnt += 1;
        float v = sinval * AMDTP_FLOAT_MULTIPLIER;
        unsigned int tmp = ((int) v);
        tmp = ( tmp & 0x00FFFFFF ) | 0x40000000;
        *target_event = CondSwapToBus32((uint32_t)tmp);
        frame_buffer++;
        target_event += dimension;
    }
}
#endif


int
am824_generate_data_payload(struct stream_settings *settings,
                            struct am824_settings *amdtp_settings,
                            uint32_t *data, size_t offset)
{
#warning TODO: generate return value
    unsigned int i;
    uint32_t *target_event;

    for(i=0; i < settings->nb_substreams; i++) {
        target_event = (quadlet_t *)(data + i);
        switch(settings->substream_types[i]) {
            case ffado_stream_type_audio:
#if TESTTONE
                if(i == 0) {
                    am824_generate_audio_channel_test(target_event,
                                                settings->substream_buffers[i],
                                                offset,
                                                amdtp_settings->syt_interval,
                                                settings->nb_substreams);
                } else {
                    am824_generate_audio_channel(target_event,
                                                settings->substream_buffers[i],
                                                offset,
                                                amdtp_settings->syt_interval,
                                                settings->nb_substreams);
                }
#else
                am824_generate_audio_channel(target_event,
                                             settings->substream_buffers[i],
                                             offset,
                                             amdtp_settings->syt_interval,
                                             settings->nb_substreams);
#endif
                break;
            case ffado_stream_type_midi:
                am824_generate_midi_channel(target_event,
                                            settings->substream_buffers[i],
                                            offset,
                                            amdtp_settings->syt_interval,
                                            settings->nb_substreams);
                break;
            default:
                break;
        }
    }
    return 0;
}

int
am824_generate_empty_payload(struct stream_settings *settings,
                             struct am824_settings *amdtp_settings,
                             uint32_t *data)
{
#warning TODO: generate return value
    unsigned int i;
    uint32_t *target_event;

    for(i=0; i < settings->nb_substreams; i++) {
        target_event = (quadlet_t *)(data + i);
        switch(settings->substream_types[i]) {
            case ffado_stream_type_audio:
                am824_generate_audio_channel_silent(target_event,
                                                    amdtp_settings->syt_interval,
                                                    settings->nb_substreams);
                break;
            case ffado_stream_type_midi:
                am824_generate_midi_channel_silent(target_event,
                                                   amdtp_settings->syt_interval,
                                                   settings->nb_substreams);
                break;
            default:
                break;
        }
    }
    return 0;
}


void
am824_generate_audio_channel(uint32_t *target_event,
                             void* buffer, size_t offset,
                             size_t nframes,
                             size_t dimension)
{
    unsigned int j;
    float *frame_buffer = (float *)buffer;
    frame_buffer += offset;
    for (j = 0;j < nframes; j += 1)
    {
#if AMDTP_CLIP_FLOATS
        // clip directly to the value of a maxed event
        if(unlikely(*frame_buffer > 1.0)) {
            *target_event = CONDSWAPTOBUS32_CONST(0x407FFFFF);
        } else if(unlikely(*frame_buffer < -1.0)) {
            *target_event = CONDSWAPTOBUS32_CONST(0x40800001);
        } else {
            float v = (*frame_buffer) * AMDTP_FLOAT_MULTIPLIER;
            unsigned int tmp = ((int) v);
            tmp = ( tmp & 0x00FFFFFF ) | 0x40000000;
            *target_event = CondSwapToBus32((uint32_t)tmp);
        }
#else
        float v = (*frame_buffer) * AMDTP_FLOAT_MULTIPLIER;
        unsigned int tmp = ((int) v);
        tmp = ( tmp & 0x00FFFFFF ) | 0x40000000;
        *target_event = CondSwapToBus32((uint32_t)tmp);
#endif
        frame_buffer++;
        target_event += dimension;
    }
}

void
am824_generate_audio_channel_silent(uint32_t *target_event,
                                    size_t nframes,
                                    size_t dimension)
{
    unsigned int j;
    for (j = 0;j < nframes; j += 1)
    {
        *target_event = CONDSWAPTOBUS32_CONST(0x40000000);
        target_event += dimension;
    }
}

void
am824_generate_midi_channel(uint32_t *target_event,
                            void* fbuff, size_t offset,
                            size_t nframes,
                            size_t dimension)
{
    uint32_t *buffer = (uint32_t *)fbuff;
    buffer += offset;
    unsigned int j;
    for (j = 0;j < nframes; j += 1)
    {
        uint32_t tmp = *buffer;

#warning FIXME: midi support
        tmp =  0x80000000;

        *target_event = CondSwapToBus32(tmp);

        buffer++;
        target_event += dimension;
    }
}

void
am824_generate_midi_channel_silent(uint32_t *target_event,
                                   size_t nframes,
                                   size_t dimension)
{
    unsigned int j;
    for (j = 0;j < nframes; j += 1)
    {
        *target_event = CONDSWAPTOBUS32_CONST(0x80000000);
        target_event += dimension;
    }
}
