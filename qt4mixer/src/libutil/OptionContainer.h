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

#ifndef __FFADO_OPTIONCONTAINER__
#define __FFADO_OPTIONCONTAINER__

#include "../debugmodule/debugmodule.h"
#include "libutil/serialize.h"

#include <vector>
#include <string>

namespace Util {

class OptionContainer {

protected:
    class Option {
    public:
        enum EType {
            EInvalid = 0,
            EString = 1,
            EBool = 2,
            EDouble = 3,
            EInt = 4,
            EUInt = 5,
        };

    public:
        Option();
        Option(std::string);
        Option(std::string, std::string);
        Option(std::string, bool);
        Option(std::string, double);
        Option(std::string, int64_t);
        Option(std::string, uint64_t);

        ~Option() {};

        std::string getName() {return m_Name;};
        enum EType getType() {return m_Type;};

        void set(std::string v);
        void set(bool v);
        void set(double v);
        void set(int64_t v);
        void set(uint64_t v);

        std::string getString() {return m_stringValue;};
        bool getBool() {return m_boolValue;};
        double getDouble() {return m_doubleValue;};
        int64_t getInt() {return m_intValue;};
        uint64_t getUInt() {return m_uintValue;};

    public: // serialization support
        bool serialize( Glib::ustring basePath, Util::IOSerialize& ser ) const;
        static Option deserialize( Glib::ustring basePath,
                                    Util::IODeserialize& deser);
    private:
        std::string m_Name;

        std::string m_stringValue;

        bool        m_boolValue;
        double      m_doubleValue;
        int64_t     m_intValue;
        uint64_t    m_uintValue;

        enum EType  m_Type;
    };

public:

    OptionContainer();
    virtual ~OptionContainer();

    bool setOption(std::string name, std::string v);
    bool setOption(std::string name, bool v);
    bool setOption(std::string name, double v);
    bool setOption(std::string name, int64_t v);
    bool setOption(std::string name, uint64_t v);
    bool setOption(std::string name, int32_t v);
    bool setOption(std::string name, uint32_t v);
    bool setOption(std::string name, int16_t v);
    bool setOption(std::string name, uint16_t v);
    bool setOption(std::string name, int8_t v);
    bool setOption(std::string name, uint8_t v);

    bool getOption(std::string name, std::string &v);
    bool getOption(std::string name, bool &v);
    bool getOption(std::string name, double &v);
    bool getOption(std::string name, float &v);
    bool getOption(std::string name, int64_t &v);
    bool getOption(std::string name, uint64_t &v);
    bool getOption(std::string name, int32_t &v);
    bool getOption(std::string name, uint32_t &v);
    bool getOption(std::string name, int16_t &v);
    bool getOption(std::string name, uint16_t &v);
    bool getOption(std::string name, int8_t &v);
    bool getOption(std::string name, uint8_t &v);

    Option::EType getOptionType(std::string name);

    bool hasOption(std::string name);

    int countOptions() {return m_Options.size();};

protected:
    bool setOption(Option o);
    Option getOption(std::string name);

    bool hasOption(Option o);

    bool addOption(Option o);

    bool removeOption(Option o);
    bool removeOption(std::string name);

    void clearOptions() {m_Options.clear();};

public: // provide an iterator interface

    typedef std::vector< Option >::iterator iterator;
    iterator begin()
    {
        return(m_Options.begin());
    }

    iterator end()
    {
        return(m_Options.end());
    }

protected: // serialization support
    bool serializeOptions( Glib::ustring basePath,
                           Util::IOSerialize& ser) const;
    static bool deserializeOptions( Glib::ustring basePath,
                                    Util::IODeserialize& deser,
                                    OptionContainer& container);

private:
    int findOption(Option o);
    int findOption(std::string name);

    typedef std::vector< Option > OptionVector;
    typedef std::vector< Option >::iterator OptionVectorIterator;
    OptionVector m_Options;

protected:
    DECLARE_DEBUG_MODULE;

};

} // end of namespace Util

#endif /* __FFADO_OPTIONCONTAINER__ */


