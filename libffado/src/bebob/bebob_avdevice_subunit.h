/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
    virtual FunctionBlockVector getFunctionBlocks() { return m_functions; };

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
    virtual bool deserializeUpdateChild( Glib::ustring basePath,
                                         Util::IODeserialize& deser );

protected:
     FunctionBlockVector m_functions;
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
    virtual bool deserializeUpdateChild( Glib::ustring basePath,
                                         Util::IODeserialize& deser );

};

}

#endif
