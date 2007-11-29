/* unittests.cpp
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "serialize.h"
#include "OptionContainer.h"

#include <libraw1394/raw1394.h>

#include <stdio.h>

using namespace Util;

#define TEST_SHOULD_RETURN_TRUE(test) \
    (test ? true : printf( "'" #test "' should return true\n") && false )
#define TEST_SHOULD_RETURN_FALSE(test) \
    (test ? printf( "'" #test "' should return true\n") && false : true )

///////////////////////////////////////

class U0_SerializeMe {
public:
    U0_SerializeMe();

    bool operator == ( const U0_SerializeMe& rhs );

    bool serialize( IOSerialize& ser );
    bool deserialize( IODeserialize& deser );

    byte_t m_byte;
    quadlet_t m_quadlet;
};

U0_SerializeMe::U0_SerializeMe()
    : m_byte( 0 )
    , m_quadlet( 0 )
{
}


//--------------------------
bool
U0_SerializeMe::operator == ( const U0_SerializeMe& rhs )
{
    return    ( m_byte == rhs.m_byte )
           && ( m_quadlet == rhs.m_quadlet );
}

bool
U0_SerializeMe::serialize( IOSerialize& ser )
{
    bool result;
    result  = ser.write( "SerializeMe/m_byte", m_byte );
    result &= ser.write( "SerializeMe/m_quadlet", m_quadlet );
    return result;
}

bool
U0_SerializeMe::deserialize( IODeserialize& deser )
{
    bool result;
    result  = deser.read( "SerializeMe/m_byte", m_byte );
    result &= deser.read( "SerializeMe/m_quadlet", m_quadlet );
    return result;
}

static bool
testU0()
{
    U0_SerializeMe sme1;

    sme1.m_byte = 0x12;
    sme1.m_quadlet = 0x12345678;

    {
        XMLSerialize xmlSerialize( "unittest_u0.xml" );
        if ( !sme1.serialize( xmlSerialize ) ) {
            printf( "(serializing failed)" );
            return false;
        }
    }

    U0_SerializeMe sme2;

    {
        XMLDeserialize xmlDeserialize( "unittest_u0.xml" );
        if ( !sme2.deserialize( xmlDeserialize ) ) {
            printf( "(deserializing failed)" );
            return false;
        }
    }

    bool result = sme1 == sme2;
    if ( !result ) {
        printf( "(wrong values)" );
    }
    return result;
}


///////////////////////////////////////

class U1_SerializeMe {
public:
    U1_SerializeMe();

    bool operator == ( const U1_SerializeMe& rhs );

    bool serialize( IOSerialize& ser );
    bool deserialize( IODeserialize& deser );

    quadlet_t m_quadlet0;
    quadlet_t m_quadlet1;
    quadlet_t m_quadlet2;
};

U1_SerializeMe::U1_SerializeMe()
    : m_quadlet0( 0 )
    , m_quadlet1( 0 )
    , m_quadlet2( 0 )
{
}


//--------------------------
bool
U1_SerializeMe::operator == ( const U1_SerializeMe& rhs )
{
    return    ( m_quadlet0 == rhs.m_quadlet0 )
           && ( m_quadlet1 == rhs.m_quadlet1)
           && ( m_quadlet2 == rhs.m_quadlet2);
}

bool
U1_SerializeMe::serialize( IOSerialize& ser )
{
    bool result;
    result  = ser.write( "here/and/not/there/m_quadlet0", m_quadlet0 );
    result &= ser.write( "here/and/not/m_quadlet1", m_quadlet1 );
    result &= ser.write( "here/and/m_quadlet2", m_quadlet2 );
    return result;
}

bool
U1_SerializeMe::deserialize( IODeserialize& deser )
{
    bool result;
    result  = deser.read( "here/and/not/there/m_quadlet0", m_quadlet0 );
    result &= deser.read( "here/and/not/m_quadlet1", m_quadlet1 );
    result &= deser.read( "here/and/m_quadlet2", m_quadlet2 );
    return result;
}

static bool
testU1()
{
    U1_SerializeMe sme1;

    sme1.m_quadlet0 = 0;
    sme1.m_quadlet1 = 1;
    sme1.m_quadlet2 = 2;

    {
        XMLSerialize xmlSerialize( "unittest_u1.xml" );
        if ( !sme1.serialize( xmlSerialize ) ) {
            printf( "(serializing failed)" );
            return false;
        }
    }

    U1_SerializeMe sme2;

    {
        XMLDeserialize xmlDeserialize( "unittest_u1.xml" );
        if ( !sme2.deserialize( xmlDeserialize ) ) {
            printf( "(deserializing failed)" );
            return false;
        }
    }

    bool result = sme1 == sme2;
    if ( !result ) {
        printf( "(wrong values)" );
    }
    return result;
}

///////////////////////////////////////

class U2_SerializeMe {
public:
    U2_SerializeMe();

    bool operator == ( const U2_SerializeMe& rhs );

    bool serialize( IOSerialize& ser );
    bool deserialize( IODeserialize& deser );

    char             m_char;
    unsigned char    m_unsigned_char;
    short            m_short;
    unsigned short   m_unsigned_short;
    int              m_int;
    unsigned int     m_unsigned_int;
};

U2_SerializeMe::U2_SerializeMe()
    : m_char( 0 )
    , m_unsigned_char( 0 )
    , m_short( 0 )
    , m_unsigned_short( 0 )
    , m_int( 0 )
    , m_unsigned_int( 0 )
{
}

//--------------------------

bool
U2_SerializeMe::operator == ( const U2_SerializeMe& rhs )
{
    return    ( m_char == rhs.m_char )
           && ( m_unsigned_char == rhs.m_unsigned_char )
           && ( m_short == rhs.m_short )
           && ( m_unsigned_short == rhs.m_unsigned_short )
           && ( m_int == rhs.m_int )
           && ( m_unsigned_int == rhs.m_unsigned_int );
}

bool
U2_SerializeMe::serialize( IOSerialize& ser )
{
    bool result;
    result  = ser.write( "m_char", m_char );
    result &= ser.write( "m_unsigned_char", m_unsigned_char );
    result &= ser.write( "m_short", m_short );
    result &= ser.write( "m_unsigned_short", m_unsigned_short );
    result &= ser.write( "m_int", m_int );
    result &= ser.write( "m_unsigned_int", m_unsigned_int );
    return result;
}

bool
U2_SerializeMe::deserialize( IODeserialize& deser )
{
    bool result;
    result  = deser.read( "m_char", m_char );
    result &= deser.read( "m_unsigned_char", m_unsigned_char );
    result &= deser.read( "m_short", m_short );
    result &= deser.read( "m_unsigned_short", m_unsigned_short );
    result &= deser.read( "m_int", m_int );
    result &= deser.read( "m_unsigned_int", m_unsigned_int );
    return result;
}

static bool
testU2execute( U2_SerializeMe& sme1 )
{
    {
        XMLSerialize xmlSerialize( "unittest_u2.xml" );
        if ( !sme1.serialize( xmlSerialize ) ) {
            printf( "(serializing failed)" );
            return false;
        }
    }

    U2_SerializeMe sme2;

    {
        XMLDeserialize xmlDeserialize( "unittest_u2.xml" );
        if ( !sme2.deserialize( xmlDeserialize ) ) {
            printf( "(deserializing failed)" );
            return false;
        }
    }

    bool result = sme1 == sme2;
    if ( !result ) {
        printf( "(wrong values)" );
    }

    return result;
}

static bool
testU2()
{
    U2_SerializeMe sme1;

    sme1.m_char = 0;
    sme1.m_unsigned_char = 1;
    sme1.m_short = 2;
    sme1.m_unsigned_short = 3;
    sme1.m_int = 4;
    sme1.m_unsigned_int = 5;

    bool result;
    result  = testU2execute( sme1 );

    sme1.m_char = 0xff;
    sme1.m_unsigned_char = 0xff;
    sme1.m_short = 0xffff;
    sme1.m_unsigned_short = 0xffff;
    sme1.m_int = 0xffffffff;
    sme1.m_unsigned_int = 0xffffffff;

    result &= testU2execute( sme1 );

    return result;
}

///////////////////////////////////////

class U3_SerializeMe {
public:
    U3_SerializeMe();
    ~U3_SerializeMe();

    bool operator == ( const U3_SerializeMe& rhs );

    bool serialize( IOSerialize& ser );
    bool deserialize( IODeserialize& deser );

    const char* m_pString;
};

U3_SerializeMe::U3_SerializeMe()
    : m_pString( 0 )
{
}


U3_SerializeMe::~U3_SerializeMe()
{
    delete m_pString;
}
//--------------------------

bool
U3_SerializeMe::operator == ( const U3_SerializeMe& rhs )
{
    return strcmp( m_pString, rhs.m_pString ) == 0;
}

bool
U3_SerializeMe::serialize( IOSerialize& ser )
{
    bool result;
    result  = ser.write( "m_pString", Glib::ustring( m_pString ) );
    return result;
}

bool
U3_SerializeMe::deserialize( IODeserialize& deser )
{
    bool result;
    Glib::ustring str;
    result  = deser.read( "m_pString", str );

    m_pString = strdup( str.c_str() );

    return result;
}

static bool
testU3()
{
    U3_SerializeMe sme1;
    sme1.m_pString = strdup( "fancy string" );

    {
        XMLSerialize xmlSerialize( "unittest_u3.xml" );
        if ( !sme1.serialize( xmlSerialize ) ) {
            printf( "(serializing failed)" );
            return false;
        }
    }

    U3_SerializeMe sme2;

    {
        XMLDeserialize xmlDeserialize( "unittest_u3.xml" );
        if ( !sme2.deserialize( xmlDeserialize ) ) {
            printf( "(deserializing failed)" );
            return false;
        }
    }

    bool result = sme1 == sme2;
    if ( !result ) {
        printf( "(wrong values)" );
    }

    return result;
}

/////////////////////////////////////
class testOC : public OptionContainer {
public:
    testOC() {};
    ~testOC() {};

    bool test() {
        bool result=true;

        Option op1=Option();
        result &= TEST_SHOULD_RETURN_FALSE(addOption(op1));

        op1=Option("option1");
        result &= TEST_SHOULD_RETURN_FALSE(addOption(op1));

        op1=Option("option1", (float)(1.0));
        result &= TEST_SHOULD_RETURN_TRUE(addOption(op1));
        result &= TEST_SHOULD_RETURN_FALSE(addOption(op1));
        result &= TEST_SHOULD_RETURN_TRUE(removeOption(op1));
        result &= TEST_SHOULD_RETURN_FALSE(hasOption(op1));


        op1=Option("option1", (int64_t)1);
        result &= TEST_SHOULD_RETURN_TRUE(addOption(op1));

        result &= TEST_SHOULD_RETURN_TRUE(removeOption("option1"));

        result &= TEST_SHOULD_RETURN_FALSE(hasOption(op1));


        op1=Option("option1", (int64_t)(-1));
        result &= TEST_SHOULD_RETURN_TRUE(addOption(op1));

        Option op2=Option("option1", (double)(1.75));
        result &= TEST_SHOULD_RETURN_FALSE(addOption(op2));


        op2=Option("option2", (double)(1.75));
        result &= TEST_SHOULD_RETURN_TRUE(addOption(op2));

        Option op3=Option("option3", (int64_t)(1.75));
        result &= TEST_SHOULD_RETURN_TRUE(addOption(op3));


        result &= TEST_SHOULD_RETURN_TRUE(countOptions() == 3);

        int i=0;
        for ( OptionContainer::iterator it = begin();
          it != end();
          ++it )
        {
    //         printf(" (%s)",(*it).getName().c_str());
            i++;
        }
        result &= TEST_SHOULD_RETURN_TRUE(i==3);

        clearOptions();
        return result;
    }

    void prepare() {
        Option op1=Option("option1", (int64_t)(-1));
        if(!addOption(op1)) {
            printf( "prepare: could not add valid option (3)\n" );
        }

        Option op2=Option("option2", (double)(1.75));
        if(!addOption(op2)) {
            printf( "prepare: adding an option with a different name should be allowed (1)\n" );
        }
        Option op3=Option("option3", (int64_t)(1.75));
        if(!addOption(op3)) {
            printf( "prepare: adding an option with a different name should be allowed (2)\n" );
        }
    }
};

static bool
testU4()
{
    bool result=true;
    testOC oc;
    result &= TEST_SHOULD_RETURN_TRUE(oc.test());

    // now manipulate it externally
    oc.prepare();

    result &= TEST_SHOULD_RETURN_TRUE(oc.hasOption("option1"));
    result &= TEST_SHOULD_RETURN_TRUE(oc.hasOption("option2"));
    result &= TEST_SHOULD_RETURN_TRUE(oc.hasOption("option3"));
    result &= TEST_SHOULD_RETURN_FALSE(oc.hasOption("option4"));

    oc.setOption("option1", 1024);
    int tst;
    result &= TEST_SHOULD_RETURN_TRUE(oc.getOption("option1", tst));
    result &= TEST_SHOULD_RETURN_TRUE(tst == 1024);

    return result;
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////


typedef bool ( *test_func ) ();
struct TestEntry {
    const char* m_pName;
    test_func m_pFunc;
};

TestEntry TestTable[] = {
    { "serialize 0",  testU0 },
    { "serialize 1",  testU1 },
    { "serialize 2",  testU2 },
    { "serialize 3",  testU3 },
    { "OptionContainer 1",  testU4 },
};

int
main( int argc,  char** argv )
{
    int iNrOfPassedTests = 0;
    int iNrOfFailedTests = 0;
    int iNrOfTests = sizeof( TestTable )/sizeof( TestTable[0] );

    for ( int i = 0; i < iNrOfTests ; ++i ) {
        TestEntry* pEntry = &TestTable[i];

        printf( "Test \"%s\"... ", pEntry->m_pName );
        if ( pEntry->m_pFunc() ) {
            puts( " passed" );
            iNrOfPassedTests++;
        } else {
            puts( " failed" );
            iNrOfFailedTests++;
        }
    }

    printf( "passed: %d\n", iNrOfPassedTests );
    printf( "failed: %d\n", iNrOfFailedTests );
    printf( "total:  %d\n", iNrOfTests );

    return 0;
}
