
/* avdevice.h
 * Copyright (C) 2004 by Daniel Wagner, Pieter Palmers
 *
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
#ifndef AVDEVICE_H
#define AVDEVICE_H

#include "ieee1394service.h"
#include "debugmodule.h"
#include "configrom.h"

#include <libxml/tree.h>

#include <vector>
using std::vector;

class AvDeviceSubunit;


class AvDevice {
 public:
    enum EStates {
	eDeviceDiscovery = 0,
	eCheckState      = 1,  // put this instance to the sleep queue
	eDestroy         = 2
    };

    AvDevice( ConfigRom& configRom);
    virtual ~AvDevice();

    void setNodeId( int iNodeId ) 
	{ m_iNodeId = iNodeId; }
    int getNodeId()
	{ return m_iNodeId; }
    void setPort( int iPort )
	{ m_iPort = iPort; }
    int getPort()
	{ return m_iPort; }
    octlet_t getGuid()
	{ return m_configRom.getGuid(); }
    const std::string getVendorName() const
	{ return m_configRom.getVendorName(); }
    const std::string getModelName() const
	{ return m_configRom.getModelName(); }



    void execute( EStates state );
    
    FBReturnCodes initialize();
    bool isInitialised();

    FBReturnCodes setInputPlugSignalFormat( unsigned char plug, 
					    unsigned char fmt, 
					    quadlet_t fdf );
    FBReturnCodes getInputPlugSignalFormat( unsigned char plug, 
					    unsigned char *fmt, 
					    quadlet_t *fdf );  
    FBReturnCodes setOutputPlugSignalFormat( unsigned char plug, 
					     unsigned char fmt, 
					     quadlet_t fdf );
    FBReturnCodes getOutputPlugSignalFormat( unsigned char plug, 
					     unsigned char *fmt, 
					     quadlet_t *fdf );  
    
    //	getSourcePlugConnection();
    void printConnections();
    void test();
    FBReturnCodes addConnectionsToXml( xmlNodePtr root );
	    
    unsigned char getNbAsyncSourcePlugs() 
	{ return m_iNbAsyncSourcePlugs; }
    unsigned char getNbAsyncDestinationPlugs() 
	{ return m_iNbAsyncDestinationPlugs; }
    unsigned char getNbIsoSourcePlugs() 
	{ return m_iNbIsoSourcePlugs; } 
    unsigned char getNbIsoDestinationPlugs() 
	{ return m_iNbIsoDestinationPlugs; } 
    unsigned char getNbExtSourcePlugs() 
	{ return m_iNbExtSourcePlugs; }
    unsigned char getNbExtDestinationPlugs() 
	{ return m_iNbExtDestinationPlugs; }

 protected:
    AvDeviceSubunit *getSubunit(unsigned char type, unsigned char id);

    FBReturnCodes create1394RawHandle();
    FBReturnCodes enumerateSubUnits();
 
 private:
    ConfigRom m_configRom;
    int m_iNodeId;
    int m_iPort;
    bool m_bInitialised;
    octlet_t m_oGuid;
    unsigned int m_iGeneration;  //Which generation this device belongs to
    vector< AvDeviceSubunit * > cSubUnits;
    
    unsigned char m_iNbAsyncDestinationPlugs;
    unsigned char m_iNbAsyncSourcePlugs;
    unsigned char m_iNbIsoDestinationPlugs;   // iPCR
    unsigned char m_iNbIsoSourcePlugs;        // oPCR
    unsigned char m_iNbExtDestinationPlugs;
    unsigned char m_iNbExtSourcePlugs;

    const char* m_modelName;

    DECLARE_DEBUG_MODULE;
};

#endif
