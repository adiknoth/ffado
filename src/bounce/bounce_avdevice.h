/* bounce_avdevice.h
 * Copyright (C) 2006 by Pieter Palmers
 * Copyright (C) 2006 by Daniel Wagner
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

#ifndef BOUNCEDEVICE_H
#define BOUNCEDEVICE_H

#include "iavdevice.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_extended_cmd_generic.h"
#include "libfreebob/xmlparser.h"

class ConfigRom;
class Ieee1394Service;

namespace Bounce {

class BounceDevice : public IAvDevice {
public:
    BounceDevice( Ieee1394Service& ieee1394Service,
		  int nodeId,
		  int verboseLevel );
    virtual ~BounceDevice();

    virtual bool discover();
    virtual ConfigRom& getConfigRom() const;
    virtual bool addXmlDescription( xmlNodePtr deviceNode );
    virtual bool setSamplingFrequency( ESamplingFrequency samplingFrequency );
    virtual void showDevice() const;

protected:
    Ieee1394Service* m_1394Service;
    ConfigRom*       m_configRom;
    int              m_nodeId;
    int              m_verboseLevel;

    DECLARE_DEBUG_MODULE;
};

}

#endif
