/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#ifndef AVCSIGNALSOURCE_H
#define AVCSIGNALSOURCE_H

#include "../general/avc_generic.h"
#include "../avc_definitions.h"

#include <libavc1394/avc1394.h>

namespace AVC {


class SignalAddress: public IBusData
{
public:
    enum EPlugId {
        ePI_AnyAvailableSerialBusPlug = 0x7e,
        ePI_Invalid                   = 0xfe,
        ePI_AnyAvailableExternalPlug  = 0xff,
    };
};

class SignalUnitAddress: public SignalAddress
{
public:
    SignalUnitAddress();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );
    virtual SignalUnitAddress* clone() const;

    byte_t m_plugId;
};

class SignalSubunitAddress: public SignalAddress
{
public:
    SignalSubunitAddress();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );
    virtual SignalSubunitAddress* clone() const;

    byte_t m_subunitType;
    byte_t m_subunitId;
    byte_t m_plugId;
};

class SignalSourceCmd: public AVCCommand
{
public:
    SignalSourceCmd( Ieee1394Service& ieee1394service );
    virtual ~SignalSourceCmd();

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "SignalSourceCmd"; }

    bool setSignalSource( SignalUnitAddress& signalAddress );
    bool setSignalSource( SignalSubunitAddress& signalAddress );
    bool setSignalDestination( SignalUnitAddress& signalAddress );
    bool setSignalDestination( SignalSubunitAddress& signalAddress );

    SignalAddress* getSignalSource();
    SignalAddress* getSignalDestination();

    // Control response
    byte_t m_resultStatus;

    // Status response
    byte_t m_outputStatus;
    byte_t m_conv;
    byte_t m_signalStatus;

    SignalAddress* m_signalSource;
    SignalAddress* m_signalDestination;
};

}

#endif // AVCSIGNALSOURCE_H
