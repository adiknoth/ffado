/* bounce_avdevice.h
 * Copyright (C) 2006 by Pieter Palmers
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
#ifndef BOUNCEDEVICE_H
#define BOUNCEDEVICE_H

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebob/xmlparser.h"

#include "libstreaming/AmdtpStreamProcessor.h"
#include "libstreaming/AmdtpPort.h"
#include "libstreaming/AmdtpPortInfo.h"

#include "libieee1394/ARMHandler.h"

#include "iavdevice.h"
#include "libfreebob/freebob_bounce.h"

#include <vector>

#define BOUNCE_REGISTER_BASE 0x0000FFFFE0000000ULL
#define BOUNCE_REGISTER_LENGTH (4*256)
#define BOUNCE_REGISTER_TX_ISOCHANNEL 0x10
#define BOUNCE_REGISTER_RX_ISOCHANNEL 0x14
#define BOUNCE_INVALID_OFFSET 0xFFFFF00000000000ULL

class ConfigRom;
class Ieee1394Service;

namespace Bounce {

// struct to define the supported devices
struct VendorModelEntry {
    unsigned int vendor_id;
    unsigned int model_id;
    unsigned int unit_specifier_id;
    char *vendor_name;
    char *model_name;
};

class BounceDevice : public IAvDevice {
private:
    class BounceNotifier;
public:
    BounceDevice( std::auto_ptr<ConfigRom>( configRom ),
		  Ieee1394Service& ieee1394Service,
		  int nodeId,
		  int verboseLevel );
    virtual ~BounceDevice();

    static bool probe( ConfigRom& configRom );
    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;
    
    virtual bool addXmlDescription( xmlNodePtr deviceNode );
    
    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
	virtual int getSamplingFrequency( );
    
	virtual bool prepare();
    bool lock();
    bool unlock();
	
	virtual int getStreamCount();
	
	virtual Streaming::StreamProcessor *getStreamProcessorByIndex(int i);
	
    bool startStreamByIndex(int i);
    bool stopStreamByIndex(int i);
    
    virtual void showDevice() const;
    virtual bool setId(unsigned int id);

protected:
    std::auto_ptr<ConfigRom>( m_configRom );
    Ieee1394Service* m_p1394Service;
    int              m_nodeId;
    int              m_verboseLevel;

private:
	std::string xmlDescription;

    unsigned int m_samplerate;
    struct VendorModelEntry* m_model;

    // streaming stuff
    typedef std::vector< Streaming::StreamProcessor * > StreamProcessorVector;
    StreamProcessorVector m_receiveProcessors;
    StreamProcessorVector m_transmitProcessors;

    bool addPortsToProcessor(
       Streaming::StreamProcessor *processor, 
       Streaming::Port::E_Direction direction);

    DECLARE_DEBUG_MODULE;
    
private: // generic helpers
    int allocateIsoChannel(unsigned int packet_size);
    bool deallocateIsoChannel(int channel);
    
private: // I/O helpers
    // quadlet read/write routines
    bool readReg(fb_nodeaddr_t, fb_quadlet_t *);
    bool writeReg(fb_nodeaddr_t, fb_quadlet_t);
    bool readRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);
    bool writeRegBlock(fb_nodeaddr_t, fb_quadlet_t *, size_t);
    
private:
    BounceNotifier *m_Notifier;
    /**
     * this class reacts on the ohter side writing to the 
     * hosts address space
     */
    #define BOUNCE_NOTIFIER_BASE_ADDRESS 0x0000FFFFE0000000ULL
    #define BOUNCE_NOTIFIER_BLOCK_LENGTH 4
    class BounceNotifier : public ARMHandler
    {
    public:
        BounceNotifier(BounceDevice *, nodeaddr_t start);
        virtual ~BounceNotifier();
        
    private:
        BounceDevice *m_bouncedevice;
    };
};

}

#endif
