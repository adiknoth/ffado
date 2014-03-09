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
namespace Inspire1394 {

Command::Command(Ieee1394Service& ieee1394service)
    : VendorDependentCmd( ieee1394service )
    , m_subfunc( 0x00 )
    , m_idx( 0x00 )
    , m_arg( 0x00 )
{
    m_companyId = 0x000a92;
    setSubunitType( AVC::eST_Audio );
    setSubunitId( 0x00 );
}
bool Command::serialize( Util::Cmd::IOSSerialize& se )
{
    bool result = true;
    result &= VendorDependentCmd::serialize( se );
    result &= se.write(m_subfunc, "Cmd subfunc");
    result &= se.write(m_idx, "Cmd idx");
    result &= se.write(m_arg, "Cmd arg");

    return result;
}
bool Command::deserialize( Util::Cmd::IISDeserialize& de )
{
    bool result = true;
    result &= VendorDependentCmd::deserialize( de );
    result &= de.read( &m_subfunc);
    result &= de.read( &m_idx );
    result &= de.read( &m_arg );

    return result;
}

BinaryControl::BinaryControl(Device& parent,
                ECmdSubfunc subfunc,
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

Device::Device(DeviceManager& d, std::auto_ptr<ConfigRom>(configRom))
    : BeBoB::Device( d, configRom )
{
    addSpecificControls();
}
Device::~Device(void) 
{
}
void Device::showDevice()
{
    debugOutput(DEBUG_LEVEL_NORMAL,
                "This is a BeBoB::Presonus::Inspire1394::Device\n");
    BeBoB::Device::showDevice();
}
bool Device::addSpecificControls(void)
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
    ctl = new BinaryControl(*this, ECmdSubfuncPhono,
                        "PhonoSwitch", "Phono Switch", "Phono Switch");
    result &= ctls->addElement(ctl);

    // 48V for Analog In 1/2
    ctl = new BinaryControl(*this, ECmdSubfuncPhantom,
                        "PhantomPower", "Phantom Power", "Phantom Power");
    result &= ctls->addElement(ctl);

    // +20dB for Analog In 1/2 
    ctl = new BinaryControl(*this, ECmdSubfuncBoost,
                        "MicBoost", "Mic Boost", "Mic Boost");
    result &= ctls->addElement(ctl);

    // Limitter of preamp for Analog In 1/2
    ctl = new BinaryControl(*this, ECmdSubfuncLimit,
                        "MicLimit", "Mic Limit", "Mic Limit");
    result &= ctls->addElement(ctl);

    if ( !result ) {
        debugWarning("Any controls could not be added\n");
        destroyMixer();
        return false;
    }

    return true;
}
bool Device::getSpecificValue(ECmdSubfunc subfunc,
                                         int idx, uint8_t *val)
{
    Command cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Status );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );

    cmd.setSubfunc(subfunc);
    cmd.setIdx(idx);
    cmd.setArg(0xff);

    if ( !cmd.fire() ) {
        debugError( "Cmd failed\n" );
        return false;
    } else if (cmd.getResponse() != AVC::AVCCommand::eR_Implemented ) {
        debugError("Cmd received error response\n");
        return false;
    }

    *val = cmd.getArg();

    return true;
}
bool Device::setSpecificValue(ECmdSubfunc subfunc,
                                         int idx, uint8_t val)
{
    Command cmd( get1394Service() );
    cmd.setCommandType( AVC::AVCCommand::eCT_Control );
    cmd.setNodeId( getConfigRom().getNodeId() );
    cmd.setVerbose( getDebugLevel() );

    cmd.setSubfunc(subfunc);
    cmd.setIdx(idx);
    cmd.setArg(val);

    if ( !cmd.fire() ) {
        debugError( "Cmd failed\n" );
        return false;
    } else if (cmd.getResponse() != AVC::AVCCommand::eR_Accepted) {
        debugError("Cmd received error response\n");
        return false;
    }

    return true;
}

} // namespace Inspire1394
} // namespace Presonus
} // namespace BeBoB
