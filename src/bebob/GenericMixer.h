/*
 * Copyright (C) 2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
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

#ifndef __FFAD0_GENERICMIXER__
#define __FFAD0_GENERICMIXER__

#include "src/debugmodule/debugmodule.h"

#include "libosc/OscNode.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/configrom.h"

namespace BeBoB {
class AvDevice;

class GenericMixer 
    : public OSC::OscNode {

public:

    GenericMixer(
        Ieee1394Service& ieee1394service,
        AvDevice &d);
    virtual ~GenericMixer();

    virtual OSC::OscResponse processOscMessage(OSC::OscMessage *m);

protected:
    OSC::OscResponse processOscMessageRoot(OSC::OscMessage *m);
    OSC::OscResponse processOscMessageSelector(std::string, OSC::OscMessage *m);
    OSC::OscResponse processOscMessageFeature(std::string, OSC::OscMessage *m);
    OSC::OscResponse processOscMessageProcessing(std::string, OSC::OscMessage *m);

protected:
    OSC::OscResponse rootListChildren();
    OSC::OscResponse selectorListChildren(int);
    OSC::OscResponse selectorListParams(int id);
    OSC::OscResponse selectorGetParam(int id, std::string p, OSC::OscMessage *src);
    OSC::OscResponse selectorSetParam(int id, std::string p, OSC::OscMessage *src);

    OSC::OscResponse featureListChildren(int);
    OSC::OscResponse featureListParams(int id);
    OSC::OscResponse featureGetParam(int id, std::string p, OSC::OscMessage *src);
    OSC::OscResponse featureSetParam(int id, std::string p, OSC::OscMessage *src);
    
    OSC::OscResponse processingListChildren(int);
    OSC::OscResponse codecListChildren(int);
    
    bool getSelectorValue(int id, int &value);
    bool setSelectorValue(int id, int value);
    
    bool getFeatureVolumeValue(int fb_id, int channel, int &volume);
    bool setFeatureVolumeValue(int id, int channel, int volume);
    
protected:
    Ieee1394Service      &m_p1394Service;
    AvDevice             &m_device;

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace BeBoB

#endif /* __FFAD0_GENERICMIXER__ */


