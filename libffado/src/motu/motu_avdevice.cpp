/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Jonathan Woithe
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

#include "motu/motu_avdevice.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_definitions.h"

#include "debugmodule/debugmodule.h"

#include "libstreaming/motu/MotuReceiveStreamProcessor.h"
#include "libstreaming/motu/MotuTransmitStreamProcessor.h"
#include "libstreaming/motu/MotuPort.h"

#include "libutil/DelayLockedLoop.h"
#include "libutil/Time.h"
#include "libutil/Configuration.h"

#include "libcontrol/BasicElements.h"

#include <string>
#include <stdint.h>
#include <assert.h>
#include "libutil/ByteSwap.h"
#include <iostream>
#include <sstream>

#include <libraw1394/csr.h>

namespace Motu {

// Define the supported devices.  Device ordering is arbitary here.
static VendorModelEntry supportedDeviceList[] =
{
//  {vendor_id, model_id, unit_version, unit_specifier_id, model, vendor_name,model_name}
    {FW_VENDORID_MOTU, 0, 0x00000003, 0x000001f2, MOTU_MODEL_828mkII, "MOTU", "828MkII"},
    {FW_VENDORID_MOTU, 0, 0x00000009, 0x000001f2, MOTU_MODEL_TRAVELER, "MOTU", "Traveler"},
    {FW_VENDORID_MOTU, 0, 0x0000000d, 0x000001f2, MOTU_MODEL_ULTRALITE, "MOTU", "UltraLite"},
    {FW_VENDORID_MOTU, 0, 0x0000000f, 0x000001f2, MOTU_MODEL_8PRE, "MOTU", "8pre"},
    {FW_VENDORID_MOTU, 0, 0x00000001, 0x000001f2, MOTU_MODEL_828MkI, "MOTU", "828MkI"},
    {FW_VENDORID_MOTU, 0, 0x00000005, 0x000001f2, MOTU_MODEL_896HD, "MOTU", "896HD"},
    {FW_VENDORID_MOTU, 0, 0x00000015, 0x000001f2, MOTU_MODEL_828mk3, "MOTU", "828Mk3"},
};

// Ports declarations
const PortEntry Ports_828MKI[] =
{
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 31},
    {"SPDIF1", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 34},
    {"SPDIF2", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 37},
    {"ADAT1", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 40},
    {"ADAT2", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 43},
    {"ADAT3", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 46},
    {"ADAT4", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 49},
    {"ADAT5", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 52},
    {"ADAT6", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 55},
    {"ADAT7", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 58},
    {"ADAT8", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 61},
};

const PortEntry Ports_896HD[] =
{
    {"Mix-L", MOTU_PA_IN | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTU_PA_IN | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 13},
    {"Phones-L", MOTU_PA_OUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTU_PA_OUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 10},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 31},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 34},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 37},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 31},
    {"MainOut-L", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 40},
    {"MainOut-R", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 43},
    {"AES/EBU1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 46},
    {"AES/EBU2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 49},
    {"ADAT1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 52},
    {"ADAT2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 55},
    {"ADAT3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 58},
    {"ADAT4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 61},
    {"ADAT5", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 64},
    {"ADAT6", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 67},
    {"ADAT7", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 70},
    {"ADAT8", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 73},
};

const PortEntry Ports_828MKII[] =
{
    {"Main-L", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 40},
    {"Main-R", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 43},
    {"Mix-L", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 37},
    {"Phones-L", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"Mic1", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 40},
    {"Mic2", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 43},
    {"SPDIF1", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 46},
    {"SPDIF2", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 49},
    {"ADAT1", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 52},
    {"ADAT2", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 55},
    {"ADAT3", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 58},
    {"ADAT4", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 61},
    {"ADAT5", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 64},
    {"ADAT6", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 67},
    {"ADAT7", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 70},
    {"ADAT8", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 73},
};

const PortEntry Ports_TRAVELER[] = 
{
    {"Mix-L", MOTU_PA_IN | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTU_PA_IN | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 13},
    {"Phones-L", MOTU_PA_OUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTU_PA_OUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 10},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 31},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 34},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 37},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 31},
    {"AES/EBU1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 40},
    {"AES/EBU2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 43},
    {"SPDIF1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ADAT, 46},
    {"SPDIF2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ADAT, 49},
    {"Toslink1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_TOSLINK, 46},
    {"Toslink2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_TOSLINK, 49},
    {"ADAT1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 52},
    {"ADAT2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 55},
    {"ADAT3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 58},
    {"ADAT4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 61},
    {"ADAT5", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 64},
    {"ADAT6", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 67},
    {"ADAT7", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 70},
    {"ADAT8", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 73},
};

const PortEntry Ports_ULTRALITE[] =
{
    {"Main-L", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 40},
    {"Main-R", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 43},
    {"Mix-L", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"Mic1", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 16},
    {"Mic2", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog1", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 37},
    {"Phones-L", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"Padding1", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY|MOTU_PA_PADDING, 46},
    {"Padding2", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY|MOTU_PA_PADDING, 49},
    {"SPDIF1", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 46},
    {"SPDIF2", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 49},
};

const PortEntry Ports_8PRE[] =
{
    {"Analog1", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog2", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog3", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog4", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog5", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog6", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 31},
    {"Analog7", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 34},
    {"Analog8", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 37},
    {"Mix-L", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"Main-L", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 16},
    {"Main-R", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 19},
    {"Phones-L", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ANY, 13},
    {"ADAT1", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 40},
    {"ADAT1", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 22},
    {"ADAT2", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 43},
    {"ADAT2", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 25},
    {"ADAT3", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 46},
    {"ADAT3", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 28},
    {"ADAT4", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 49},
    {"ADAT4", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 31},
    {"ADAT5", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 52},
    {"ADAT5", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 34},
    {"ADAT6", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 55},
    {"ADAT6", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 37},
    {"ADAT7", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 58},
    {"ADAT7", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 40},
    {"ADAT8", MOTU_PA_IN | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 61},
    {"ADAT8", MOTU_PA_OUT | MOTU_PA_RATE_ANY|MOTU_PA_OPTICAL_ADAT, 43},
};

const PortEntry Ports_828mk3[] = 
{
    {"Mix-L", MOTU_PA_IN | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 10},
    {"Mix-R", MOTU_PA_IN | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 13},
    {"Phones-L", MOTU_PA_OUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 10},
    {"Phones-R", MOTU_PA_OUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog1", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 10},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog2", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 13},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog3", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 16},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog4", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 19},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog5", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 22},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 31},
    {"Analog6", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 25},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 34},
    {"Analog7", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 28},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ANY, 37},
    {"Analog8", MOTU_PA_INOUT | MOTU_PA_RATE_4x|MOTU_PA_OPTICAL_ANY, 31},
    {"SPDIF1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ADAT, 40},
    {"SPDIF2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ADAT, 43},
    {"Padding1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ADAT|MOTU_PA_PADDING, 46},
    {"Padding2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_OFF|MOTU_PA_OPTICAL_ADAT|MOTU_PA_PADDING, 49},
    {"Toslink1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_TOSLINK, 40},
    {"Toslink2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_TOSLINK, 43},
    {"Toslink3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_TOSLINK, 46},
    {"Toslink4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_TOSLINK, 49},
    {"ADAT1", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 52},
    {"ADAT2", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 55},
    {"ADAT3", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 58},
    {"ADAT4", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 61},
    {"ADAT5", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 64},
    {"ADAT6", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 67},
    {"ADAT7", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 70},
    {"ADAT8", MOTU_PA_INOUT | MOTU_PA_RATE_1x2x|MOTU_PA_OPTICAL_ADAT, 73},
    {"ADAT9", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 76},
    {"ADAT10", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 79},
    {"ADAT11", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 82},
    {"ADAT12", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 85},
    {"ADAT13", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 88},
    {"ADAT14", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 91},
    {"ADAT15", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 94},
    {"ADAT16", MOTU_PA_INOUT | MOTU_PA_RATE_1x|MOTU_PA_OPTICAL_ADAT, 97},
};

// Mixer registers
const MatrixMixBus MixerBuses_Traveler[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_Traveler[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    {"AES/EBU 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"AES/EBU 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, 0x0028, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, 0x002c, },
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, 0x0030, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, 0x0034, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, 0x0038, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, 0x003c, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, 0x0040, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, 0x0044, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, 0x0048, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, 0x004c, },
};

const MixerCtrl MixerCtrls_Traveler[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },

    /* For mic/line input controls, the "register" is the zero-based channel number */
    {"Control/Ana1_", "Analog 1 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 0},
    {"Control/Ana2_", "Analog 2 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 1},
    {"Control/Ana3_", "Analog 3 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 2},
    {"Control/Ana4_", "Analog 4 input ", "", MOTU_CTRL_TRAVELER_MIC_INPUT_CTRLS, 3},
    {"Control/Ana5_", "Analog 5 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 4},
    {"Control/Ana6_", "Analog 6 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 5},
    {"Control/Ana7_", "Analog 7 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 6},
    {"Control/Ana8_", "Analog 8 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 7},

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MatrixMixBus MixerBuses_Ultralite[] = {
    {"Mix 1", 0x4000, },
    {"Mix 2", 0x4100, },
    {"Mix 3", 0x4200, },
    {"Mix 4", 0x4300, },
};

const MatrixMixChannel MixerChannels_Ultralite[] = {
    {"Analog 1", MOTU_CTRL_STD_CHANNEL, 0x0000, },
    {"Analog 2", MOTU_CTRL_STD_CHANNEL, 0x0004, },
    {"Analog 3", MOTU_CTRL_STD_CHANNEL, 0x0008, },
    {"Analog 4", MOTU_CTRL_STD_CHANNEL, 0x000c, },
    {"Analog 5", MOTU_CTRL_STD_CHANNEL, 0x0010, },
    {"Analog 6", MOTU_CTRL_STD_CHANNEL, 0x0014, },
    {"Analog 7", MOTU_CTRL_STD_CHANNEL, 0x0018, },
    {"Analog 8", MOTU_CTRL_STD_CHANNEL, 0x001c, },
    {"AES/EBU 1", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"AES/EBU 2", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"SPDIF 1", MOTU_CTRL_STD_CHANNEL, 0x0020, },
    {"SPDIF 2", MOTU_CTRL_STD_CHANNEL, 0x0024, },
    {"ADAT 1", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 2", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 3", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 4", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 5", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 6", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 7", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
    {"ADAT 8", MOTU_CTRL_STD_CHANNEL, MOTU_CTRL_NONE, },
};

const MixerCtrl MixerCtrls_Ultralite[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },

    /* For mic/line input controls, the "register" is the zero-based channel number */
    {"Control/Ana1_", "Analog 1 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 0},
    {"Control/Ana2_", "Analog 2 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 1},
    {"Control/Ana3_", "Analog 3 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 2},
    {"Control/Ana4_", "Analog 4 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 3},
    {"Control/Ana5_", "Analog 5 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 4},
    {"Control/Ana6_", "Analog 6 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 5},
    {"Control/Ana7_", "Analog 7 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 6},
    {"Control/Ana8_", "Analog 8 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 7},

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MixerCtrl MixerCtrls_896HD[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},

    /* For meter controls the "register" indicates which meter controls are available */
    {"Control/Meter_", "Meter ", "", MOTU_CTRL_METER,
      MOTU_CTRL_METER_PEAKHOLD | MOTU_CTRL_METER_CLIPHOLD | MOTU_CTRL_METER_AESEBU_SRC | 
      MOTU_CTRL_METER_PROG_SRC},
};

const MixerCtrl MixerCtrls_828Mk2[] = {
    {"Mix1/Mix_", "Mix 1 ", "", MOTU_CTRL_STD_MIX, 0x0c20, },
    {"Mix2/Mix_", "Mix 2 ", "", MOTU_CTRL_STD_MIX, 0x0c24, },
    {"Mix3/Mix_", "Mix 3 ", "", MOTU_CTRL_STD_MIX, 0x0c28, },
    {"Mix4/Mix_", "Mix 4 ", "", MOTU_CTRL_STD_MIX, 0x0c2c, },

    /* For mic/line input controls, the "register" is the zero-based channel number */
    {"Control/Ana1_", "Analog 1 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 0},
    {"Control/Ana2_", "Analog 2 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 1},
    {"Control/Ana3_", "Analog 3 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 2},
    {"Control/Ana4_", "Analog 4 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 3},
    {"Control/Ana5_", "Analog 5 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 4},
    {"Control/Ana6_", "Analog 6 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 5},
    {"Control/Ana7_", "Analog 7 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 6},
    {"Control/Ana8_", "Analog 8 input ", "", MOTU_CTRL_TRAVELER_LINE_INPUT_CTRLS, 7},

    /* For phones source control, "register" is currently unused */
    {"Control/Phones_", "Phones source", "", MOTU_CTRL_PHONES_SRC, 0},

    /* For optical mode controls, the "register" is used to indicate direction */
    {"Control/OpticalIn_mode", "Optical input mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_IN},
    {"Control/OpticalOut_mode", "Optical output mode ", "", MOTU_CTRL_OPTICAL_MODE, MOTU_CTRL_DIR_OUT},
};

const MotuMixer Mixer_Traveler = MOTUMIXER(
    MixerCtrls_Traveler, MixerBuses_Traveler, MixerChannels_Traveler);

const MotuMixer Mixer_Ultralite = MOTUMIXER(
    MixerCtrls_Ultralite, MixerBuses_Ultralite, MixerChannels_Ultralite);

const MotuMixer Mixer_828Mk2 = MOTUMIXER(
    MixerCtrls_828Mk2, MixerBuses_Traveler, MixerChannels_Traveler);

const MotuMixer Mixer_896HD = MOTUMIXER(
    MixerCtrls_896HD, MixerBuses_Traveler, MixerChannels_Traveler);

/* The order of DevicesProperty entries must match the numeric order of the
 * MOTU model enumeration (EMotuModel).
 */
const DevicePropertyEntry DevicesProperty[] = {
//  { Ports_map,       N_ELEMENTS( Ports_map ),        MaxSR, MixerDescrPtr },
    { Ports_828MKII,   N_ELEMENTS( Ports_828MKII ),    96000, &Mixer_828Mk2, },
    { Ports_TRAVELER,  N_ELEMENTS( Ports_TRAVELER ),  192000, &Mixer_Traveler, },
    { Ports_ULTRALITE, N_ELEMENTS( Ports_ULTRALITE ),  96000, &Mixer_Ultralite, },
    { Ports_8PRE,      N_ELEMENTS( Ports_8PRE ),       96000 },
    { Ports_828MKI,    N_ELEMENTS( Ports_828MKI ),     48000 },
    { Ports_896HD,     N_ELEMENTS( Ports_896HD ),     192000, &Mixer_896HD, },
    { Ports_828mk3,    N_ELEMENTS( Ports_828mk3 ),    192000 },
};

MotuDevice::MotuDevice( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
    : FFADODevice( d, configRom )
    , m_motu_model( MOTU_MODEL_NONE )
    , m_iso_recv_channel ( -1 )
    , m_iso_send_channel ( -1 )
    , m_rx_bandwidth ( -1 )
    , m_tx_bandwidth ( -1 )
    , m_receiveProcessor ( 0 )
    , m_transmitProcessor ( 0 )
    , m_MixerContainer ( NULL )
    , m_ControlContainer ( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Motu::MotuDevice (NodeID %d)\n",
                 getConfigRom().getNodeId() );
}

MotuDevice::~MotuDevice()
{
    delete m_receiveProcessor;
    delete m_transmitProcessor;

    // Free ieee1394 bus resources if they have been allocated
    if (m_iso_recv_channel>=0 && !get1394Service().freeIsoChannel(m_iso_recv_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free recv iso channel %d\n", m_iso_recv_channel);
    }
    if (m_iso_send_channel>=0 && !get1394Service().freeIsoChannel(m_iso_send_channel)) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Could not free send iso channel %d\n", m_iso_send_channel);
    }

    destroyMixer();
}

bool
MotuDevice::buildMixer() {
    bool result = true;
    MotuMatrixMixer *fader_mmixer = NULL;
    MotuMatrixMixer *pan_mmixer = NULL;
    MotuMatrixMixer *solo_mmixer = NULL;
    MotuMatrixMixer *mute_mmixer = NULL;
    const struct MatrixMixBus *buses = NULL;
    const struct MatrixMixChannel *channels = NULL;
    unsigned int bus, ch, i;

    destroyMixer();

    // create the mixer object container
    m_MixerContainer = new Control::Container(this, "Mixer");
    if (!m_MixerContainer) {
        debugError("Could not create mixer container...\n");
        return false;
    } else {
        buses = DevicesProperty[m_motu_model-1].mixer->mixer_buses;
        if (buses == NULL) {
            debugOutput(DEBUG_LEVEL_WARNING, "No buses defined for model %d\n", m_motu_model);
            result = false;
        }
        channels = DevicesProperty[m_motu_model-1].mixer->mixer_channels;
        if (channels == NULL) {
            debugOutput(DEBUG_LEVEL_WARNING, "No channels defined for model %d\n", m_motu_model);
            result = false;
        }
    }

    if (result == false) {
        return true;
    }

    /* Create the matrix mixers and populate them */
    fader_mmixer = new ChannelFaderMatrixMixer(*this, "fader");
    result &= m_MixerContainer->addElement(fader_mmixer);
    pan_mmixer = new ChannelPanMatrixMixer(*this, "pan");
    result &= m_MixerContainer->addElement(pan_mmixer);
    solo_mmixer = new ChannelBinSwMatrixMixer(*this, "solo", 
        MOTU_CTRL_MASK_SOLO_VALUE, MOTU_CTRL_MASK_SOLO_SETENABLE);
    result &= m_MixerContainer->addElement(solo_mmixer);
    mute_mmixer = new ChannelBinSwMatrixMixer(*this, "mute",
        MOTU_CTRL_MASK_MUTE_VALUE, MOTU_CTRL_MASK_MUTE_SETENABLE);
    result &= m_MixerContainer->addElement(mute_mmixer);
    buses = DevicesProperty[m_motu_model-1].mixer->mixer_buses;
    for (bus=0; bus<DevicesProperty[m_motu_model-1].mixer->n_mixer_buses; bus++) {
        fader_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
        pan_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
        solo_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
        mute_mmixer->addRowInfo(buses[bus].name, 0, buses[bus].address);
    }
    channels = DevicesProperty[m_motu_model-1].mixer->mixer_channels;
    for (ch=0; ch<DevicesProperty[m_motu_model-1].mixer->n_mixer_channels; ch++) {
        uint32_t flags = channels[ch].flags;
        if (flags & MOTU_CTRL_CHANNEL_FADER)
            fader_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        if (flags & MOTU_CTRL_CHANNEL_PAN)
            pan_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        if (flags & MOTU_CTRL_CHANNEL_SOLO)
            solo_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        if (flags & MOTU_CTRL_CHANNEL_MUTE)
            mute_mmixer->addColInfo(channels[ch].name, 0, channels[ch].addr_ofs);
        flags &= ~(MOTU_CTRL_CHANNEL_FADER|MOTU_CTRL_CHANNEL_PAN|MOTU_CTRL_CHANNEL_SOLO|MOTU_CTRL_CHANNEL_MUTE);
        if (flags) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Control %s: unknown flag bits 0x%08x\n", channels[ch].name, flags);
        }
    }

    // Single non-matrixed mixer controls get added here.  Channel controls are supported
    // here, but usually these will be a part of a matrix mixer.
    for (i=0; i<DevicesProperty[m_motu_model-1].mixer->n_mixer_ctrls; i++) {
        const struct MixerCtrl *ctrl = &DevicesProperty[m_motu_model-1].mixer->mixer_ctrl[i];
        unsigned int type = ctrl->type;
        char name[100];
        char label[100];
        if (type & MOTU_CTRL_CHANNEL_FADER) {
            snprintf(name, 100, "%s%s", ctrl->name, "fader");
            snprintf(label,100, "%s%s", ctrl->label,"fader");
            result &= m_MixerContainer->addElement(
                new ChannelFader(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_FADER;
        }
        if (type & MOTU_CTRL_CHANNEL_PAN) {
            snprintf(name, 100, "%s%s", ctrl->name, "pan");
            snprintf(label,100, "%s%s", ctrl->label,"pan");
            result &= m_MixerContainer->addElement(
                new ChannelPan(*this, 
                    ctrl->dev_register,
                    name, label,
                    ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_PAN;
        }
        if (type & MOTU_CTRL_CHANNEL_MUTE) {
            snprintf(name, 100, "%s%s", ctrl->name, "mute");
            snprintf(label,100, "%s%s", ctrl->label,"mute");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, ctrl->dev_register,
                    MOTU_CTRL_MASK_MUTE_VALUE, MOTU_CTRL_MASK_MUTE_SETENABLE,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_MUTE;
        }
        if (type & MOTU_CTRL_CHANNEL_SOLO) {
            snprintf(name, 100, "%s%s", ctrl->name, "solo");
            snprintf(label,100, "%s%s", ctrl->label,"solo");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, ctrl->dev_register,
                    MOTU_CTRL_MASK_SOLO_VALUE, MOTU_CTRL_MASK_SOLO_SETENABLE,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_CHANNEL_SOLO;
        }

        if (type & MOTU_CTRL_MIX_FADER) {
            snprintf(name, 100, "%s%s", ctrl->name, "fader");
            snprintf(label,100, "%s%s", ctrl->label,"fader");
            result &= m_MixerContainer->addElement(
                new MixFader(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_MIX_FADER;
        }
        if (type & MOTU_CTRL_MIX_MUTE) {
            snprintf(name, 100, "%s%s", ctrl->name, "mute");
            snprintf(label,100, "%s%s", ctrl->label,"mute");
            result &= m_MixerContainer->addElement(
                new MixMute(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_MIX_MUTE;
        }
        if (type & MOTU_CTRL_MIX_DEST) {
            snprintf(name, 100, "%s%s", ctrl->name, "dest");
            snprintf(label,100, "%s%s", ctrl->label,"dest");
            result &= m_MixerContainer->addElement(
                new MixDest(*this, ctrl->dev_register, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_MIX_DEST;
        }

        if (type & MOTU_CTRL_INPUT_TRIMGAIN) {
            snprintf(name, 100, "%s%s", ctrl->name, "trimgain");
            snprintf(label,100, "%s%s", ctrl->label,"trimgain");
            result &= m_MixerContainer->addElement(
                new InputGainPad(*this, ctrl->dev_register, MOTU_CTRL_MODE_TRIMGAIN,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_TRIMGAIN;
        }
        if (type & MOTU_CTRL_INPUT_PAD) {
            snprintf(name, 100, "%s%s", ctrl->name, "pad");
            snprintf(label,100, "%s%s", ctrl->label,"pad");
            result &= m_MixerContainer->addElement(
                new InputGainPad(*this, ctrl->dev_register, MOTU_CTRL_MODE_PAD,
                    name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_PAD;
        }

        if (type & MOTU_CTRL_INPUT_LEVEL) {
            snprintf(name, 100, "%s%s", ctrl->name, "level");
            snprintf(label,100, "%s%s", ctrl->label,"level");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, MOTU_REG_INPUT_LEVEL,
                    1<<ctrl->dev_register, 0, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_LEVEL;
        }
        if (type & MOTU_CTRL_INPUT_BOOST) {
            snprintf(name, 100, "%s%s", ctrl->name, "boost");
            snprintf(label,100, "%s%s", ctrl->label,"boost");
            result &= m_MixerContainer->addElement(
                new MotuBinarySwitch(*this, MOTU_REG_INPUT_BOOST,
                    1<<ctrl->dev_register, 0, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_INPUT_BOOST;
        }
        if (type & MOTU_CTRL_PHONES_SRC) {
            snprintf(name, 100, "%s%s", ctrl->name, "src");
            snprintf(label,100, "%s%s", ctrl->label,"src");
            result &= m_MixerContainer->addElement(
                new PhonesSrc(*this, name, label, ctrl->desc));
            type &= ~MOTU_CTRL_PHONES_SRC;
        }
        if (type & MOTU_CTRL_OPTICAL_MODE) {
            result &= m_MixerContainer->addElement(
                new OpticalMode(*this, ctrl->dev_register,
                    ctrl->name, ctrl->label, ctrl->desc));
            type &= ~MOTU_CTRL_OPTICAL_MODE;
        }
        if (type & MOTU_CTRL_METER) {
            if (ctrl->dev_register & MOTU_CTRL_METER_PEAKHOLD) {
                snprintf(name, 100, "%s%s", ctrl->name, "peakhold_time");
                snprintf(label,100, "%s%s", ctrl->label,"peakhold time");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_PEAKHOLD_MASK,
                        MOTU_METER_PEAKHOLD_SHIFT, name, label, ctrl->desc));
            }
            if (ctrl->dev_register & MOTU_CTRL_METER_CLIPHOLD) {
                snprintf(name, 100, "%s%s", ctrl->name, "cliphold_time");
                snprintf(label,100, "%s%s", ctrl->label,"cliphold time");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_CLIPHOLD_MASK,
                        MOTU_METER_CLIPHOLD_SHIFT, name, label, ctrl->desc));
            }
            if (ctrl->dev_register & MOTU_CTRL_METER_AESEBU_SRC) {
                snprintf(name, 100, "%s%s", ctrl->name, "aesebu_src");
                snprintf(label,100, "%s%s", ctrl->label,"AESEBU source");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_AESEBU_SRC_MASK,
                        MOTU_METER_AESEBU_SRC_SHIFT, name, label, ctrl->desc));
            }
            if (ctrl->dev_register & MOTU_CTRL_METER_PROG_SRC) {
                snprintf(name, 100, "%s%s", ctrl->name, "src");
                snprintf(label,100, "%s%s", ctrl->label,"source");
                result &= m_MixerContainer->addElement(
                    new MeterControl(*this, MOTU_METER_PROG_SRC_MASK,
                        MOTU_METER_PROG_SRC_SHIFT, name, label, ctrl->desc));
            }
            type &= ~MOTU_CTRL_METER;
        }

        if (type) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Unknown mixer control type flag bits 0x%08x\n", ctrl->type);
        }
    }

    /* Now add some general device information controls.  These may yet
     * become device-specific if it turns out to be easier that way.
     */
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_MODEL, "Info/Model", "Model identifier", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_IS_STREAMING, "Info/IsStreaming", "Is device streaming", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_SAMPLE_RATE, "Info/SampleRate", "Device sample rate", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_HAS_MIC_INPUTS, "Info/HasMicInputs", "Device has mic inputs", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_HAS_AESEBU_INPUTS, "Info/HasAESEBUInputs", "Device has AES/EBU inputs", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_HAS_SPDIF_INPUTS, "Info/HasSPDIFInputs", "Device has SPDIF inputs", ""));
    result &= m_MixerContainer->addElement(
        new InfoElement(*this, MOTU_INFO_HAS_OPTICAL_SPDIF, "Info/HasOpticalSPDIF", "Device has Optical SPDIF", ""));

    if (!addElement(m_MixerContainer)) {
        debugWarning("Could not register mixer to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    // Special controls
    m_ControlContainer = new Control::Container(this, "Control");
    if (!m_ControlContainer) {
        debugError("Could not create control container...\n");
        return false;
    }

    // Special controls get added here

    if (!result) {
        debugWarning("One or more device control elements could not be created.");
        // clean up those that couldn't be created
        destroyMixer();
        return false;
    }
    if (!addElement(m_ControlContainer)) {
        debugWarning("Could not register controls to device\n");
        // clean up
        destroyMixer();
        return false;
    }

    return true;
}


bool
MotuDevice::destroyMixer() {
    debugOutput(DEBUG_LEVEL_VERBOSE, "destroy mixer...\n");

    if (m_MixerContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no mixer to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_MixerContainer)) {
        debugError("Mixer present but not registered to the avdevice\n");
        return false;
    }

    // remove and delete (as in free) child control elements
    m_MixerContainer->clearElements(true);
    delete m_MixerContainer;
    m_MixerContainer = NULL;

    // remove control container
    if (m_ControlContainer == NULL) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "no controls to destroy...\n");
        return true;
    }
    
    if (!deleteElement(m_ControlContainer)) {
        debugError("Controls present but not registered to the avdevice\n");
        return false;
    }
    
    // remove and delete (as in free) child control elements
    m_ControlContainer->clearElements(true);
    delete m_ControlContainer;
    m_ControlContainer = NULL;

    return true;
}

bool
MotuDevice::probe( Util::Configuration& c, ConfigRom& configRom, bool generic)
{
    if(generic) return false;

    unsigned int vendorId = configRom.getNodeVendorId();
    unsigned int unitVersion = configRom.getUnitVersion();
    unsigned int unitSpecifierId = configRom.getUnitSpecifierId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].unit_version == unitVersion )
             && ( supportedDeviceList[i].unit_specifier_id == unitSpecifierId )
           )
        {
            return true;
        }
    }

    return false;
}

FFADODevice *
MotuDevice::createDevice(DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ))
{
    return new MotuDevice(d, configRom);
}

bool
MotuDevice::discover()
{
    unsigned int vendorId = getConfigRom().getNodeVendorId();
    unsigned int unitVersion = getConfigRom().getUnitVersion();
    unsigned int unitSpecifierId = getConfigRom().getUnitSpecifierId();

    for ( unsigned int i = 0;
          i < ( sizeof( supportedDeviceList )/sizeof( VendorModelEntry ) );
          ++i )
    {
        if ( ( supportedDeviceList[i].vendor_id == vendorId )
             && ( supportedDeviceList[i].unit_version == unitVersion )
             && ( supportedDeviceList[i].unit_specifier_id == unitSpecifierId )
           )
        {
            m_model = &(supportedDeviceList[i]);
            m_motu_model=supportedDeviceList[i].model;
        }
    }

    if (m_model == NULL) {
        return false;
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "found %s %s\n",
        m_model->vendor_name, m_model->model_name);

    if (!buildMixer()) {
        debugWarning("Could not build mixer\n");
    }

    return true;
}

enum FFADODevice::eStreamingState
MotuDevice::getStreamingState()
{
    unsigned int val = ReadRegister(MOTU_REG_ISOCTRL);
    /* Streaming is active if either bit 22 (Motu->PC streaming
     * enable) or bit 30 (PC->Motu streaming enable) is set.
     */
    debugOutput(DEBUG_LEVEL_VERBOSE, "MOTU_REG_ISOCTRL: %08x\n", val);

    if((val & 0x40400000) != 0) {
        return eSS_Both;
    } else if ((val & 0x40000000) != 0) {
        return eSS_Receiving;
    } else if ((val & 0x00400000) != 0) {
        return eSS_Sending;
    } else {
        return eSS_Idle;
    }
}

int
MotuDevice::getSamplingFrequency( ) {
/*
 * Retrieve the current sample rate from the MOTU device.
 */
    quadlet_t q = ReadRegister(MOTU_REG_CLK_CTRL);
    int rate = 0;

    switch (q & MOTU_RATE_BASE_MASK) {
        case MOTU_RATE_BASE_44100:
            rate = 44100;
            break;
        case MOTU_RATE_BASE_48000:
            rate = 48000;
            break;
    }
    switch (q & MOTU_RATE_MULTIPLIER_MASK) {
        case MOTU_RATE_MULTIPLIER_2X:
            rate *= 2;
            break;
        case MOTU_RATE_MULTIPLIER_4X:
            rate *= 4;
            break;
    }
    return rate;
}

int
MotuDevice::getConfigurationId()
{
    return 0;
}

bool
MotuDevice::setClockCtrlRegister(signed int samplingFrequency, unsigned int clock_source)
{
/*
 * Set the MOTU device's samplerate and/or clock source via the clock
 * control register.  If samplingFrequency <= 0 it remains unchanged.  If
 * clock_source is MOTU_CLKSRC_UNCHANGED the clock source remains unchanged.
 */
    const char *src_name;
    quadlet_t q, new_rate=0xffffffff;
    int i, supported=true, cancel_adat=false;
    quadlet_t reg;

    /* Don't touch anything if there's nothing to do */
    if (samplingFrequency<=0 && clock_source==MOTU_CLKSRC_NONE)
        return true;

    if ( samplingFrequency > DevicesProperty[m_motu_model-1].MaxSampleRate )
       return false; 

    reg = ReadRegister(MOTU_REG_CLK_CTRL);

    switch ( samplingFrequency ) {
        case -1:
            break;
        case 44100:
            new_rate = MOTU_RATE_BASE_44100 | MOTU_RATE_MULTIPLIER_1X;
            break;
        case 48000:
            new_rate = MOTU_RATE_BASE_48000 | MOTU_RATE_MULTIPLIER_1X;
            break;
        case 88200:
            new_rate = MOTU_RATE_BASE_44100 | MOTU_RATE_MULTIPLIER_2X;
            break;
        case 96000:
            new_rate = MOTU_RATE_BASE_48000 | MOTU_RATE_MULTIPLIER_2X;
            break;
        case 176400:
            new_rate = MOTU_RATE_BASE_44100 | MOTU_RATE_MULTIPLIER_4X;
            cancel_adat = true;  // current ADAT protocol doesn't support sample rate > 96000
            break;
        case 192000:
            new_rate = MOTU_RATE_BASE_48000 | MOTU_RATE_MULTIPLIER_4X;
            cancel_adat = true;
            break;
        default:
            supported=false;
    }

    // Sanity check the clock source
    if ((clock_source>7 || clock_source==6) && clock_source!=MOTU_CLKSRC_UNCHANGED)
        supported = false;

    // Update the clock control register.  FIXME: while this is now rather
    // comprehensive there may still be a need to manipulate MOTU_REG_CLK_CTRL
    // a little more than we do.
    if (supported) {

        // If optical port must be disabled (because a 4x sample rate has
        // been selected) then do so before changing the sample rate.  At
        // this stage it will be up to the user to re-enable the optical
        // port if the sample rate is set to a 1x or 2x rate later.
        if (cancel_adat) {
            setOpticalMode(MOTU_CTRL_DIR_INOUT, MOTU_OPTICAL_MODE_OFF);
        }

        // Set up new frequency if requested
        if (new_rate != 0xffffffff) {
            reg &= ~(MOTU_RATE_BASE_MASK|MOTU_RATE_MULTIPLIER_MASK);
            reg |= new_rate;
        }

        // Set up new clock source if required
        if (clock_source != MOTU_CLKSRC_UNCHANGED) {
            reg &= ~MOTU_CLKSRC_MASK;
            reg |= (clock_source & MOTU_CLKSRC_MASK);
        }

        // Bits 24-26 of MOTU_REG_CLK_CTRL behave a little differently
        // depending on the model.  In addition, different bit patterns are
        // written depending on whether streaming is enabled, disabled or is
        // changing state.  For now we go with the combination used when
        // streaming is enabled since it seems to work for the other states
        // as well.  Since device muting can be effected by these bits, we
        // may utilise this in future during streaming startup to prevent
        // noises during stabilisation.
        //
        // For most models (possibly all except the Ultralite) all 3 bits
        // can be zero and audio is still output.
        //
        // For the Traveler, if bit 26 is set (as it is under other OSes),
        // bit 25 functions as a device mute bit: if set, audio is output
        // while if 0 the entire device is muted.  If bit 26 is unset,
        // setting bit 25 doesn't appear to be detrimental.
        //
        // For the Ultralite, other OSes leave bit 26 unset.  However, unlike
        // other devices bit 25 seems to function as a mute bit in this case.
        //
        // The function of bit 24 is currently unknown.  Other OSes set it
        // for all devices so we will too.
        reg &= 0xf8ffffff;
        if (m_motu_model == MOTU_MODEL_TRAVELER)
            reg |= 0x04000000;
        reg |= 0x03000000;
        if (WriteRegister(MOTU_REG_CLK_CTRL, reg) == 0) {
            supported=true;
        } else {
            supported=false;
        }
        // A write to the rate/clock control register requires the
        // textual name of the current clock source be sent to the
        // clock source name registers.
        switch (reg & MOTU_CLKSRC_MASK) {
            case MOTU_CLKSRC_INTERNAL:
                src_name = "Internal        ";
                break;
            case MOTU_CLKSRC_ADAT_OPTICAL:
                src_name = "ADAT Optical    ";
                break;
            case MOTU_CLKSRC_SPDIF_TOSLINK:
                if (getOpticalMode(MOTU_DIR_IN) == MOTU_OPTICAL_MODE_TOSLINK)
                    src_name = "TOSLink         ";
                else
                    src_name = "SPDIF           ";
                break;
            case MOTU_CLKSRC_SMPTE:
                src_name = "SMPTE           ";
                break;
            case MOTU_CLKSRC_WORDCLOCK:
                src_name = "Word Clock In   ";
                break;
            case MOTU_CLKSRC_ADAT_9PIN:
                src_name = "ADAT 9-pin      ";
                break;
            case MOTU_CLKSRC_AES_EBU:
                src_name = "AES-EBU         ";
                break;
            default:
                src_name = "Unknown         ";
        }
        for (i=0; i<16; i+=4) {
            q = (src_name[i]<<24) | (src_name[i+1]<<16) |
                (src_name[i+2]<<8) | src_name[i+3];
            WriteRegister(MOTU_REG_CLKSRC_NAME0+i, q);
        }
    }
    return supported;
}

bool
MotuDevice::setSamplingFrequency( int samplingFrequency )
{
/*
 * Set the MOTU device's samplerate.
 */
    return setClockCtrlRegister(samplingFrequency, MOTU_CLKSRC_UNCHANGED);
}

std::vector<int>
MotuDevice::getSupportedSamplingFrequencies()
{
    std::vector<int> frequencies;
    signed int max_freq = DevicesProperty[m_motu_model-1].MaxSampleRate;

    /* All MOTUs support 1x rates.  All others must be conditional. */
    frequencies.push_back(44100);
    frequencies.push_back(48000);

    if (88200 <= max_freq)
        frequencies.push_back(88200);
    if (96000 <= max_freq)
        frequencies.push_back(96000);
    if (176400 <= max_freq)
        frequencies.push_back(176400);
    if (192000 <= max_freq)
        frequencies.push_back(192000);
    return frequencies;
}

FFADODevice::ClockSource
MotuDevice::clockIdToClockSource(unsigned int id) {
    ClockSource s;
    s.id = id;

    // Assume a clock source is valid/active unless otherwise overridden.
    s.valid = true;
    s.locked = true;
    s.active = true;

    switch (id) {
        case MOTU_CLKSRC_INTERNAL:
            s.type = eCT_Internal;
            s.description = "Internal sync";
            break;
        case MOTU_CLKSRC_ADAT_OPTICAL:
            s.type = eCT_ADAT;
            s.description = "ADAT optical";
            break;
        case MOTU_CLKSRC_SPDIF_TOSLINK:
            s.type = eCT_SPDIF;
            s.description = "SPDIF/Toslink";
            break;
        case MOTU_CLKSRC_SMPTE:
            s.type = eCT_SMPTE;
            s.description = "SMPTE";
            // Since we don't currently know how to deal with SMPTE on these devices
            // make sure the SMPTE clock source is disabled.
            s.valid = false;
            s.active = false;
            s.locked = false;
            break;
        case MOTU_CLKSRC_WORDCLOCK:
            s.type = eCT_WordClock;
            s.description = "Wordclock";
            break;
        case MOTU_CLKSRC_ADAT_9PIN:
            s.type = eCT_ADAT;
            s.description = "ADAT 9-pin";
            break;
        case MOTU_CLKSRC_AES_EBU:
            s.type = eCT_AES;
            s.description = "AES/EBU";
            break;
        default:
            s.type = eCT_Invalid;
    }

    s.slipping = false;
    return s;
}

FFADODevice::ClockSourceVector
MotuDevice::getSupportedClockSources() {
    FFADODevice::ClockSourceVector r;
    ClockSource s;

    /* Form a list of clocks supported by MOTU interfaces */
    s = clockIdToClockSource(MOTU_CLKSRC_INTERNAL);
    r.push_back(s);
    s = clockIdToClockSource(MOTU_CLKSRC_ADAT_OPTICAL);
    r.push_back(s);
    s = clockIdToClockSource(MOTU_CLKSRC_SPDIF_TOSLINK);
    r.push_back(s);
    s = clockIdToClockSource(MOTU_CLKSRC_SMPTE);
    r.push_back(s);
    s = clockIdToClockSource(MOTU_CLKSRC_WORDCLOCK);
    r.push_back(s);
    s = clockIdToClockSource(MOTU_CLKSRC_ADAT_9PIN);
    r.push_back(s);
    s = clockIdToClockSource(MOTU_CLKSRC_AES_EBU);
    r.push_back(s);

    return r;
}

bool
MotuDevice::setActiveClockSource(ClockSource s) {
    debugOutput(DEBUG_LEVEL_VERBOSE, "setting clock source to id: %d\n",s.id);

    // FIXME: this could do with some error checking
    return setClockCtrlRegister(-1, s.id);
}

FFADODevice::ClockSource
MotuDevice::getActiveClockSource() {
    ClockSource s;
    quadlet_t clock_id = ReadRegister(MOTU_REG_CLK_CTRL) & MOTU_CLKSRC_MASK;
    s = clockIdToClockSource(clock_id);
    s.active = true;
    return s;
}

bool
MotuDevice::lock() {

    return true;
}


bool
MotuDevice::unlock() {

    return true;
}

void
MotuDevice::showDevice()
{
    debugOutput(DEBUG_LEVEL_VERBOSE,
        "%s %s at node %d\n", m_model->vendor_name, m_model->model_name,
        getNodeId());
}

bool
MotuDevice::prepare() {

    int samp_freq = getSamplingFrequency();
    unsigned int optical_in_mode = getOpticalMode(MOTU_DIR_IN);
    unsigned int optical_out_mode = getOpticalMode(MOTU_DIR_OUT);
    unsigned int event_size_in = getEventSize(MOTU_DIR_IN);
    unsigned int event_size_out= getEventSize(MOTU_DIR_OUT);

    debugOutput(DEBUG_LEVEL_NORMAL, "Preparing MotuDevice...\n" );

    // Explicitly set the optical mode, primarily to ensure that the
    // MOTU_REG_OPTICAL_CTRL register is initialised.  We need to do this to
    // because some interfaces (the Ultralite for example) appear to power
    // up without this set to anything sensible.  In this case, writes to
    // MOTU_REG_ISOCTRL fail more often than not, which is bad.
    setOpticalMode(MOTU_DIR_IN, optical_in_mode);
    setOpticalMode(MOTU_DIR_OUT, optical_out_mode);

    // Allocate bandwidth if not previously done.
    // FIXME: The bandwidth allocation calculation can probably be
    // refined somewhat since this is currently based on a rudimentary
    // understanding of the ieee1394 iso protocol.
    // Currently we assume the following.
    //   * Ack/iso gap = 0.05 us
    //   * DATA_PREFIX = 0.16 us
    //   * DATA_END    = 0.26 us
    // These numbers are the worst-case figures given in the ieee1394
    // standard.  This gives approximately 0.5 us of overheads per packet -
    // around 25 bandwidth allocation units (from the ieee1394 standard 1
    // bandwidth allocation unit is 125/6144 us).  We further assume the
    // MOTU is running at S400 (which it should be) so one allocation unit
    // is equivalent to 1 transmitted byte; thus the bandwidth allocation
    // required for the packets themselves is just the size of the packet. 
    // We used to allocate based on the maximum packet size (1160 bytes at
    // 192 kHz for the traveler) but now do this based on the actual device
    // state by utilising the result from getEventSize() and remembering
    // that each packet has an 8 byte CIP header.  Note that bandwidth is
    // allocated on a *per stream* basis - it must be allocated for both the
    // transmit and receive streams.  While most MOTU modules are close to
    // symmetric in terms of the number of in/out channels there are
    // exceptions, so we deal with receive and transmit bandwidth separately.
    signed int n_events_per_packet = samp_freq<=48000?8:(samp_freq<=96000?16:32);
    m_rx_bandwidth = 25 + (n_events_per_packet*event_size_in);
    m_tx_bandwidth = 25 + (n_events_per_packet*event_size_out);

    // Assign iso channels if not already done
    if (m_iso_recv_channel < 0)
        m_iso_recv_channel = get1394Service().allocateIsoChannelGeneric(m_rx_bandwidth);

    if (m_iso_send_channel < 0)
        m_iso_send_channel = get1394Service().allocateIsoChannelGeneric(m_tx_bandwidth);

    debugOutput(DEBUG_LEVEL_VERBOSE, "recv channel = %d, send channel = %d\n",
        m_iso_recv_channel, m_iso_send_channel);

    if (m_iso_recv_channel<0 || m_iso_send_channel<0) {
        // be nice and deallocate
        if (m_iso_recv_channel >= 0)
            get1394Service().freeIsoChannel(m_iso_recv_channel);
        if (m_iso_send_channel >= 0)
            get1394Service().freeIsoChannel(m_iso_send_channel);

        debugFatal("Could not allocate iso channels!\n");
        return false;
    }

    m_receiveProcessor=new Streaming::MotuReceiveStreamProcessor(*this, event_size_in);

    // The first thing is to initialize the processor.  This creates the
    // data structures.
    if(!m_receiveProcessor->init()) {
        debugFatal("Could not initialize receive processor!\n");
        return false;
    }
    m_receiveProcessor->setVerboseLevel(getDebugLevel());

    // Now we add ports to the processor
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to receive processor\n");

    char *buff;
    Streaming::Port *p=NULL;

    // retrieve the ID
    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defaulting to 'dev?'\n");
    }

    // Add audio capture ports
    if (!addDirPorts(Streaming::Port::E_Capture, samp_freq, optical_in_mode)) {
        return false;
    }

    // Add MIDI port.  The MOTU only has one MIDI input port, with each
    // MIDI byte sent using a 3 byte sequence starting at byte 4 of the
    // event data.
    asprintf(&buff,"%s_cap_MIDI0",id.c_str());
    p = new Streaming::MotuMidiPort(*m_receiveProcessor, buff,
        Streaming::Port::E_Capture, 4);
    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
    }
    free(buff);

    // example of adding an control port:
//    asprintf(&buff,"%s_cap_%s",id.c_str(),"myportnamehere");
//    p=new Streaming::MotuControlPort(
//            buff,
//            Streaming::Port::E_Capture,
//            0 // you can add all other port specific stuff you
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//
//        if (!m_receiveProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }

    // Do the same for the transmit processor
    m_transmitProcessor=new Streaming::MotuTransmitStreamProcessor(*this, event_size_out);

    m_transmitProcessor->setVerboseLevel(getDebugLevel());

    if(!m_transmitProcessor->init()) {
        debugFatal("Could not initialize transmit processor!\n");
        return false;
    }

    // Now we add ports to the processor
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding ports to transmit processor\n");

    // Add audio playback ports
    if (!addDirPorts(Streaming::Port::E_Playback, samp_freq, optical_out_mode)) {
        return false;
    }

    // Add MIDI port.  The MOTU only has one output MIDI port, with each
    // MIDI byte transmitted using a 3 byte sequence starting at byte 4
    // of the event data.
    asprintf(&buff,"%s_pbk_MIDI0",id.c_str());
    p = new Streaming::MotuMidiPort(*m_transmitProcessor, buff,
        Streaming::Port::E_Playback, 4);
    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n", buff);
    }
    free(buff);

    // example of adding an control port:
//    asprintf(&buff,"%s_pbk_%s",id.c_str(),"myportnamehere");
//
//    p=new Streaming::MotuControlPort(
//            buff,
//            Streaming::Port::E_Playback,
//            0 // you can add all other port specific stuff you
//              // need to pass by extending MotuXXXPort and MotuPortInfo
//    );
//    free(buff);
//
//    if (!p) {
//        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",buff);
//    } else {
//        if (!m_transmitProcessor->addPort(p)) {
//            debugWarning("Could not register port with stream processor\n");
//            return false;
//        } else {
//            debugOutput(DEBUG_LEVEL_VERBOSE, "Added port %s\n",buff);
//        }
//    }

    return true;
}

int
MotuDevice::getStreamCount() {
     return 2; // one receive, one transmit
}

Streaming::StreamProcessor *
MotuDevice::getStreamProcessorByIndex(int i) {
    switch (i) {
    case 0:
        return m_receiveProcessor;
    case 1:
         return m_transmitProcessor;
    default:
        return NULL;
    }
    return 0;
}

bool
MotuDevice::startStreamByIndex(int i) {

quadlet_t isoctrl = ReadRegister(MOTU_REG_ISOCTRL);

    // NOTE: this assumes that you have two streams
    switch (i) {
    case 0:
        // TODO: do the stuff that is nescessary to make the device
        // receive a stream

        // Set the streamprocessor channel to the one obtained by
        // the connection management
        m_receiveProcessor->setChannel(m_iso_recv_channel);

        // Mask out current transmit settings of the MOTU and replace
        // with new ones.  Turn bit 24 on to enable changes to the
        // MOTU's iso transmit settings when the iso control register
        // is written.  Bit 23 enables iso transmit from the MOTU.
        isoctrl &= 0xff00ffff;
        isoctrl |= (m_iso_recv_channel << 16);
        isoctrl |= 0x00c00000;
        WriteRegister(MOTU_REG_ISOCTRL, isoctrl);
        break;
    case 1:
        // TODO: do the stuff that is nescessary to make the device
        // transmit a stream

        // Set the streamprocessor channel to the one obtained by
        // the connection management
        m_transmitProcessor->setChannel(m_iso_send_channel);

        // Mask out current receive settings of the MOTU and replace
        // with new ones.  Turn bit 31 on to enable changes to the
        // MOTU's iso receive settings when the iso control register
        // is written.  Bit 30 enables iso receive by the MOTU.
        isoctrl &= 0x00ffffff;
        isoctrl |= (m_iso_send_channel << 24);
        isoctrl |= 0xc0000000;
        WriteRegister(MOTU_REG_ISOCTRL, isoctrl);
        break;

    default: // Invalid stream index
        return false;
    }

    return true;
}

bool
MotuDevice::stopStreamByIndex(int i) {

quadlet_t isoctrl = ReadRegister(MOTU_REG_ISOCTRL);

    // TODO: connection management: break connection
    // cfr the start function

    // NOTE: this assumes that you have two streams
    switch (i) {
    case 0:
        // Turn bit 22 off to disable iso send by the MOTU.  Turn
        // bit 23 on to enable changes to the MOTU's iso transmit
        // settings when the iso control register is written.
        isoctrl &= 0xffbfffff;
        isoctrl |= 0x00800000;
        WriteRegister(MOTU_REG_ISOCTRL, isoctrl);
        break;
    case 1:
        // Turn bit 30 off to disable iso receive by the MOTU.  Turn
        // bit 31 on to enable changes to the MOTU's iso receive
        // settings when the iso control register is written.
        isoctrl &= 0xbfffffff;
        isoctrl |= 0x80000000;
        WriteRegister(MOTU_REG_ISOCTRL, isoctrl);
        break;

    default: // Invalid stream index
        return false;
    }

    return true;
}

signed int MotuDevice::getIsoRecvChannel(void) {
    return m_iso_recv_channel;
}

signed int MotuDevice::getIsoSendChannel(void) {
    return m_iso_send_channel;
}

unsigned int MotuDevice::getOpticalMode(unsigned int dir) {
    unsigned int reg = ReadRegister(MOTU_REG_ROUTE_PORT_CONF);

debugOutput(DEBUG_LEVEL_VERBOSE, "optical mode: %x %x %x %x\n",dir, reg, reg & MOTU_OPTICAL_IN_MODE_MASK,
reg & MOTU_OPTICAL_OUT_MODE_MASK);

    if (dir == MOTU_DIR_IN)
        return (reg & MOTU_OPTICAL_IN_MODE_MASK) >> 8;
    else
        return (reg & MOTU_OPTICAL_OUT_MODE_MASK) >> 10;
}

signed int MotuDevice::setOpticalMode(unsigned int dir, unsigned int mode) {
    unsigned int reg = ReadRegister(MOTU_REG_ROUTE_PORT_CONF);
    unsigned int opt_ctrl = 0x0000002;

    /* THe 896HD doesn't have an SPDIF/TOSLINK optical mode, so don't try to
     * set it
     */
    if (m_motu_model==MOTU_MODEL_896HD && mode==MOTU_OPTICAL_MODE_TOSLINK)
        return -1;

    // Set up the optical control register value according to the current
    // optical port modes.  At this stage it's not completely understood
    // what the "Optical control" register does, so the values it's set to
    // are more or less "magic" numbers.
    if ((reg & MOTU_OPTICAL_IN_MODE_MASK) != (MOTU_OPTICAL_MODE_ADAT<<8))
        opt_ctrl |= 0x00000080;
    if ((reg & MOTU_OPTICAL_OUT_MODE_MASK) != (MOTU_OPTICAL_MODE_ADAT<<10))
        opt_ctrl |= 0x00000040;

    if (dir & MOTU_DIR_IN) {
        reg &= ~MOTU_OPTICAL_IN_MODE_MASK;
        reg |= (mode << 8) & MOTU_OPTICAL_IN_MODE_MASK;
        if (mode != MOTU_OPTICAL_MODE_ADAT)
            opt_ctrl |= 0x00000080;
        else
            opt_ctrl &= ~0x00000080;
    }
    if (dir & MOTU_DIR_OUT) {
        reg &= ~MOTU_OPTICAL_OUT_MODE_MASK;
        reg |= (mode <<10) & MOTU_OPTICAL_OUT_MODE_MASK;
        if (mode != MOTU_OPTICAL_MODE_ADAT)
            opt_ctrl |= 0x00000040;
        else
            opt_ctrl &= ~0x00000040;
    }

    // FIXME: there seems to be more to it than this, but for
    // the moment at least this seems to work.
    WriteRegister(MOTU_REG_ROUTE_PORT_CONF, reg);
    return WriteRegister(MOTU_REG_OPTICAL_CTRL, opt_ctrl);
}

signed int MotuDevice::getEventSize(unsigned int direction) {
//
// Return the size in bytes of a single event sent to (dir==MOTU_OUT) or
// from (dir==MOTU_IN) the MOTU as part of an iso data packet.
//
// FIXME: for performance it may turn out best to calculate the event
// size in setOpticalMode and cache the result in a data field.  However,
// as it stands this will not adapt to dynamic changes in sample rate - we'd
// need a setFrameRate() for that.
//
// At the very least an event consists of the SPH (4 bytes) and the control/MIDI
// bytes (6 bytes).
// Note that all audio channels are sent using 3 bytes.
signed int sample_rate = getSamplingFrequency();
signed int optical_mode = getOpticalMode(direction);
signed int size = 4+6;

unsigned int i;
unsigned int dir = direction==Streaming::Port::E_Capture?MOTU_PA_IN:MOTU_PA_OUT;
unsigned int flags = (1 << ( optical_mode + 4 ));

    if ( sample_rate > 96000 )
        flags |= MOTU_PA_RATE_4x;
    else if ( sample_rate > 48000 )
        flags |= MOTU_PA_RATE_2x;
    else
        flags |= MOTU_PA_RATE_1x;

    // Don't test for padding port flag here since we need to include such
    // pseudo-ports when calculating the event size.
    for (i=0; i < DevicesProperty[m_motu_model-1].n_port_entries; i++) {
        if (( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & dir ) &&
	   ( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & MOTU_PA_RATE_MASK & flags ) &&
	   ( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & MOTU_PA_OPTICAL_MASK & flags )) {
            size += 3;
        }
    }

    // Finally round size up to the next quadlet boundary
    return ((size+3)/4)*4;
}
/* ======================================================================= */

bool MotuDevice::addPort(Streaming::StreamProcessor *s_processor,
  char *name, enum Streaming::Port::E_Direction direction,
  int position, int size) {
/*
 * Internal helper function to add a MOTU port to a given stream processor.
 * This just saves the unnecessary replication of what is essentially
 * boilerplate code.  Note that the port name is freed by this function
 * prior to exit.
 */
Streaming::Port *p=NULL;

    p = new Streaming::MotuAudioPort(*s_processor, name, direction, position, size);

    if (!p) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Skipped port %s\n",name);
    }
    free(name);
    return true;
}
/* ======================================================================= */

bool MotuDevice::addDirPorts(
  enum Streaming::Port::E_Direction direction,
  unsigned int sample_rate, unsigned int optical_mode) {
/*
 * Internal helper method: adds all required ports for the given direction
 * based on the indicated sample rate and optical mode.
 *
 * Notes: currently ports are not created if they are disabled due to sample
 * rate or optical mode.  However, it might be better to unconditionally
 * create all ports and just disable those which are not active.
 */
const char *mode_str = direction==Streaming::Port::E_Capture?"cap":"pbk";
Streaming::StreamProcessor *s_processor;
unsigned int i;
char *buff;
unsigned int dir = direction==Streaming::Port::E_Capture?MOTU_PA_IN:MOTU_PA_OUT;
unsigned int flags = (1 << ( optical_mode + 4 ));

    if ( sample_rate > 96000 )
        flags |= MOTU_PA_RATE_4x;
    else if ( sample_rate > 48000 )
        flags |= MOTU_PA_RATE_2x;
    else
        flags |= MOTU_PA_RATE_1x;

    // retrieve the ID
    std::string id=std::string("dev?");
    if(!getOption("id", id)) {
        debugWarning("Could not retrieve id parameter, defaulting to 'dev?'\n");
    }

    if (direction == Streaming::Port::E_Capture) {
        s_processor = m_receiveProcessor;
    } else {
        s_processor = m_transmitProcessor;
    }

    for (i=0; i < DevicesProperty[m_motu_model-1].n_port_entries; i++) {
        if (( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & dir ) &&
	   ( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & MOTU_PA_RATE_MASK & flags ) &&
	   ( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & MOTU_PA_OPTICAL_MASK & flags ) &&
	   !( DevicesProperty[m_motu_model-1].port_entry[i].port_flags & MOTU_PA_PADDING )) {
	    asprintf(&buff,"%s_%s_%s" , id.c_str(), mode_str,
              DevicesProperty[m_motu_model-1].port_entry[i].port_name);
            if (!addPort(s_processor, buff, direction, DevicesProperty[m_motu_model-1].port_entry[i].port_offset, 0))
                return false;
        }
    }
    
    return true;
}
/* ======================================================================== */

unsigned int MotuDevice::ReadRegister(unsigned int reg) {
/*
 * Attempts to read the requested register from the MOTU.
 */

  quadlet_t quadlet;

  quadlet = 0;
  // Note: 1394Service::read() expects a physical ID, not the node id
  if (get1394Service().read(0xffc0 | getNodeId(), MOTU_BASE_ADDR+reg, 1, &quadlet) <= 0) {
    debugError("Error doing motu read from register 0x%06x\n",reg);
  }

  return CondSwapFromBus32(quadlet);
}

signed int MotuDevice::WriteRegister(unsigned int reg, quadlet_t data) {
/*
 * Attempts to write the given data to the requested MOTU register.
 */

  unsigned int err = 0;
  data = CondSwapToBus32(data);

  // Note: 1394Service::write() expects a physical ID, not the node id
  if (get1394Service().write(0xffc0 | getNodeId(), MOTU_BASE_ADDR+reg, 1, &data) <= 0) {
    err = 1;
    debugError("Error doing motu write to register 0x%06x\n",reg);
  }

  SleepRelativeUsec(100);
  return (err==0)?0:-1;
}

}
