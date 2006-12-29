/* bebob_serialize.h
 * Copyright (C) 2006 by Daniel Wagner
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

#include <libraw1394/raw1394.h>
#include <libxml++/libxml++.h>

#include <iostream>

namespace BeBoB {
    class IOSerialize {
    public:
        IOSerialize() {}
        virtual ~IOSerialize() {}

        virtual bool write( const char* pMemberName,
                            byte_t value ) = 0;
        virtual bool write( const char* pMemberName,
                            quadlet_t value ) = 0;

        template <typename T>  bool write( const char* pMemberName, T value );
    };

    class IODeserialize {
    public:
        IODeserialize() {}
        virtual ~IODeserialize() {}

        virtual bool read( const char* pMemberName,
                           byte_t& value ) = 0;
        virtual bool read( const char* pMemberName,
                           quadlet_t& value ) = 0;

        template <typename T> bool read( const char* pMemberName, T& value );
    };

    class XMLSerialize: public IOSerialize {
    public:
        XMLSerialize( const char* pFileName );
        virtual ~XMLSerialize();

        virtual bool write( const char* pMemberName,
                            byte_t value );
        virtual bool write( const char* pMemberName,
                            quadlet_t value );
    private:
        Glib::ustring    m_filepath;
        xmlpp::Document  m_doc;

	xmlpp::Node* getNodePath( xmlpp::Node* pRootNode, std::vector<std::string>& tokens );
    };

    class XMLDeserialize: public IODeserialize {
    public:
        XMLDeserialize( const char* pFileName );
        virtual ~XMLDeserialize();

        virtual bool read( const char* pMemberName,
                           byte_t& value );
        virtual bool read( const char* pMemberName,
                           quadlet_t& value );
    private:
        Glib::ustring    m_filepath;
        xmlpp::DomParser m_parser;
    };


    //////////////////////////////////////////

    template <typename T> bool IOSerialize::write( const char* pMemberName, T value )
    {
        return write( pMemberName, static_cast<quadlet_t>( value ) );
    }

    template <typename T> bool IODeserialize::read( const char* pMemberName, T& value )
    {
        return read( pMemberName, reinterpret_cast<quadlet_t&>( value ) );
    }
}

#endif
