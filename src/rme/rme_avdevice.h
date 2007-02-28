/* rme_avdevice.h
 * Copyright (C) 2006 Jonathan Woithe
 * Copyright (C) 2006,2007 by Pieter Palmers
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */
#ifndef RMEDEVICE_H
#define RMEDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

// #include "libstreaming/RmeStreamProcessor.h"

class ConfigRom;
class Ieee1394Service;

namespace Rme {

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    char *vendor_name;
    char *model_name;
};

class RmeDevice : public IAvDevice {
public:

    RmeDevice( std::auto_ptr<ConfigRom>( configRom ),
	      Ieee1394Service& ieee1394Service,
		  int nodeId );
    virtual ~RmeDevice();

    static bool probe( ConfigRom& configRom );
    bool discover();

    void showDevice() const;

    bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    int getSamplingFrequency( );

    int getStreamCount();
    Streaming::StreamProcessor *getStreamProcessorByIndex(int i);

    bool prepare();
    bool lock();
    bool unlock();

    bool startStreamByIndex(int i);
    bool stopStreamByIndex(int i);

protected:
    struct VendorModelEntry *m_model;
};

}

#endif
