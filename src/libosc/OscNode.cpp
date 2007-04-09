/*
 * Copyright (C) 2005-2007 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "OscNode.h"
#include "OscMessage.h"
#include "OscResponse.h"
#include <assert.h>

namespace OSC {

IMPL_DEBUG_MODULE( OscNode, OscNode, DEBUG_LEVEL_VERBOSE );

OscNode::OscNode()
    : m_oscBase("")
    , m_oscAutoDelete(false)
{}

OscNode::OscNode(string b)
    : m_oscBase(b)
    , m_oscAutoDelete(false)
{}

OscNode::OscNode(string b, bool autodelete)
    : m_oscBase(b)
    , m_oscAutoDelete(autodelete)
{}

OscNode::~OscNode() {
    debugOutput( DEBUG_LEVEL_VERY_VERBOSE, "Removing child nodes\n");

    for ( OscNodeVectorIterator it = m_ChildNodes.begin();
      it != m_ChildNodes.end();
      ++it )
    {
        if((*it)->doOscNodeAutoDelete()) {
            debugOutput( DEBUG_LEVEL_VERY_VERBOSE, " Auto-deleting child node %s\n",
                         (*it)->getOscBase().c_str());
            delete (*it);
        }
    }
}

void
OscNode::setVerboseLevel(int l) {
    setDebugLevel(l);
    for ( OscNodeVectorIterator it = m_ChildNodes.begin();
      it != m_ChildNodes.end();
      ++it )
    {
        (*it)->setVerboseLevel(l);
    }
}

// generic message processing
OscResponse
OscNode::processOscMessage(string path, OscMessage *m)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) {%s} MSG: %s\n", this, m_oscBase.c_str(), path.c_str());

    // delete leading slash
    if(path.find_first_of('/')==0) path=path.substr(1,path.size());

    // delete trailing slash
    if(path.find_last_of('/')==path.size()-1) path=path.substr(0,path.size()-1);

    // continue processing
    int firstsep=path.find_first_of('/');

    if (firstsep == -1) {
        OscResponse retVal;

        // process the message
        m->setPath(""); // handled by the node itself
        retVal=processOscMessage(m);

        if(retVal.isHandled()) {
            return retVal; // completely handled
        } else { // (partially) unhandled
            return processOscMessageDefault(m, retVal);
        }

    } else { // it targets a deeper node
        OscResponse retVal;
        
        string newpath=path.substr(firstsep+1);
        int secondsep=newpath.find_first_of('/');
        string newbase;
        if (secondsep==-1) {
            newbase=newpath;
        } else {
            newbase=path.substr(firstsep+1,secondsep);
        }

        // first try to find a child node that might be able
        // to handle this.
        // NOTE: the current model allows only one node to 
        //       handle a request, and then the default
        //       handler.
        for ( OscNodeVectorIterator it = m_ChildNodes.begin();
          it != m_ChildNodes.end();
          ++it )
        {
            if((*it)->getOscBase() == newbase) {
                return (*it)->processOscMessage(newpath,m);
            }
        }
        
        // The path is not registered as a child node.
        // This node should handle it
        debugOutput( DEBUG_LEVEL_VERBOSE, "Child node %s not found \n",newbase.c_str());

        m->setPath(newpath); // the remaining portion of the path
        retVal=processOscMessage(m);

        if(retVal.isHandled()) {
            return retVal; // completely handled
        }
        // (partially) unhandled
        return processOscMessageDefault(m, retVal);
    }
    return OscResponse(OscResponse::eError);
}

// default handlers

// overload this to make a node process a message
OscResponse
OscNode::processOscMessage(OscMessage *m)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) {%s} DEFAULT PROCESS: %s\n", this, m_oscBase.c_str(), m->getPath().c_str());
    m->print();
    return OscResponse(OscResponse::eUnhandled); // handled but no response
}

// this provides support for messages that are
// supported by all nodes
OscResponse
OscNode::processOscMessageDefault(OscMessage *m, OscResponse r)
{
    unsigned int nbArgs=m->nbArguments();
    if (nbArgs>=1) {
        OscArgument arg0=m->getArgument(0);
        if(arg0.isString()) { // first argument should be a string command
            string cmd=arg0.getString();
            debugOutput( DEBUG_LEVEL_VERBOSE, "(%p) CMD? %s\n", this, cmd.c_str());
            if(cmd == "list") {
                debugOutput( DEBUG_LEVEL_VERBOSE, "Listing node children...\n");
                OscMessage rm=oscListChildren(r.getMessage());
                return OscResponse(rm);
            }
        }
    }
    // default action: don't change response
    return r;
}

OscMessage
OscNode::oscListChildren(OscMessage m) {

    for ( OscNodeVectorIterator it = m_ChildNodes.begin();
      it != m_ChildNodes.end();
      ++it )
    {
        m.addArgument((*it)->getOscBase());
    }
    m.print();
    return m;
}

// child management

bool
OscNode::addChildOscNode(OscNode *n)
{
    assert(n);

    debugOutput( DEBUG_LEVEL_VERBOSE, "Adding child node %s\n",n->getOscBase().c_str());
    for ( OscNodeVectorIterator it = m_ChildNodes.begin();
      it != m_ChildNodes.end();
      ++it )
    {
        if(*it == n) {
            debugOutput( DEBUG_LEVEL_VERBOSE,
                "Child node %s already registered\n",
                n->getOscBase().c_str());
            return false;
        }
    }
    m_ChildNodes.push_back(n);
    return true;
}

/**
 * Add a child node under a certain path.
 * e.g. if you have a node with base 'X' and
 * you want it to be at /base/level1/level2/X
 * you would add it using this function, by
 *  addChildOscNode(n, "/base/level1/level2")
 *
 * @param n
 * @param
 * @return
 */
bool
OscNode::addChildOscNode(OscNode *n, string path)
{
    debugOutput( DEBUG_LEVEL_VERBOSE, "add node to: %s\n",path.c_str());

    // delete leading slashes
    if(path.find_first_of('/')==0) path=path.substr(1,path.size());

    // delete trailing slashes
    if(path.find_last_of('/')==path.size()-1) path=path.substr(0,path.size()-1);

    // continue processing
    int firstsep=path.find_first_of('/');

    if (firstsep == -1) {
        return addChildOscNode(n);
    } else { // it targets a deeper node
        string newpath=path.substr(firstsep+1);
        int secondsep=newpath.find_first_of('/');
        string newbase;
        if (secondsep==-1) {
            newbase=newpath;
        } else {
            newbase=path.substr(firstsep+1,secondsep);
        }

        // if the path is already present, find it
        for ( OscNodeVectorIterator it = m_ChildNodes.begin();
          it != m_ChildNodes.end();
          ++it )
        {
            if((*it)->getOscBase() == newbase) {
                return (*it)->addChildOscNode(n,newpath);
            }
        }
        debugOutput( DEBUG_LEVEL_VERBOSE, "node %s not found, creating auto-node\n",newbase.c_str());

        OscNode *autoNode=new OscNode(newbase,true);

        // add the auto-node to this node
        m_ChildNodes.push_back(autoNode);

        // add the child to the node
        return autoNode->addChildOscNode(n,newpath);
    }
    return false;
}

bool
OscNode::removeChildOscNode(OscNode *n)
{
    assert(n);
    debugOutput( DEBUG_LEVEL_VERBOSE, "Removing child node %s\n",n->getOscBase().c_str());

    for ( OscNodeVectorIterator it = m_ChildNodes.begin();
      it != m_ChildNodes.end();
      ++it )
    {
        if(*it == n) {
            m_ChildNodes.erase(it);
            if((*it)->doOscNodeAutoDelete()) {
                debugOutput( DEBUG_LEVEL_VERBOSE, " Auto-deleting child node %s\n",n->getOscBase().c_str());
                delete (*it);
            }
            return true;
        }
    }

    debugOutput( DEBUG_LEVEL_VERBOSE, "Child node %s not found \n",n->getOscBase().c_str());

    return false; //not found
}

void
OscNode::printOscNode()
{
    printOscNode("");
}

void
OscNode::printOscNode(string path)
{
    debugOutput( DEBUG_LEVEL_NORMAL, "%s/%s\n",
                 path.c_str(),getOscBase().c_str());

    for ( OscNodeVectorIterator it = m_ChildNodes.begin();
      it != m_ChildNodes.end();
      ++it )
    {
        (*it)->printOscNode(path+"/"+getOscBase());
    }
}

} // end of namespace OSC
