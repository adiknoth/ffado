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

#include "libavc/general/avc_vendor_dependent_cmd.h"

namespace BeBoB {
namespace Presonus {

enum EInspire1394CmdSubfunc {
    EInspire1394CmdSubfuncPhono = 0,
    EInspire1394CmdSubfuncPhantom,
    EInspire1394CmdSubfuncBoost,
    EInspire1394CmdSubfuncLimit
};

class Inspire1394Device : public BeBoB::Device {
public:
    Inspire1394Device( DeviceManager& d, std::auto_ptr<ConfigRom>( configRom ));
    virtual ~Inspire1394Device();

    virtual void showDevice();

    bool setSpecificValue(EInspire1394CmdSubfunc subfunc,
                          int idx, uint8_t val);
    bool getSpecificValue(EInspire1394CmdSubfunc subfunc,
                          int idx, uint8_t *val);
private:
    bool addSpecificControls(void);
};

class Inspire1394Cmd : public AVC::VendorDependentCmd
{
public:
    Inspire1394Cmd( Ieee1394Service& ieee1394service );
    virtual ~Inspire1394Cmd() {};

    virtual bool serialize( Util::Cmd::IOSSerialize& se);
    virtual bool deserialize( Util::Cmd::IISDeserialize& de );

    virtual const char* getCmdName() const
    { return "Inspire1394Cmd"; }

    virtual void setSubfunc(uint8_t subfunc)
    { m_subfunc = subfunc; }
    virtual void setIdx(int idx)
    { m_idx = idx; }
    virtual void setArg(int arg)
    { m_arg = arg; }

    virtual int getArg(void)
    { return m_arg; }

protected:
    uint8_t m_subfunc;
    uint8_t m_idx;
    uint8_t m_arg;
};

class BinaryControl : public Control::Discrete
{
public:
    BinaryControl(Inspire1394Device& parent,
                  EInspire1394CmdSubfunc subfunc,
                  std::string name, std::string label, std::string desc);

    virtual bool setValue(int idx, int val);
    virtual int getValue(int idx);
    virtual bool setValue(int val)
        { return setValue(0, val); }
    virtual int getValue(void)
        { return getValue(0); }

    virtual int getMinimum() { return 0; };
    virtual int getMaximum() { return 1; };

private:
    Inspire1394Device&      m_Parent;
    EInspire1394CmdSubfunc  m_subfunc;
};

} // namespace Presonus
} // namespace BeBoB

#endif
