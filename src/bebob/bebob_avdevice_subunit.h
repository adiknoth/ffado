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

class SubunitAudio : public AVC::SubunitAudio
{
public:
    SubunitAudio( AVC::Unit& avDevice,
                  AVC::subunit_t id );
    SubunitAudio();
    virtual ~SubunitAudio();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual AVC::Plug *createPlug( AVC::Unit* unit,
                                   AVC::Subunit* subunit,
                                   AVC::function_block_type_t functionBlockType,
                                   AVC::function_block_type_t functionBlockId,
                                   AVC::Plug::EPlugAddressType plugAddressType,
                                   AVC::Plug::EPlugDirection plugDirection,
                                   AVC::plug_id_t plugId );

    virtual const char* getName();

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

};

/////////////////////////////

class SubunitMusic : public AVC::SubunitMusic
{
 public:
    SubunitMusic( AVC::Unit& avDevice,
              AVC::subunit_t id );
    SubunitMusic();
    virtual ~SubunitMusic();

    virtual bool discover();
    
    virtual AVC::Plug *createPlug( AVC::Unit* unit,
                                   AVC::Subunit* subunit,
                                   AVC::function_block_type_t functionBlockType,
                                   AVC::function_block_type_t functionBlockId,
                                   AVC::Plug::EPlugAddressType plugAddressType,
                                   AVC::Plug::EPlugDirection plugDirection,
                                   AVC::plug_id_t plugId );

    virtual const char* getName();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AVC::Unit& unit );

};

}

#endif
