/*
 * Copyright (C) 2005-2008 by Daniel Wagner
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2008 by Jonathan Woithe
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

#include "debugmodule/debugmodule.h"

#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"

namespace Motu {

class MotuDevice;

#define MOTU_CTRL_CHANNEL_FADER   0x0001
#define MOTU_CTRL_CHANNEL_PAN     0x0002
#define MOTU_CTRL_CHANNEL_SOLO    0x0004
#define MOTU_CTRL_CHANNEL_MUTE    0x0008
#define MOTU_CTRL_BUS_FADER       0x0100

#define MOTU_CTRL_STD_CHANNEL \
    (MOTU_CTRL_CHANNEL_FADER|MOTU_CTRL_CHANNEL_PAN|\
     MOTU_CTRL_CHANNEL_SOLO|MOTU_CTRL_CHANNEL_MUTE)

class MotuDiscreteCtrl
    : public Control::Discrete
{
public:
    MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg);
    MotuDiscreteCtrl(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

protected:
    MotuDevice    &m_parent;
    unsigned int  m_register;
};

class ChannelFader
    : public MotuDiscreteCtrl
{
public:
    ChannelFader(MotuDevice &parent, unsigned int dev_reg);
    ChannelFader(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

class ChannelPan
    : public MotuDiscreteCtrl
{
public:
    ChannelPan(MotuDevice &parent, unsigned int dev_reg);
    ChannelPan(MotuDevice &parent, unsigned int dev_reg,
          std::string name, std::string label, std::string descr);

    virtual bool setValue(int v);
    virtual int getValue();
};

}
