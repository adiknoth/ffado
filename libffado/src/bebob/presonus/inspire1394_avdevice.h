/*
 * Copyright (C) 2014      by Takashi Sakamoto
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef BEBOB_PRESONUS_INSPIRE1394_DEVICE_H
#define BEBOB_PRESONUS_INSPIRE1394_DEVICE_H

#include "debugmodule/debugmodule.h"
#include "bebob/bebob_avdevice.h"

namespace BeBoB {
namespace Presonus {

enum EInspire1394CmdSubFunc {
    EInspire1394CmdSubFuncPhono,
    EInspire1394CmdSubFuncPhantom,
    EInspire1394CmdSubFuncBoost,
    EInspire1394CmdSubFuncLimit
};

class Inspire1394VendorDependentCmd:: public AVC::VendorDependentCmd
{
public:
    Inspire1394VendorDependentCmd( Ieee1394Service& ieee1394service );
    virtual ~Inspire1394VendorDependentCmd;

    virtual bool serialize( Util::Cmd::IOSSerialize& se);
    virtual bool deserialize( Util::Cmd::IOSDeserialize& de );

    virtual const char* getCmdName() const
    { return "Inspire1394VendorDependentCmd"; }

    virtual void setSubFunc(EInspire1394CmdSubFunc subfunc)
    { m_subfunc = subfunc; }
    virtual void setIdx(int idx)
    { m_idx = idx; }
    virtual void setArg(int arg)
    { m_arg = arg; }

    virtual int getArg(void)
    { return m_arg; }

protected:
    EInspire1394CmdSubFunc m_subfunc;
    uint32_t m_idx;
    uint32_t m_arg;
}

class BinaryControl
    : public Control::Discrete
{
public:
    BinaryControl(Inspire1394Device& parent,
                  EInspire1394CmdSubFunc subfunc,
                  std::string name, std::string label, std::string desc);

    virtual bool setValue(int val) {
        { return setValue(0, val); }
    virtual int getValue() {
        { return getValue(0); }
    virtual bool setValue(int idx, int val);
    virtual int getValue(int idx);

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 1;};

private:
    Inspire1394Device&      m_Parent;
    EInspire1394CmdSubFuncm_subfunc;
};

class Inspire1394Device : public BeBoB::Device {
public:
    Inspire1394Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Inspire1394Device();

    virtual void showDevice();

private:
    bool getSpecificValue();
    bool setSpecificValue();
};

} // namespace Presonus
} // namespace BeBoB

#endif
