/* avc_extended_cmd_generic.h
 * Copyright (C) 2005 by Daniel Wagner
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

#ifndef AVCEXTENDEDCMDGENERIC_H
#define AVCEXTENDEDCMDGENERIC_H

#include "avc_generic.h"

////////////////////////////////////////////////////////////

class PlugAddressData : public IBusData {
};

////////////////////////////////////////////////////////////

class UnitPlugAddress : public PlugAddressData
{
public:
    enum EPlugType {
        ePT_PCR              = 0x00,
        ePT_ExternalPlug     = 0x01,
        ePT_AsynchronousPlug = 0x02,
	ePT_Unknown          = 0xff,
    };

    UnitPlugAddress( EPlugType plugType,  plug_type_t plugId );
    virtual ~UnitPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual UnitPlugAddress* clone() const;

    plug_id_t   m_plugType;
    plug_type_t m_plugId;
    reserved_t  m_reserved;
};

////////////////////////////////////////////////////////////

class SubunitPlugAddress : public PlugAddressData
{
public:
    SubunitPlugAddress( plug_id_t plugId );
    virtual ~SubunitPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual SubunitPlugAddress* clone() const;

    plug_id_t m_plugId;
    reserved_t m_reserved0;
    reserved_t m_reserved1;
};


////////////////////////////////////////////////////////////

class FunctionBlockPlugAddress : public PlugAddressData
{
public:
    FunctionBlockPlugAddress( function_block_type_t functionBlockType,
                              function_block_id_t functionBlockId,
                              plug_id_t plugId );
    virtual ~FunctionBlockPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FunctionBlockPlugAddress* clone() const;

    function_block_type_t m_functionBlockType;
    function_block_id_t   m_functionBlockId;
    plug_id_t             m_plugId;
};

////////////////////////////////////////////////////////////

class UndefinedPlugAddress : public PlugAddressData
{
public:
    UndefinedPlugAddress();
    virtual ~UndefinedPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual UndefinedPlugAddress* clone() const;

    reserved_t m_reserved0;
    reserved_t m_reserved1;
    reserved_t m_reserved2;
};

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

class UnitPlugSpecificDataPlugAddress : public PlugAddressData
{
public:
    enum EPlugType {
        ePT_PCR              = 0x00,
        ePT_ExternalPlug     = 0x01,
        ePT_AsynchronousPlug = 0x02,
    };

    UnitPlugSpecificDataPlugAddress( EPlugType plugType,
                                     plug_type_t plugId );
    virtual ~UnitPlugSpecificDataPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual UnitPlugSpecificDataPlugAddress* clone() const;

    plug_type_t m_plugType;
    plug_id_t   m_plugId;
    reserved_t  m_reserved0;
    reserved_t  m_reserved1;
    reserved_t  m_reserved2;
};

////////////////////////////////////////////////////////////

class SubunitPlugSpecificDataPlugAddress : public PlugAddressData
{
public:
    SubunitPlugSpecificDataPlugAddress( AVCCommand::ESubunitType subunitType,
                                        subunit_id_t subunitId,
                                        plug_id_t plugId );
    virtual ~SubunitPlugSpecificDataPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual SubunitPlugSpecificDataPlugAddress* clone() const;

    subunit_type_t m_subunitType;
    subunit_id_t   m_subunitId;
    plug_id_t      m_plugId;
    reserved_t     m_reserved0;
    reserved_t     m_reserved1;
};

////////////////////////////////////////////////////////////

class FunctionBlockPlugSpecificDataPlugAddress : public PlugAddressData
{
public:
    FunctionBlockPlugSpecificDataPlugAddress( AVCCommand::ESubunitType subunitType,
                                              subunit_id_t subunitId,
                                              function_block_type_t functionBlockType,
                                              function_block_id_t functionBlockId,
                                              plug_id_t plugId);
    virtual ~FunctionBlockPlugSpecificDataPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual FunctionBlockPlugSpecificDataPlugAddress* clone() const;

    subunit_type_t        m_subunitType;
    subunit_id_t          m_subunitId;
    function_block_type_t m_functionBlockType;
    function_block_id_t   m_functionBlockId;
    plug_id_t             m_plugId;
};

////////////////////////////////////////////////////////////

class UndefinedPlugSpecificDataPlugAddress : public PlugAddressData
{
public:
    UndefinedPlugSpecificDataPlugAddress();
    virtual ~UndefinedPlugSpecificDataPlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );
    virtual UndefinedPlugSpecificDataPlugAddress* clone() const;

    reserved_t m_reserved0;
    reserved_t m_reserved1;
    reserved_t m_reserved2;
    reserved_t m_reserved3;
    reserved_t m_reserved4;
};

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

class PlugAddress : public IBusData {
public:
    enum EPlugDirection {
        ePD_Input     = 0x00,
        ePD_Output    = 0x01,
	ePD_Undefined = 0xff,
    };

    enum EPlugAddressMode {
        ePAM_Unit          = 0x00,
        ePAM_Subunit       = 0x01,
        ePAM_FunctionBlock = 0x02,
	ePAM_Undefined     = 0xff,
    };

    PlugAddress( EPlugDirection plugDirection,
                 EPlugAddressMode plugAddressMode,
                 UnitPlugAddress& unitPlugAddress );
    PlugAddress( EPlugDirection plugDirection,
                 EPlugAddressMode plugAddressMode,
                 SubunitPlugAddress& subUnitPlugAddress );
    PlugAddress( EPlugDirection plugDirection,
                 EPlugAddressMode plugAddressMode,
                 FunctionBlockPlugAddress& functionBlockPlugAddress );
    PlugAddress( ); // undefined plug address
    PlugAddress( const PlugAddress& pa );

    virtual ~PlugAddress();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual PlugAddress* clone() const;

    plug_direction_t    m_plugDirection;
    plug_address_mode_t m_addressMode;
    PlugAddressData*    m_plugAddressData;
};

const char* plugAddressPlugDirectionToString( PlugAddress::EPlugDirection direction  );
const char* plugAddressAddressModeToString( PlugAddress::EPlugAddressMode mode );

////////////////////////////////////////////////////////////

class PlugAddressSpecificData : public IBusData {
public:
    enum EPlugDirection {
        ePD_Input  = 0x00,
        ePD_Output = 0x01,
    };

    enum EPlugAddressMode {
        ePAM_Unit          = 0x00,
        ePAM_Subunit       = 0x01,
        ePAM_FunctionBlock = 0x02,
        ePAM_Undefined     = 0xff,
    };

    PlugAddressSpecificData( EPlugDirection plugDirection,
			     EPlugAddressMode plugAddressMode,
			     UnitPlugSpecificDataPlugAddress& unitPlugAddress );
    PlugAddressSpecificData( EPlugDirection plugDirection,
			     EPlugAddressMode plugAddressMode,
			     SubunitPlugSpecificDataPlugAddress& subUnitPlugAddress );
    PlugAddressSpecificData( EPlugDirection plugDirection,
			     EPlugAddressMode plugAddressMode,
			     FunctionBlockPlugSpecificDataPlugAddress& functionBlockPlugAddress );
    PlugAddressSpecificData( EPlugDirection plugDirection,
			     EPlugAddressMode plugAddressMode );
    PlugAddressSpecificData( const  PlugAddressSpecificData& pa );

    virtual ~PlugAddressSpecificData();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual PlugAddressSpecificData* clone() const;

    plug_direction_t    m_plugDirection;
    plug_address_mode_t m_addressMode;
    PlugAddressData*    m_plugAddressData;
};


#endif
