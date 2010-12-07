/*
 * Copyright (C) 2009-2010 by Pieter Palmers
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

#include "config.h"

#include <poll.h>

#include <assert.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

typedef __u32 quadlet_t;

#include "libutil/ByteSwap.h"
#include "libstreaming/util/cip.h"
#include "libieee1394/cycletimer2.h"

#include "libutil/SystemTimeSource.h"

#include "debugmodule/debugmodule.h"

#include "connections.h"
#include "streamer.h"

#define INCREMENT_AND_WRAP(val, incr, wrapat) \
    assert(incr < wrapat); \
    val += incr; \
    if(val >= wrapat) { \
        val -= wrapat; \
    } \
    assert(val < wrapat);

#define DLL_PI        (3.141592653589793238)
#define DLL_SQRT2     (1.414213562373095049)
#define DLL_2PI       (2.0 * DLL_PI)

DECLARE_GLOBAL_DEBUG_MODULE;

enum streamer_poll_state {
    POLL_NONE_ACTIVE,
    POLL_ERROR,
    POLL_TIMEOUT,
    POLL_OK,
};

static inline void streamer_update_sync_of_connections(streamer_t s);
static inline __u32 streamer_ctr_read(streamer_t s);

static inline enum streamer_poll_state streamer_poll(streamer_t s, __u32 tsp);

static inline int streamer_read_packet_headers(streamer_t s);

static inline void streamer_update_sync_stream(streamer_t s, int idx);
static inline enum streamer_poll_state streamer_poll_until(streamer_t s, __u32 cycle);

struct streamer {
    struct connection rcv[MAX_NB_CONNECTIONS_RCV]; // will point to the receive connections
    size_t nb_active_rcv;

    struct connection xmt[MAX_NB_CONNECTIONS_XMT]; // will point to the transmit connections
    size_t nb_active_xmt;

    int sync_master_index;
    struct stream_info *sync_master;
    struct stream_info syncinfo[MAX_NB_CONNECTIONS_RCV + MAX_NB_CONNECTIONS_XMT];

    // to poll the connections
    struct pollfd fds[MAX_NB_CONNECTIONS_RCV + MAX_NB_CONNECTIONS_XMT];
    struct connection *fd_map[MAX_NB_CONNECTIONS_RCV + MAX_NB_CONNECTIONS_XMT];

    // status info
    __u64 need_align;

    // timing info
    __u32 prev_period_start_tsp;
    __u32 period_start_tsp;

    // utility FD for CTR reads
    int util_fd;

    // settings
    size_t period_size;
    size_t nb_periods;
    size_t frame_slack;
    size_t iso_slack;

    size_t nominal_frame_rate;

    // write TSP DLL
    float last_write_tsp;
    float next_write_tsp;
    float dll_b, dll_c, dll_e2;
};

/**
 * Poll all active streams up until all of them
 * saw a specific cycle
 * @param s 
 * @param timeout 
 * @return 
 */
enum streamer_poll_state streamer_poll_until(streamer_t s, __u32 cycle)
{
    assert(s);

    debugOutput(DEBUG_LEVEL_VERBOSE, "Poll until cycle %d\n", cycle);
    size_t c;
    int status;

    struct fw_cdev_get_cycle_timer ctr;

    // present two views on the event
    union
    {
        char buf[4096];
        union fw_cdev_event e;
    } e;

    enum streamer_poll_state retval = POLL_OK;

    // add all fd's
    size_t nfds = 0;
    __u64 timeout_status = 0;
    for(c = 0; c < s->nb_active_rcv; c++) {
        struct connection *conn = s->rcv + c;
        if(conn->state == CONN_STATE_RUNNING) {
            s->fds[nfds].fd = conn->fd;
            s->fds[nfds].events = POLLIN;
            s->fd_map[nfds] = conn;
            nfds++;
            timeout_status <<= 1;
            timeout_status |= 1;
        }
    }
    for(c = 0; c < s->nb_active_xmt; c++) {
        struct connection *conn = s->xmt + c;
        if(conn->state == CONN_STATE_RUNNING) {
            s->fds[nfds].fd = conn->fd;
            s->fds[nfds].events = POLLIN;
            s->fd_map[nfds] = conn;
            nfds++;
            timeout_status <<= 1;
            timeout_status |= 1;
        }
    }

    if(nfds == 0) {
        debugError("No connections to poll\n");
        return POLL_NONE_ACTIVE;
    }

    do {
        // NOTE: it is assumed that we have waited until
        //       the packets are available, so all streams
        //       should have data available. Therefore the
        //       poll should return immediately. If it times
        //       out, the streams still under consideration
        //       are over-due.
        status = poll ( s->fds, nfds, 0);
        if(status == 0) {
            debugWarning("poll timed out\n");
            retval = POLL_TIMEOUT;
        } else if(status < 0) {
            debugError("poll failed\n");
            return POLL_ERROR;
        }

#ifdef DEBUG
        // read cycle timer
        status = ioctl ( s->sync_master->connection->fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ctr );
        if(status < 0) {
            debugError("Failed to get ctr\n");
            return POLL_ERROR;
        }

        debugOutput(DEBUG_LEVEL_INFO,
                    " poll return at [%3ds %4dc %4dt]\n",
                    (int)CYCLE_TIMER_GET_SECS(ctr.cycle_timer),
                    (int)CYCLE_TIMER_GET_CYCLES(ctr.cycle_timer),
                    (int)CYCLE_TIMER_GET_OFFSET(ctr.cycle_timer)
                );
#endif

        for(c = 0; c < nfds; c++) {
            if(s->fds[c].revents & POLLIN ) {
                struct connection *conn = s->fd_map[c];
                assert(conn->fd == s->fds[c].fd);
                // read from the fd
                int nread = read ( conn->fd, &e, sizeof ( e ) );
                if ( nread < 1 ) {
                    debugError("Failed to read fd\n");
                    return POLL_ERROR;
                }

                // determine event type
                switch ( e.e.common.type )
                {
                    case FW_CDEV_EVENT_BUS_RESET:
                        debugError("BUS RESET\n");
                        break; // do nothing

                    case FW_CDEV_EVENT_RESPONSE:
                        debugError("Event response\n");
                        return POLL_ERROR;

                    case FW_CDEV_EVENT_REQUEST:
                        debugError("Event request\n");
                        return POLL_ERROR;

                    case FW_CDEV_EVENT_ISO_INTERRUPT:
                        conn_interrupt(conn, &e.e.iso_interrupt);

                        // see if this stream has passed the given cycle
                        int hw_cycle = conn_get_next_hw_cycle(conn);
                        if(hw_cycle < 0) {
                            debugWarning("hw_cycle < 0: %d\n", hw_cycle);
                        } else {
                            if(diffCycles(hw_cycle, cycle) > 0) {
                                // this stream has seen the cycle requested
                                // disable polling on this FD
                                s->fds[c].events &= ~POLLIN;
                                // stream did not time out
                                timeout_status &= ~(1 << c);
                            }
                        }
                }
            }
        }

        if(timeout_status && retval == POLL_OK) {
            debugWarning("not all streams reached cycle %d, go again\n", cycle);
        }

    // loop while nescessary and not a time-out
    } while(timeout_status && retval == POLL_OK);

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "poll retval: %d , timeout %16llX\n",
                retval, timeout_status);

    // mark all connections that have timed out
    if(retval == POLL_TIMEOUT) {
        for(c = 0; c < nfds; c++) {
            if(timeout_status & 0x01) {
                struct connection *conn = s->fd_map[c];
                conn_timeout(conn);
            }
            timeout_status >>= 1;
        }
    }

    return retval;
}

/**
 * 
 * @param s 
 * @param tsp 
 * @return 
 */
enum streamer_poll_state streamer_poll(streamer_t s, __u32 tsp)
{
    assert(s);
    assert(tsp < MAX_TICKS);

    int status;
    struct fw_cdev_get_cycle_timer ctr;

    // read cycle timer to find out where we are now
    status = ioctl ( s->sync_master->connection->fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ctr );
    if(status < 0) {
        debugError("Failed to get ctr\n");
        return POLL_ERROR;
    }

    debugOutput(DEBUG_LEVEL_INFO, 
                " poll starts at [%1ds %4dc %4dt]\n",
                (int)CYCLE_TIMER_GET_SECS(ctr.cycle_timer),
                (int)CYCLE_TIMER_GET_CYCLES(ctr.cycle_timer),
                (int)CYCLE_TIMER_GET_OFFSET(ctr.cycle_timer)
            );

    __u32 ctr_reg = ctr.cycle_timer & CYCLE_TIMER_MASK; // mask
    __u32 ctr_ticks = CYCLE_TIMER_TO_TICKS(ctr_reg);

    // add one extra cycle to the TSP to cope with rate differences
    // and uncertainties
    __u32 wake_at_tsp = addTicks(tsp, TICKS_PER_CYCLE);
    int time_to_wait = diffTicks(wake_at_tsp, ctr_ticks);

    debugOutput(DEBUG_LEVEL_INFO, 
                " waiting %d usecs for tsp [%1ds %4dc %4dt] now: [%1ds %4dc %4dt]\n",
                (int)(time_to_wait * USECS_PER_TICK),
                (int)TICKS_TO_SECS(tsp),
                (int)TICKS_TO_CYCLES(tsp),
                (int)TICKS_TO_OFFSET(tsp),
                (int)TICKS_TO_SECS(ctr_ticks),
                (int)TICKS_TO_CYCLES(ctr_ticks),
                (int)TICKS_TO_OFFSET(ctr_ticks));

    if(time_to_wait > 0) {
        int time_to_wait_usec = time_to_wait * USECS_PER_TICK;
#ifdef DEBUG
        int period_in_usec = (s->period_size * 1000 * 1000) / s->nominal_frame_rate;
        if(time_to_wait_usec > period_in_usec) {
            debugWarning("Wait longer than one period: %dus (period = %dus)\n",
                         time_to_wait_usec, period_in_usec);
        }
#endif
        ffado_microsecs_t wake_time = ctr.local_time + time_to_wait_usec;
        Util::SystemTimeSource::SleepUsecAbsolute(wake_time);
    }

    return streamer_poll_until(s, TICKS_TO_CYCLES(tsp));
}

streamer_t streamer_new()
{
    streamer_t strm = (streamer_t) calloc(1, sizeof(struct streamer));
    return strm;
}

/**
 * 
 * @param s 
 * @return 
 */
__u32 streamer_ctr_read(streamer_t s) {
    assert(s);

    // read cycle timer
    struct fw_cdev_get_cycle_timer ctr;
    int status;
    status = ioctl ( s->util_fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ctr );
    if(status < 0) {
        debugError("failed to read CTR\n");
        return CTR_BAD_TIMESTAMP;
    }
    __u32 ctr_reg = ctr.cycle_timer & CYCLE_TIMER_MASK; // mask
    __u32 ctr_ticks = CYCLE_TIMER_TO_TICKS(ctr_reg);

    return ctr_ticks;
}

/**
 * 
 * @param s 
 * @param period_size 
 * @param nb_periods 
 * @param frame_slack 
 * @param iso_slack 
 * @param nominal_frame_rate 
 * @return 
 */
int streamer_init(streamer_t s, unsigned int util_port,
                  size_t period_size, size_t nb_periods,
                  size_t frame_slack, size_t iso_slack,
                  size_t nominal_frame_rate)
{
    assert(s);
    s->sync_master_index = -1;
    s->sync_master = NULL;

    s->nb_active_rcv = 0;
    s->nb_active_xmt = 0;
    s->need_align = 0;

    s->period_size = period_size;
    s->nb_periods = nb_periods;
    s->frame_slack = frame_slack;
    s->iso_slack = iso_slack;

    s->nominal_frame_rate = nominal_frame_rate;

    s->last_write_tsp = INVALID_TIMESTAMP_TICKS;
    s->next_write_tsp = INVALID_TIMESTAMP_TICKS;

    // utility file descriptor
    char *device = NULL;
    if(asprintf(&device, "/dev/fw%d", util_port) < 0) {
        debugError("Failed create device string\n");
        return -1;
    }

    // open the specified port
    debugOutput(DEBUG_LEVEL_INFO,
                "Open device %s...\n", device);

    s->util_fd = open( device, O_RDWR );
    if(s->util_fd < 0) {
        debugError("Failed to open %s\n", device);
        free(device);
        return -1;
    }
    free(device);

    return 0;
}

/**
 * 
 * @param s 
 */
void streamer_free(streamer_t s)
{
    size_t c;
    for(c = 0; c < s->nb_active_rcv; c++) {
        conn_free(s->rcv + c);
    }
    for(c = 0; c < s->nb_active_xmt; c++) {
        conn_free(s->xmt + c);
    }

    if(s->util_fd > 2) {
        close(s->util_fd);
    }

    free(s);
}

/**
 * 
 * @param s 
 * @return 
 */
int streamer_start(streamer_t s)
{
    assert(s);

    // initialize the period counters
    const __u32 one_period_in_ticks = TICKS_PER_SECOND * s->period_size / s->nominal_frame_rate;

    __u32 ctr_ticks = streamer_ctr_read(s);
    if(ctr_ticks == CTR_BAD_TIMESTAMP) {
        debugError("Failed to read CTR\n");
        return -1;
    } else {
        s->prev_period_start_tsp = substractTicks(ctr_ticks, one_period_in_ticks);
    }

    // init the DLL
    const double bw = 1;
    double tupdate = s->period_size / s->nominal_frame_rate;
    double bw_rel = bw * tupdate;
    if(bw_rel >= 0.5) {
        debugError("Requested bandwidth out of range: %f > %f\n", bw, 0.5 / tupdate);
        return -1;
    }

    s->dll_b = bw_rel * (DLL_SQRT2 * DLL_2PI);
    s->dll_c = bw_rel * bw_rel * DLL_2PI * DLL_2PI;

    return 0;
}

/**
 * 
 * @param s 
 * @return 
 */
int streamer_stop(streamer_t s)
{
    return 0;
}

/**
 * 
 * @param s 
 * @return 
 */
int streamer_wait_for_period(streamer_t s)
{
    assert(s);

    // prepare all periods
    size_t c;
    for(c = 0; c < s->nb_active_rcv; c++) {
        conn_prepare_period(s->rcv + c);
    }
    for(c = 0; c < s->nb_active_xmt; c++) {
        conn_prepare_period(s->xmt + c);
    }

    assert(s->sync_master);
    const __u32 one_period_in_ticks = s->sync_master->tpf * s->period_size;

    // The sync tsp is the timestamp of the first frame in
    // the buffer of the sync connection. Ultimately if
    // we are at this point, the frame buffer should be empty.
    // In reality, some frames will be present as we are not
    // working on the edge.
    // The next poll() should return at period_start_tsp + one period

#warning FIXME
    s->period_start_tsp = s->sync_master->last_tsp;

    streamer_print_stream_state(s);

    // check tsp
    if(s->period_start_tsp == CTR_BAD_TIMESTAMP) {
        debugOutput(DEBUG_LEVEL_VERBOSE,
                    "No sync tsp available, using last period tsp + one period\n");
        s->period_start_tsp = addTicks(s->prev_period_start_tsp, one_period_in_ticks);
    } else {
        int one_packet_in_ticks = s->sync_master->tpf * 8; // FIXME: move to SP itself
        s->period_start_tsp = addTicks(s->period_start_tsp, one_packet_in_ticks);
#ifdef DEBUG
        __u32 pred_period_tsp = addTicks(s->prev_period_start_tsp, one_period_in_ticks);
        int tsp_diff = (int)diffTicks(s->period_start_tsp, pred_period_tsp);
        if(tsp_diff > 500 || tsp_diff < -500) {
            debugWarning("large tsp diff: %d \n", tsp_diff);
        }
#endif
    }

#ifdef DEBUG
    {
    __u32 ctr = streamer_ctr_read(s);
    debugOutput(DEBUG_LEVEL_INFO,
                "prepared at [%1ds %4dc %4dt]"
                " Sync: [%1ds %4dc %4dt]\n", 
                (int)TICKS_TO_SECS(ctr),
                (int)TICKS_TO_CYCLES(ctr),
                (int)TICKS_TO_OFFSET(ctr),
                (int)TICKS_TO_SECS(s->period_start_tsp),
                (int)TICKS_TO_CYCLES(s->period_start_tsp),
                (int)TICKS_TO_OFFSET(s->period_start_tsp));
    }
#endif

    // poll all connections until all of them (should) have passed next_period_tsp
    __u32 next_period_tsp = addTicks(s->period_start_tsp, one_period_in_ticks);

    // NOTE: it would be good to start any new streams here using the next_period_tsp
    // to init the start_on_cycle. It would definitely reduce the issues with stream
    // startup.
    enum streamer_poll_state poll_ret = streamer_poll(s, next_period_tsp);
    switch(poll_ret) {
        case POLL_NONE_ACTIVE:
            debugWarning("No active connections\n");
            // sleep?
            break;
        case POLL_ERROR:
            debugError("Poll error\n");
            return -1;
        case POLL_TIMEOUT:
            debugWarning("One or more streams timed out\n");
            break; // the streamer will know what to do with timed-out streams
        case POLL_OK:
            break;
    };

    // process packet descriptor headers
    streamer_read_packet_headers(s);

    return 0;
}

/**
 * 
 * @param s 
 * @param irq_tsp 
 * @return 
 */
int streamer_queue_next_period(streamer_t s)
{
    assert(s);

    // figure out when the presentation time of the first frame of
    // each period boundary will be. These are the optimal points
    // for interrupts as at that point we should have received 
    // sufficient data for one period.

    // period_start_tsp is the TSP of the first frame in the frame buffer.
    // this run should fill this frame buffer up to one period.
    // this means that the interrupt for this run should occur
    // at period_start_tsp + one_period_in_ticks + frame_slack

    // note: the interrupts calculated are:
    //  0: optimal tsp for the start of the period we will capture
    //  1: optimal current interrupt tsp, i.e. the tsp this interrupt
    //     should have had.
    //  2: optimal IRQ cycle for the next period (already queued)
    // ...
    //  arguments.nb_periods + 1: to be queued in this run

#ifdef DEBUG
    __u32 period_start_tsp_dbg = s->period_start_tsp;
    if(period_start_tsp_dbg != CTR_BAD_TIMESTAMP) {
        debugOutput(DEBUG_LEVEL_INFO, "Interrupts should occur at: ");
        size_t c;
        for(c = 0; c <= (unsigned)s->nb_periods + 1; c++) {
            __u64 irq_tsp_dbg  = addTicks(period_start_tsp_dbg,
                                          s->sync_master->tpf *
                                          (c * s->period_size));
            debugOutputShort(DEBUG_LEVEL_INFO,
                        "%2zd: [%1ds %4dc %4dt]", c,
                        (int)TICKS_TO_SECS(irq_tsp_dbg),
                        (int)TICKS_TO_CYCLES(irq_tsp_dbg),
                        (int)TICKS_TO_OFFSET(irq_tsp_dbg));
        }
        debugOutputShort(DEBUG_LEVEL_INFO, "\n");
    }
#endif

    // find out where to request the next interrupt
    __u32 delay_frames = (s->nb_periods + 1) * s->period_size;
    __u32 delay_ticks = delay_frames * s->sync_master->tpf;
    __u32 irq_tsp = addTicks(s->period_start_tsp, delay_ticks);

    debugOutput(DEBUG_LEVEL_INFO, 
                "  >>> IRQ: [%1ds %4dc %4dt] (delay: %d frames = %f periods = %d ticks, tpf: %f)\n",
                (int)TICKS_TO_SECS(irq_tsp),
                (int)TICKS_TO_CYCLES(irq_tsp),
                (int)TICKS_TO_OFFSET(irq_tsp),
                delay_frames, (float)delay_frames / s->period_size,
                delay_ticks, s->sync_master->tpf
                );

    size_t int_cycle = TICKS_TO_CYCLES(irq_tsp);

    // do the actual re-queue
    size_t c;
    for(c = 0; c < s->nb_active_rcv; c++) {
        struct connection *conn = s->rcv + c;
        if(conn->state != CONN_STATE_RUNNING) {
            debugWarning("Skip rcv %zd as it is not running\n", c);
            continue;
        }

        int curr_write_cycle = conn_get_queue_cycle(conn);
        if(curr_write_cycle < 0) {
            debugWarning("No receive stream present at %zd\n", c);
            continue;
        }

        // calculate the amount of packets to queue
        // this is the difference between the current write cycle (= last queued + 1)
        // and the target interrupt cycle
        int npackets = diffCycles(int_cycle, curr_write_cycle) + 1;
        if(npackets < 0) {
            int new_int_cycle = curr_write_cycle;
            debugOutput(DEBUG_LEVEL_INFO,
                        "packet for cycle %zd already queued, have to delay interrupt to %d\n",
                        int_cycle, new_int_cycle);
            int_cycle = new_int_cycle;
            npackets = 1;
        } else {
            debugOutput(DEBUG_LEVEL_INFO,
                        "will request interrupt at %zd, queue %d packets to get there\n",
                        int_cycle, npackets);
        }

        // prepare the packets for re-queue
        int npackets_ready = conn_prepare_packets(conn, npackets);
        if(npackets_ready < 0) {
            return -1;
        } else if (npackets_ready != npackets) {
            debugWarning("Could only prepare %d packets instead of %d\n",
                         npackets_ready, npackets);
            assert(npackets_ready < npackets);

            int diff = npackets - npackets_ready;
            assert(diff < 8000);

            int tmp = int_cycle - diff;
            if(tmp < 0) tmp += 8000;
            assert(tmp >= 0);

            int_cycle = tmp;
        }
        conn_request_interrupt(conn, int_cycle);

        // do the actual re-queue
        if(conn_queue_packets(conn, npackets_ready) < 0) {
            return -1;
        }
    }

    for(c = 0; c < s->nb_active_xmt; c++) {
        struct connection *conn = s->xmt + c;
        if(conn->state != CONN_STATE_RUNNING) {
            debugWarning("Skip xmt %zd as it is not running\n", c);
            continue;
        }

        int curr_write_cycle = conn_get_queue_cycle(conn);
        if(curr_write_cycle < 0) {
            debugWarning("No transmit stream present at %zd\n", c);
            continue;
        }

        // calculate the amount of packets to queue
        // this is the difference between the current write cycle (= last queued + 1)
        // and the target interrupt cycle
        int npackets = diffCycles(int_cycle, curr_write_cycle) + 1;
        if(npackets < 0) {
            int new_int_cycle = curr_write_cycle;
            debugOutput(DEBUG_LEVEL_INFO,
                        "packet for cycle %zd already queued, have to delay interrupt to %d\n",
                        int_cycle, new_int_cycle);
            int_cycle = new_int_cycle;
            npackets = 1;
        } else {
            debugOutput(DEBUG_LEVEL_INFO,
                        "will request interrupt at %zd, queue %d packets to get there\n",
                        int_cycle, npackets);
        }

        // prepare the packets for re-queue
        int npackets_ready = conn_prepare_packets(conn, -1);
        if(npackets_ready < 0) {
            return -1;
        } else if (npackets_ready < npackets) {
            debugWarning("Could only prepare %d packets instead of %d\n",
                         npackets_ready, npackets);
            assert(npackets_ready < npackets);

            int diff = npackets - npackets_ready;
            assert(diff < 8000);

            int tmp = int_cycle - diff;
            if(tmp < 0) tmp += 8000;
            assert(tmp >= 0);

            int_cycle = tmp;
        }
#ifdef DEBUG
        else {
            debugOutput(DEBUG_LEVEL_INFO,
                        "Queued %d packets instead of %d\n",
                        npackets_ready, npackets);
        }
#endif
        conn_request_interrupt(conn, int_cycle);

        // do the actual re-queue
        if(conn_queue_packets(conn, npackets_ready) < 0) {
            return -1;
        }
    }

    s->prev_period_start_tsp = s->period_start_tsp;
    return 0;
}

int streamer_read_packet_headers(streamer_t s)
{
    assert(s);
    // after all fd's have been iterated, we can consume the frames
    size_t c;
    for(c = 0; c < s->nb_active_rcv; c++) {
        struct connection *conn = s->rcv + c;
        if(conn->state != CONN_STATE_RUNNING) {
            continue;
        }
        int status = conn_process_headers(conn);
        if(status < 0) {
            debugError("failed to read packet headers from rcv connection %zd\n",
                       c);
            return -1;
        }
    }
    for(c = 0; c < s->nb_active_xmt; c++) {
        struct connection *conn = s->xmt + c;
        if(conn->state != CONN_STATE_RUNNING) {
            continue;
        }
        int status = conn_process_headers(conn);
        if(status < 0) {
            debugError("failed to read packet headers from xmt connection %zd\n",
                       c);
            return -1;
        }
    }
    return 0;
}

static inline int streamer_connection_xxx_frames(struct connection *conn,
                                                 size_t nframes,
                                                 __u32 tsp)
{
    assert(conn);
    assert(conn->nstreams == 1);
    assert(conn->streams);
    assert(tsp != INVALID_TIMESTAMP_TICKS);

    // set the target frame buffer fills for this connection
    conn->streams->todo = nframes;
    conn->streams->offset = 0;
    conn->streams->base_tsp = tsp;

    int npackets = conn_process_data(conn, conn->buffer_size);
    if(npackets < 0) {
        return -1;
    }

    if(conn->streams->todo != 0) {
        // TODO: xrun?
        debugWarning("xrun? todo: %d\n", conn->streams->todo);
    }

    return npackets;
}

static inline int streamer_xxx_frames(struct connection * set,
                                      size_t nconnections,
                                      size_t nframes,
                                      __u32 tsp)
{
    assert(set);
    // after all fd's have been iterated, we can consume the frames
    size_t c;
    int min_npackets = 8000;
    for(c = 0; c < nconnections; c++) {
        struct connection *conn = set + c;
        if(conn->state != CONN_STATE_RUNNING) {
            continue;
        }

        int npackets = streamer_connection_xxx_frames(conn, nframes, tsp);
        if(npackets < 0) {
            debugError("failed to read/write frames from/to connection %zd\n",
                       c);
            return -1;
        } else if(npackets < min_npackets) {
            min_npackets = npackets;
        }
    }
    return min_npackets;
}

int streamer_read_frames(streamer_t s)
{
    assert(s);
    debugOutput(DEBUG_LEVEL_INFO,
                "read frames from %12d %03ds %04dc %04dt\n",
                s->period_start_tsp,
                (unsigned int)TICKS_TO_SECS(s->period_start_tsp),
                (unsigned int)TICKS_TO_CYCLES(s->period_start_tsp),
                (unsigned int)TICKS_TO_OFFSET(s->period_start_tsp)
               );
    return streamer_xxx_frames(s->rcv, s->nb_active_rcv, s->period_size, s->period_start_tsp);
}

int streamer_write_frames(streamer_t s)
{
    assert(s);
    assert(s->sync_master);
    assert(s->period_start_tsp != CTR_BAD_TIMESTAMP);

    const __u32 one_buffer_in_ticks = s->sync_master->tpf * (s->period_size * s->nb_periods +
                                                             s->frame_slack);
    __u32 write_tsp = addTicks(s->period_start_tsp, one_buffer_in_ticks);

    if(s->last_write_tsp == CTR_BAD_TIMESTAMP) {
        //// INIT
        s->dll_e2 = s->period_size * s->sync_master->tpf;
        s->last_write_tsp = write_tsp;
        s->next_write_tsp = addTicks(write_tsp, s->dll_e2);
    } else {
        //// UPDATE
        // read timer and calculate loop error
        int err = write_tsp - s->next_write_tsp;
        //
        s->last_write_tsp = s->next_write_tsp;
        s->next_write_tsp += s->dll_b * err + s->dll_e2;
        s->dll_e2 += s->dll_c * err;
    }
    write_tsp = s->last_write_tsp;

    debugOutput(DEBUG_LEVEL_INFO,
                "write frames from %12d %03ds %04dc %04dt\n",
                write_tsp,
                (unsigned int)TICKS_TO_SECS(write_tsp),
                (unsigned int)TICKS_TO_CYCLES(write_tsp),
                (unsigned int)TICKS_TO_OFFSET(write_tsp)
               );

    return streamer_xxx_frames(s->xmt, s->nb_active_xmt, s->period_size, write_tsp);
}

#define STREAMER_CONN_ID_RCV( x ) ((__u32)((((x) & 0x3FFF) <<  0) | 0x00004000))
#define STREAMER_CONN_ID_XMT( x ) ((__u32)((((x) & 0x3FFF) << 16) | 0x40000000))

#define STREAMER_CONN_ID_IS_RCV( x ) (((x) & 0x00004000) != 0)
#define STREAMER_CONN_ID_IS_XMT( x ) (((x) & 0x40000000) != 0)

#define STREAMER_CONN_ID_GET_RCV( x ) (((x) >>  0) & 0x00003FFF)
#define STREAMER_CONN_ID_GET_XMT( x ) (((x) >> 16) & 0x00003FFF)

int streamer_add_stream(streamer_t s,
                        struct stream_settings *settings)
{
    assert(s);
    assert(settings);
    assert(sizeof(int)*8 >= 32);

    switch(settings->type) {
        case STREAM_TYPE_RECEIVE:
            if(s->nb_active_rcv < MAX_NB_CONNECTIONS_RCV) {
                struct connection *conn = s->rcv + s->nb_active_rcv;
                struct stream_info *strm = s->syncinfo + s->nb_active_rcv;

                // keep track of settings
                strm->settings = settings;

                // figure out buffer sizing
                size_t buffersize_frames = (s->period_size * s->nb_periods + s->frame_slack);
                size_t buffersize_cycles = buffersize_frames * CYCLES_PER_SECOND / s->nominal_frame_rate;

                if(conn_init(conn, FW_CDEV_ISO_CONTEXT_RECEIVE, 
                             strm->settings->port,
                             strm->settings->tag,
                             strm->settings->max_packet_size,
                             buffersize_cycles, s->iso_slack) < 0) {
                    debugError("Failed to init rcv connection\n");
                    return -1;
                }
                // FIXME: fixed 1:1 mapping of streams to connections
                conn->streams = s->syncinfo + s->nb_active_rcv;
                conn->nstreams = 1;
                conn->streams->connection = conn;

                debugOutput(DEBUG_LEVEL_INFO, "Linked connection %p with stream %p\n", conn, conn->streams);
                conn->streams->master = s->sync_master;

                s->nb_active_rcv++;
                return STREAMER_CONN_ID_RCV(s->nb_active_rcv-1);
            } else {
                debugError("Max nb of receive connections (%d) reached\n",
                           MAX_NB_CONNECTIONS_RCV);
                return -1;
            }
            break;
        case STREAM_TYPE_TRANSMIT:
            if(s->nb_active_xmt < MAX_NB_CONNECTIONS_XMT) {
                struct connection *conn = s->xmt + s->nb_active_xmt;
                struct stream_info *strm = s->syncinfo + MAX_NB_CONNECTIONS_RCV + s->nb_active_xmt;

                // keep track of settings
                strm->settings = settings;

                // figure out buffer sizing
                size_t buffersize_frames = (s->period_size * s->nb_periods + s->frame_slack);
                size_t buffersize_cycles = buffersize_frames * CYCLES_PER_SECOND / s->nominal_frame_rate;

                if(conn_init(conn, FW_CDEV_ISO_CONTEXT_TRANSMIT,
                             strm->settings->port,
                             strm->settings->tag,
                             strm->settings->max_packet_size,
                             buffersize_cycles, s->iso_slack) < 0) {
                    debugError("Failed to init xmt connection\n");
                    return -1;
                }

                // FIXME: fixed 1:1 mapping of streams to connections
                conn->streams = s->syncinfo + s->nb_active_xmt + MAX_NB_CONNECTIONS_RCV;
                conn->nstreams = 1;
                conn->streams->connection = conn;
                conn->streams->master = s->sync_master;

                debugOutput(DEBUG_LEVEL_INFO, "Linked connection %p with stream %p\n", conn, conn->streams);

                s->nb_active_xmt++;
                return STREAMER_CONN_ID_XMT(s->nb_active_xmt-1);
            } else {
                debugError("Max nb of transmit connections (%d) reached\n", 
                           MAX_NB_CONNECTIONS_XMT);
                return -1;
            }
            break;
        default:
            debugError("Bad connection type: %d\n", settings->type);
            return -1;
    }
    return -1; // unreachable
}

struct connection *
streamer_get_connection_from_id(streamer_t s, __u32 id)
{
    assert(s);
    if(STREAMER_CONN_ID_IS_RCV(id)) {
        size_t c = STREAMER_CONN_ID_GET_RCV(id);
        assert(c < s->nb_active_rcv);
        struct connection *conn = s->rcv + c;
        return conn;
    } else if (STREAMER_CONN_ID_IS_XMT(id)) {
        size_t c = STREAMER_CONN_ID_GET_XMT(id);
        assert(c < s->nb_active_xmt);
        struct connection *conn = s->xmt + c;
        return conn;

    } else {
        debugError("Bogus id: %08X\n", id);
        return NULL;
    }
}

static inline int streamer_start_connection_helper(streamer_t s, struct connection *conn, int cycle)
{
    assert(conn);
    assert(conn->nstreams == 1);
    assert(conn->streams);

    //// stream info ////

    // initialize stream info
    conn->streams->last_recv_tsp = INVALID_TIMESTAMP_TICKS;
    conn->streams->last_tsp = INVALID_TIMESTAMP_TICKS;
    conn->streams->base_tsp = INVALID_TIMESTAMP_TICKS;
    conn->streams->tpf = TICKS_PER_SECOND / s->nominal_frame_rate;// FIXME

    // the nominal interrupt interval
    size_t irq_interval = s->period_size * CYCLES_PER_SECOND / s->nominal_frame_rate;
    size_t irq_offset = 0;

    return conn_start_iso(conn, conn->streams->settings->channel,
                          cycle, irq_interval, irq_offset);
}

int streamer_start_connection(streamer_t s, __u32 id, int cycle)
{
    assert(s);
    if(STREAMER_CONN_ID_IS_RCV(id)) {
        size_t c = STREAMER_CONN_ID_GET_RCV(id);
        assert(c < s->nb_active_rcv);
        struct connection *conn = s->rcv + c;
        s->need_align |= (1<<c);
        return streamer_start_connection_helper(s, conn, cycle);

    } else if (STREAMER_CONN_ID_IS_XMT(id)) {
        size_t c = STREAMER_CONN_ID_GET_XMT(id);
        assert(c < s->nb_active_xmt);
        struct connection *conn = s->xmt + c;
        return streamer_start_connection_helper(s, conn, cycle);

    } else {
        debugError("Bogus id: %08X\n", id);
        return -1;
    }
}

int streamer_set_sync_connection(streamer_t s, __u32 id)
{
    assert(s);
    struct connection *conn = NULL;
    size_t c;
    if(STREAMER_CONN_ID_IS_RCV(id)) {
        c = STREAMER_CONN_ID_GET_RCV(id);
        assert(c < s->nb_active_rcv);
        conn = s->rcv + c;
        s->sync_master_index = c;

    } else if (STREAMER_CONN_ID_IS_XMT(id)) {
        c = STREAMER_CONN_ID_GET_XMT(id);
        assert(c < s->nb_active_xmt);
        conn = s->xmt + c;

        // for standalone transmit just use 'now'
        if(conn->streams->last_tsp == INVALID_TIMESTAMP_TICKS) {
            struct fw_cdev_get_cycle_timer ctr;
            int status = ioctl ( conn->fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ctr );
            if(status < 0) {
                debugError("Failed to get CTR for connection\n");
                return -1;
            }
            __u32 tsp = CYCLE_TIMER_TO_TICKS(ctr.cycle_timer);
            conn->streams->last_tsp = tsp;
        }
        s->sync_master_index = MAX_NB_CONNECTIONS_RCV + c;

    } else {
        debugError("Bogus id: %08X\n", id);
        return -1;
    }

    if(conn->state != CONN_STATE_RUNNING) {
        debugError("Cannot select a non-running connection as sync source\n");
        return -1;
    }

    streamer_update_sync_stream(s, s->sync_master_index);

    return 0;
}

void streamer_print_connection_state(streamer_t s, size_t c, struct connection *conn, __u32 sync_tsp)
{
    assert(conn);

    if(conn->state == CONN_STATE_RUNNING) {
//         __u32 this_tsp = conn->streams->next_tsp;
        __u32 this_tsp = conn->streams->base_tsp;
        if(this_tsp != INVALID_TIMESTAMP_TICKS) {
            debugOutput(DEBUG_LEVEL_INFO, 
                        " [%2zd] {%1ds %4dc %4dt | %8d | %9.4f} [%5d] [%4d] %s %s\n",
                        c,
                        (int)TICKS_TO_SECS(this_tsp),
                        (int)TICKS_TO_CYCLES(this_tsp),
                        (int)TICKS_TO_OFFSET(this_tsp),
                        sync_tsp == INVALID_TIMESTAMP_TICKS ? 0 : (int)diffTicks(this_tsp, sync_tsp),
                        sync_tsp == INVALID_TIMESTAMP_TICKS ? 0 : (float)diffTicks(this_tsp, sync_tsp) / conn->streams->tpf,
                        conn->streams->todo, (int)((conn->streams->last_recv_tsp) & 0x1FFF),
                        conn->streams == s->sync_master ? "*" : " ",
                        FLAG_SET(conn->flags, CONN_TIMED_OUT) ? "T" : " ");
        } else {
            debugOutput(DEBUG_LEVEL_INFO, 
                        " [%2zd] {            no sync info             } [%5d] [%4d] %s %s\n",
                        c, conn->streams->todo, (int)((conn->streams->last_recv_tsp) & 0x1FFF),
                        conn->streams == s->sync_master ? "*" : " ",
                        FLAG_SET(conn->flags, CONN_TIMED_OUT) ? "T" : " ");
        }
    } else {
            debugOutput(DEBUG_LEVEL_INFO, 
                        " [%2zd] {             not active              }\n", c);
    }
}

void streamer_print_stream_state(streamer_t s)
{
    assert(s);
    size_t c;
    __u32 sync_tsp = s->period_start_tsp;

    if(sync_tsp == INVALID_TIMESTAMP_TICKS) {
        debugWarning("BAD SYNC TSP\n");
    }

    debugOutput(DEBUG_LEVEL_INFO, "RCV connections: \n");
    if(s->nb_active_rcv == 0) {
        debugOutput(DEBUG_LEVEL_INFO, 
                        " [no connections]\n");
    } else {
        for(c = 0; c < s->nb_active_rcv; c++) {
            streamer_print_connection_state(s, c, s->rcv + c, sync_tsp);
        }
    }

    debugOutput(DEBUG_LEVEL_INFO, "XMT connections: \n");
    if(s->nb_active_xmt == 0) {
        debugOutput(DEBUG_LEVEL_INFO, 
                        " [no connections]\n");
    } else {
        for(c = 0; c < s->nb_active_xmt; c++) {
            streamer_print_connection_state(s, c, s->xmt + c, sync_tsp);
        }
    }
}

void streamer_update_sync_of_connections(streamer_t s)
{
    assert(s);
    struct connection *conn = NULL;
    size_t c;

    // update all connections to reflect the change in sync master
    for(c = 0; c < s->nb_active_rcv; c++) {
        conn = s->rcv + c;
        conn->streams->master = s->sync_master;
    }

    for(c = 0; c < s->nb_active_xmt; c++) {
        conn = s->xmt + c;
        conn->streams->master = s->sync_master;
    }
}

void streamer_update_sync_stream(streamer_t s, int idx)
{
    assert(idx < MAX_NB_CONNECTIONS_RCV + MAX_NB_CONNECTIONS_XMT);
    if(idx < 0) {
        s->sync_master_index = -1;
        s->sync_master = NULL;

    } else if(idx < MAX_NB_CONNECTIONS_RCV) {
        assert(idx < s->nb_active_rcv);
    } else if(idx < MAX_NB_CONNECTIONS_XMT) {
        assert(idx - MAX_NB_CONNECTIONS_RCV < s->nb_active_xmt);
    }

    s->sync_master_index = idx;
    s->sync_master = s->syncinfo + s->sync_master_index;

    debugOutput(DEBUG_LEVEL_INFO,
                "Updated sync master to %d: stream %p (%d)\n",
                 idx, s->sync_master, s->sync_master_index);

    // update all streams to reflect the change in sync master
    // FIXME: 1:1 mapping of connections and streams assumed
    size_t c;
    for(c = 0; c < s->nb_active_rcv; c++) {
        struct stream_info *strm = s->syncinfo + c;
        strm->master = s->sync_master;
    }

    for(c = 0; c < s->nb_active_xmt; c++) {
        struct stream_info *strm = s->syncinfo + c + MAX_NB_CONNECTIONS_RCV;
        strm->master = s->sync_master;
    }

    // all receive streams have to be re-aligned
    s->need_align = 0;
    for(c = 0; c < s->nb_active_rcv; c++) {
        s->need_align |= (1<<c);
    }
}

int stream_settings_alloc_substreams(struct stream_settings *s, size_t nb_substreams)
{
    assert(s->nb_substreams == 0);

    s->nb_substreams = nb_substreams;
    s->substream_buffers = (void **)calloc(s->nb_substreams, sizeof(void *));
    if(s->substream_buffers == NULL) {
        debugError("Failed to allocate substream buffer pointer array\n");
        return -1;
    }
    s->substream_names = (char **)calloc(s->nb_substreams, sizeof(char *));
    if(s->substream_names == NULL) {
        debugError("Failed to allocate substream name pointer array\n");
        s->nb_substreams = 0;
        free(s->substream_buffers);
        s->substream_buffers = NULL;
        return -1;
    }
    s->substream_states = (enum stream_port_state *)
                                calloc(s->nb_substreams, 
                                        sizeof(enum stream_port_state));
    if(s->substream_states == NULL) {
        debugError("Failed to allocate substream state pointer array\n");
        s->nb_substreams = 0;
        free(s->substream_buffers);
        s->substream_buffers = NULL;
        free(s->substream_names);
        s->substream_names = NULL;
        return -1;
    }
    s->substream_types = (ffado_streaming_stream_type *)
                                calloc(s->nb_substreams, 
                                       sizeof(ffado_streaming_stream_type));
    if(s->substream_types == NULL) {
        debugError("Failed to allocate substream type pointer array\n");
        s->nb_substreams = 0;
        free(s->substream_buffers);
        s->substream_buffers = NULL;
        free(s->substream_names);
        s->substream_names = NULL;
        free(s->substream_states);
        s->substream_states = NULL;
        return -1;
    }
    
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "buffers for %p at %p => %p\n",
                s, s->substream_buffers,
                s->substream_buffers + nb_substreams - 1);
    
    return 0;
}

void stream_settings_free_substreams(struct stream_settings *s)
{
    if(s->substream_buffers) {
        free(s->substream_buffers);
        s->substream_buffers = NULL;
    }
    if(s->substream_names) {
        unsigned int i;
        for(i = 0; i < s->nb_substreams; i++) {
            if(s->substream_names[i] != NULL) {
                free(s->substream_names[i]);
            }
        }
        free(s->substream_names);
        s->substream_names = NULL;
    }
    if(s->substream_states) {
        free(s->substream_states);
        s->substream_states = NULL;
    }
    if(s->substream_types) {
        free(s->substream_types);
        s->substream_types = NULL;
    }
}

void stream_settings_set_substream_name(struct stream_settings *s,
                                        unsigned int i, const char *name)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] setting name of %d to %s\n",
                s, i, name);

    if(s->substream_names[i] != NULL) {
        free(s->substream_names[i]);
    }
    s->substream_names[i] = strdup(name);
}

void stream_settings_set_substream_type(struct stream_settings *s,
                                        unsigned int i,
                                        ffado_streaming_stream_type type)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] setting type of %d to %d\n",
                s, i, type);
    s->substream_types[i] = type;
}

void stream_settings_set_substream_state(struct stream_settings *s,
                                         unsigned int i,
                                         enum stream_port_state state)
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
                "[%p] setting state of %d to %d\n",
                s, i, state);
    s->substream_states[i] = state;
}
