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
#include "debugmodule/debugmodule.h"

namespace BeBoB {

class AvDeviceSubunit;

class FunctionBlock {
public:
    enum EFunctionBlockType {
        eFBT_AllFunctinBlockType    = 0xff,
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

    FunctionBlock( AvDeviceSubunit& subunit,
           function_block_type_t type,
                   function_block_type_t subtype,
                   function_block_id_t id,
                   ESpecialPurpose purpose,
                   no_of_input_plugs_t nrOfInputPlugs,
                   no_of_output_plugs_t nrOfOutputPlugs,
                   int verbose );
    FunctionBlock( const FunctionBlock& rhs );
    FunctionBlock();
    virtual ~FunctionBlock();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual const char* getName() = 0;

    bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
    static FunctionBlock* deserialize( Glib::ustring basePath,
                       Util::IODeserialize& deser,
                       AvDevice& avDevice,
                                       AvDeviceSubunit& subunit);
protected:
    bool discoverPlugs( AvPlug::EAvPlugDirection plugDirection,
                        plug_id_t plugMaxId );

protected:
    AvDeviceSubunit*      m_subunit;
    function_block_type_t m_type;
    function_block_type_t m_subtype;
    function_block_id_t   m_id;
    ESpecialPurpose       m_purpose;
    no_of_input_plugs_t   m_nrOfInputPlugs;
    no_of_output_plugs_t  m_nrOfOutputPlugs;
    int                   m_verbose;
    AvPlugVector          m_plugs;

    DECLARE_DEBUG_MODULE;
};

typedef std::vector<FunctionBlock*> FunctionBlockVector;

/////////////////////////////////////
/////////////////////////////////////

class FunctionBlockSelector: public FunctionBlock
{
public:
    FunctionBlockSelector(AvDeviceSubunit& subunit,
                          function_block_id_t id,
                          ESpecialPurpose purpose,
                          no_of_input_plugs_t nrOfInputPlugs,
                          no_of_output_plugs_t nrOfOutputPlugs,
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
    FunctionBlockFeature(AvDeviceSubunit& subunit,
                         function_block_id_t id,
                         ESpecialPurpose purpose,
                         no_of_input_plugs_t nrOfInputPlugs,
                         no_of_output_plugs_t nrOfOutputPlugs,
                         int verbose);
    FunctionBlockFeature( const FunctionBlockFeature& rhs );
    FunctionBlockFeature();
    virtual ~FunctionBlockFeature();

    virtual const char* getName();

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
    FunctionBlockEnhancedMixer( AvDeviceSubunit& subunit,
                                function_block_id_t id,
                                ESpecialPurpose purpose,
                                no_of_input_plugs_t nrOfInputPlugs,
                                no_of_output_plugs_t nrOfOutputPlugs,
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
    FunctionBlockProcessing( AvDeviceSubunit& subunit,
                             function_block_id_t id,
                             ESpecialPurpose purpose,
                             no_of_input_plugs_t nrOfInputPlugs,
                             no_of_output_plugs_t nrOfOutputPlugs,
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
    FunctionBlockCodec(AvDeviceSubunit& subunit,
                       function_block_id_t id,
                       ESpecialPurpose purpose,
                       no_of_input_plugs_t nrOfInputPlugs,
                       no_of_output_plugs_t nrOfOutputPlugs,
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
