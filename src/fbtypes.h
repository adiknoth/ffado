/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
