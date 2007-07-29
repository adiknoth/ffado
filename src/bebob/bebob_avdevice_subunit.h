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
#include "libavc/general/avc_extended_subunit_info.h"
#include "libavc/avc_definitions.h"
#include "libavc/general/avc_generic.h"

#include <vector>

#include "libavc/general/avc_subunit.h"
#include "libavc/musicsubunit/avc_musicsubunit.h"
#include "libavc/audiosubunit/avc_audiosubunit.h"
#include "libavc/general/avc_plug.h"

namespace BeBoB {

/////////////////////////////

class Subunit {
public:
    Subunit() {};
    virtual ~Subunit() {};
    virtual bool discover();
    virtual bool discoverConnections();
    
    // required functions
    // implemented by the derived class
    virtual const char* getName() = 0;
    virtual AVC::subunit_t getSubunitId() = 0;
    virtual AVC::ESubunitType getSubunitType() = 0;
    virtual AVC::Unit& getUnit() const = 0;
    virtual AVC::Subunit& getSubunit() = 0;
    virtual AVC::PlugVector& getPlugs() = 0;

protected:
    bool discoverPlugs();
    bool discoverPlugs(Plug::EPlugDirection plugDirection,
                       AVC::plug_id_t plugMaxId );
private:
    DECLARE_DEBUG_MODULE;
};

/////////////////////////////

class SubunitAudio : public AVC::SubunitAudio
                   , public BeBoB::Subunit
{
public:
    SubunitAudio( AVC::Unit& avDevice,
                  AVC::subunit_t id );
    SubunitAudio();
    virtual ~SubunitAudio();

    virtual bool discover();
    virtual bool discoverConnections();

    // required interface for BeBoB::Subunit
    virtual const char* getName();
    virtual AVC::subunit_t getSubunitId() 
        { return AVC::SubunitAudio::getSubunitId(); };
    virtual AVC::ESubunitType getSubunitType()
        { return AVC::SubunitAudio::getSubunitType(); };
    virtual AVC::Unit& getUnit() const
        { return AVC::SubunitAudio::getUnit(); };
    virtual AVC::Subunit& getSubunit()
        { return *this; };
    virtual AVC::PlugVector& getPlugs()
        { return AVC::SubunitAudio::getPlugs(); };

protected:
    bool discoverFunctionBlocks();
    bool discoverFunctionBlocksDo(
        AVC::ExtendedSubunitInfoCmd::EFunctionBlockType fbType );
    bool createFunctionBlock(
        AVC::ExtendedSubunitInfoCmd::EFunctionBlockType fbType,
        AVC::ExtendedSubunitInfoPageData& data );

    FunctionBlock::ESpecialPurpose convertSpecialPurpose(
        AVC::function_block_special_purpose_t specialPurpose );

    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AVC::Unit& unit );
private:
    DECLARE_DEBUG_MODULE;
};

/////////////////////////////

class SubunitMusic : public AVC::SubunitMusic
                   , public BeBoB::Subunit
{
 public:
    SubunitMusic( AVC::Unit& avDevice,
              AVC::subunit_t id );
    SubunitMusic();
    virtual ~SubunitMusic();

    // required interface for BeBoB::Subunit
    virtual const char* getName();
    virtual AVC::subunit_t getSubunitId() 
        { return AVC::SubunitMusic::getSubunitId(); };
    virtual AVC::ESubunitType getSubunitType()
        { return AVC::SubunitMusic::getSubunitType(); };
    virtual AVC::Unit& getUnit() const
        { return AVC::SubunitMusic::getUnit(); };
    virtual AVC::Subunit& getSubunit()
        { return *this; };
    virtual AVC::PlugVector& getPlugs()
        { return AVC::SubunitMusic::getPlugs(); };

    
    virtual bool discover();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AVC::Unit& unit );
private:
    DECLARE_DEBUG_MODULE;
};

}

#endif
