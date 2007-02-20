/* maudioe_avdevice.h
 * Copyright (C) 2006 by Daniel Wagner
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

#ifndef MAUDIODEVICE_H
#define MAUDIODEVICE_H

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebob/xmlparser.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "iavdevice.h"

class ConfigRom;
class Ieee1394Service;

namespace MAudio {

class AvDevice : public IAvDevice {
public:
    AvDevice( std::auto_ptr<ConfigRom>( configRom ),
	      Ieee1394Service& ieee1394Service,
              int nodeId,
	      int verboseLevel );
    virtual ~AvDevice();

    static bool probe( ConfigRom& configRom );
    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;
    virtual bool addXmlDescription( xmlNodePtr pDeviceNode );
    virtual void showDevice() const;
    
    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual int getSamplingFrequency( );
        
    virtual bool setId(unsigned int id);

    virtual int getStreamCount();
    virtual FreebobStreaming::StreamProcessor *getStreamProcessorByIndex(int i);

    virtual bool prepare();

    virtual int startStreamByIndex(int i);
    virtual int stopStreamByIndex(int i);

protected:
    std::auto_ptr<ConfigRom>( m_pConfigRom );
    Ieee1394Service* m_p1394Service;
    int              m_iNodeId;
    int              m_iVerboseLevel;
    const char*      m_pFilename;
    
    unsigned int m_id;

    // streaming stuff
    FreebobStreaming::AmdtpReceiveStreamProcessor *m_receiveProcessor;
    int m_receiveProcessorBandwidth;

    FreebobStreaming::AmdtpTransmitStreamProcessor *m_transmitProcessor;
    int m_transmitProcessorBandwidth;
    
    DECLARE_DEBUG_MODULE;
};

}

#endif
