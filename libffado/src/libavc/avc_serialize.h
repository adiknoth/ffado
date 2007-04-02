/* avc_serialize.h
 * Copyright (C) 2005,07 by Daniel Wagner
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

#ifndef Serialize_h
#define Serialize_h

#include <libraw1394/raw1394.h> // byte_t and quadlet_t declaration
#include <string>

// Interfaces

class IOSSerialize {
public:
    IOSSerialize() {}
    virtual ~IOSSerialize() {}

    virtual bool write( byte_t value, const char* name = "" ) = 0;
    virtual bool write( quadlet_t value, const char* name = "" ) = 0;
};

class IISDeserialize {
public:
    IISDeserialize() {}
    virtual ~IISDeserialize() {}

    virtual bool read( byte_t* value ) = 0;
    virtual bool read( quadlet_t* value ) = 0;
    virtual bool read( char** value, size_t length ) = 0;
    virtual bool peek( byte_t* value ) = 0;
};

// Specialized implementations of previously defined interfaces

class CoutSerializer: public IOSSerialize {
public:
    CoutSerializer()
        : IOSSerialize()
        , m_cnt( 0 )
        {}
    virtual ~CoutSerializer() {}

    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );

private:
    unsigned int m_cnt;

};

class StringSerializer: public IOSSerialize {
public:
    StringSerializer()
        : IOSSerialize()
        , m_cnt( 0 )
        {}
    virtual ~StringSerializer() {}

    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );
    virtual std::string getString( ) { return m_string;};

private:
    unsigned int m_cnt;
    std::string m_string;

};

class BufferSerialize: public IOSSerialize {
public:
    BufferSerialize( unsigned char* buffer, size_t length )
        : IOSSerialize()
        , m_buffer( buffer )
        , m_curPos( m_buffer )
        , m_length( length )
        {}
    virtual ~BufferSerialize() {}

    virtual bool write( byte_t value, const char* name = "" );
    virtual bool write( quadlet_t value,  const char* name = "" );

    int getNrOfProducesBytes() const
	{ return m_curPos - m_buffer; }

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
        : IISDeserialize()
        , m_buffer( const_cast<unsigned char*>( buffer ) )
        , m_curPos( m_buffer )
        , m_length( length )
        {}
    virtual ~BufferDeserialize() {}

    virtual bool read( byte_t* value );
    virtual bool read( quadlet_t* value );
    virtual bool read( char** value, size_t length );
    virtual bool peek( byte_t* value );

    int getNrOfConsumedBytes()  const
        { return m_curPos - m_buffer; }

protected:
    inline bool isCurPosValid() const;

private:
    unsigned char* m_buffer; // start of the buffer
    unsigned char* m_curPos; // current read pos
    size_t m_length;         // size of buffer
};

#endif // Serialize_h

