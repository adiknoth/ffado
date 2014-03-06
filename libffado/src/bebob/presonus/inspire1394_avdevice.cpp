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
#include "libutil/cmd_serialize.h"

namespace BeBoB {
namespace Presonus {

Inspire1394Cmd::Inspire1394Cmd(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_subfunc( 0x00 )
    , m_idx( 0x00 )
    , m_arg( 0x00 )
{
    m_companyId = 0x000a92;
    setSubunitType( AVC::eST_Audio );
    setSubunitId( 0x00 );
}
bool Inspire1394Cmd::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_subfunc, "Inspire1394Cmd subfunc");
    result &= se.write(m_idx, "Inspire1394Cmd idx");
    result &= se.write(m_arg, "Inspire1394Cmd arg");

    return result;
}
bool Inspire1394Cmd::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read( &m_subfunc);
    result &= de.read( &m_idx );
    result &= de.read( &m_arg );

    return result;
}

BinaryControl::BinaryControl(Inspire1394Device& parent,
                EInspire1394CmdSubfunc subfunc,
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
    uint8_t val = v;

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
    uint8_t val;

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
Inspire1394Device::~Inspire1394Device(void) 
{
}
void Inspire1394Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL,
                "This is a BeBoB::Presonus::Inspire1394Device\n");
    BeBoB::Device::showDevice();
}
bool Inspire1394Device::addSpecificControls(void)
{
    Control::Container *ctls;
    BinaryControl *ctl;
    bool result = true;

    debugOutput(DEBUG_LEVEL_VERBOSE,
                "Building a PreSonus Inspire1394 mixer...\n");

    ctls = new Control::Container(this, "Preamp");
    if ( !addElement(ctls) ) {
        debugWarning("Could not register specific controls to device\n");
        delete ctls;
        return false;
    }

    // RIAA equalization curve for Analog In 3/4
    ctl = new BinaryControl(*this, EInspire1394CmdSubfuncPhono,
                        "PhonoSwitch", "Phono Switch", "Phono Switch");
    result &= ctls->addElement(ctl);

    // 48V for Analog In 1/2
    ctl = new BinaryControl(*this, EInspire1394CmdSubfuncPhantom,
                        "PhantomPower", "Phantom Power", "Phantom Power");
    result &= ctls->addElement(ctl);

    // +20dB for Analog In 1/2 
    ctl = new BinaryControl(*this, EInspire1394CmdSubfuncBoost,
                        "MicBoost", "Mic Boost", "Mic Boost");
    result &= ctls->addElement(ctl);

    // Limitter of preamp for Analog In 1/2
    ctl = new BinaryControl(*this, EInspire1394CmdSubfuncLimit,
                        "MicLimit", "Mic Limit", "Mic Limit");
    result &= ctls->addElement(ctl);

    if ( !result ) {
        debugWarning("Any controls could not be added\n");
        destroyMixer();
        return false;
    }

    return true;
}
bool Inspire1394Device::getSpecificValue(EInspire1394CmdSubfunc subfunc,
                                         int idx, uint8_t *val)
{
    Inspire1394Cmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );

    cmd.setSubfunc(subfunc);
    cmd.setIdx(idx);
    cmd.setArg(0xff);

    if ( !cmd.fire() ) {
        debugError( "Inspire1394Cmd failed\n" );
        return false;
    } else if (cmd.getResponse() != AVC::AVCCommand::eR_Implemented ) {
        debugError("Inspire1394Cmd received error response\n");
        return false;
    }

    *val = cmd.getArg();

    return true;
}
bool Inspire1394Device::setSpecificValue(EInspire1394CmdSubfunc subfunc,
                                         int idx, uint8_t val)
{
    Inspire1394Cmd cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );

    cmd.setSubfunc(subfunc);
    cmd.setIdx(idx);
    cmd.setArg(val);

    if ( !cmd.fire() ) {
        debugError( "Inspire1394Cmd failed\n" );
        return false;
    } else if (cmd.getResponse() != AVC::AVCCommand::eR_Accepted) {
        debugError("Inspire1394Cmd received error response\n");
        return false;
    }

    return true;
}

} // namespace Presonus
} // namespace BeBoB
