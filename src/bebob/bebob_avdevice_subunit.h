/*
 * Copyright (C) 2005-2007 by Daniel Wagner
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

#ifndef BEBOB_AVDEVICESUBUNIT_H
#define BEBOB_AVDEVICESUBUNIT_H

#include "bebob/bebob_avplug.h"
#include "bebob/bebob_functionblock.h"

#include "debugmodule/debugmodule.h"
#include "libavc/avc_extended_subunit_info.h"
#include "libavc/avc_definitions.h"
#include "libavc/avc_generic.h"

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


    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static AvDeviceSubunit* deserialize( Glib::ustring basePath,
                     Util::IODeserialize& deser,
                                         AvDevice& avDevice );
 protected:
    AvDeviceSubunit();

    bool discoverPlugs();
    bool discoverPlugs(AvPlug::EAvPlugDirection plugDirection,
                       plug_id_t plugMaxId );

    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const = 0;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice ) = 0;

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
    AvDeviceSubunitAudio();
    virtual ~AvDeviceSubunitAudio();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual const char* getName();

    FunctionBlockVector getFunctionBlocks() { return m_functions; };
    
protected:
    bool discoverFunctionBlocks();
    bool discoverFunctionBlocksDo(
        ExtendedSubunitInfoCmd::EFunctionBlockType fbType );
    bool createFunctionBlock(
        ExtendedSubunitInfoCmd::EFunctionBlockType fbType,
        ExtendedSubunitInfoPageData& data );

    FunctionBlock::ESpecialPurpose convertSpecialPurpose(
        function_block_special_purpose_t specialPurpose );

    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice );

protected:
    FunctionBlockVector m_functions;
};

/////////////////////////////

class AvDeviceSubunitMusic: public AvDeviceSubunit {
 public:
    AvDeviceSubunitMusic( AvDevice& avDevice,
              subunit_t id,
              int verboseLevel );
    AvDeviceSubunitMusic();
    virtual ~AvDeviceSubunitMusic();

    virtual const char* getName();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice );
};

}

#endif
