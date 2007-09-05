/*
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#ifndef TERRATECVENDORDEPENDENT_H
#define TERRATECVENDORDEPENDENT_H

#include "libavc/general/avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

#include <libavc1394/avc1394.h>

namespace BeBoB {
namespace Terratec {

class TerratecVendorDependentCmd: public AVC::VendorDependentCmd
{
public:
    TerratecVendorDependentCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecVendorDependentCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecVendorDependentCmd"; }

protected:
    byte_t m_subfunction;
};

class TerratecSyncStateCmd: public TerratecVendorDependentCmd
{
public:
    TerratecSyncStateCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecSyncStateCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecSyncStateCmd"; }

    byte_t m_syncstate;
};

class TerratecStoreMixerSettingsCmd: public TerratecVendorDependentCmd
{
public:
    TerratecStoreMixerSettingsCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecStoreMixerSettingsCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecStoreMixerSettingsCmd"; }
};

class TerratecSetMidiRemoteChannelCmd: public TerratecVendorDependentCmd
{
public:
    TerratecSetMidiRemoteChannelCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecSetMidiRemoteChannelCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecSetMidiRemoteChannelCmd"; }

    byte_t m_midichannel;
};

class TerratecSetMidiControlCmd: public TerratecVendorDependentCmd
{
public:
    TerratecSetMidiControlCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecSetMidiControlCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecSetMidiControlCmd"; }

    byte_t m_mixercontrol;
    byte_t m_midicontroller;
};

class TerratecSetDefaultRoutingCmd: public TerratecVendorDependentCmd
{
public:
    TerratecSetDefaultRoutingCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecSetDefaultRoutingCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecSetDefaultRoutingCmd"; }

    byte_t m_output;
    byte_t m_source;
};

class TerratecDeviceIdCmd: public TerratecVendorDependentCmd
{
public:
    TerratecDeviceIdCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecDeviceIdCmd() {};

    virtual bool serialize( Util::IOSSerialize& se );
    virtual bool deserialize( Util::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecDeviceIdCmd"; }

    byte_t m_deviceid;
};

}
}

#endif // TERRATECVENDORDEPENDENT_H
