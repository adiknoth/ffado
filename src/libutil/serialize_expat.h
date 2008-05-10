/*
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef __FFADO_UTIL_SERIALIZE_EXPAT_H__
#define __FFADO_UTIL_SERIALIZE_EXPAT_H__

#include "debugmodule/debugmodule.h"

#include "serialize_expat_xml.h"

#include <iostream>
#include <string>

namespace Util {

     class IOSerialize {
     public:
         IOSerialize() {}
         virtual ~IOSerialize() {}

         virtual bool write( std::string strMemberName,
                             long long value ) = 0;
         virtual bool write( std::string strMemberName,
                             std::string str) = 0;

         template <typename T>  bool write( std::string strMemberName, T value );
     };

    class IODeserialize {
    public:
        IODeserialize() {}
        virtual ~IODeserialize() {}

        virtual bool read( std::string strMemberName,
                           long long& value ) = 0;
        virtual bool read( std::string strMemberName,
                           std::string& str ) = 0;

        template <typename T> bool read( std::string strMemberName, T& value );

        virtual bool isExisting( std::string strMemberName ) = 0;
    };

    class XMLSerialize: public IOSerialize {
    public:
        XMLSerialize( std::string fileName );
        XMLSerialize( std::string fileName, int verboseLevel );
        virtual ~XMLSerialize();

        virtual bool write( std::string strMemberName,
                            long long value );
        virtual bool write( std::string strMemberName,
                            std::string str);
    private:
        void writeVersion();

        std::string      m_filepath;
        Xml::Document    m_doc;
        int              m_verboseLevel;

        DECLARE_DEBUG_MODULE;

        Xml::Node* getNodePath( Xml::Node* pRootNode,
                                std::vector<std::string>& tokens );
    };

    class XMLDeserialize: public IODeserialize {
    public:
        XMLDeserialize( std::string fileName );
        XMLDeserialize( std::string fileName, int verboseLevel );
        virtual ~XMLDeserialize();

        virtual bool read( std::string strMemberName,
                           long long& value );
        virtual bool read( std::string strMemberName,
                           std::string& str );

        virtual bool isExisting( std::string strMemberName );
        bool isValid();
        bool checkVersion();
    private:
        std::string      m_filepath;
        Xml::Document    m_doc;
        int              m_verboseLevel;

        DECLARE_DEBUG_MODULE;
    };


//////////////////////////////////////////

    template <typename T> bool IOSerialize::write( std::string strMemberName,
                                                   T value )
    {
        return write( strMemberName, static_cast<long long>( value ) );
    }

    template <typename T> bool IODeserialize::read( std::string strMemberName,
                                                    T& value )
    {
        long long tmp;
        bool result = read( strMemberName, tmp );
        value = static_cast<T>( tmp );
        return result;
    }
}

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ");

#endif
