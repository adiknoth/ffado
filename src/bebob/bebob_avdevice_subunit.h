/* bebob_avdevice_subunit.h
 * Copyright (C) 2005,06 by Daniel Wagner
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

#ifndef BEBOB_AVDEVICESUBUNIT_H
#define BEBOB_AVDEVICESUBUNIT_H

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_functionblock.h"

#include "debugmodule/debugmodule.h"
#include "libfreebobavc/avc_extended_subunit_info.h"
#include "libfreebobavc/avc_definitions.h"
#include "libfreebobavc/avc_generic.h"

#include <vector>

namespace BeBoB {

class AvDevice;

class AvDeviceSubunit {
 public:
    AvDeviceSubunit( AvDevice& avDevice,
		     AVCCommand::ESubunitType type,
		     subunit_t id,
		     int verboseLevel );
    virtual ~AvDeviceSubunit();

    virtual bool discover();
    virtual bool discoverConnections();
    virtual const char* getName() = 0;

    bool addPlug( AvPlug& plug );

    subunit_t getSubunitId()
	{ return m_sbId; }
    AVCCommand::ESubunitType getSubunitType()
	{ return m_sbType; }

    AvPlugVector& getPlugs()
	{ return m_plugs; }
    AvPlug* getPlug(AvPlug::EAvPlugDirection direction, plug_id_t plugId);


    AvDevice& getAvDevice() const
        { return *m_avDevice; }

 protected:
    bool discoverPlugs();
    bool discoverPlugs(AvPlug::EAvPlugDirection plugDirection,
                       plug_id_t plugMaxId );

 protected:
    AvDevice*                m_avDevice;
    AVCCommand::ESubunitType m_sbType;
    subunit_t                m_sbId;
    int                      m_verboseLevel;

    AvPlugVector             m_plugs;

    DECLARE_DEBUG_MODULE;
};

typedef std::vector<AvDeviceSubunit*> AvDeviceSubunitVector;

/////////////////////////////

class AvDeviceSubunitAudio: public AvDeviceSubunit {
 public:
    AvDeviceSubunitAudio( AvDevice& avDevice,
			  subunit_t id,
			  int verboseLevel );
    virtual ~AvDeviceSubunitAudio();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual const char* getName();


protected:
    bool discoverFunctionBlocks();
    bool discoverFunctionBlocksDo(
        ExtendedSubunitInfoCmd::EFunctionBlockType fbType );
    bool createFunctionBlock(
        ExtendedSubunitInfoCmd::EFunctionBlockType fbType,
        ExtendedSubunitInfoPageData& data );

    FunctionBlock::ESpecialPurpose convertSpecialPurpose(
        function_block_special_purpose_t specialPurpose );

protected:
    FunctionBlockVector m_functions;
};

/////////////////////////////

class AvDeviceSubunitMusic: public AvDeviceSubunit {
 public:
    AvDeviceSubunitMusic( AvDevice& avDevice,
			  subunit_t id,
			  int verboseLevel );
    virtual ~AvDeviceSubunitMusic();

    virtual const char* getName();
};

}

#endif
