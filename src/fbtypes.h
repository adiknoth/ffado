/* fbtypes.h
 * Copyright (C) 2005-2007 by by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef FBTYPES_H
#define FBTYPES_H

#include <libraw1394/raw1394.h>

typedef quadlet_t   fb_quadlet_t;
typedef byte_t      fb_byte_t;
typedef octlet_t    fb_octlet_t;
typedef nodeid_t    fb_nodeid_t;
typedef nodeaddr_t  fb_nodeaddr_t;

class DeviceManager;

struct ffado_handle {
    DeviceManager*   m_deviceManager;
};

#endif
