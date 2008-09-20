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

#include "controlserver.h"
#include "libcontrol/Element.h"
#include "libcontrol/BasicElements.h"
#include "libcontrol/MatrixMixer.h"
#include "libutil/Time.h"
#include "libutil/PosixMutex.h"

namespace DBusControl {

IMPL_DEBUG_MODULE( Element, Element, DEBUG_LEVEL_NORMAL );

// --- Element
Element::Element( DBus::Connection& connection, std::string p, Element* parent, Control::Element &slave)
: DBus::ObjectAdaptor(connection, p)
, m_Parent(parent)
, m_Slave(slave)
, m_UpdateLock( NULL )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Element on '%s'\n",
                 path().c_str() );
    // allocate a lock
    if(parent == NULL) {
        m_UpdateLock = new Util::PosixMutex();
    } else {
        m_UpdateLock = NULL;
    }
    // set verbose level AFTER allocating the lock
    setVerboseLevel(m_Slave.getVerboseLevel());
}

void Element::setVerboseLevel( const DBus::Int32 &i)
{
    setDebugLevel(i);
    m_Slave.setVerboseLevel(i);
    if(m_UpdateLock) m_UpdateLock->setVerboseLevel(i);
}

DBus::Int32 Element::getVerboseLevel()
{
    return getDebugLevel();
}

void
Element::Lock()
{
    if(m_Parent) {
        m_Parent->Lock();
    } else {
        m_UpdateLock->Lock();
    }
}

void
Element::Unlock()
{
    if(m_Parent) {
        m_Parent->Unlock();
    } else {
        m_UpdateLock->Unlock();
    }
}

Util::Mutex*
Element::getLock()
{
    if(m_Parent) {
        return m_Parent->getLock();
    } else {
        return m_UpdateLock;
    }
}

DBus::UInt64
Element::getId( )
{
    return m_Slave.getId();
}

DBus::String
Element::getName( )
{
    return DBus::String(m_Slave.getName());
}

DBus::String
Element::getLabel( )
{
    return DBus::String(m_Slave.getLabel());
}

DBus::String
Element::getDescription( )
{
    return DBus::String(m_Slave.getDescription());
}

// --- Container
Container::Container( DBus::Connection& connection, std::string p, Element* parent, Control::Container &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Container on '%s'\n",
                 path().c_str() );

    setDebugLevel(slave.getVerboseLevel());

    // register an update signal handler
    m_updateFunctor = new MemberSignalFunctor< Container*,
                      void (Container::*)(int) >
                      ( this, &Container::updated, (int)Control::Container::eS_Updated );
    if(m_updateFunctor) {
        if(!slave.addSignalHandler(m_updateFunctor)) {
            debugWarning("Could not add update signal functor\n");
        }
    } else {
        debugWarning("Could not create update signal functor\n");
    }

    // build the initial tree
    m_Slave = slave;
    updateTree();
}

Container::~Container() {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Deleting Container on '%s'\n",
                 path().c_str() );
    if(m_updateFunctor) {
        if(!m_Slave.remSignalHandler(m_updateFunctor)) {
            debugWarning("Could not remove update signal functor\n");
        }
    }
    delete m_updateFunctor;

    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        delete (*it);
    }
}

void
Container::setVerboseLevel( const DBus::Int32 & i)
{
    Element::setVerboseLevel(i);
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        (*it)->setVerboseLevel(i);
    }
}

DBus::Int32
Container::getNbElements( ) {
    return m_Slave.countElements();
}

DBus::String
Container::getElementName( const DBus::Int32& i ) {
    int nbElements=m_Slave.countElements();
    if (i<nbElements) {
        m_Slave.lockControl();
        const Control::ElementVector elements = m_Slave.getElementVector();
        Control::Element *e = elements.at(i);
        std::string name;
        if(e) name = e->getName();
        m_Slave.unlockControl();
        return name;
    } else return "";
}
//     Util::MutexLockHelper lock(*m_access_lock);

// NOTE: call with tree locked
void
Container::updateTree()
{
    bool something_changed = false;
    debugOutput( DEBUG_LEVEL_VERBOSE, "Updating tree...\n");
    debugOutput( DEBUG_LEVEL_VERBOSE, "Add handlers for elements...\n");
    // add handlers for the slaves that don't have one yet
    const Control::ElementVector elements = m_Slave.getElementVector();
    for ( Control::ConstElementVectorIterator it = elements.begin();
      it != elements.end();
      ++it )
    {
        Element *e = findElementForControl((*it));
        if(e == NULL) { // element not in tree
            e = createHandler(this, *(*it));
            if (e) {
                e->setVerboseLevel(getDebugLevel());
                m_Children.push_back(e);
                debugOutput( DEBUG_LEVEL_VERBOSE, "Created handler %p for Control::Element %s...\n",
                            e, (*it)->getName().c_str());
                something_changed = true;
            } else {
                debugWarning("Failed to create handler for Control::Element %s\n",
                    (*it)->getName().c_str());
            }
        } else {
            // element already present
            debugOutput( DEBUG_LEVEL_VERBOSE, "Already have handler (%p) for Control::Element %s...\n",
                         e, (*it)->getName().c_str());
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Remove handlers without element...\n");
    std::vector<Element *> to_remove;
    // remove handlers that don't have a slave anymore
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        Element *e = *it;
        bool found = false;
        for ( Control::ConstElementVectorIterator it2 = elements.begin();
              it2 != elements.end();
              ++it2 )
        {
            if(&(e)->m_Slave == *it2) {
                found = true;
                debugOutput( DEBUG_LEVEL_VERBOSE, "Slave for handler %p at %s is present: Control::Element %s...\n",
                            e, e->path().c_str(), (*it)->getName().c_str());
                break;
            }
        }

        if (!found) {
            debugOutput(DEBUG_LEVEL_VERBOSE, 
                        "going to remove handler %p on path %s since slave is gone\n",
                        e, e->path().c_str());
            // can't remove while iterating
            to_remove.push_back(e);
            something_changed = true;
        }
    }
    // do the actual remove
    while(to_remove.size()) {
        Element * e = *(to_remove.begin());
        removeElement(e);
        to_remove.erase(to_remove.begin());
    }

    if(something_changed) {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "send dbus signal for path %s since something changed\n",
                    path().c_str());
        // send a dbus signal
        Updated();
    }
}

void
Container::removeElement(Element *e)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "removing handler %p on path %s\n",
                e, path().c_str());
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        if(*it == e) {
            m_Children.erase(it);
            delete e;
            return;
        }
    }
    debugError("BUG: Element %p not found!\n", e);
}

// NOTE: call with access lock held!
Element *
Container::findElementForControl(Control::Element *e)
{
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        if(&(*it)->m_Slave == e) return (*it);
    }
    return NULL;
}

void
Container::updated(int new_nb_elements)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Got updated signal, new count='%d'\n",
                 new_nb_elements );
    // we lock the tree first
    Lock();

    // also lock the slave tree
    m_Slave.lockControl();

    // update our tree
    updateTree();

    // now unlock the slave tree
    m_Slave.unlockControl();

    // and unlock the access
    Unlock();
}

/**
 * \brief create a correct DBusControl counterpart for a given Control::Element
 */
Element *
Container::createHandler(Element *parent, Control::Element& e) {
    debugOutput( DEBUG_LEVEL_VERBOSE, "Creating handler for '%s'\n",
                 e.getName().c_str() );
                 
    if (dynamic_cast<Control::Container *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Container\n");
        
        return new Container(conn(), std::string(path()+"/"+e.getName()), 
            parent, *dynamic_cast<Control::Container *>(&e));
    }
    
    if (dynamic_cast<Control::Continuous *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Continuous\n");
        
        return new Continuous(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::Continuous *>(&e));
    }
    
    if (dynamic_cast<Control::Discrete *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Discrete\n");
        
        return new Discrete(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::Discrete *>(&e));
    }
    
    if (dynamic_cast<Control::Text *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Text\n");
        
        return new Text(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::Text *>(&e));
    }

    if (dynamic_cast<Control::Register *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Register\n");
        
        return new Register(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::Register *>(&e));
    }

    // note that we have to check this before checking the Enum,
    // since Enum is a base class
    if (dynamic_cast<Control::AttributeEnum *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::AttributeEnum\n");
        
        return new AttributeEnum(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::AttributeEnum *>(&e));
    }
    
    if (dynamic_cast<Control::Enum *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Enum\n");
        
        return new Enum(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::Enum *>(&e));
    }
    
    if (dynamic_cast<ConfigRom *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a ConfigRom\n");
        
        return new ConfigRomX(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<ConfigRom *>(&e));
    }
    
    if (dynamic_cast<Control::MatrixMixer *>(&e) != NULL) {
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::MatrixMixer\n");
        
        return new MatrixMixer(conn(), std::string(path()+"/"+e.getName()),
            parent, *dynamic_cast<Control::MatrixMixer *>(&e));
    }
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Element\n");
    return new Element(conn(), std::string(path()+"/"+e.getName()), parent, e);
}

// --- Continuous

Continuous::Continuous( DBus::Connection& connection, std::string p, Element* parent, Control::Continuous &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Continuous on '%s'\n",
                 path().c_str() );
}

DBus::Double
Continuous::setValue( const DBus::Double& value )
{
    m_Slave.setValue(value);
/*    
    SleepRelativeUsec(1000*500);
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%lf) => %lf\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Double
Continuous::getValue(  )
{
    double val = m_Slave.getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %lf\n", val );
    return val;
}

DBus::Double
Continuous::setValueIdx( const DBus::Int32 & idx, const DBus::Double& value )
{
    m_Slave.setValue(idx, value);
/*    
    SleepRelativeUsec(1000*500);
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%lf) => %lf\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Double
Continuous::getValueIdx( const DBus::Int32 & idx )
{
    double val = m_Slave.getValue(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue(%d) => %lf\n", idx, val );
    return val;
}

DBus::Double
Continuous::getMinimum()
{
    double val = m_Slave.getMinimum();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getMinimum() => %lf\n", val );
    return val;
}

DBus::Double
Continuous::getMaximum()
{
    double val = m_Slave.getMaximum();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getMaximum() => %lf\n", val );
    return val;
}

// --- Discrete

Discrete::Discrete( DBus::Connection& connection, std::string p, Element* parent, Control::Discrete &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Discrete on '%s'\n",
                 path().c_str() );
}

DBus::Int32
Discrete::setValue( const DBus::Int32& value )
{
    m_Slave.setValue(value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Int32
Discrete::getValue()
{
    int32_t val = m_Slave.getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %d\n", val );
    return val;
}

DBus::Int32
Discrete::setValueIdx( const DBus::Int32& idx, const DBus::Int32& value )
{
    m_Slave.setValue(idx, value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::Int32
Discrete::getValueIdx( const DBus::Int32& idx )
{
    int32_t val = m_Slave.getValue(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue(%d) => %d\n", idx, val );
    return val;
}

// --- Text

Text::Text( DBus::Connection& connection, std::string p, Element* parent, Control::Text &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Text on '%s'\n",
                 path().c_str() );
}

DBus::String
Text::setValue( const DBus::String& value )
{
    m_Slave.setValue(value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::String
Text::getValue()
{
    std::string val = m_Slave.getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %s\n", val.c_str() );
    return val;
}

// --- Register

Register::Register( DBus::Connection& connection, std::string p, Element* parent, Control::Register &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Register on '%s'\n",
                 path().c_str() );
}

DBus::UInt64
Register::setValue( const DBus::UInt64& addr, const DBus::UInt64& value )
{
    m_Slave.setValue(addr, value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

DBus::UInt64
Register::getValue( const DBus::UInt64& addr )
{
    DBus::UInt64 val = m_Slave.getValue(addr);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue(%lld) => %lld\n", addr, val );
    return val;
}

// --- Enum

Enum::Enum( DBus::Connection& connection, std::string p, Element* parent, Control::Enum &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Enum on '%s'\n",
                 path().c_str() );
}

DBus::Int32
Enum::select( const DBus::Int32& idx )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "select(%d)\n", idx );
    return  m_Slave.select(idx);
}

DBus::Int32
Enum::selected()
{
    int retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

DBus::Int32
Enum::count()
{
    int retval = m_Slave.count();
    debugOutput( DEBUG_LEVEL_VERBOSE, "count() => %d\n", retval );
    return retval;
}

DBus::String
Enum::getEnumLabel( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getEnumLabel(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getEnumLabel(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

// --- AttributeEnum
AttributeEnum::AttributeEnum( DBus::Connection& connection, std::string p, Element* parent, Control::AttributeEnum &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Enum on '%s'\n",
                 path().c_str() );
}

DBus::Int32
AttributeEnum::select( const DBus::Int32& idx )
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "select(%d)\n", idx );
    return  m_Slave.select(idx);
}

DBus::Int32
AttributeEnum::selected()
{
    int retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

DBus::Int32
AttributeEnum::count()
{
    int retval = m_Slave.count();
    debugOutput( DEBUG_LEVEL_VERBOSE, "count() => %d\n", retval );
    return retval;
}

DBus::Int32
AttributeEnum::attributeCount()
{
    int retval = m_Slave.attributeCount();
    debugOutput( DEBUG_LEVEL_VERBOSE, "attributeCount() => %d\n", retval );
    return retval;
}

DBus::String
AttributeEnum::getEnumLabel( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getEnumLabel(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getEnumLabel(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

DBus::String
AttributeEnum::getAttributeValue( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getAttributeValue(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getAttributeValue(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

DBus::String
AttributeEnum::getAttributeName( const DBus::Int32 & idx )
{
    std::string retval = m_Slave.getAttributeName(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getAttributeName(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

// --- ConfigRom

ConfigRomX::ConfigRomX( DBus::Connection& connection, std::string p, Element* parent, ConfigRom &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created ConfigRomX on '%s'\n",
                 path().c_str() );
}

DBus::String
ConfigRomX::getGUID( )
{
    return m_Slave.getGuidString();
}

DBus::String
ConfigRomX::getVendorName( )
{
    return m_Slave.getVendorName();
}

DBus::String
ConfigRomX::getModelName( )
{
    return m_Slave.getModelName();
}

DBus::Int32
ConfigRomX::getVendorId( )
{
    return m_Slave.getNodeVendorId();
}

DBus::Int32
ConfigRomX::getModelId( )
{
    return m_Slave.getModelId();
}

DBus::Int32
ConfigRomX::getUnitVersion( )
{
    return m_Slave.getUnitVersion();
}

// --- MatrixMixer

MatrixMixer::MatrixMixer( DBus::Connection& connection, std::string p, Element* parent, Control::MatrixMixer &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created MatrixMixer on '%s'\n",
                 path().c_str() );
}

DBus::String
MatrixMixer::getRowName( const DBus::Int32& row) {
    return m_Slave.getRowName(row);
}

DBus::String
MatrixMixer::getColName( const DBus::Int32& col) {
    return m_Slave.getColName(col);
}

DBus::Int32
MatrixMixer::canWrite( const DBus::Int32& row, const DBus::Int32& col) {
    return m_Slave.canWrite(row,col);
}

DBus::Double
MatrixMixer::setValue( const DBus::Int32& row, const DBus::Int32& col, const DBus::Double& val ) {
    return m_Slave.setValue(row,col,val);
}

DBus::Double
MatrixMixer::getValue( const DBus::Int32& row, const DBus::Int32& col) {
    return m_Slave.getValue(row,col);
}

DBus::Int32
MatrixMixer::getRowCount( ) {
    return m_Slave.getRowCount();
}

DBus::Int32
MatrixMixer::getColCount( ) {
    return m_Slave.getColCount();
}

} // end of namespace Control
