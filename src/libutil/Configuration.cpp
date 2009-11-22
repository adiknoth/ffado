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

#include "Configuration.h"

#include <stdint.h>
#include <stdlib.h>

using namespace libconfig;
namespace Util {

IMPL_DEBUG_MODULE( Configuration, Configuration, DEBUG_LEVEL_NORMAL );

Configuration::Configuration()
{

}

Configuration::~Configuration()
{
    while(m_ConfigFiles.size()) {
        delete m_ConfigFiles.back();
        m_ConfigFiles.pop_back();
    }
}

bool
Configuration::openFile(std::string filename, enum eFileMode mode)
{
    // check if not already open
    if(findFileName(filename) >= 0) {
        debugError("file already open\n");
        return false;
    }

    ConfigFile *c = new ConfigFile(*this, filename, mode);
    switch(mode) {
        case eFM_ReadOnly:
        case eFM_ReadWrite:
            try {
                c->readFile();
            } catch (FileIOException& e) {
                debugOutput(DEBUG_LEVEL_VERBOSE, "Could not open file: %s\n", filename.c_str());
                delete c;
                return false;
            } catch (ParseException& e) {
                debugWarning("Could not parse file: %s\n", filename.c_str());
                delete c;
                return false;
            } catch (...) {
                debugWarning("Unknown exception when opening file: %s\n", filename.c_str());
                delete c;
                return false;
            }
            break;
        default:
            break;
    }
    m_ConfigFiles.push_back(c);
    return true;
}

bool
Configuration::closeFile(std::string filename)
{
    int idx = findFileName(filename);
    if(idx >= 0) {
        debugOutput(DEBUG_LEVEL_VERBOSE, "Closing config file: %s\n", filename.c_str());
        ConfigFile *c = m_ConfigFiles.at(idx);
        m_ConfigFiles.erase(m_ConfigFiles.begin()+idx);
        delete c;
        return true;
    } else {
        debugError("file not open\n");
        return false;
    }
}

bool
Configuration::saveFile(std::string name)
{
    int idx = findFileName(name);
    if(idx >= 0) {
        ConfigFile *c = m_ConfigFiles.at(idx);
        switch(c->getMode()) {
        case eFM_ReadOnly:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Not saving readonly config file: %s\n", c->getName().c_str());
            break;
        case eFM_Temporary:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Not saving temporary config file: %s\n", c->getName().c_str());
            break;
        case eFM_ReadWrite:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Saving config file: %s\n", c->getName().c_str());
            try {
                c->writeFile();
            } catch (...) {
                debugError("Could not write file: %s\n", c->getName().c_str());
                return false;
            }
        default:
            debugOutput(DEBUG_LEVEL_VERBOSE, "bad mode for file: %s\n", c->getName().c_str());
        }
    }
    return true;
}

bool
Configuration::save()
{
    bool retval = true;
    for (unsigned int idx = 0; idx < m_ConfigFiles.size(); idx++) {
        ConfigFile *c = m_ConfigFiles.at(idx);
        switch(c->getMode()) {
        case eFM_ReadOnly:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Not saving readonly config file: %s\n", c->getName().c_str());
            break;
        case eFM_Temporary:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Not saving temporary config file: %s\n", c->getName().c_str());
            break;
        case eFM_ReadWrite:
            debugOutput(DEBUG_LEVEL_VERBOSE, "Saving config file: %s\n", c->getName().c_str());
            try {
                c->writeFile();
            } catch (...) {
                debugError("Could not write file: %s\n", c->getName().c_str());
                retval = false;
            }
        default:
            debugOutput(DEBUG_LEVEL_VERBOSE, "bad mode for file: %s\n", c->getName().c_str());
        }
    }
    return retval;
}

void
Configuration::ConfigFile::showSetting(libconfig::Setting &s, std::string prefix)
{
    unsigned int children = s.getLength();
    Setting::Type t = s.getType();

    switch(t) {
    case Setting::TypeGroup:
        debugOutput(DEBUG_LEVEL_NORMAL, "  %sGroup: %s\n", prefix.c_str(), s.getName());
        for(unsigned int i = 0; i < children; i++) {
            showSetting(s[i], prefix + "  ");
        }
        break;
    case Setting::TypeList:
        debugOutput(DEBUG_LEVEL_NORMAL, "  %sList: %s\n", prefix.c_str(), s.getName());
        for(unsigned int i = 0; i < children; i++) {
            showSetting(s[i], prefix + "  ");
        }
        break;
    case Setting::TypeArray:
        debugOutput(DEBUG_LEVEL_NORMAL, "  %sArray: %s\n", prefix.c_str(), s.getName());
        for(unsigned int i = 0; i < children; i++) {
            showSetting(s[i], prefix + "  ");
        }
        break;
    case Setting::TypeInt:
        {
            int32_t i = s;
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "  %s%s = %d (0x%08X)\n",
                        prefix.c_str(), s.getName(), i, i);
        }
        break;
    case Setting::TypeInt64:
        {
            int64_t i = s;
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "  %s%s = %"PRId64" (0x%016"PRIX64")\n",
                        prefix.c_str(), s.getName(), i, i);
        }
        break;
    case Setting::TypeFloat:
        {
            float f = s;
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "  %s%s = %f\n",
                        prefix.c_str(), s.getName(), f);
        }
        break;
    case Setting::TypeString:
        {
            std::string str = s;
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "  %s%s = %s\n",
                        prefix.c_str(), s.getName(), str.c_str());
        }
        break;
    case Setting::TypeBoolean:
        {
            bool b = s;
            std::string str = (b?"true":"false");
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "  %s%s = %s\n",
                        prefix.c_str(), s.getName(), str.c_str());
        }
        break;
    default:
        {
            debugOutput(DEBUG_LEVEL_NORMAL,
                        "  %s%s = Unsupported Type\n",
                        prefix.c_str(), s.getName());
        }
        break;
    }
}

bool
Configuration::getValueForSetting(std::string path, int32_t &ref)
{
    libconfig::Setting *s = getSetting( path );
    if(s) {
        // FIXME: this can be done using the libconfig methods
        Setting::Type t = s->getType();
        if(t == Setting::TypeInt) {
            ref = *s;
            debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' has value %d\n", path.c_str(), ref);
            return true;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' has wrong type\n", path.c_str());
            return false;
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' not found\n", path.c_str());
        return false;
    }
}

bool
Configuration::getValueForSetting(std::string path, int64_t &ref)
{
    libconfig::Setting *s = getSetting( path );
    if(s) {
        // FIXME: this can be done using the libconfig methods
        Setting::Type t = s->getType();
        if(t == Setting::TypeInt64) {
            ref = *s;
            debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' has value %"PRId64"\n", path.c_str(), ref);
            return true;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' has wrong type\n", path.c_str());
            return false;
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' not found\n", path.c_str());
        return false;
    }
}

bool
Configuration::getValueForSetting(std::string path, float &ref)
{
    libconfig::Setting *s = getSetting( path );
    if(s) {
        // FIXME: this can be done using the libconfig methods
        Setting::Type t = s->getType();
        if(t == Setting::TypeFloat) {
            ref = *s;
            debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' has value %f\n", path.c_str(), ref);
            return true;
        } else {
            debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' has wrong type\n", path.c_str());
            return false;
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "path '%s' not found\n", path.c_str());
        return false;
    }
}

libconfig::Setting *
Configuration::getSetting( std::string path )
{
    for ( std::vector<ConfigFile *>::iterator it = m_ConfigFiles.begin();
      it != m_ConfigFiles.end();
      ++it )
    {
        ConfigFile *c = *it;
        try {
            Setting &s = c->lookup(path);
            return &s;
        } catch (...) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  %s has no setting %s\n",
                        c->getName().c_str(), path.c_str());
        }
    }
    return NULL;
}

bool
Configuration::getValueForDeviceSetting(unsigned int vendor_id, unsigned model_id, std::string setting, int32_t &ref)
{
    libconfig::Setting *s = getDeviceSetting( vendor_id, model_id );
    if(s) {
        try {
            return s->lookupValue(setting, ref);
        } catch (...) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Setting %s not found\n", setting.c_str());
            return false;
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "device %X/%X not found\n", vendor_id, model_id);
        return false;
    }
}

bool
Configuration::getValueForDeviceSetting(unsigned int vendor_id, unsigned model_id, std::string setting, int64_t &ref)
{
    libconfig::Setting *s = getDeviceSetting( vendor_id, model_id );
    if(s) {
        try {
            return s->lookupValue(setting, ref);
        } catch (...) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Setting %s not found\n", setting.c_str());
            return false;
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "device %X/%X not found\n", vendor_id, model_id);
        return false;
    }
}

bool
Configuration::getValueForDeviceSetting(unsigned int vendor_id, unsigned model_id, std::string setting, float &ref)
{
    libconfig::Setting *s = getDeviceSetting( vendor_id, model_id );
    if(s) {
        try {
            return s->lookupValue(setting, ref);
        } catch (...) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "Setting %s not found\n", setting.c_str());
            return false;
        }
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, "device %X/%X not found\n", vendor_id, model_id);
        return false;
    }
}

libconfig::Setting *
Configuration::getDeviceSetting( unsigned int vendor_id, unsigned model_id )
{
    for ( std::vector<ConfigFile *>::iterator it = m_ConfigFiles.begin();
      it != m_ConfigFiles.end();
      ++it )
    {
        ConfigFile *c = *it;
        try {
            Setting &list = c->lookup("device_definitions");
            unsigned int children = list.getLength();
            for(unsigned int i = 0; i < children; i++) {
                Setting &s = list[i];
                try {
                    Setting &vendorid = s["vendorid"];
                    Setting &modelid = s["modelid"];
                    uint32_t vid = vendorid;
                    uint32_t mid = modelid;
                    if (vendor_id == vid && model_id == mid) {
                        debugOutput(DEBUG_LEVEL_VERBOSE,
                                    "  device VME for %X:%x found in %s\n",
                                    vendor_id, model_id, c->getName().c_str());
                        c->showSetting(s);
                        return &s;
                    }
                } catch (...) {
                    debugWarning("Bogus format\n");
                }
            }
        } catch (...) {
            debugOutput(DEBUG_LEVEL_VERBOSE, "  %s has no device definitions\n", c->getName().c_str());
        }
    }
    return NULL;
}



Configuration::VendorModelEntry
Configuration::findDeviceVME( unsigned int vendor_id, unsigned model_id )
{

    // FIXME: clean this pointer/reference mess please
    Setting *ps = getDeviceSetting(vendor_id, model_id);

    if(ps) {
        Setting &s = *ps;
        try {
            Setting &vendorid = s["vendorid"];
            Setting &modelid = s["modelid"];
            uint32_t vid = vendorid;
            uint32_t mid = modelid;
            if (vendor_id == vid && model_id == mid) {
                struct VendorModelEntry vme;
                vme.vendor_id = vendorid;
                vme.model_id = modelid;

                const char *tmp = s["vendorname"];
                vme.vendor_name = tmp;
                tmp = s["modelname"];
                vme.model_name = tmp;
                vme.driver = s["driver"];
                return vme;
            } else {
                debugError("BUG: vendor/model found but not found?\n");
            }
        } catch (...) {
            debugWarning("Bogus format\n");
        }
    }
    struct VendorModelEntry invalid;
    return invalid;
}

bool
Configuration::isDeviceVMEPresent( unsigned int vendor_id, unsigned model_id )
{
    return isValid(findDeviceVME( vendor_id, model_id ));
}

bool
Configuration::isValid( const Configuration::VendorModelEntry& vme )
{
    struct VendorModelEntry invalid;
    return !(vme==invalid);
}

void
Configuration::ConfigFile::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, " config file: %s\n", getName().c_str());
    Setting &root = getRoot();
    if(root.getLength()) {
        showSetting(root, "");
    } else {
        debugOutput(DEBUG_LEVEL_NORMAL, "  Empty\n");
    }
}

void
Configuration::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "Configuration:\n");
    for (unsigned int idx = 0; idx < m_ConfigFiles.size(); idx++) {
        ConfigFile *c = m_ConfigFiles.at(idx);
        c->show();
    }
}

int
Configuration::findFileName(std::string s) {
    int i=0;
    for ( std::vector<ConfigFile *>::iterator it = m_ConfigFiles.begin();
      it != m_ConfigFiles.end();
      ++it )
    {
        if((*it)->getName() == s) {
            return i;
        }
        i++;
    }
    return -1;
}

void
Configuration::ConfigFile::readFile()
{
    std::string filename = m_name;
    // fix up the '~' as homedir
    std::string::size_type pos = filename.find_first_of("~");
    if(pos != std::string::npos) {
        char *homedir = getenv("HOME");
        if(homedir) {
            std::string home = homedir;
            filename.replace( pos, 1, home, 0, home.length());
        }
    }
    Config::readFile(filename.c_str());
}

void
Configuration::ConfigFile::writeFile()
{
    std::string filename = m_name;
    // fix up the '~' as homedir
    std::string::size_type pos = filename.find_first_of("~");
    if(pos != std::string::npos) {
        char *homedir = getenv("HOME");
        if(homedir) {
            std::string home = homedir;
            filename.replace( pos, 1, home, 0, home.length());
        }
    }
    Config::writeFile(filename.c_str());
}

Configuration::VendorModelEntry::VendorModelEntry()
    : vendor_id( 0 )
    , model_id( 0 )
    , driver( 0 )
{
}

Configuration::VendorModelEntry::VendorModelEntry( const VendorModelEntry& rhs )
    : vendor_id( rhs.vendor_id )
    , model_id( rhs.model_id )
    , vendor_name( rhs.vendor_name )
    , model_name( rhs.model_name )
    , driver( rhs.driver )
{
}

Configuration::VendorModelEntry&
Configuration::VendorModelEntry::operator = ( const VendorModelEntry& rhs )
{
    // check for assignment to self
    if ( this == &rhs ) return *this;

    vendor_id   = rhs.vendor_id;
    model_id    = rhs.model_id;
    vendor_name = rhs.vendor_name;
    model_name  = rhs.model_name;
    driver      = rhs.driver;

    return *this;
}

bool
Configuration::VendorModelEntry::operator == ( const VendorModelEntry& rhs ) const
{
    bool equal=true;

    equal &= (vendor_id   == rhs.vendor_id);
    equal &= (model_id    == rhs.model_id);
    equal &= (vendor_name == rhs.vendor_name);
    equal &= (model_name  == rhs.model_name);
    equal &= (driver      == rhs.driver);

    return equal;
}

Configuration::VendorModelEntry::~VendorModelEntry()
{
}

} // namespace Util
