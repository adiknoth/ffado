/*
 * Copyright (C) 2014      by Takashi Sakamoto
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

#ifndef BEBOB_MAUDIO_SPECIAL_MIXER_H
#define BEBOB_MAUDIO_SPECIAL_MIXER_H

#include "src/debugmodule/debugmodule.h"
#include "libcontrol/BasicElements.h"

namespace BeBoB {
namespace MAudio {
namespace Special {

class Device;

class Mixer : public Control::Container
{
public:
    Mixer(Device &dev);
    virtual ~Mixer() {};

    bool initialize(Device &dev);

    virtual std::string getName()
        {return "Mixer";};
    virtual bool setName(std::string n)
        {return false;};

protected:
    DECLARE_DEBUG_MODULE;

private:
    Device *m_dev;
};

class Selector
    : public Control::Discrete
{
public:
	Selector(Device& dev, unsigned int id);
    virtual ~Selector() {};

    virtual bool setValue(int idx, int v);
    virtual int getValue(int idx);

    virtual bool setValue(int v)
        {return setValue(1, v);};
    virtual int getValue()
        {return getValue(1);};
    virtual int getMinimum()
        {return 1;};
    virtual int getMaximum()
        {return 4;};

private:
    uint64_t getOffset();
    Device   *m_dev;
    unsigned int    m_id;
};

class Volume
    : public Control::Continuous
{
public:
    Volume(Device& m_dev, unsigned int id);
    virtual ~Volume() {};

    virtual bool setValue(int idx, double v);
    virtual double getValue(int idx);

    virtual bool setValue(double v)
        {return setValue(1, v);};
    virtual double getValue()
        {return getValue(1);};
    virtual double getMinimum()
        {return 0x8000;};
    virtual double getMaximum()
        {return 0x0000;};

private:
    uint64_t getOffset();
    Device   *m_dev;
    unsigned int    m_id;
};

class LRBalance
    : public Control::Continuous
{
public:
	LRBalance(Device& dev, unsigned int id);
    virtual ~LRBalance() {};

    virtual bool setValue(int idx, double v);
    virtual double getValue(int idx);

    virtual bool setValue(double v)
        {return setValue(1, v);};
    virtual double getValue()
        {return getValue(1);};
    virtual double getMinimum()
        {return -32766;};
    virtual double getMaximum()
        {return 32768;};

private:
    uint64_t getOffset();
    Device   *m_dev;
    unsigned int    m_id;
};

class Processing
    : public Control::Continuous
{
public:
	Processing(Device& dev, unsigned int id);
    virtual ~Processing() {};

    virtual bool setValue(int idx, double v);
    virtual double getValue(int idx);

    virtual bool setValue(double v)
        {return setValue(1, v);};
    virtual double getValue()
        {return getValue(1);};
    virtual double getMinimum()
        {return 0x8000;};
    virtual double getMaximum()
        {return 0x0000;};

private:
    uint64_t getOffset(int iPlugNum);
    Device   *m_dev;
    unsigned int    m_id;
};

} // namespace Special
} // namespace MAudio
} // namespace BeBoB

#endif /* BEBOB_MAUDIO_SPECIAL_MIXER_H */
