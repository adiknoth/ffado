/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */
#ifndef __FREEBOB_IEC61883__
#define __FREEBOB_IEC61883__

#include "../debugmodule/debugmodule.h"

/*
 * This is shamelessly stolen from iec61883-private, 
 * but I need these functions!
 * FIXME: this will only work until somebody decides to change
 *        these in libiec61883.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plug Control Registers
 **/

/* maximum number of PCRs allowed within the standard 
 * MPR/PCR addresses defined in IEC-61883.
 * This refers to the number of output or input PCRs--
 * not the MPRs and not the combined total.
 */
#define IEC61883_PCR_MAX 31

/* standard CSR offsets for plugs */
#define CSR_O_MPR   0x900
#define CSR_O_PCR_0 0x904

#define CSR_I_MPR   0x980
#define CSR_I_PCR_0 0x984

#if ( __BYTE_ORDER == __BIG_ENDIAN )

struct iec61883_oMPR {
    unsigned int data_rate:2;
    unsigned int bcast_channel:6;
    unsigned int non_persist_ext:8;
    unsigned int persist_ext:8;
    unsigned int reserved:3;
    unsigned int n_plugs:5;
};

struct iec61883_iMPR {
    unsigned int data_rate:2;
    unsigned int reserved:6;
    unsigned int non_persist_ext:8;
    unsigned int persist_ext:8;
    unsigned int reserved2:3;
    unsigned int n_plugs:5;
};

struct iec61883_oPCR {
    unsigned int online:1;
    unsigned int bcast_connection:1;
    unsigned int n_p2p_connections:6;
    unsigned int reserved:2;
    unsigned int channel:6;
    unsigned int data_rate:2;
    unsigned int overhead_id:4;
    unsigned int payload:10;
};

struct iec61883_iPCR {
    unsigned int online:1;
    unsigned int bcast_connection:1;
    unsigned int n_p2p_connections:6;
    unsigned int reserved:2;
    unsigned int channel:6;
    unsigned int reserved2:16;
};

#else

struct iec61883_oMPR {
    unsigned int n_plugs:5;
    unsigned int reserved:3;
    unsigned int persist_ext:8;
    unsigned int non_persist_ext:8;
    unsigned int bcast_channel:6;
    unsigned int data_rate:2;
};

struct iec61883_iMPR {
    unsigned int n_plugs:5;
    unsigned int reserved2:3;
    unsigned int persist_ext:8;
    unsigned int non_persist_ext:8;
    unsigned int reserved:6;
    unsigned int data_rate:2;
};

struct iec61883_oPCR {
    unsigned int payload:10;
    unsigned int overhead_id:4;
    unsigned int data_rate:2;
    unsigned int channel:6;
    unsigned int reserved:2;
    unsigned int n_p2p_connections:6;
    unsigned int bcast_connection:1;
    unsigned int online:1;
};

struct iec61883_iPCR {
    unsigned int reserved2:16;
    unsigned int channel:6;
    unsigned int reserved:2;
    unsigned int n_p2p_connections:6;
    unsigned int bcast_connection:1;
    unsigned int online:1;
};

#endif

/**
 * iec61883_plug_get - Read a node's plug register.
 * @h: A raw1394 handle.
 * @n: The node id of the node to read
 * @a: The CSR offset address (relative to base) of the register to read.
 * @value: A pointer to a quadlet where the plug register's value will be stored.
 * 
 * This function handles bus to host endian conversion. It returns 0 for 
 * suceess or -1 for error (errno available).
 **/
int
iec61883_plug_get(raw1394handle_t h, nodeid_t n, nodeaddr_t a, quadlet_t *value);


/** 
 * iec61883_plug_set - Write a node's plug register.
 * @h: A raw1394 handle.
 * @n: The node id of the node to read
 * @a: The CSR offset address (relative to CSR base) of the register to write.
 * @value: A quadlet containing the new register value.
 *
 * This uses a compare/swap lock operation to safely write the
 * new register value, as required by IEC 61883-1.
 * This function handles host to bus endian conversion. It returns 0 for success
 * or -1 for error (errno available).
 **/
int
iec61883_plug_set(raw1394handle_t h, nodeid_t n, nodeaddr_t a, quadlet_t value);

/**
 * High level plug access macros
 */

#define iec61883_get_oMPR(h,n,v) iec61883_plug_get((h), (n), CSR_O_MPR, (quadlet_t *)(v))
#define iec61883_set_oMPR(h,n,v) iec61883_plug_set((h), (n), CSR_O_MPR, *((quadlet_t *)&(v)))
#define iec61883_get_oPCR0(h,n,v) iec61883_plug_get((h), (n), CSR_O_PCR_0, (quadlet_t *)(v))
#define iec61883_set_oPCR0(h,n,v) iec61883_plug_set((h), (n), CSR_O_PCR_0, *((quadlet_t *)&(v)))
#define iec61883_get_oPCRX(h,n,v,x) iec61883_plug_get((h), (n), CSR_O_PCR_0+(4*(x)), (quadlet_t *)(v))
#define iec61883_set_oPCRX(h,n,v,x) iec61883_plug_set((h), (n), CSR_O_PCR_0+(4*(x)), *((quadlet_t *)&(v)))
#define iec61883_get_iMPR(h,n,v) iec61883_plug_get((h), (n), CSR_I_MPR, (quadlet_t *)(v))
#define iec61883_set_iMPR(h,n,v) iec61883_plug_set((h), (n), CSR_I_MPR, *((quadlet_t *)&(v)))
#define iec61883_get_iPCR0(h,n,v) iec61883_plug_get((h), (n), CSR_I_PCR_0, (quadlet_t *)(v))
#define iec61883_set_iPCR0(h,n,v) iec61883_plug_set((h), (n), CSR_I_PCR_0, *((quadlet_t *)&(v)))
#define iec61883_get_iPCRX(h,n,v,x) iec61883_plug_get((h), (n), CSR_I_PCR_0+(4*(x)), (quadlet_t *)(v))
#define iec61883_set_iPCRX(h,n,v,x) iec61883_plug_set((h), (n), CSR_I_PCR_0+(4*(x)), *((quadlet_t *)&(v)))


#ifdef __cplusplus
}
#endif

class IEC61883 {

public:

    IEC61883();
    virtual ~IEC61883();

protected:
    DECLARE_DEBUG_MODULE;

};

#endif /* __FREEBOB_IEC61883__ */


