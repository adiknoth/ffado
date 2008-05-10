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

#ifndef CONTROL_ELEMENT_H
#define CONTROL_ELEMENT_H

#include "debugmodule/debugmodule.h"

#include <vector>
#include <string>

#include "libutil/Mutex.h"

namespace Control {

/*!
@brief Base class for control elements

 This class should be subclassed to implement ffado control elements.
*/
class Element
{
public:
    Element(Element *);
    Element(Element *, std::string n);
    virtual ~Element();

    virtual std::string getName() {return m_Name;};
    virtual bool setName( std::string n )
        { m_Name=n; return true;};

    virtual std::string getLabel() {return m_Label;};
    virtual bool setLabel( std::string n )
        { m_Label=n; return true;};

    virtual std::string getDescription() {return m_Description;};
    virtual bool setDescription( std::string n )
        { m_Description=n; return true;};

    uint64_t getId()
        {return m_id;};

    // these allow to prevent external access to the control elements
    // e.g. when the config tree is rebuilt
    virtual void lockControl();
    virtual void unlockControl();

    virtual void show();

    /**
     * set verbosity level
     */
    virtual void setVerboseLevel(int l);
    virtual int getVerboseLevel() {return getDebugLevel();};

protected:
    Util::Mutex&    getLock();

private:
    Util::Mutex*    m_element_lock;
    Element*        m_parent;
    std::string m_Name;
    std::string m_Label;
    std::string m_Description;
    
    uint64_t m_id;

protected:
    DECLARE_DEBUG_MODULE;

};
typedef std::vector<Element *> ElementVector;
typedef std::vector<Element *>::iterator ElementVectorIterator;
typedef std::vector<Element *>::const_iterator ConstElementVectorIterator;

/*!
@brief Base class for control containers

 This class should be subclassed to implement ffado control container elements.
 Containers are classes that can hold a set of control elements. They themselves
 are control elements such that hierarchies can be defined using them.
 
 Special control containers that act on all of their children can also be 
 implemented.
*/
class Container : public Element
{
public:
    Container(Element *);
    Container(Element *, std::string n);
    virtual ~Container() {};
    
    virtual bool addElement(Element *e);
    virtual bool deleteElement(Element *e);
    virtual bool clearElements()
        {return clearElements(false);};
    virtual bool clearElements(bool delete_pointers);

    unsigned int countElements();


    /**
     * Returns and locks the element vector. No changes will be made to the vector
     * until releaseElementVector is called.
     * @return 
     */
    const ElementVector & getElementVector();

    /**
     * Releases the lock on the element vector.
     */
    void releaseElementVector();

    virtual void show();
    virtual void setVerboseLevel(int l);

protected:
    ElementVector m_Children;
};


}; // namespace Control

#endif // CONTROL_ELEMENT_H
