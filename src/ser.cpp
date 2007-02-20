#include "devicemanager.h"
#include "libutil/serialize.h"

#include <iostream>

const char FileName[] = "bebob.xml";

static bool
serialize( const char* pFileName )
{
    DeviceManager devMgr;
    devMgr.initialize( 0 );
    devMgr.discover( 0 );
    return devMgr.saveCache( pFileName );
}

static bool
deserialize( const char* pFileName )
{
    DeviceManager devMgr;
    devMgr.initialize( 0 );
    return devMgr.loadCache( pFileName );
}

int
main(  int argc,  char** argv )
{
    if ( !serialize( FileName ) ) {
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
