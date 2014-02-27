/*
 * Copyright (C) 2014      by Takashi Sakamoto
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "./inspire1394_avdevice.h"

#include "libutil/ByteSwap.h"
#include <iostream>

namespace BeBoB {
namespace Presonus {

Inspire1394VendorDependentCmd::Inspire1394VendorDependentCmd(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_subfunc( 0x00 )
    , m_idx( 0x00 )
    , m_arg( 0x00 )
{
    m_companyId = 0x000a92;
}
bool Inspire1394VendorDependentCmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_subfunc, "Inspire1394VendorDependentCmd subfunc");
    result &= se.write(m_idx, "Inspire1394VendorDependentCmd idx");
    result &= se.write(m_arg, "Inspire1394VendorDependentCmd arg");

    return result;
}
bool Inspire1394VendorDependentCmd::deserialize( Util::Cmd::IOSDeserialize& de )
{
    bool result = true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read( &m_subfunc);
    result &= de.read( &m_idx );
    result &= de.read( &m_arg );

    return result;
}

BinaryControl::BinaryControl(Inspire1394Device& parent,
                EInspire1394SubFunc subfunc,
                std::string name, std::string label, std::string desc)
    : Control::Discrete(&parent)
    , m_Parent(parent)
    , m_subfunc( subfunc )
{
    setName(name);
    setLabel(label);
    setDescription(desc);
}
bool BinaryControl::setValue(int idx, int v)
{
    uint32_t val = v;

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "setValue for type: %d, idx: %d, val: %d\n", 
                m_subfunc, idx, val);

    if ( !m_Parent.setSpecificValue(m_subfunc, idx, val) ) {
        debugError( "setSpecificValue failed\n" );
        return false;
    }

    return true;
}
int BinaryControl::getValue(int idx)
{
    uint32_t val;

    if ( !m_Parent.getSpecificValue(m_subfunc, idx, &val) ) {
        debugError( "getSpecificValue failed\n" );
        return 0;
    }

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "getValue for type: %d, idx: %d, val: %d\n", 
                m_subfunc, idx, val);

    return val;
}

Inspire1394Device::Inspire1394Device(DeviceManager& d,
                                     std::auto_ptr<ConfigRom>(configRom))
    : BeBoB::Device( d, configRom )
{
    addSpecificControls();
}
void Inspire1394Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL,
                "This is a BeBoB::Presonus::Inspire1394Device\n");
    BeBoB::Device::showDevice();
}
bool Inspire1394Device::addSpecificControls()
{
    Control::Container *ctls;
    BinaryControl ctl;
    bool result = true;

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Building a PreSonus Inspire1394 mixer...\n");

    ctls = new Control::Container(m_eap, "GlobalMute");
    if ( !addElement(ctls) ) {
        debugWarning("Could not register specific controls to device\n");
        delete ctls;
        return false;
    }

    ctl = BinaryControl(*this, Inspire1394CmdTypePhono,
                        "PhonoSwitch", "Phono Switch", "Phono Switch");
    result &= ctls->addElement(ctl);

    ctl = BinaryControl(*this, Inspire1394CmdTypePhantom,
                        "PhantomPower", "Phantom Power", "Phantom Power");
    result &= ctls->addElement(ctl);

    ctl = BinaryControl(*this, Inspire1394CmdTypeBoost,
                        "MicBoost", "Mic Boost", "Mic Boost");
    result &= ctls->addElement(ctl);

    ctl = BinaryControl(*this, Inspire1394CmdTypeLine,
                        "LineSwitch", "Line Switch", "Line Switch");
    result &= ctls->addElement(ctl);

    if ( !result ) {
        debugWarning("Any controls could not be added\n");
        destroyMixer();
        return false;
    }

    return true;
}
bool Inspire1394Device::getSpecificValue(EInspire1394CmdSubFunc subfunc,
                                         int idx, uint32_t *val)
{
    Inspire1394VendorDependentCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getConfigRom() );
    cmd.setVerbose( getDebugLevel() );

    cmd.setSubFunc(subfunc);
    cmd.setIdx(idx);
    cmd.setArg(0xff);

    if ( !cmd.fire() ) {
        debugError( "Inspire1394VendorDependentCmd failed\n" );
        return false;
    }

    *val = cmd.getArg();

    return true;
}
bool Inspire1394Device::setSpecificValue(EInspire1394CmdSubFUnc type,
                                         int idx, uint32_t val)
{
    Inspire1394VendorDependentCmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getConfigRom() );
    cmd.setVerbose( getDebugLevel() );

    cmd.setSubFunc(type);
    cmd.setIdx(idx);
    cmd.setArg(val);

    if ( !cmd.fire() ) {
        debugError( "Inspire1394VendorDependentCmd failed\n" );
        return false;
    }

    return true;
}
}

} // namespace Presonus
} // namespace BeBoB
