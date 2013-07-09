/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#include "DeviceStringParser.h"

#include <stdlib.h>
#include <errno.h>

#include <iostream>
#include <sstream>

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

IMPL_DEBUG_MODULE( DeviceStringParser, DeviceStringParser, DEBUG_LEVEL_NORMAL );

DeviceStringParser::DeviceString::DeviceString(DeviceStringParser& parent)
    : m_Parent(parent)
    , m_node( -1 )
    , m_port( -1 )
    , m_guid( 0 )
    , m_String("")
    , m_Type(eInvalid)
    , m_debugModule( parent.m_debugModule )
{
}

DeviceStringParser::DeviceString::~DeviceString()
{
}

bool
DeviceStringParser::DeviceString::parse(std::string s)
{
    m_String = s;
    debugOutput(DEBUG_LEVEL_VERBOSE, "parse: %s\n", s.c_str());
    std::string prefix = s.substr(0,3);
    
    if(s.compare(0,3,"hw:")==0) {
        m_Type = eBusNode;
        std::string detail = s.substr(3);
        std::string::size_type comma_pos = detail.find_first_of(",");
        if(comma_pos == std::string::npos) {
            // node is unspecified
            m_node = -1;
            std::string port = detail;
            errno = 0;
            m_port = strtol(port.c_str(), NULL, 0);
            if(errno) {
                m_Type = eInvalid;
                m_port = -1;
                m_node = -1;
                debugOutput(DEBUG_LEVEL_VERBOSE, "failed to parse port\n");
                return false;
            }
        } else {
            std::string port = detail.substr(0, comma_pos);
            std::string node = detail.substr(comma_pos+1);
            errno = 0;
            m_port = strtol(port.c_str(), NULL, 0);
            if(errno) {
                m_Type = eInvalid;
                m_port = -1;
                m_node = -1;
                debugOutput(DEBUG_LEVEL_VERBOSE, "failed to parse port\n");
                return false;
            }
            errno = 0;
            m_node = strtol(node.c_str(), NULL, 0);
            if(errno) {
                m_Type = eInvalid;
                m_port = -1;
                m_node = -1;
                debugOutput(DEBUG_LEVEL_VERBOSE, "failed to parse node\n");
                return false;
            }
        }
    } else if (s.compare(0,5,"guid:")==0) {
        std::string detail = s.substr(5);
        m_Type = eGUID;
        errno = 0;
        m_guid = strtoll(detail.c_str(), NULL, 0);
        if(errno) {
            m_Type = eInvalid;
            m_guid = 0;
            debugOutput(DEBUG_LEVEL_VERBOSE, "failed to parse guid\n");
            return false;
        }
    } else {
        m_Type = eInvalid;
        debugOutput(DEBUG_LEVEL_VERBOSE, "invalid\n");
        return false;
    }
    return true;
}

bool
DeviceStringParser::DeviceString::isValidString(std::string s)
{
    std::string prefix = s.substr(0,3);
    if(s.compare(0,3,"hw:")==0) {
        std::string detail = s.substr(3);
        std::string::size_type comma_pos = detail.find_first_of(",");
        if(comma_pos == std::string::npos) {
            std::string port = detail;
            errno = 0;
            strtol(port.c_str(), NULL, 0);
            if(errno) {
                return false;
            }
        } else {
            std::string port = detail.substr(0, comma_pos);
            std::string node = detail.substr(comma_pos+1);
            errno = 0;
            strtol(port.c_str(), NULL, 0);
            if(errno) {
                return false;
            }
            errno = 0;
            strtol(node.c_str(), NULL, 0);
            if(errno) {
                return false;
            }
        }
    } else if (s.compare(0,5,"guid:")==0) {
        std::string detail = s.substr(5);
        errno = 0;
        strtoll(detail.c_str(), NULL, 0);
        if(errno) {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

bool
DeviceStringParser::DeviceString::match(ConfigRom& configRom)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "match %p (%s)\n", &configRom, configRom.getGuidString().c_str());
    bool match;
    switch(m_Type) {
        case eBusNode:
            if(m_port < 0) {
                debugWarning("Need at least a port spec\n");
                return false;
            }
            match = configRom.get1394Service().getPort() == m_port;
            if(m_node >= 0) {
                match &= ((configRom.getNodeId() & 0x3F) == m_node);
            }
            if(match) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "(eBusNode) device matches device string %s\n", m_String.c_str());
            }
            return match;
        case eGUID:
            //GUID should not be 0
            match = m_guid && (m_guid == configRom.getGuid());
            if(match) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "(eGUID) device matches device string %s\n", m_String.c_str());
            }
            return match;
        case eInvalid:
        default:
            debugError("invalid DeviceString type (%d)\n", m_Type);
            return false;
    }
    return false;
}

bool
DeviceStringParser::DeviceString::operator==(const DeviceString& x)
{
    bool retval;
    switch(m_Type) {
        case eBusNode:
            retval = (m_port == x.m_port) && (m_node == x.m_node);
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "eBusNode %d,%d == %d,%d? %d\n",
                        m_port, m_node, x.m_port, x.m_node, retval);
            return retval;
        case eGUID:
            retval = m_guid && (m_guid == x.m_guid);
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "eGUID 0x%016"PRIX64" == 0x%016"PRIX64"? %d\n",
                        m_guid, x.m_guid, retval);
            return retval;
        case eInvalid:
            debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "eInvalid \n");
        default:
            return false;
    }
}

void
DeviceStringParser::DeviceString::show()
{
    debugOutput(DEBUG_LEVEL_INFO, "string: %s\n", m_String.c_str());
    switch(m_Type) {
        case eBusNode:
            debugOutput(DEBUG_LEVEL_INFO, "type: eBusNode\n");
            debugOutput(DEBUG_LEVEL_INFO, " Port: %d, Node: %d\n",
                        m_port, m_node);
            break;
        case eGUID:
            debugOutput(DEBUG_LEVEL_INFO, "type: eGUID\n");
            debugOutput(DEBUG_LEVEL_INFO, " GUID: %016"PRIX64"\n", m_guid);
            break;
        case eInvalid:
        default:
            debugOutput(DEBUG_LEVEL_INFO, "type: eInvalid\n");
            break;
    }
}

// ------------------------
DeviceStringParser::DeviceStringParser()
{}

DeviceStringParser::~DeviceStringParser()
{
    while(m_DeviceStrings.size()) {
        DeviceString *tmp = m_DeviceStrings.at(0);
        debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "removing device string: %p\n", tmp);
        m_DeviceStrings.erase(m_DeviceStrings.begin());
        delete tmp;
    }
}

bool
DeviceStringParser::parseString(std::string s)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "parse: %s\n", s.c_str());

    std::string::size_type next_sep;
    std::string tmp = s;
    do {
        debugOutput(DEBUG_LEVEL_VERBOSE, " left: %s\n", tmp.c_str());
        next_sep = tmp.find_first_of(";");
        std::string to_parse = tmp.substr(0, next_sep);
        DeviceString *d = new DeviceString(*this);
        if(d == NULL) {
            debugError("failed to allocate memory for device string\n");
            continue;
        }
        if(d->parse(to_parse)) {
            addDeviceString(d);
        } else {
            debugWarning("Failed to parse device substring: \"%s\"\n",
                         to_parse.c_str());
            delete d; 
        }
        tmp = tmp.substr(next_sep+1);
    } while(tmp.size() && next_sep != std::string::npos);

    pruneDuplicates();

    return true;
}

bool
DeviceStringParser::isValidString(std::string s)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, "isvalid? %s\n", s.c_str());
    return DeviceString::isValidString(s);
}

bool
DeviceStringParser::match(ConfigRom& c)
{
    return matchPosition(c) != -1;
}

int
DeviceStringParser::matchPosition(ConfigRom& c)
{
    int pos = 0;
    for ( DeviceStringVectorIterator it = m_DeviceStrings.begin();
      it != m_DeviceStrings.end();
      ++it )
    {
        if((*it)->match(c)) {
            return pos;
        }
        pos++;
    }
    return -1;
}

bool
DeviceStringParser::addDeviceString(DeviceString *o)
{
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "adding device string: %p\n", o);
    if (hasDeviceString(o)){
        return false;
    }
    m_DeviceStrings.push_back(o);
    return true;
}

bool
DeviceStringParser::removeDeviceString(DeviceString *o)
{
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE, "removing device string: %p\n", o);
    int i=findDeviceString(o);
    if (i<0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "not found\n");
        return false;
    } else {
        DeviceString *tmp = m_DeviceStrings.at(i);
        m_DeviceStrings.erase(m_DeviceStrings.begin()+i);
        delete tmp;
        return true;
    }
}

bool
DeviceStringParser::hasDeviceString(DeviceString *o)
{
    return (findDeviceString(o) >= 0);
}

int
DeviceStringParser::findDeviceString(DeviceString *o)
{
    int i=0;
    for ( DeviceStringVectorIterator it = m_DeviceStrings.begin();
      it != m_DeviceStrings.end();
      ++it )
    {
        if(*it == o) {
            return i;
        }
        i++;
    }
    return -1;
}

void
DeviceStringParser::pruneDuplicates()
{
    DeviceStringVector duplicates;
    // find duplicates
    for ( DeviceStringVectorIterator it = m_DeviceStrings.begin();
      it != m_DeviceStrings.end();
      ++it )
    {
        for ( DeviceStringVectorIterator it2 = it+1;
          it2 != m_DeviceStrings.end();
          ++it2 )
        {

            if(**it == **it2) {
                duplicates.push_back(*it2);
            }
        }
    }

    // remove duplicates
    for ( DeviceStringVectorIterator it = duplicates.begin();
      it != duplicates.end();
      ++it )
    {
        removeDeviceString(*it);
    }
}

void
DeviceStringParser::show()
{
    debugOutput(DEBUG_LEVEL_INFO, "DeviceStringParser: %p\n", this);
    for ( DeviceStringVectorIterator it = m_DeviceStrings.begin();
      it != m_DeviceStrings.end();
      ++it )
    {
        (*it)->show();
    }
}

void
DeviceStringParser::setVerboseLevel(int i)
{
    setDebugLevel(i);
}
