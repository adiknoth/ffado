/* test_debug_module.cpp
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

#include "../src/debugmodule.h"

class TestDebugModule {
public:

    TestDebugModule() {
        setDebugLevel( DEBUG_LEVEL_ALL );
    }

    ~TestDebugModule() {}

    void run() {

        // Simpel output
        debugPrint( DEBUG_LEVEL_INFO,       "DEBUG_LEVEL_INFO\n" );
        debugPrint( DEBUG_LEVEL_DEVICE,     "DEBUG_LEVEL_DEVICE\n" );
        debugPrint( DEBUG_LEVEL_SUBUNIT,    "DEBUG_LEVEL_SUBUNIT\n" );
        debugPrint( DEBUG_LEVEL_DESCRIPTOR, "DEBUG_LEVEL_DESCRIPTOR\n" );
        debugPrint( DEBUG_LEVEL_INFOBLOCK,  "DEBUG_LEVEL_INFOBLOCK\n" );
        debugPrint( DEBUG_LEVEL_TRANSFERS,  "DEBUG_LEVEL_TRANSFERS\n" );

        debugPrintShort( DEBUG_LEVEL_INFO,       "DEBUG_LEVEL_INFO\n" );
        debugPrintShort( DEBUG_LEVEL_DEVICE,     "DEBUG_LEVEL_DEVICE\n" );
        debugPrintShort( DEBUG_LEVEL_SUBUNIT,    "DEBUG_LEVEL_SUBUNIT\n" );
        debugPrintShort( DEBUG_LEVEL_DESCRIPTOR, "DEBUG_LEVEL_DESCRIPTOR\n" );
        debugPrintShort( DEBUG_LEVEL_INFOBLOCK,  "DEBUG_LEVEL_INFOBLOCK\n" );
        debugPrintShort( DEBUG_LEVEL_TRANSFERS,  "DEBUG_LEVEL_TRANSFERS\n" );

        debugError( "DEBUGERROR\n" );

        // "Advance stuff"
        debugError( "Wrong %s\n", "answer" );

        debugPrint( DEBUG_LEVEL_INFOBLOCK, "The answer is %d\n", 42 );
        debugPrint( DEBUG_LEVEL_INFOBLOCK, "This %s test\n", "is a" );


    }

protected:
    DECLARE_DEBUG_MODULE;
};

int
main( int argc,  char** argv )
{
    TestDebugModule test;

    printf( "start...\n" );
    test.run();
    printf( "...done\n" );
    return 0;
}

/*
 * Local variables:
 *  compile-command: "g++ -DDEBUG -Wall -g -c ../src/debugmodule.cpp -I/usr/local/include && g++ -DDEBUG -Wall -g -c test_debug_module.cpp -I/usr/local/include && g++ -DDEBUG -Wall -g -o test_debug_module debugmodule.o test_debug_module.o"
 * End:
 */
