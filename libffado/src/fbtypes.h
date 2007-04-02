/* fbtypes.h
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
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

struct freebob_handle {
    DeviceManager*   m_deviceManager;
};

#endif
