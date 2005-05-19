/* serialize.h
 * Copyright (C) 2005 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef Serialize_h
#define Serialize_h

#include <libraw1394/raw1394.h> // byte_t and quadlet_t declaration

// Interfaces

class IOSSerialize {
public:
    virtual bool write( byte_t value, const char* name = "" ) = 0;
    virtual bool write( quadlet_t value, const char* name = "" ) = 0;
};

class IISDeserialize {
public:
    virtual bool read( byte_t* value ) = 0;
    virtual bool read( quadlet_t* value ) = 0;
};

// Specialized implementations of previously defined interfaces

class CoutSerializer: public IOSSerialize {
public:
    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );
};

class BufferSerialize: public IOSSerialize {
public:
    BufferSerialize( unsigned char* buffer, size_t length )
        : m_buffer( buffer )
        , m_curPos( m_buffer )
        , m_length( length )
        {}

    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );

protected:
    inline bool isCurPosValid() const;

private:
    unsigned char* m_buffer;
    unsigned char* m_curPos;
    size_t m_length;
};

class BufferDeserialize: public IISDeserialize {
public:
    BufferDeserialize( const unsigned char* buffer, size_t length )
        : m_buffer( const_cast<unsigned char*>( buffer ) )
        , m_curPos( m_buffer )
        , m_length( length )
        {}

    virtual bool read( byte_t* value );
    virtual bool read( quadlet_t* value );

protected:
    inline bool isCurPosValid() const;

private:
    unsigned char* m_buffer; // start of the buffer
    unsigned char* m_curPos; // current read pos
    size_t m_length; // length of buffer
};

#endif // Serialize_h

