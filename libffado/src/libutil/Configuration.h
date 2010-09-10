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

#ifndef _FFADO_UTIL_CONFIGURATION_
#define _FFADO_UTIL_CONFIGURATION_

#include "debugmodule/debugmodule.h"
#include "libconfig.h++"

#include <vector>

namespace Util {
/**
 * A class that manages several configuration files
 * the idea is that you can have a system config file
 * and then a user-defined config file
 *
 * note: not thread safe!
 */

class Configuration {
public:
    // driver ID's to be used in the config file
    enum eDrivers {
        eD_Unknown     = 0,
        eD_BeBoB       = 1,
        eD_FireWorks   = 2,
        eD_GenericAVC  = 3,
        eD_Oxford      = 4,
        eD_MAudio      = 5,
        eD_MOTU        = 10,
        eD_DICE        = 20,
        eD_MetricHalo  = 30,
        eD_RME         = 40,
        eD_Bounce      = 50,
    };

    // the modes a config file can have
    enum eFileMode {
        eFM_ReadOnly,
        eFM_ReadWrite,
        eFM_Temporary, // this won't be saved to dist
    };

    // struct to define the supported devices
    struct VendorModelEntry {
        VendorModelEntry();
        VendorModelEntry(const VendorModelEntry& rhs);
        VendorModelEntry& operator = (const VendorModelEntry& rhs);
        bool operator == (const VendorModelEntry& rhs) const;
        virtual ~VendorModelEntry();

        unsigned int vendor_id;
        unsigned int model_id;
        std::string vendor_name;
        std::string model_name;
        unsigned int driver;
    };

    typedef std::vector<VendorModelEntry> VendorModelEntryVector;

private:
    class ConfigFile : public libconfig::Config {
    public:
        ConfigFile(Configuration &c, std::string n, enum eFileMode mode = eFM_ReadOnly)
        : Config()
        , m_parent(c)
        , m_name( n )
        , m_mode( mode )
        , m_debugModule(c.m_debugModule)
        {};
        ~ConfigFile() {};
        void readFile();
        void writeFile();
        void show();
        void showSetting(libconfig::Setting &, std::string prefix = "");

        std::string getName() {return m_name;};
        enum eFileMode getMode() {return m_mode;};
    private:
        Configuration &m_parent;
        std::string    m_name;
        enum eFileMode m_mode;
    private:
        DECLARE_DEBUG_MODULE_REFERENCE;
    };

public:
    Configuration();
    virtual ~Configuration();

    virtual bool openFile(std::string filename, enum eFileMode = eFM_ReadOnly);
    virtual bool closeFile(std::string filename);
    virtual bool saveFile(std::string filename);
    virtual bool save();

    VendorModelEntry findDeviceVME( unsigned int vendor_id,  unsigned model_id );
    bool isDeviceVMEPresent( unsigned int vendor_id,  unsigned model_id );
    static bool isValid( const VendorModelEntry& vme );

    // access functions
    /**
     * @brief retrieves a setting for a given path
     * 
     * the value in the ref parameter is not changed if
     * the function returns false.
     * 
     * @param path path to the setting
     * @param ref reference to the integer that will hold the value.
     * @return true if successful, false if not
     */
    bool getValueForSetting(std::string path, int32_t &ref);
    bool getValueForSetting(std::string path, int64_t &ref);
    bool getValueForSetting(std::string path, float &ref);

    /**
     * @brief retrieves a setting for a given device
     * 
     * the value in the ref parameter is not changed if
     * the function returns false.
     * 
     * @param vendor_id vendor id for the device
     * @param model_id  model id for the device
     * @param setting name of the setting
     * @param ref reference to the integer that will hold the value.
     * @return true if successful, false if not
     */
    bool getValueForDeviceSetting(unsigned int vendor_id, unsigned model_id, std::string setting, int32_t &ref);
    bool getValueForDeviceSetting(unsigned int vendor_id, unsigned model_id, std::string setting, int64_t &ref);
    bool getValueForDeviceSetting(unsigned int vendor_id, unsigned model_id, std::string setting, float &ref);

    virtual void setVerboseLevel(int l) {setDebugLevel(l);};
    virtual void show();

private:
    libconfig::Setting *getSetting( std::string path );
    libconfig::Setting *getDeviceSetting( unsigned int vendor_id, unsigned model_id );

    int findFileName(std::string s);

    // important: keep 1-1 mapping for these two!
    // cannot use map since we need the vector order to
    // provide priorities
    std::vector<ConfigFile *> m_ConfigFiles;

    DECLARE_DEBUG_MODULE;
};


} // namespace Util

#endif // _FFADO_UTIL_CONFIGURATION_
