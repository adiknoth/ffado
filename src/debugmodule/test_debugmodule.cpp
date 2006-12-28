#include "debugmodule.h"

#include <iostream>

using namespace std;

class Test
{
public:
    Test()
        {}
    ~Test()
        {}

    bool run() {
        cout << "######################" << endl;
        cout << "### Test arguments ###" << endl;
        cout << "######################" << endl;
        debugOutput( DEBUG_LEVEL_NORMAL, "arg0 = %d, arg1 = 0x%08x\n"
                     , 1, 0xdeedbeef );
        cout << endl << endl;


        cout << "###################" << endl;
        cout << "### Test levels ###" << endl;
        cout << "###################" << endl;
        for ( debug_level_t level = DEBUG_LEVEL_VERBOSE;
              level >= 0;
              --level )
        {
            DebugModuleManager::instance()->setMgrDebugLevel( "Test", level );

            cout << endl << "*** Debug Level = " << level << endl << endl;

            debugFatal( "fatal text\n" );
            debugError( "error text\n" );
            debugWarning( "warning text\n" );
            debugOutput( DEBUG_LEVEL_NORMAL, "normal output\n" );
            debugOutput( DEBUG_LEVEL_VERBOSE,  "verbose output\n" );
            debugFatalShort( "fatal short text\n" );
            debugErrorShort( "error short text\n" );
            debugWarningShort( "warning short text\n" );
            debugOutputShort( DEBUG_LEVEL_NORMAL, "normal short output\n" );
            debugOutputShort( DEBUG_LEVEL_VERBOSE,  "verbose short output\n" );
        }
        cout << endl << endl;

        return true;
    }

    DECLARE_DEBUG_MODULE;
};

IMPL_DEBUG_MODULE( Test, Test, DEBUG_LEVEL_VERBOSE );

DECLARE_GLOBAL_DEBUG_MODULE;
IMPL_GLOBAL_DEBUG_MODULE( Test, DEBUG_LEVEL_VERBOSE );

int main( int argc, char** argv )
{
    cout << "#################################" << endl;
    cout << "### Test global debug module  ###" << endl;
    cout << "#################################" << endl;
    debugOutput( DEBUG_LEVEL_NORMAL, "foobar\n" );
    cout << endl << endl;

    Test test;
    test.run();

    return 0;
}

/*
 * Local variables:
 *  compile-command: "g++ -Wall -g -DDEBUG -o test test.cpp -L. -ldebugmodule"
 * End:
 */

