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

#ifndef __FFAD0_BEBOB_MIXER__
#define __FFAD0_BEBOB_MIXER__

#include "src/debugmodule/debugmodule.h"

#include "libcontrol/BasicElements.h"

#include <vector>

namespace BeBoB {

class Device;
class FunctionBlock;
class FunctionBlockFeature;
class FunctionBlockSelector;
class FunctionBlockEnhancedMixer;

class Mixer 
    : public Control::Container
{
public:
    Mixer(Device &d);
    virtual ~Mixer();

    virtual std::string getName()
        { return "Mixer"; };
    virtual bool setName( std::string n )
        { return false; };

    template<typename FBType, typename MixerType> bool addElementForFunctionBlock(FBType& b);
    bool addElementForAllFunctionBlocks();

    // manipulation of the contained elements
    bool addElement(Control::Element *)
        {debugWarning("not allowed"); return false;};
    bool deleteElement(Control::Element *);
    bool clearElements();

    Device& getParent()
        {return m_device;};

protected:
    Device&            m_device;
protected:
    DECLARE_DEBUG_MODULE;
};

class MixerFBFeatureVolume
    : public Control::Continuous
{
public:
    MixerFBFeatureVolume(Mixer& parent, FunctionBlockFeature&);
    virtual ~MixerFBFeatureVolume();

    virtual bool setValue(double v);
    virtual double getValue();
    virtual bool setValue(int idx, double v);
    virtual double getValue(int idx);
    virtual double getMinimum();
    virtual double getMaximum();

private:
    Mixer&                  m_Parent;
    FunctionBlockFeature&   m_Slave;
};

class MixerFBFeatureLRBalance
    : public Control::Continuous
{
public:
    MixerFBFeatureLRBalance(Mixer& parent, FunctionBlockFeature&);
    virtual ~MixerFBFeatureLRBalance();

    virtual bool setValue(double v);
    virtual double getValue();
    virtual bool setValue(int idx, double v);
    virtual double getValue(int idx);
    virtual double getMinimum();
    virtual double getMaximum();

private:
    Mixer&                  m_Parent;
    FunctionBlockFeature&   m_Slave;
};

class EnhancedMixerFBFeature
    : public Control::Continuous
{
public:
    EnhancedMixerFBFeature(Mixer& parent, FunctionBlockEnhancedMixer&);
    virtual ~EnhancedMixerFBFeature();

    virtual bool setValue(double v);
    virtual double getValue();
    virtual bool setValue(int idx, double v)
        {return setValue(v);};
    virtual double getValue(int idx)
        {return getValue();};

    virtual double getMinimum() {return -32768;};
    virtual double getMaximum() {return 0;};

private:
    Mixer&                      m_Parent;
    FunctionBlockEnhancedMixer& m_Slave;
};

class MixerFBSelector
    : public Control::Discrete
{
public:
    MixerFBSelector(Mixer& parent, FunctionBlockSelector&);
    virtual ~MixerFBSelector();

    virtual bool setValue(int v);
    virtual int getValue();
    virtual bool setValue(int idx, int v)
        {return setValue(v);};
    virtual int getValue(int idx)
        {return getValue();};

    virtual int getMinimum() {return 0;};
    virtual int getMaximum() {return 0;};

private:
    Mixer&                  m_Parent;
    FunctionBlockSelector&  m_Slave;
};

} // end of namespace BeBoB

#endif /* __FFAD0_BEBOB_MIXER__ */


