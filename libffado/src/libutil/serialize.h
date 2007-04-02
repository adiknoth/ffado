/* serialize.h
 * Copyright (C) 2007 by Daniel Wagner
 *
 * This file is part of FreeBoB.
 *
 * FreeBoB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBoB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBoB; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef bebob_serialize_h
#define bebob_serialize_h

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
        virtual ~XMLSerialize();

        virtual bool write( std::string strMemberName,
                            long long value );
        virtual bool write( std::string strMemberName,
                            Glib::ustring str);
    private:
        Glib::ustring    m_filepath;
        xmlpp::Document  m_doc;

        xmlpp::Node* getNodePath( xmlpp::Node* pRootNode,
                                  std::vector<std::string>& tokens );
    };

    class XMLDeserialize: public IODeserialize {
    public:
        XMLDeserialize( Glib::ustring fileName );
        virtual ~XMLDeserialize();

        virtual bool read( std::string strMemberName,
                           long long& value );
        virtual bool read( std::string strMemberName,
                           Glib::ustring& str );

        virtual bool isExisting( std::string strMemberName );

    private:
        Glib::ustring    m_filepath;
        xmlpp::DomParser m_parser;
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

#endif
