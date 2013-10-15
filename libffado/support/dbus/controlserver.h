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
: public org::ffado::Control::Element::Element_adaptor
, public DBus::IntrospectableAdaptor
, public DBus::ObjectAdaptor
{
friend class Container; // required to have container access other slave elements
public:

    Element( DBus::Connection& connection,
             std::string p, Element *,
             Control::Element &slave );

    uint64_t getId( );
    std::string getName( );
    std::string getLabel( );
    std::string getDescription( );

    bool canChangeValue( );

    void setVerboseLevel( const int32_t &);
    int32_t getVerboseLevel();

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
: public org::ffado::Control::Element::Container_adaptor
, public DBusControl::Element
{
public:
    Container( DBus::Connection& connection,
                  std::string p, Element *,
                  Control::Container &slave );
    virtual ~Container();

    int32_t getNbElements( );
    std::string getElementName( const int32_t& );

    void updated(int new_nb_elements);
    void destroyed();

    void setVerboseLevel( const int32_t &);
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
: public org::ffado::Control::Element::Continuous_adaptor
, public Element
{
public:
    Continuous( DBus::Connection& connection,
                  std::string p, Element *,
                  Control::Continuous &slave );
    
    double setValue( const double & value );
    double getValue( );
    double getMinimum( );
    double getMaximum( );
    double setValueIdx( const int32_t & idx,
                              const double & value );
    double getValueIdx( const int32_t & idx );

private:
    Control::Continuous &m_Slave;
};

class Discrete
: public org::ffado::Control::Element::Discrete_adaptor
, public Element
{
public:
    Discrete( DBus::Connection& connection,
              std::string p, Element *,
              Control::Discrete &slave );
    
    int32_t setValue( const int32_t & value );
    int32_t getValue( );
    int32_t setValueIdx( const int32_t & idx,
                             const int32_t & value );
    int32_t getValueIdx( const int32_t & idx );

private:
    Control::Discrete &m_Slave;
};

class Text
: public org::ffado::Control::Element::Text_adaptor
, public Element
{
public:
    Text( DBus::Connection& connection,
          std::string p, Element *,
          Control::Text &slave );

    std::string setValue( const std::string & value );
    std::string getValue( );

private:
    Control::Text &m_Slave;
};

class Register
: public org::ffado::Control::Element::Register_adaptor
, public Element
{
public:
    Register( DBus::Connection& connection,
              std::string p, Element *,
              Control::Register &slave );
    
    uint64_t setValue( const uint64_t & addr, const uint64_t & value );
    uint64_t getValue( const uint64_t & addr );

private:
    Control::Register &m_Slave;
};

class Enum
: public org::ffado::Control::Element::Enum_adaptor
, public Element
{
public:
    Enum( DBus::Connection& connection,
          std::string p, Element *,
          Control::Enum &slave );
    
    int32_t select( const int32_t & idx );
    int32_t selected( );
    int32_t count( );
    std::string getEnumLabel( const int32_t & idx );
    bool devConfigChanged( const int32_t & );

private:
    Control::Enum &m_Slave;
};

class AttributeEnum
: public org::ffado::Control::Element::AttributeEnum_adaptor
, public Element
{
public:
    AttributeEnum( DBus::Connection& connection,
                   std::string p, Element *,
                   Control::AttributeEnum &slave );
    
    int32_t select( const int32_t & idx );
    int32_t selected( );
    int32_t count( );
    int32_t attributeCount();
    std::string getEnumLabel( const int32_t & idx );
    std::string getAttributeValue( const int32_t & idx );
    std::string getAttributeName( const int32_t & idx );

private:
    Control::AttributeEnum &m_Slave;
};

// FIXME: to change this to a normal ConfigRom class name we have to
// put the 1394 config rom class into a separate namespace.
class ConfigRomX
: public org::ffado::Control::Element::ConfigRomX_adaptor
, public Element
{
public:
    ConfigRomX( DBus::Connection& connection,
                  std::string p, Element *,
                  ConfigRom &slave );

    std::string getGUID( );
    std::string getVendorName( );
    std::string getModelName( );
    int32_t getVendorId( );
    int32_t getModelId( );
    int32_t getUnitVersion( );

private:
    ConfigRom &m_Slave;
};

class MatrixMixer
: public org::ffado::Control::Element::MatrixMixer_adaptor
, public Element
{
public:
    MatrixMixer(  DBus::Connection& connection,
                  std::string p, Element *,
                  Control::MatrixMixer &slave );

    int32_t getRowCount( );
    int32_t getColCount( );

    int32_t canWrite( const int32_t&, const int32_t& );
    double setValue( const int32_t&, const int32_t&, const double& );
    double getValue( const int32_t&, const int32_t& );

    bool hasNames();
    std::string getRowName( const int32_t& );
    std::string getColName( const int32_t& );
    bool setRowName( const int32_t&, const std::string& );
    bool setColName( const int32_t&, const std::string& );

    bool canConnect();
    std::vector<std::string> availableConnectionsForRow( const int32_t& );
    std::vector<std::string> availableConnectionsForCol( const int32_t& );
    bool connectRowTo( const int32_t&, const std::string& );
    bool connectColTo( const int32_t&, const std::string& );

private:
    Control::MatrixMixer &m_Slave;
};

class CrossbarRouter
: public org::ffado::Control::Element::CrossbarRouter_adaptor
, public Element
{
public:
    CrossbarRouter(  DBus::Connection& connection,
                  std::string p, Element *,
                  Control::CrossbarRouter &slave );

    std::vector< std::string > getSourceNames();
    std::vector< std::string > getDestinationNames();

    std::vector< std::string > getDestinationsForSource(const std::string &);
    std::string getSourceForDestination(const std::string &);

    bool  canConnect(const std::string &source, const std::string &dest);
    bool  setConnectionState(const std::string &source, const std::string &dest, const bool &enable);
    bool  getConnectionState(const std::string &source, const std::string &dest);

    bool  clearAllConnections();

    bool  hasPeakMetering();
    double getPeakValue(const std::string &dest);
    std::vector< DBus::Struct<std::string, double> > getPeakValues();

private:
    Control::CrossbarRouter &m_Slave;
};

class Boolean
: public org::ffado::Control::Element::Boolean_adaptor
, public Element
{
public:
    Boolean( DBus::Connection& connection,
          std::string p, Element *,
          Control::Boolean &slave );
    
    bool select( const bool& value );
    bool selected();
    std::string getBooleanLabel( const bool& value );

private:
    Control::Boolean &m_Slave;
};
}

#endif // CONTROLSERVER_H
