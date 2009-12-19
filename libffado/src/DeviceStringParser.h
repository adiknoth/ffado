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

#ifndef __FFADO_DEVICESTRINGPARSER__
#define __FFADO_DEVICESTRINGPARSER__

#include "debugmodule/debugmodule.h"

#include <vector>
#include <string>
#include <stdint.h>

class ConfigRom;

class DeviceStringParser {

protected:
    class DeviceString {
    public:
        enum eType {
            eInvalid = 0,
            eBusNode = 1, // old-style hw:bus,node
            eGUID = 2,    // GUID match
        };

        DeviceString(DeviceStringParser&);
        ~DeviceString();

        bool parse(std::string s);
        bool match(ConfigRom &);

        std::string getString() {return m_String;};
        void show();

        bool operator==(const DeviceString& x);
        static bool isValidString(std::string s);

    private:
        DeviceStringParser & m_Parent;

        int m_node;
        int m_port;
        uint64_t m_guid;
        std::string m_String;
        enum eType  m_Type;

        DECLARE_DEBUG_MODULE_REFERENCE;
    };

public:
    DeviceStringParser();
    virtual ~DeviceStringParser();

    int countDeviceStrings() {return m_DeviceStrings.size();};

    bool match(ConfigRom &);
    int matchPosition(ConfigRom& c);

    bool parseString(std::string s);
    void show();
    void setVerboseLevel(int i);

    static bool isValidString(std::string s);

protected:
    bool removeDeviceString(DeviceString *);
    bool addDeviceString(DeviceString *);
    bool hasDeviceString(DeviceString *);

private:
    int findDeviceString(DeviceString *);

    void pruneDuplicates();

    typedef std::vector< DeviceString* > DeviceStringVector;
    typedef std::vector< DeviceString* >::iterator DeviceStringVectorIterator;
    DeviceStringVector m_DeviceStrings;

protected:
    DECLARE_DEBUG_MODULE;

};

#endif /* __FFADO_DEVICESTRINGPARSER__ */


