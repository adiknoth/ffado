/* avc_plug_info.h
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

#ifndef AVCPlugInfo_h
#define AVCPlugInfo_h

#include "avc_generic.h"

#include <libavc1394/avc1394.h>

#define AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_ISOCHRONOUS_AND_EXTERNAL_PLUG 0x00
#define AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_ASYNCHRONOUS_PLUG             0x01
#define AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_GENERIC_BUS_PLUG_BLUETOOTH    0x40
#define AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_NOT_USED                      0xFF


class PlugInfoCmd: public AVCCommand
{
public:
    enum ESubFunction {
        eSF_SerialBusIsochronousAndExternalPlug  = AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_ISOCHRONOUS_AND_EXTERNAL_PLUG,
        eSF_SerialBusAsynchonousPlug             = AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_ASYNCHRONOUS_PLUG,
        eSF_SerialBusPlug                        = AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_GENERIC_BUS_PLUG_BLUETOOTH,
        eSF_NotUsed                              = AVC1394_PLUG_INFO_SUBFUNCTION_SERIAL_BUS_NOT_USED,
    };

    PlugInfoCmd( Ieee1394Service& ieee1394service,
		 ESubFunction eSubFunction = eSF_SerialBusIsochronousAndExternalPlug );
    PlugInfoCmd( const PlugInfoCmd& rhs );
    virtual ~PlugInfoCmd();

    virtual bool serialize( IOSSerialize& se );
    virtual bool deserialize( IISDeserialize& de );

    virtual const char* getCmdName() const
	{ return "PlugInfoCmd"; }

    nr_of_plugs_t m_serialBusIsochronousInputPlugs;
    nr_of_plugs_t m_serialBusIsochronousOutputPlugs;
    nr_of_plugs_t m_externalInputPlugs;
    nr_of_plugs_t m_externalOutputPlugs;
    nr_of_plugs_t m_serialBusAsynchronousInputPlugs;
    nr_of_plugs_t m_serialBusAsynchronousOuputPlugs;
    nr_of_plugs_t m_destinationPlugs;
    nr_of_plugs_t m_sourcePlugs;

    bool setSubFunction( ESubFunction subFunction );

protected:
    subfunction_t m_subFunction;

};


#endif // AVCPlugInfo_h
