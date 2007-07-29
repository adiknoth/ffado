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

#ifndef BEBOB_FUNCTION_BLOCK_H
#define BEBOB_FUNCTION_BLOCK_H

#include "bebob/bebob_avplug.h"

#include "libavc/avc_definitions.h"
// #include "libavc/general/avc_subunit.h"

#include "debugmodule/debugmodule.h"

namespace AVC {
    class Subunit;
}

namespace BeBoB {

class FunctionBlock {
public:
    enum EFunctionBlockType {
        eFBT_AllFunctionBlockType   = 0xff,
        eFBT_AudioSubunitSelector   = 0x80,
        eFBT_AudioSubunitFeature    = 0x81,
        eFBT_AudioSubunitProcessing = 0x82,
        eFBT_AudioSubunitCodec      = 0x83,
    };

    enum ESpecialPurpose {
        eSP_InputGain,
        eSP_OutputVolume,
        eSP_NoSpecialPurpose
    };

    FunctionBlock( AVC::Subunit& subunit,
                   AVC::function_block_type_t type,
                   AVC::function_block_type_t subtype,
                   AVC::function_block_id_t id,
                   ESpecialPurpose purpose,
                   AVC::no_of_input_plugs_t nrOfInputPlugs,
                   AVC::no_of_output_plugs_t nrOfOutputPlugs,
                   int verbose );
    FunctionBlock( const FunctionBlock& rhs );
    FunctionBlock();
    virtual ~FunctionBlock();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual const char* getName() = 0;
    
    AVC::function_block_type_t getType() {return m_type;};
    AVC::function_block_type_t getSubtype() {return m_subtype;};
    AVC::function_block_id_t getId() {return m_id;};

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static FunctionBlock* deserialize( Glib::ustring basePath,
                       Util::IODeserialize& deser,
                       AVC::Unit& unit,
                       AVC::Subunit& subunit);
protected:
    bool discoverPlugs( AVC::Plug::EPlugDirection plugDirection,
                        AVC::plug_id_t plugMaxId );

protected:
    AVC::Subunit*      m_subunit;
    AVC::function_block_type_t m_type;
    AVC::function_block_type_t m_subtype;
    AVC::function_block_id_t   m_id;
    ESpecialPurpose       m_purpose;
    AVC::no_of_input_plugs_t   m_nrOfInputPlugs;
    AVC::no_of_output_plugs_t  m_nrOfOutputPlugs;
    int                   m_verbose;
    AVC::PlugVector          m_plugs;

    DECLARE_DEBUG_MODULE;
};

typedef std::vector<FunctionBlock*> FunctionBlockVector;

/////////////////////////////////////
/////////////////////////////////////

class FunctionBlockSelector: public FunctionBlock
{
public:
    FunctionBlockSelector(AVC::Subunit& subunit,
                          AVC::function_block_id_t id,
                          ESpecialPurpose purpose,
                          AVC::no_of_input_plugs_t nrOfInputPlugs,
                          AVC::no_of_output_plugs_t nrOfOutputPlugs,
                          int verbose);
    FunctionBlockSelector( const FunctionBlockSelector& rhs );
    FunctionBlockSelector();
    virtual ~FunctionBlockSelector();

    virtual const char* getName();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice );
};

/////////////////////////////////////

class FunctionBlockFeature: public FunctionBlock
{
public:
    FunctionBlockFeature(AVC::Subunit& subunit,
                         AVC::function_block_id_t id,
                         ESpecialPurpose purpose,
                         AVC::no_of_input_plugs_t nrOfInputPlugs,
                         AVC::no_of_output_plugs_t nrOfOutputPlugs,
                         int verbose);
    FunctionBlockFeature( const FunctionBlockFeature& rhs );
    FunctionBlockFeature();
    virtual ~FunctionBlockFeature();

    virtual const char* getName();
    
    // FIXME: this is not pretty!
    enum EControlSelectorEncoding {
        eCSE_Feature_Unknown            = 0x00,
        eCSE_Feature_Mute               = 0x01,
        eCSE_Feature_Volume             = 0x02,
        eCSE_Feature_LRBalance          = 0x03,
        eCSE_Feature_FRBalance          = 0x04,
        eCSE_Feature_Bass               = 0x05,
        eCSE_Feature_Mid                = 0x06,
        eCSE_Feature_Treble             = 0x07,
        eCSE_Feature_GEQ                = 0x08,
        eCSE_Feature_AGC                = 0x09,
        eCSE_Feature_Delay              = 0x0a,
        eCSE_Feature_BassBoost          = 0x0b,
        eCSE_Feature_Loudness           = 0x0c,
    };

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice );
};

/////////////////////////////////////

class FunctionBlockEnhancedMixer: public FunctionBlock
{
public:
    FunctionBlockEnhancedMixer( AVC::Subunit& subunit,
                                AVC::function_block_id_t id,
                                ESpecialPurpose purpose,
                                AVC::no_of_input_plugs_t nrOfInputPlugs,
                                AVC::no_of_output_plugs_t nrOfOutputPlugs,
                                int verbose );
    FunctionBlockEnhancedMixer();
    FunctionBlockEnhancedMixer( const FunctionBlockEnhancedMixer& rhs );
    virtual ~FunctionBlockEnhancedMixer();

    virtual const char* getName();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice );
};

/////////////////////////////////////

class FunctionBlockProcessing: public FunctionBlock
{
public:
    FunctionBlockProcessing( AVC::Subunit& subunit,
                             AVC::function_block_id_t id,
                             ESpecialPurpose purpose,
                             AVC::no_of_input_plugs_t nrOfInputPlugs,
                             AVC::no_of_output_plugs_t nrOfOutputPlugs,
                             int verbose );
    FunctionBlockProcessing( const FunctionBlockProcessing& rhs );
    FunctionBlockProcessing();
    virtual ~FunctionBlockProcessing();

    virtual const char* getName();

protected:
    virtual bool serializeChild( Glib::ustring basePath,
                                 Util::IOSerialize& ser ) const;
    virtual bool deserializeChild( Glib::ustring basePath,
                                   Util::IODeserialize& deser,
                                   AvDevice& avDevice );
};

/////////////////////////////////////

class FunctionBlockCodec: public FunctionBlock
{
public:
    FunctionBlockCodec(AVC::Subunit& subunit,
                       AVC::function_block_id_t id,
                       ESpecialPurpose purpose,
                       AVC::no_of_input_plugs_t nrOfInputPlugs,
                       AVC::no_of_output_plugs_t nrOfOutputPlugs,
                       int verbose);
    FunctionBlockCodec( const FunctionBlockCodec& rhs );
    FunctionBlockCodec();
    virtual ~FunctionBlockCodec();

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
