/* avdevice.h
 * Copyright (C) 2005 by Daniel Wagner
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

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebob/xmlparser.h"

#include "avplug.h"
#include "avdevicesubunit.h"

class ConfigRom;
class Ieee1394Service;
class SubunitPlugSpecificDataPlugAddress;

class AvDevice {
public:
    friend class AvDeviceSubunitAudio;
    friend class AvDeviceSubunitMusic;

    // takes ownership of config rom
    AvDevice( Ieee1394Service* ieee1394service,
              ConfigRom* configRom,
              int nodeId );
    virtual ~AvDevice();

    bool discover();

    std::string getVendorName();
    std::string getModelName();

    bool addXmlDescription( xmlNodePtr deviceNode );
    int getNodeId()
        { return m_nodeId; }

protected:
    bool discoverStep1();
    bool discoverStep2();
    bool discoverStep3();
    bool discoverStep4();
    bool discoverStep5();
    bool discoverStep6();
    bool discoverStep7();
    bool discoverStep8();
    bool discoverStep9();

    bool discoverPlugConnection( AvPlug& srcPlug,
                                 SubunitPlugSpecificDataPlugAddress& subunitPlugAddress );

    bool enumerateSubUnits();

    AvDeviceSubunit* getSubunit( subunit_type_t subunitType,
                                 subunit_id_t subunitId ) const;

    unsigned int getNrOfSubunits( subunit_type_t subunitType ) const;
    AvPlugConnection* getPlugConnection( AvPlug& srcPlug ) const;


    AvPlug* getPlugById( AvPlugVector& plugs, int id );
    bool addPlugToXmlDescription( AvPlug& plug, xmlNodePtr conectionSet );

protected:
    Ieee1394Service* m_1394Service;
    ConfigRom*       m_configRom;
    int              m_nodeId;

    AvPlugVector           m_isoInputPlugs;
    AvPlugVector           m_isoOutputPlugs;

    AvPlugConnectionVector m_plugConnections;

    AvDeviceSubunitVector  m_subunits;

    nr_of_plugs_t m_serialBusIsochronousInputPlugs;
    nr_of_plugs_t m_serialBusIsochronousOutputPlugs;

    DECLARE_DEBUG_MODULE;
};

#endif
