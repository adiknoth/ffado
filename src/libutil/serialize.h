/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
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

#ifndef bebob_serialize_h
#define bebob_serialize_h

#include "debugmodule/debugmodule.h"

#include <libxml++/libxml++.h>

#include <iostream>

namespace Util {

     class IOSerialize {
     public:
         IOSerialize() {}
         virtual ~IOSerialize() {}

         virtual bool write( std::string strMemberName,
                             long long value ) = 0;
         virtual bool write( std::string strMemberName,
                             Glib::ustring str) = 0;

         template <typename T>  bool write( std::string strMemberName, T value );
     };

    class IODeserialize {
    public:
        IODeserialize() {}
        virtual ~IODeserialize() {}

        virtual bool read( std::string strMemberName,
                           long long& value ) = 0;
        virtual bool read( std::string strMemberName,
                           Glib::ustring& str ) = 0;

        template <typename T> bool read( std::string strMemberName, T& value );

        virtual bool isExisting( std::string strMemberName ) = 0;
    };

    class XMLSerialize: public IOSerialize {
    public:
        XMLSerialize( Glib::ustring fileName );
	XMLSerialize( Glib::ustring fileName, int verboseLevel );
        virtual ~XMLSerialize();

        virtual bool write( std::string strMemberName,
                            long long value );
        virtual bool write( std::string strMemberName,
                            Glib::ustring str);
    private:
        Glib::ustring    m_filepath;
        xmlpp::Document  m_doc;
        int              m_verboseLevel;

        DECLARE_DEBUG_MODULE;

        xmlpp::Node* getNodePath( xmlpp::Node* pRootNode,
                                  std::vector<std::string>& tokens );
    };

    class XMLDeserialize: public IODeserialize {
    public:
        XMLDeserialize( Glib::ustring fileName );
	XMLDeserialize( Glib::ustring fileName, int verboseLevel );
        virtual ~XMLDeserialize();

        virtual bool read( std::string strMemberName,
                           long long& value );
        virtual bool read( std::string strMemberName,
                           Glib::ustring& str );

        virtual bool isExisting( std::string strMemberName );

    private:
        Glib::ustring    m_filepath;
        xmlpp::DomParser m_parser;
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
