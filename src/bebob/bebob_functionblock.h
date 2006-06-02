/* bebob_functionblock.h
 * Copyright (C) 2006 by Daniel Wagner
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

#ifndef BEBOB_FUNCTION_BLOCK_H
#define BEBOB_FUNCTION_BLOCK_H

#include "bebob/bebob_avplug.h"

#include "libfreebobavc/avc_definitions.h"
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
                   function_block_id_t id,
                   ESpecialPurpose purpose,
                   no_of_input_plugs_t nrOfInputPlugs,
                   no_of_output_plugs_t nrOfOutputPlugs,
                   bool verbose );
    FunctionBlock( const FunctionBlock& rhs );
    virtual ~FunctionBlock();

    virtual bool discover();
    virtual bool discoverConnections();

    virtual const char* getName() = 0;

protected:
    bool discoverPlugs( AvPlug::EAvPlugDirection plugDirection,
                        plug_id_t plugMaxId );

protected:
    AvDeviceSubunit*      m_subunit;
    function_block_type_t m_type;
    function_block_id_t   m_id;
    ESpecialPurpose       m_purpose;
    no_of_input_plugs_t   m_nrOfInputPlugs;
    no_of_output_plugs_t  m_nrOfOutputPlugs;
    bool m_verbose;

    AvPlugVector m_plugs;

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
                          bool verbose);
    FunctionBlockSelector( const FunctionBlockSelector& rhs );
    virtual ~FunctionBlockSelector();

    virtual const char* getName();
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
                         bool verbose);
    FunctionBlockFeature( const FunctionBlockFeature& rhs );
    virtual ~FunctionBlockFeature();

    virtual const char* getName();
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
                                bool verbose );
    FunctionBlockEnhancedMixer( const FunctionBlockEnhancedMixer& rhs );
    virtual ~FunctionBlockEnhancedMixer();

    virtual const char* getName();
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
                             bool verbose );
    FunctionBlockProcessing( const FunctionBlockProcessing& rhs );
    virtual ~FunctionBlockProcessing();

    virtual const char* getName();
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
                       bool verbose);
    FunctionBlockCodec( const FunctionBlockCodec& rhs );
    virtual ~FunctionBlockCodec();

    virtual const char* getName();
};

}

#endif
