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

class AvDevice;
class FunctionBlock;
class FunctionBlockFeature;
class FunctionBlockSelector;
class FunctionBlockEnhancedMixer;

class Mixer 
    : public Control::Container
{
public:
    Mixer(AvDevice &d);
    virtual ~Mixer();

    virtual std::string getName()
        { return "Mixer"; };
    virtual bool setName( std::string n )
        { return false; };

    bool addElementForFunctionBlock(FunctionBlock& b);
    bool addElementForAllFunctionBlocks();

    // manipulation of the contained elements
    bool addElement(Control::Element *)
        {debugWarning("not allowed"); return false;};
    bool deleteElement(Control::Element *);
    bool clearElements();

    AvDevice& getParent()
        {return m_device;};

protected:
    AvDevice&            m_device;
protected:
    DECLARE_DEBUG_MODULE;
};

class MixerFBFeature
    : public Control::Continuous
{
public:
    MixerFBFeature(Mixer& parent, FunctionBlockFeature&);
    
    virtual bool setValue(double v);
    virtual double getValue();
    
private:
    Mixer&                  m_Parent;
    FunctionBlockFeature&   m_Slave;
};

class EnhancedMixerFBFeature
    : public Control::Continuous
{
public:
    EnhancedMixerFBFeature(Mixer& parent, FunctionBlockEnhancedMixer&);
    
    virtual bool setValue(double v);
    virtual double getValue();
    
private:
    Mixer&                      m_Parent;
    FunctionBlockEnhancedMixer& m_Slave;
};

class MixerFBSelector
    : public Control::Discrete
{
public:
    MixerFBSelector(Mixer& parent, FunctionBlockSelector&);
    
    virtual bool setValue(int v);
    virtual int getValue();
    
private:
    Mixer&                  m_Parent;
    FunctionBlockSelector&  m_Slave;
};

} // end of namespace BeBoB

#endif /* __FFAD0_BEBOB_MIXER__ */


