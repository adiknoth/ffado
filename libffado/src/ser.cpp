/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * FFADO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFADO; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#include "devicemanager.h"
#include "libutil/serialize.h"

#include <iostream>

const char FileName[] = "serialize-test.xml";

static bool
serialize( const char* pFileName, int port )
{
    DeviceManager devMgr;
    if (!devMgr.initialize( port )) {
        std::cerr << "could not init DeviceManager" << std::endl;
        return false;
    }

//     devMgr.setVerboseLevel(DEBUG_LEVEL_VERBOSE);

    if (!devMgr.discover( )) {
        std::cerr << "could not discover devices" << std::endl;
        return false;
    }
    return devMgr.saveCache( pFileName );
}

static bool
deserialize( const char* pFileName )
{
    DeviceManager devMgr;
    if (!devMgr.initialize( 0 )) {
        std::cerr << "could not init DeviceManager" << std::endl;
        return false;
    }
    return devMgr.loadCache( pFileName );
}

int
main(  int argc,  char** argv )
{
    int port=0;
    if (argc==2) {
        port=atoi(argv[1]);
    }

    if ( !serialize( FileName, port ) ) {
        std::cerr << "serializing failed" << std::endl;
        return -1;
    }
    if ( !deserialize( FileName ) ) {
        std::cerr << "deserializing failed" << std::endl;
        return -1;
    }

    std::cout << "passed" << std::endl;
    return 0;
}
