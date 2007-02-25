#include "devicemanager.h"
#include "libutil/serialize.h"

#include <iostream>

const char FileName[] = "serialize-test.xml";

static bool
serialize( const char* pFileName )
{
    DeviceManager devMgr;
    if (!devMgr.initialize( 0 )) {
        std::cerr << "could not init DeviceManager" << std::endl;
        return false;
    }
    if (!devMgr.discover( 0 )) {
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
