/*
 * Copyright (C) 2009-2010 by Pieter Palmers
 *
 * Based upon code provided by Jay Fenlason
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


#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>

#define CHECK_EVENT_CYCLE 0 /* disabled */
#define PKT_SIZE 64
#define NUM_PKTS 64
#define MEM_SIZE 131072

/* #define U(x) ((__u64)(unsigned long)(x)) */
#define U(x) ((__u64)(x))

#define R_MAGIC FW_CDEV_ISO_HEADER_LENGTH(8) | FW_CDEV_ISO_PAYLOAD_LENGTH(PKT_SIZE)
#define X_MAGIC FW_CDEV_ISO_HEADER_LENGTH(0) | FW_CDEV_ISO_PAYLOAD_LENGTH(PKT_SIZE)

struct connection
{
    int fd;
    void *mem;
    int start_cycle;
    __u64 closure;
    __u32 channel;
    int toggle;
    struct fw_cdev_iso_packet packets[NUM_PKTS*2];
};

int main ( int argc, char ** argv )
{
    char *device;
    union
    {
        char buf[4096];
        union fw_cdev_event e;
    } e;
    unsigned u;
    unsigned pass;
    struct connection r;
    struct connection x;
    struct fw_cdev_create_iso_context create;
    struct fw_cdev_queue_iso queue;
    struct fw_cdev_start_iso start;
    int status;
    char *endp;
    __u32 *p;

    unsigned x_old_cycle = -1;
    unsigned x_current_cycle;

    unsigned n_different = 0;
    unsigned new_n_different;
    unsigned different_by = 0;
    unsigned new_different_by;

    if ( argc != 4 ) {
        exit(-1);
    }
    device = argv[1];
    errno = 0;
    r.channel = strtoul ( argv[2], &endp, 0 );
    assert ( endp && *endp == '\0' && !errno && r.channel<64 );
    errno = 0;
    x.channel = strtoul ( argv[3], &endp, 0 );
    assert ( endp && *endp == '\0' && !errno && x.channel<64 );
    r.start_cycle = -1;
    r.toggle = 0;
    r.closure = 0x1234;
    x.start_cycle = -1;
    x.toggle = 0;
    x.closure = 0x4321;

    r.fd = open ( device, O_RDWR );
    assert ( r.fd>=0 );
    r.mem = mmap ( NULL, MEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, r.fd, 0 );
    assert ( r.mem != MAP_FAILED );
    create.type = FW_CDEV_ISO_CONTEXT_RECEIVE;
    create.header_size = 8;
    create.channel = r.channel;
    create.speed = SCODE_400;
    create.closure = r.closure;
    create.handle = 0;
    status = ioctl ( r.fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create );
    assert ( status>=0 );
    assert ( create.handle==0 );
    for ( u = 0; u < NUM_PKTS-1; u++ )
        r.packets[u].control = R_MAGIC;
    r.packets[u++].control = R_MAGIC | FW_CDEV_ISO_INTERRUPT;
    while ( u < NUM_PKTS*2 - 1 )
    {
        r.packets[u].control = R_MAGIC;
        u++;
    }
    r.packets[u].control = R_MAGIC | FW_CDEV_ISO_INTERRUPT;
    queue.packets = U ( r.packets );
    queue.data = U ( r.mem );
    queue.size = sizeof ( r.packets );
    queue.handle = 0;
    status = ioctl ( r.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
    assert ( status>=0 );
    start.cycle = r.start_cycle;
    start.sync = 0;
    start.tags = FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS;
    start.handle = 0;
    status = ioctl ( r.fd, FW_CDEV_IOC_START_ISO, &start );
    assert ( status>=0 );

    x.fd = open ( device, O_RDWR );
    assert ( x.fd>=0 );
    x.mem = mmap ( NULL, MEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, x.fd, 0 );
    assert ( x.mem!=MAP_FAILED );
    create.type = FW_CDEV_ISO_CONTEXT_TRANSMIT;
    create.header_size = 0;
    create.channel = x.channel;
    create.speed = SCODE_400;
    create.closure = x.closure;
    create.handle = 0;
    status = ioctl ( x.fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create );
    assert ( status>=0 );
    assert ( create.handle==0 );
    for ( u = 0; u < NUM_PKTS-1; u++ )
        x.packets[u].control = X_MAGIC;
    x.packets[u++].control = X_MAGIC | FW_CDEV_ISO_INTERRUPT;
    while ( u < NUM_PKTS*2-1 )
    {
        x.packets[u].control = X_MAGIC;
        u++;
    }
    x.packets[u++].control = X_MAGIC | FW_CDEV_ISO_INTERRUPT;
    queue.packets = U ( x.packets );
    queue.data = U ( x.mem );
    queue.size = sizeof ( x.packets );
    queue.handle = 0;
    status = ioctl ( x.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
    assert ( status>=0 );
    start.cycle = x.start_cycle;
    start.sync = 0;
    start.tags = FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS;
    start.handle = 0;
    for ( pass = 0; pass < 2; pass ++ )
    {
        for ( u = 0; u < NUM_PKTS; u++ )
        {
            unsigned n;

            p = x.mem;
            p+= ( pass * NUM_PKTS + u ) * PKT_SIZE / 4;
            *p++ = pass;
            *p++ = u;
            for ( n = 2; n < PKT_SIZE/4; n++ )
                *p++ = ~0;
        }
    }
    status = ioctl ( x.fd, FW_CDEV_IOC_START_ISO, &start );
    assert ( status>=0 );

    int npktin = -1;
    int npktout = -1;
    int have_recv = 0;
    int have_xmit = 0;

    for ( pass = 0; ; pass++ )
    {
        struct pollfd fds[2];
        int status;

        ssize_t nread;

        fds[0].fd = r.fd;
        fds[0].events = POLLIN;
        fds[1].fd = x.fd;
        fds[1].events = POLLIN;

        fprintf ( stderr, "Pass %6u, IN: %8d, OUT: %8d [%s%s]\n", 
                  pass, npktin, npktout, 
                  have_recv ? "r" : " ",
                  have_xmit ? "x" : " "
                );
        have_recv = 0;
        have_xmit = 0;
        status = poll ( fds, 2, -1 );
        assert ( status>0 );
        if ( fds[0].revents&POLLIN )
        {
//             fprintf ( stderr, "r" );
            nread = read ( r.fd, &e, sizeof ( e ) );
            if ( nread < 1 )
                break;
            switch ( e.e.common.type )
            {
                case FW_CDEV_EVENT_BUS_RESET:
                    fprintf ( stderr, "BUS RESET\n" );
                    goto out;

                case FW_CDEV_EVENT_RESPONSE:
                    assert ( 0 );
                    goto out;

                case FW_CDEV_EVENT_REQUEST:
                    assert ( 0 );
                    goto out;

                case FW_CDEV_EVENT_ISO_INTERRUPT:
                    break;
            }
            // this assumes that NUM_PKTS have been received
            //
            if ( e.e.iso_interrupt.header_length != NUM_PKTS * 2 * 4 )
            {
                fprintf ( stderr, "got header_length %u not %u\n", e.e.iso_interrupt.header_length, NUM_PKTS * 2 );
//                 goto out;
            }
            npktin += NUM_PKTS;
            have_recv = 1;
            new_n_different = 0;
            for ( u = 0; u < NUM_PKTS; u++ )
            {
                __u32 h0, h1;
                __u32 expected_h0;
                __u32 start_cycle;
                __u32 pkt_cycle;
                __u32 h1_cycle;
                __u32 expected_cycle;

                h0 = ntohl ( e.e.iso_interrupt.header[u*2] );
                h1 = ntohl ( e.e.iso_interrupt.header[u*2+1] );
                expected_h0 = ( PKT_SIZE << 16 ) | ( r.channel << 8 ) | 0xa0;
                if ( h0 != expected_h0 )
                    fprintf ( stderr,
                              " packet %u header is %x not %x\n",
                              u, h0, expected_h0 );
#if CHECK_EVENT_CYCLE
                start_cycle = 8000 + e.e.iso_interrupt.cycle - ( NUM_PKTS-1 );
                start_cycle %= 8000;
                pkt_cycle = start_cycle + u;
                pkt_cycle %= 8000;
                h1_cycle = h1 & 0x1FFF;
                if ( h1_cycle != pkt_cycle )
                {
                    if ( h1_cycle > pkt_cycle )
                        new_different_by = h1_cycle - pkt_cycle;
                    else
                        new_different_by = pkt_cycle - h1_cycle;
                    new_n_different++;
                }
#endif
                h1_cycle = h1 & 0x1FFF;
                p = r.mem;
                if ( r.toggle )
                    p += ( NUM_PKTS + u ) * PKT_SIZE/4;
                else
                    p += u * PKT_SIZE/4;
                expected_cycle = ntohl ( p[2] );
                /* Initial packets aren't expected to go out on a specific cycle */
                if ( expected_cycle < 8000 )
                {
                    if ( expected_cycle != h1_cycle )
                    {
                        unsigned n;

                        fprintf ( stderr, " r: packet %u off by %d (%x %x):", u, expected_cycle - h1_cycle, expected_cycle, h1_cycle );
//                         for ( n = 0; n < PKT_SIZE/4; n++ )
//                             fprintf ( stderr, " %x", p[n] );
//                         fprintf ( stderr, "\n" );
                    }
                }
            }
#if CHECK_EVENT_CYCLE
            if ( new_n_different != n_different || new_different_by != different_by )
            {
                if ( new_n_different )
                    fprintf ( stderr, " r: %u different cycles (%d)\n", new_n_different, new_different_by );
                else
                    fprintf ( stderr, " r: back in sync\n" );
                n_different = new_n_different;
                different_by = new_different_by;
            }
#endif
            if ( r.toggle )
            {
                queue.packets = U ( &r.packets[NUM_PKTS] );
                queue.data = U ( r.mem+NUM_PKTS*PKT_SIZE );
                r.toggle = 0;
            }
            else
            {
                queue.packets = U ( r.packets );
                queue.data = U ( r.mem );
                r.toggle = 1;
            }
            queue.size = sizeof ( r.packets ) /2;
            queue.handle = 0;
            status = ioctl ( r.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
            assert ( status>=0 );
        }
        if ( fds[1].revents&POLLIN )
        {
            unsigned previous_cycle;
            __u32 new_cycle;

//             fprintf ( stderr, "x" );
            nread = read ( x.fd, &e, sizeof ( e ) );
            if ( nread < 1 )
                break;
            switch ( e.e.common.type )
            {
                case FW_CDEV_EVENT_BUS_RESET:
                    fprintf ( stderr, "Bus reset\n" );
                    goto out;

                case FW_CDEV_EVENT_RESPONSE:
                    assert ( 0 );
                    goto out;

                case FW_CDEV_EVENT_REQUEST:
                    assert ( 0 );
                    goto out;

                case FW_CDEV_EVENT_ISO_INTERRUPT:
                    break;
            }
            if ( e.e.iso_interrupt.header_length != NUM_PKTS * 4 )
            {
                fprintf ( stderr, " x: got header_length %u not %u\n", e.e.iso_interrupt.header_length, NUM_PKTS * 4 );
//                 goto out;
            }
            npktout += NUM_PKTS;
            have_xmit = 1;
            previous_cycle = ntohl ( e.e.iso_interrupt.header[0] );
            previous_cycle &= 0x1fff;
            x_current_cycle = previous_cycle;

            /* Confirm that the packets were received in sequence, without a gap or stutter from the previous packets */
            if ( x_old_cycle < 8000 )
            {
                u = x_old_cycle + 1;
                u %= 8000;
                if ( x_current_cycle != u )
                    fprintf ( stderr, " x: inter-group jitter %d\n", x_current_cycle - u );
            }
            for ( u = 1; u < NUM_PKTS; u++ )
            {
                unsigned next_cycle;

                previous_cycle++;
                previous_cycle %= 8000;
                next_cycle = ntohl ( e.e.iso_interrupt.header[u] );
                next_cycle &= 0x1fff;
                next_cycle %= 8000;
                if ( previous_cycle != next_cycle )
                {
                    unsigned n;

//                     fprintf ( stderr, " x: %u: intra-group jitter %d %u %u:", u, next_cycle - previous_cycle, previous_cycle, next_cycle );
//                     for ( n = 0; n < NUM_PKTS; n++ )
//                         fprintf ( stderr, " %x", ntohl ( e.e.iso_interrupt.header[n] ) );
                    fprintf ( stderr, "\n" );
                    previous_cycle = next_cycle;
                }
            }
#if CHECK_EVENT_CYCLE
            x_current_cycle = e.e.iso_interrupt.cycle;
            x_current_cycle %= 8000;
            if ( x_old_cycle >= 0 )
            {
                unsigned expected_new_cycle;

                expected_new_cycle = x_old_cycle + NUM_PKTS;
                expected_new_cycle %= 8000;
                if ( x_current_cycle != expected_new_cycle )
                {
                    int jitter;

                    jitter = expected_new_cycle - x_current_cycle;
                    fprintf ( stderr, " x: jitter %d (got %u expected %u)\n", jitter, x_current_cycle, expected_new_cycle );
                }
            }
            x_old_cycle = x_current_cycle;
#endif
            p = x.mem;
            if ( x.toggle )
            {
                queue.packets = U ( &x.packets[NUM_PKTS] );
                p += NUM_PKTS*PKT_SIZE/4;
                x.toggle = 0;
            }
            else
            {
                queue.packets = U ( x.packets );
                x.toggle = 1;
            }
            queue.data = U ( p );
            new_cycle = ntohl ( e.e.iso_interrupt.header[NUM_PKTS-1] );
            new_cycle &= 0x1FFF;
            new_cycle += NUM_PKTS;
            for ( u = 0; u < NUM_PKTS; u++ )
            {
                unsigned n;

                *p++ = pass;
                *p++ = u;
                new_cycle++;
                new_cycle %= 8000;
                for ( n = 8; n < PKT_SIZE; n +=4 )
                    *p++ = htonl ( new_cycle );
            }

            queue.size = sizeof ( x.packets ) /2;
            queue.handle = 0;
            status = ioctl ( x.fd, FW_CDEV_IOC_QUEUE_ISO, &queue );
            assert ( status>=0 );
        }
    }
out:
    return 0;
}

