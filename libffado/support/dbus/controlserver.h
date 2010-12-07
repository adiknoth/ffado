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

#ifndef CONTROLSERVER_H
#define CONTROLSERVER_H

#include "debugmodule/debugmodule.h"

#include <dbus-c++/dbus.h>

#include "controlserver-glue.h"

#include "libcontrol/BasicElements.h"
#include "libieee1394/configrom.h"
#include "libutil/Mutex.h"

namespace Control {
    class MatrixMixer;
    class CrossbarRouter;
};

namespace DBusControl {

class Element;
class Container;

template< typename CalleePtr, typename MemFunPtr >
class MemberSignalFunctor0
    : public Control::SignalFunctor
{
public:
    MemberSignalFunctor0( const CalleePtr& pCallee,
            MemFunPtr pMemFun,
            int pSignalId)
        : Control::SignalFunctor( pSignalId )
        , m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
        {}

    virtual ~MemberSignalFunctor0()
        {}

    virtual void operator() ()
        {
            ( ( *m_pCallee ).*m_pMemFun )();
        }
    virtual void operator() (int) {}
private:
    CalleePtr  m_pCallee;
    MemFunPtr  m_pMemFun;
};

template< typename CalleePtr, typename MemFunPtr >
class MemberSignalFunctor1
    : public Control::SignalFunctor
{
public:
    MemberSignalFunctor1( const CalleePtr& pCallee,
            MemFunPtr pMemFun,
            int pSignalId)
        : Control::SignalFunctor( pSignalId )
        , m_pCallee( pCallee )
        , m_pMemFun( pMemFun )
        {}

    virtual ~MemberSignalFunctor1()
        {}

    virtual void operator() () {}

    virtual void operator() (int value)
        {
            ( ( *m_pCallee ).*m_pMemFun )(value);
        }
private:
    CalleePtr  m_pCallee;
    MemFunPtr  m_pMemFun;
};

class Element
: public org::ffado::Control::Element::Element
, public DBus::IntrospectableAdaptor
, public DBus::ObjectAdaptor
{
friend class Container; // required to have container access other slave elements
public:

    Element( DBus::Connection& connection,
             std::string p, Element *,
             Control::Element &slave );

    DBus::UInt64 getId( );
    DBus::String getName( );
    DBus::String getLabel( );
    DBus::String getDescription( );

    DBus::Bool canChangeValue( );

    void setVerboseLevel( const DBus::Int32 &);
    DBus::Int32 getVerboseLevel();

protected:
    void Lock();
    void Unlock();
    bool isLocked();
    Util::Mutex* getLock();

    Element *           m_Parent;
    Control::Element &  m_Slave;
private:
    Util::Mutex*        m_UpdateLock;
protected:
    DECLARE_DEBUG_MODULE;
};
typedef std::vector<Element *> ElementVector;
typedef std::vector<Element *>::iterator ElementVectorIterator;
typedef std::vector<Element *>::const_iterator ConstElementVectorIterator;

class Container
: public org::ffado::Control::Element::Container
, public DBusControl::Element
{
public:
    Container( DBus::Connection& connection,
                  std::string p, Element *,
                  Control::Container &slave );
    virtual ~Container();

    DBus::Int32 getNbElements( );
    DBus::String getElementName( const DBus::Int32& );

    void updated(int new_nb_elements);
    void destroyed();

    void setVerboseLevel( const DBus::Int32 &);
private:
    Element *createHandler(Element *, Control::Element& e);
    void updateTree();
    Element * findElementForControl(Control::Element *e);
    void removeElement(Element *e);

    Control::Container &        m_Slave;
    ElementVector               m_Children;
    Control::SignalFunctor *    m_updateFunctor;
};

class Continuous
: public org::ffado::Control::Element::Continuous
, public Element
{
public:
    Continuous( DBus::Connection& connection,
                  std::string p, Element *,
                  Control::Continuous &slave );
    
    DBus::Double setValue( const DBus::Double & value );
    DBus::Double getValue( );
    DBus::Double getMinimum( );
    DBus::Double getMaximum( );
    DBus::Double setValueIdx( const DBus::Int32 & idx,
                              const DBus::Double & value );
    DBus::Double getValueIdx( const DBus::Int32 & idx );

private:
    Control::Continuous &m_Slave;
};

class Discrete
: public org::ffado::Control::Element::Discrete
, public Element
{
public:
    Discrete( DBus::Connection& connection,
              std::string p, Element *,
              Control::Discrete &slave );
    
    DBus::Int32 setValue( const DBus::Int32 & value );
    DBus::Int32 getValue( );
    DBus::Int32 setValueIdx( const DBus::Int32 & idx,
                             const DBus::Int32 & value );
    DBus::Int32 getValueIdx( const DBus::Int32 & idx );

private:
    Control::Discrete &m_Slave;
};

class Text
: public org::ffado::Control::Element::Text
, public Element
{
public:
    Text( DBus::Connection& connection,
          std::string p, Element *,
          Control::Text &slave );

    DBus::String setValue( const DBus::String & value );
    DBus::String getValue( );

private:
    Control::Text &m_Slave;
};

class Register
: public org::ffado::Control::Element::Register
, public Element
{
public:
    Register( DBus::Connection& connection,
              std::string p, Element *,
              Control::Register &slave );
    
    DBus::UInt64 setValue( const DBus::UInt64 & addr, const DBus::UInt64 & value );
    DBus::UInt64 getValue( const DBus::UInt64 & addr );

private:
    Control::Register &m_Slave;
};

class Enum
: public org::ffado::Control::Element::Enum
, public Element
{
public:
    Enum( DBus::Connection& connection,
          std::string p, Element *,
          Control::Enum &slave );
    
    DBus::Int32 select( const DBus::Int32 & idx );
    DBus::Int32 selected( );
    DBus::Int32 count( );
    DBus::String getEnumLabel( const DBus::Int32 & idx );

private:
    Control::Enum &m_Slave;
};

class AttributeEnum
: public org::ffado::Control::Element::AttributeEnum
, public Element
{
public:
    AttributeEnum( DBus::Connection& connection,
                   std::string p, Element *,
                   Control::AttributeEnum &slave );
    
    DBus::Int32 select( const DBus::Int32 & idx );
    DBus::Int32 selected( );
    DBus::Int32 count( );
    DBus::Int32 attributeCount();
    DBus::String getEnumLabel( const DBus::Int32 & idx );
    DBus::String getAttributeValue( const DBus::Int32 & idx );
    DBus::String getAttributeName( const DBus::Int32 & idx );

private:
    Control::AttributeEnum &m_Slave;
};

// FIXME: to change this to a normal ConfigRom class name we have to
// put the 1394 config rom class into a separate namespace.
class ConfigRomX
: public org::ffado::Control::Element::ConfigRomX
, public Element
{
public:
    ConfigRomX( DBus::Connection& connection,
                  std::string p, Element *,
                  ConfigRom &slave );

    DBus::String getGUID( );
    DBus::String getVendorName( );
    DBus::String getModelName( );
    DBus::Int32 getVendorId( );
    DBus::Int32 getModelId( );
    DBus::Int32 getUnitVersion( );

private:
    ConfigRom &m_Slave;
};

class MatrixMixer
: public org::ffado::Control::Element::MatrixMixer
, public Element
{
public:
    MatrixMixer(  DBus::Connection& connection,
                  std::string p, Element *,
                  Control::MatrixMixer &slave );

    DBus::Int32 getRowCount( );
    DBus::Int32 getColCount( );

    DBus::Int32 canWrite( const DBus::Int32&, const DBus::Int32& );
    DBus::Double setValue( const DBus::Int32&, const DBus::Int32&, const DBus::Double& );
    DBus::Double getValue( const DBus::Int32&, const DBus::Int32& );

    DBus::Bool hasNames();
    DBus::String getRowName( const DBus::Int32& );
    DBus::String getColName( const DBus::Int32& );
    DBus::Bool setRowName( const DBus::Int32&, const DBus::String& );
    DBus::Bool setColName( const DBus::Int32&, const DBus::String& );

    DBus::Bool canConnect();
    std::vector<DBus::String> availableConnectionsForRow( const DBus::Int32& );
    std::vector<DBus::String> availableConnectionsForCol( const DBus::Int32& );
    DBus::Bool connectRowTo( const DBus::Int32&, const DBus::String& );
    DBus::Bool connectColTo( const DBus::Int32&, const DBus::String& );

private:
    Control::MatrixMixer &m_Slave;
};

class CrossbarRouter
: public org::ffado::Control::Element::CrossbarRouter
, public Element
{
public:
    CrossbarRouter(  DBus::Connection& connection,
                  std::string p, Element *,
                  Control::CrossbarRouter &slave );

    DBus::String getSourceName(const DBus::Int32 &);
    DBus::String getDestinationName(const DBus::Int32 &);
    DBus::Int32 getSourceIndex(const DBus::String &);
    DBus::Int32 getDestinationIndex(const DBus::String &);

    std::vector< DBus::String > getSourceNames();
    std::vector< DBus::String > getDestinationNames();

    std::vector< DBus::Struct<DBus::String, int> > getSources();
    std::vector< DBus::Struct<DBus::String, int> > getDestinations();

    std::vector< DBus::Int32 > getDestinationsForSource(const DBus::Int32 &);
    DBus::Int32 getSourceForDestination(const DBus::Int32 &);

    DBus::Bool  canConnect(const DBus::Int32 &source, const DBus::Int32 &dest);
    DBus::Bool  setConnectionState(const DBus::Int32 &source, const DBus::Int32 &dest, const DBus::Bool &enable);
    DBus::Bool  getConnectionState(const DBus::Int32 &source, const DBus::Int32 &dest);

    DBus::Bool  canConnectNamed(const DBus::String&, const DBus::String&);
    DBus::Bool  setConnectionStateNamed(const DBus::String&, const DBus::String&, const DBus::Bool &enable);
    DBus::Bool  getConnectionStateNamed(const DBus::String&, const DBus::String&);

    DBus::Bool  clearAllConnections();

    DBus::Int32 getNbSources();
    DBus::Int32 getNbDestinations();

    DBus::Bool  hasPeakMetering();
    DBus::Double getPeakValue(const DBus::Int32 &source, const DBus::Int32 &dest);

    std::vector< DBus::Struct<int, double> > getPeakValues();

    std::vector< DBus::Int32 > getConnectionMap();
    DBus::Int32 setConnectionMap(const std::vector< DBus::Int32 >&);

private:
    Control::CrossbarRouter &m_Slave;
};

class Boolean
: public org::ffado::Control::Element::Boolean
, public Element
{
public:
    Boolean( DBus::Connection& connection,
          std::string p, Element *,
          Control::Boolean &slave );
    
    DBus::Bool select( const DBus::Bool& value );
    DBus::Bool selected();
    DBus::String getBooleanLabel( const DBus::Bool& value );

private:
    Control::Boolean &m_Slave;
};
}

#endif // CONTROLSERVER_H
