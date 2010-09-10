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
#include "libcontrol/CrossbarRouter.h"
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
        m_UpdateLock = new Util::PosixMutex("CTLSVEL");
    } else {
        m_UpdateLock = NULL;
    }
    // set verbose level AFTER allocating the lock
    setVerboseLevel(m_Slave.getVerboseLevel());
}

void Element::setVerboseLevel( const int32_t &i)
{
    setDebugLevel(i);
    m_Slave.setVerboseLevel(i);
    if(m_UpdateLock) m_UpdateLock->setVerboseLevel(i);
}

int32_t Element::getVerboseLevel()
{
    return getDebugLevel();
}

bool
Element::canChangeValue()
{
    return m_Slave.canChangeValue();
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

bool
Element::isLocked()
{
    if(m_Parent) {
        return m_Parent->isLocked();
    } else {
        return m_UpdateLock->isLocked();
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

uint64_t
Element::getId( )
{
    return m_Slave.getId();
}

std::string
Element::getName( )
{
    return std::string(m_Slave.getName());
}

std::string
Element::getLabel( )
{
    return std::string(m_Slave.getLabel());
}

std::string
Element::getDescription( )
{
    return std::string(m_Slave.getDescription());
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
    m_updateFunctor = new MemberSignalFunctor1< Container*,
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

    Destroyed(); //send dbus signal

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
Container::setVerboseLevel( const int32_t & i)
{
    Element::setVerboseLevel(i);
    for ( ElementVectorIterator it = m_Children.begin();
      it != m_Children.end();
      ++it )
    {
        (*it)->setVerboseLevel(i);
    }
}

int32_t
Container::getNbElements( ) {
    return m_Slave.countElements();
}

std::string
Container::getElementName( const int32_t& i ) {
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
    // send a pre update signal
    PreUpdate();
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
    // send a post update signal
    PostUpdate();
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
    try {
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
        
        if (dynamic_cast<Control::Boolean *>(&e) != NULL) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Boolean\n");
            
            return new Boolean(conn(), std::string(path()+"/"+e.getName()),
                parent, *dynamic_cast<Control::Boolean *>(&e));
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
        
        if (dynamic_cast<Control::CrossbarRouter *>(&e) != NULL) {
            debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::CrossbarRouter\n");
            
            return new CrossbarRouter(conn(), std::string(path()+"/"+e.getName()),
                parent, *dynamic_cast<Control::CrossbarRouter *>(&e));
        }
        
        debugOutput( DEBUG_LEVEL_VERBOSE, "Source is a Control::Element\n");
        return new Element(conn(), std::string(path()+"/"+e.getName()), parent, e);
    } catch (...) {
        debugWarning("Could not register %s\n", std::string(path()+"/"+e.getName()).c_str());
        if(e.isControlLocked()) {
            e.unlockControl();
        }
        if(isLocked()) {
            Unlock();
        }
        return NULL;
    };
}

// --- Continuous

Continuous::Continuous( DBus::Connection& connection, std::string p, Element* parent, Control::Continuous &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Continuous on '%s'\n",
                 path().c_str() );
}

double
Continuous::setValue( const double& value )
{
    m_Slave.setValue(value);
/*    
    SleepRelativeUsec(1000*500);
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%lf) => %lf\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

double
Continuous::getValue(  )
{
    double val = m_Slave.getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %lf\n", val );
    return val;
}

double
Continuous::setValueIdx( const int32_t & idx, const double& value )
{
    m_Slave.setValue(idx, value);
/*    
    SleepRelativeUsec(1000*500);
    
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%lf) => %lf\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

double
Continuous::getValueIdx( const int32_t & idx )
{
    double val = m_Slave.getValue(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue(%d) => %lf\n", idx, val );
    return val;
}

double
Continuous::getMinimum()
{
    double val = m_Slave.getMinimum();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getMinimum() => %lf\n", val );
    return val;
}

double
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

int32_t
Discrete::setValue( const int32_t& value )
{
    m_Slave.setValue(value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

int32_t
Discrete::getValue()
{
    int32_t val = m_Slave.getValue();
    debugOutput( DEBUG_LEVEL_VERBOSE, "getValue() => %d\n", val );
    return val;
}

int32_t
Discrete::setValueIdx( const int32_t& idx, const int32_t& value )
{
    m_Slave.setValue(idx, value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

int32_t
Discrete::getValueIdx( const int32_t& idx )
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

std::string
Text::setValue( const std::string& value )
{
    m_Slave.setValue(value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

std::string
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

uint64_t
Register::setValue( const uint64_t& addr, const uint64_t& value )
{
    m_Slave.setValue(addr, value);
    
/*    SleepRelativeUsec(1000*500);
    debugOutput( DEBUG_LEVEL_VERBOSE, "setValue(%d) => %d\n", value, m_Slave.getValue() );
    
    return m_Slave.getValue();*/
    return value;
}

uint64_t
Register::getValue( const uint64_t& addr )
{
    uint64_t val = m_Slave.getValue(addr);
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

int32_t
Enum::select( const int32_t& idx )
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "select(%d)\n", idx );
    return  m_Slave.select(idx);
}

int32_t
Enum::selected()
{
    int retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

int32_t
Enum::count()
{
    int retval = m_Slave.count();
    debugOutput( DEBUG_LEVEL_VERBOSE, "count() => %d\n", retval );
    return retval;
}

std::string
Enum::getEnumLabel( const int32_t & idx )
{
    std::string retval = m_Slave.getEnumLabel(idx);
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "getEnumLabel(%d) => %s\n", idx, retval.c_str() );
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

int32_t
AttributeEnum::select( const int32_t& idx )
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "select(%d)\n", idx );
    return  m_Slave.select(idx);
}

int32_t
AttributeEnum::selected()
{
    int retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

int32_t
AttributeEnum::count()
{
    int retval = m_Slave.count();
    debugOutput( DEBUG_LEVEL_VERBOSE, "count() => %d\n", retval );
    return retval;
}

int32_t
AttributeEnum::attributeCount()
{
    int retval = m_Slave.attributeCount();
    debugOutput( DEBUG_LEVEL_VERBOSE, "attributeCount() => %d\n", retval );
    return retval;
}

std::string
AttributeEnum::getEnumLabel( const int32_t & idx )
{
    std::string retval = m_Slave.getEnumLabel(idx);
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "getEnumLabel(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

std::string
AttributeEnum::getAttributeValue( const int32_t & idx )
{
    std::string retval = m_Slave.getAttributeValue(idx);
    debugOutput( DEBUG_LEVEL_VERBOSE, "getAttributeValue(%d) => %s\n", idx, retval.c_str() );
    return retval;
}

std::string
AttributeEnum::getAttributeName( const int32_t & idx )
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

std::string
ConfigRomX::getGUID( )
{
    return m_Slave.getGuidString();
}

std::string
ConfigRomX::getVendorName( )
{
    return m_Slave.getVendorName();
}

std::string
ConfigRomX::getModelName( )
{
    return m_Slave.getModelName();
}

int32_t
ConfigRomX::getVendorId( )
{
    return m_Slave.getNodeVendorId();
}

int32_t
ConfigRomX::getModelId( )
{
    return m_Slave.getModelId();
}

int32_t
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

int32_t
MatrixMixer::getRowCount( ) {
    return m_Slave.getRowCount();
}

int32_t
MatrixMixer::getColCount( ) {
    return m_Slave.getColCount();
}

int32_t
MatrixMixer::canWrite( const int32_t& row, const int32_t& col) {
    return m_Slave.canWrite(row,col);
}

double
MatrixMixer::setValue( const int32_t& row, const int32_t& col, const double& val ) {
    return m_Slave.setValue(row,col,val);
}

double
MatrixMixer::getValue( const int32_t& row, const int32_t& col) {
    return m_Slave.getValue(row,col);
}

bool
MatrixMixer::hasNames() {
    return m_Slave.hasNames();
}
std::string
MatrixMixer::getRowName( const int32_t& row) {
    return m_Slave.getRowName(row);
}
std::string
MatrixMixer::getColName( const int32_t& col) {
    return m_Slave.getColName(col);
}
bool
MatrixMixer::setRowName( const int32_t& row, const std::string& name) {
    return m_Slave.setRowName(row, name);
}
bool
MatrixMixer::setColName( const int32_t& col, const std::string& name) {
    return m_Slave.setColName(col, name);
}

bool
MatrixMixer::canConnect() {
    return m_Slave.canConnect();
}
std::vector<std::string>
MatrixMixer::availableConnectionsForRow( const int32_t& row) {
    return m_Slave.availableConnectionsForRow(row);
}
std::vector<std::string>
MatrixMixer::availableConnectionsForCol( const int32_t& col) {
    return m_Slave.availableConnectionsForCol(col);
}
bool
MatrixMixer::connectRowTo( const int32_t& row, const std::string& target) {
    return m_Slave.connectRowTo(row, target);
}
bool
MatrixMixer::connectColTo( const int32_t& col, const std::string& target) {
    return m_Slave.connectColTo(col, target);
}

// --- CrossbarRouter

CrossbarRouter::CrossbarRouter( DBus::Connection& connection, std::string p, Element* parent, Control::CrossbarRouter &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created CrossbarRouter on '%s'\n",
                 path().c_str() );
}

/*int32_t
CrossbarRouter::getSourceIndex(const std::string &name)
{
    return m_Slave.getSourceIndex(name);
}

int32_t
CrossbarRouter::getDestinationIndex(const std::string &name)
{
    return m_Slave.getDestinationIndex(name);
}*/

std::vector< std::string >
CrossbarRouter::getSourceNames()
{
    return m_Slave.getSourceNames();
}

std::vector< std::string >
CrossbarRouter::getDestinationNames()
{
    return m_Slave.getDestinationNames();
}

std::vector< std::string >
CrossbarRouter::getDestinationsForSource(const std::string &idx)
{
    return m_Slave.getDestinationsForSource(idx);
}

std::string
CrossbarRouter::getSourceForDestination(const std::string &idx)
{
    return m_Slave.getSourceForDestination(idx);
}

bool
CrossbarRouter::canConnect(const std::string &source, const std::string &dest)
{
    return m_Slave.canConnect(source, dest);
}

bool
CrossbarRouter::setConnectionState(const std::string &source, const std::string &dest, const bool &enable)
{
    return m_Slave.setConnectionState(source, dest, enable);
}

bool
CrossbarRouter::getConnectionState(const std::string &source, const std::string &dest)
{
    return m_Slave.getConnectionState(source, dest);
}

bool
CrossbarRouter::clearAllConnections()
{
    return m_Slave.clearAllConnections();
}

bool
CrossbarRouter::hasPeakMetering()
{
    return m_Slave.hasPeakMetering();
}

double
CrossbarRouter::getPeakValue(const std::string &dest)
{
    return m_Slave.getPeakValue(dest);
}
std::vector< DBus::Struct<std::string, double> >
CrossbarRouter::getPeakValues()
{
    std::map<std::string, double> peakvalues = m_Slave.getPeakValues();
    std::vector< DBus::Struct<std::string, double> > ret;
    for (std::map<std::string, double>::iterator it=peakvalues.begin(); it!=peakvalues.end(); ++it) {
        DBus::Struct<std::string, double> tmp;
        tmp._1 = it->first;
        tmp._2 = it->second;
        ret.push_back(tmp);
    }
    return ret;
    /*std::vector< DBus::Struct<int, double> > out;
    Control::CrossbarRouter::PeakValues values = m_Slave.getPeakValues();
    for ( unsigned int i=0; i<values.size(); ++i ) {
        DBus::Struct<int, double> tmp;
        tmp._1 = values[i].destination;
        tmp._2 = values[i].peakvalue;
        out.push_back(tmp);
    }
    return out;*/
}

// --- Boolean

Boolean::Boolean( DBus::Connection& connection, std::string p, Element* parent, Control::Boolean &slave)
: Element(connection, p, parent, slave)
, m_Slave(slave)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "Created Boolean on '%s'\n",
                 path().c_str() );
}

bool
Boolean::select( const bool& value )
{
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "select(%d)\n", value );
    return  m_Slave.select(value);
}

bool
Boolean::selected()
{
    bool retval = m_Slave.selected();
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "selected() => %d\n", retval );
    return retval;
}

std::string
Boolean::getBooleanLabel( const bool& value )
{
    std::string retval = m_Slave.getBooleanLabel(value);
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "getBooleanLabel(%d) => %s\n", value, retval.c_str() );
    return retval;
}


} // end of namespace Control
