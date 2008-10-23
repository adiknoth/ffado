/*
 * Copyright (C) 2005-2008 by Pieter Palmers
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

#ifndef TERRATECVENDORDEPENDENT_H
#define TERRATECVENDORDEPENDENT_H

#include "libavc/general/avc_generic.h"
#include "libutil/cmd_serialize.h"
#include "libavc/general/avc_vendor_dependent_cmd.h"

namespace BeBoB {
namespace Terratec {

class TerratecVendorDependentCmd: public AVC::VendorDependentCmd
{
public:
    TerratecVendorDependentCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecVendorDependentCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

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

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecSyncStateCmd"; }

    byte_t m_syncstate;
};

class TerratecStoreMixerSettingsCmd: public TerratecVendorDependentCmd
{
public:
    TerratecStoreMixerSettingsCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecStoreMixerSettingsCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecStoreMixerSettingsCmd"; }
};

class TerratecSetMidiRemoteChannelCmd: public TerratecVendorDependentCmd
{
public:
    TerratecSetMidiRemoteChannelCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecSetMidiRemoteChannelCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecSetMidiRemoteChannelCmd"; }

    byte_t m_midichannel;
};

class TerratecSetMidiControlCmd: public TerratecVendorDependentCmd
{
public:
    TerratecSetMidiControlCmd(Ieee1394Service& ieee1394service);
    virtual ~TerratecSetMidiControlCmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

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

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

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

    virtual bool serialize( Util::Cmd::IOSSerialize& se );
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "TerratecDeviceIdCmd"; }

    byte_t m_deviceid;
};

}
}

#endif // TERRATECVENDORDEPENDENT_H
