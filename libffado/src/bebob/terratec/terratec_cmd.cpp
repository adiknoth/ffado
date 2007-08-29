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

#include "terratec_cmd.h"

#include <netinet/in.h>
#include <iostream>

using namespace std;
using namespace AVC;

namespace BeBoB {
namespace Terratec {

TerratecVendorDependentCmd::TerratecVendorDependentCmd(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_subfunction ( 0x00 )
{
    m_companyId=0x000aac;
}

bool
TerratecVendorDependentCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_subfunction,"TerratecVendorDependentCmd subfunction");

    return result;
}

bool
TerratecVendorDependentCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read(&m_subfunction);


    return result;
}

//---------

TerratecSyncStateCmd::TerratecSyncStateCmd(Ieee1394Service& ieee1394service)
    : TerratecVendorDependentCmd( ieee1394service )
{
    m_subfunction=0x21;
}

bool
TerratecSyncStateCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::serialize( se );
    result &= se.write(m_syncstate,"TerratecSyncStateCmd m_syncstate");
    return result;
}

bool
TerratecSyncStateCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::deserialize( de );
    result &= de.read(&m_syncstate);
    return result;
}

//---------

TerratecStoreMixerSettingsCmd::TerratecStoreMixerSettingsCmd(Ieee1394Service& ieee1394service)
    : TerratecVendorDependentCmd( ieee1394service )
{
    m_subfunction=0x22;
}

bool
TerratecStoreMixerSettingsCmd::serialize( IOSSerialize& se )
{
    return TerratecVendorDependentCmd::serialize( se );;
}

bool
TerratecStoreMixerSettingsCmd::deserialize( IISDeserialize& de )
{
    return TerratecVendorDependentCmd::deserialize( de );;
}


//---------

TerratecSetMidiRemoteChannelCmd::TerratecSetMidiRemoteChannelCmd(Ieee1394Service& ieee1394service)
    : TerratecVendorDependentCmd( ieee1394service )
{
    m_subfunction=0x23;
}

bool
TerratecSetMidiRemoteChannelCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::serialize( se );
    result &= se.write(m_midichannel,"TerratecSetMidiRemoteChannelCmd m_midichannel");
    return result;
}

bool
TerratecSetMidiRemoteChannelCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::deserialize( de );
    result &= de.read(&m_midichannel);
    return result;
}

//---------

TerratecSetMidiControlCmd::TerratecSetMidiControlCmd(Ieee1394Service& ieee1394service)
    : TerratecVendorDependentCmd( ieee1394service )
{
    m_subfunction=0x24;
}

bool
TerratecSetMidiControlCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::serialize( se );
    result &= se.write(m_mixercontrol,"TerratecSetMidiControlCmd m_mixercontrol");
    result &= se.write(m_midicontroller,"TerratecSetMidiControlCmd m_midicontroller");
    return result;
}

bool
TerratecSetMidiControlCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::deserialize( de );
    result &= de.read(&m_mixercontrol);
    result &= de.read(&m_midicontroller);
    return result;
}

//---------

TerratecSetDefaultRoutingCmd::TerratecSetDefaultRoutingCmd(Ieee1394Service& ieee1394service)
    : TerratecVendorDependentCmd( ieee1394service )
{
    m_subfunction=0x25;
}

bool
TerratecSetDefaultRoutingCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::serialize( se );
    result &= se.write(m_output,"TerratecSetDefaultRoutingCmd m_output");
    result &= se.write(m_source,"TerratecSetDefaultRoutingCmd m_source");
    return result;
}

bool
TerratecSetDefaultRoutingCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::deserialize( de );
    result &= de.read(&m_output);
    result &= de.read(&m_source);
    return result;
}

//---------

TerratecDeviceIdCmd::TerratecDeviceIdCmd(Ieee1394Service& ieee1394service)
    : TerratecVendorDependentCmd( ieee1394service )
{
    m_subfunction=0x26;
}

bool
TerratecDeviceIdCmd::serialize( IOSSerialize& se )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::serialize( se );
    result &= se.write(m_deviceid,"TerratecDeviceIdCmd m_deviceid");
    return result;
}

bool
TerratecDeviceIdCmd::deserialize( IISDeserialize& de )
{
    bool result=true;
    result &= TerratecVendorDependentCmd::deserialize( de );
    result &= de.read(&m_deviceid);
    return result;
}

}
}
