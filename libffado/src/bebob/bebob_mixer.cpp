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

#include "bebob/bebob_mixer.h"

#include "bebob/bebob_avdevice.h"
#include "bebob/bebob_avdevice_subunit.h"

#include "libavc/audiosubunit/avc_function_block.h"
#include "libutil/cmd_serialize.h"

#include "libcontrol/BasicElements.h"

#include "libieee1394/ieee1394service.h"
#include "libieee1394/configrom.h"

#include <string>
#include <sstream>
#include <iostream>

#include <assert.h>

using namespace AVC;

namespace BeBoB {

template <class T>
bool from_string(T& t, 
                 const std::string& s, 
                 std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}

IMPL_DEBUG_MODULE( Mixer, Mixer, DEBUG_LEVEL_NORMAL );

Mixer::Mixer(AvDevice &d)
    : Control::Container(&d)
    , m_device(d)
{
    addElementForAllFunctionBlocks();
    if (!d.addElement(this)) {
        debugWarning("Could not add myself to Control::Container\n");
    }
}

Mixer::~Mixer() {
    
    debugOutput(DEBUG_LEVEL_VERBOSE,"Unregistering from Control::Container...\n");
    if (!m_device.deleteElement(this)) {
        debugWarning("Could not delete myself from Control::Container\n");
    }
    
    // delete all our elements since we should have created
    // them ourselves.
    for ( Control::ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        debugOutput(DEBUG_LEVEL_VERBOSE,"deleting %s...\n", (*it)->getName().c_str());
        delete *it;
    }
}

bool
Mixer::deleteElement(Element *e)
{
    if (Control::Container::deleteElement(e)) {
        delete e;
        return true;
    } else return false;
}

bool
Mixer::clearElements() {
    // delete all our elements since we should have created
    // them ourselves.
    for ( Control::ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        delete *it;
    }
    m_Children.clear();
    
    return true;
}

template<typename FBType, typename MixerType> 
bool
Mixer::addElementForFunctionBlock(FBType& b)
{
    Control::Element *e = new MixerType(*this, b);
    if (!e) {
        debugError("Control element creation failed\n");
        return false;
    }

    e->setVerboseLevel(getDebugLevel());
    return Control::Container::addElement(e);
}

bool
Mixer::addElementForAllFunctionBlocks() {
    debugOutput(DEBUG_LEVEL_VERBOSE,"Adding elements for functionblocks...\n");

    bool retval = true;

    BeBoB::SubunitAudio *asu =
        dynamic_cast<BeBoB::SubunitAudio *>(m_device.getAudioSubunit(0));

    if(asu == NULL) {
        debugWarning("No BeBoB audio subunit found\n");
        return false;
    }
    FunctionBlockVector functions=asu->getFunctionBlocks();

    for ( FunctionBlockVector::iterator it = functions.begin();
          it != functions.end();
          ++it )
    {
        FunctionBlock *pfb = *it;
        FunctionBlockSelector *ps;
        FunctionBlockFeature *pf;
        FunctionBlockEnhancedMixer *pm;

        if ((ps = dynamic_cast<FunctionBlockSelector *>(pfb))) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "FB is a SelectorFunctionBlock\n");
            retval = addElementForFunctionBlock<FunctionBlockSelector, MixerFBSelector>(*ps);
        } else if ((pf = dynamic_cast<FunctionBlockFeature *>(pfb))) {
            // We might should check if really both feature function
            // blocks exists and only then announce them. The FA-101,
            // FA-66 and the Ref BCO Audio5 do have both.
            debugOutput( DEBUG_LEVEL_VERBOSE, "FB is a FeatureFunctionBlock\n");
            retval  = addElementForFunctionBlock<FunctionBlockFeature, MixerFBFeatureVolume>(*pf);
            retval &= addElementForFunctionBlock<FunctionBlockFeature, MixerFBFeatureLRBalance>(*pf);
        } else if ((pm = dynamic_cast<FunctionBlockEnhancedMixer *>(pfb))) {
            // All BeBoB devices lock the mixer feature function
            // block. The AV/C model for this mixer is just to
            // complex and the BridgeCo guys decided to use the above
            // function feature blocks (level and balance) to achive
            // the same.
            debugOutput( DEBUG_LEVEL_VERBOSE, "FB is a FunctionBlockEnhancedMixer\n");
            retval = addElementForFunctionBlock<FunctionBlockEnhancedMixer, EnhancedMixerFBFeature>(*pm);
        }

        if (!retval) {
            std::ostringstream ostrm;
            ostrm << (*it)->getName() << " " << (int)((*it)->getId());
            
            debugWarning("Failed to add element for function block %s\n",
                ostrm.str().c_str());
        };
    }
    return retval;
}

// --- element implementation classes

MixerFBFeatureVolume::MixerFBFeatureVolume(Mixer& parent, FunctionBlockFeature& s)
: Control::Continuous(&parent)
, m_Parent(parent) 
, m_Slave(s)
{
    std::ostringstream ostrm;
    ostrm << s.getName() << "_Volume_" << (int)(s.getId());
    
    Control::Continuous::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Label for " << s.getName() << "_Volume " << (int)(s.getId());
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Description for " << s.getName() << "_Volume " << (int)(s.getId());
    setDescription(ostrm.str());
}

MixerFBFeatureVolume::~MixerFBFeatureVolume()
{
}

bool
MixerFBFeatureVolume::setValue(double v)
{
    return setValue(0, v);
}

bool
MixerFBFeatureVolume::setValue(int idx, double v)
{
    int volume=(int)v;
    debugOutput(DEBUG_LEVEL_NORMAL,"Set feature volume %d to %d...\n",
        m_Slave.getId(), volume);
    return m_Parent.getParent().setFeatureFBVolumeCurrent(m_Slave.getId(), idx, volume);
}

double
MixerFBFeatureVolume::getValue()
{
    return getValue(0);
}

double
MixerFBFeatureVolume::getValue(int idx)
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature volume %d...\n",
        m_Slave.getId());

    return m_Parent.getParent().getFeatureFBVolumeCurrent(m_Slave.getId(), idx);
}

double
MixerFBFeatureVolume::getMinimum()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature minimum volume %d...\n",
        m_Slave.getId());

    return m_Parent.getParent().getFeatureFBVolumeMinimum(m_Slave.getId(), 0);
}

double
MixerFBFeatureVolume::getMaximum()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature maximum volume %d...\n",
        m_Slave.getId());

    return m_Parent.getParent().getFeatureFBVolumeMaximum(m_Slave.getId(), 0);
}

// --- element implementation classes

MixerFBFeatureLRBalance::MixerFBFeatureLRBalance(Mixer& parent, FunctionBlockFeature& s)
: Control::Continuous(&parent)
, m_Parent(parent) 
, m_Slave(s)
{
    std::ostringstream ostrm;
    ostrm << s.getName() << "_LRBalance_" << (int)(s.getId());
    
    Control::Continuous::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Label for " << s.getName() << "_LRBalance " << (int)(s.getId());
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Description for " << s.getName() << "_LRBalance " << (int)(s.getId());
    setDescription(ostrm.str());
}

MixerFBFeatureLRBalance::~MixerFBFeatureLRBalance()
{
}

bool
MixerFBFeatureLRBalance::setValue(double v)
{
    return setValue(0, v);
}

bool
MixerFBFeatureLRBalance::setValue(int idx, double v)
{
    int volume=(int)v;
    debugOutput(DEBUG_LEVEL_NORMAL,"Set feature balance %d to %d...\n",
        m_Slave.getId(), volume);
    return m_Parent.getParent().setFeatureFBLRBalanceCurrent(m_Slave.getId(), idx, volume);
}

double
MixerFBFeatureLRBalance::getValue()
{
    return getValue(0);
}

double
MixerFBFeatureLRBalance::getValue(int idx)
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature balance %d...\n",
        m_Slave.getId());

    return m_Parent.getParent().getFeatureFBLRBalanceCurrent(m_Slave.getId(), idx);
}

double
MixerFBFeatureLRBalance::getMinimum()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature balance volume %d...\n",
        m_Slave.getId());

    return m_Parent.getParent().getFeatureFBLRBalanceMinimum(m_Slave.getId(), 0);
}

double
MixerFBFeatureLRBalance::getMaximum()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature maximum balance %d...\n",
        m_Slave.getId());

    return m_Parent.getParent().getFeatureFBLRBalanceMaximum(m_Slave.getId(), 0);
}

// --- element implementation classes

EnhancedMixerFBFeature::EnhancedMixerFBFeature(Mixer& parent, FunctionBlockEnhancedMixer& s)
: Control::Continuous(&parent)
, m_Parent(parent) 
, m_Slave(s)
{
    std::ostringstream ostrm;
    ostrm << s.getName() << "_" << (int)(s.getId());
    
    Control::Continuous::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Label for " << s.getName() << " " << (int)(s.getId());
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Description for " << s.getName() << " " << (int)(s.getId());
    setDescription(ostrm.str());
}

EnhancedMixerFBFeature::~EnhancedMixerFBFeature()
{
}

bool
EnhancedMixerFBFeature::setValue(double v)
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Set feature volume %d to %d...\n",
                m_Slave.getId(), (int)v);

    return true;
}

double
EnhancedMixerFBFeature::getValue()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get feature volume %d...\n",
                m_Slave.getId());

    return 0;
}

// --- element implementation classes

MixerFBSelector::MixerFBSelector(Mixer& parent, FunctionBlockSelector& s)
: Control::Discrete(&parent)
, m_Parent(parent) 
, m_Slave(s)
{
    std::ostringstream ostrm;
    ostrm << s.getName() << "_" << (int)(s.getId());
    
    Control::Discrete::setName(ostrm.str());
    
    ostrm.str("");
    ostrm << "Label for " << s.getName() << " " << (int)(s.getId());
    setLabel(ostrm.str());
    
    ostrm.str("");
    ostrm << "Description for " << s.getName() << " " << (int)(s.getId());
    setDescription(ostrm.str());
}

MixerFBSelector::~MixerFBSelector()
{
}

bool
MixerFBSelector::setValue(int v)
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Set selector %d to %d...\n",
        m_Slave.getId(), v);
    return m_Parent.getParent().setSelectorFBValue(m_Slave.getId(), v);
}

int
MixerFBSelector::getValue()
{
    debugOutput(DEBUG_LEVEL_NORMAL,"Get selector %d...\n",
        m_Slave.getId());
    return m_Parent.getParent().getSelectorFBValue(m_Slave.getId());
}


} // end of namespace BeBoB
