/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 * Copyright (C) 2005-2007 by Pieter Palmers
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

#include "OscServer.h"
#include "OscArgument.h"
#include "OscMessage.h"
#include "OscResponse.h"
#include "OscNode.h"

#include <signal.h>
#include <stdio.h>
#include <string>

using namespace OSC;

#define TEST_SHOULD_RETURN_TRUE(test) \
    (test ? true : printf( "'" #test "' should return true\n") && false )
#define TEST_SHOULD_RETURN_FALSE(test) \
    (test ? printf( "'" #test "' should return true\n") && false : true )

static int run=0;

static void sighandler (int sig)
{
        run = 0;
}

///////////////////////////////////////

static bool
test_OscMessage_T1() {
    bool result=true;
    OscMessage m=OscMessage();

    m.addArgument((float)1.1);
    m.addArgument("teststring");
    m.addArgument(1);
    m.print();

    result &= TEST_SHOULD_RETURN_TRUE(m.nbArguments()==3);
    result &= TEST_SHOULD_RETURN_TRUE(m.getArgument(0).getFloat()==(float)1.1);
    result &= TEST_SHOULD_RETURN_TRUE(m.getArgument(1).getString()==string("teststring"));
    result &= TEST_SHOULD_RETURN_TRUE(m.getArgument(2).getInt()==1);

    return result;
}


static bool
test_OscNode_T1() {
    bool result=true;
    OscMessage m=OscMessage();

    OscNode n1=OscNode("base1");
    OscNode n2=OscNode("child1");
    OscNode n3=OscNode("child2");
    OscNode n4=OscNode("subchild1");
    OscNode n5=OscNode("subchild2");

    OscNode n6=OscNode("subsubchild1");
    OscNode n7=OscNode("subsubchild2");

    OscNode n8=OscNode("auto1");
    OscNode n9=OscNode("auto2");

    n1.addChildOscNode(&n2);
    n1.addChildOscNode(&n3);
    n2.addChildOscNode(&n4);
    n2.addChildOscNode(&n5);

    n1.addChildOscNode(&n6,"base1/child1/");
    n1.addChildOscNode(&n7,"base1/child1/");

    n1.addChildOscNode(&n8,"auto1/test/");
    n1.addChildOscNode(&n9,"auto1/test2/");

    n1.printOscNode();

    result &= TEST_SHOULD_RETURN_FALSE(n1.processOscMessage("base1",&m).isError());
    result &= TEST_SHOULD_RETURN_FALSE(n1.processOscMessage("base1/child1",&m).isError());
    result &= TEST_SHOULD_RETURN_FALSE(n1.processOscMessage("base1/child1/subchild1",&m).isError());
    result &= TEST_SHOULD_RETURN_FALSE(n1.processOscMessage("base1/child1/subchild2",&m).isError());
    result &= TEST_SHOULD_RETURN_TRUE(n1.processOscMessage("base1/child1/subchild3",&m).isError());
    result &= TEST_SHOULD_RETURN_FALSE(n1.processOscMessage("base1/child2",&m).isError());
    result &= TEST_SHOULD_RETURN_TRUE(n1.processOscMessage("base1/child2/subchild1",&m).isError());

    return result;
}

////////////////////////////////

class OscTestNode
    : public OscNode
{
public:
    OscTestNode(std::string s)
        : OscNode(s) {};
    virtual ~OscTestNode() {};

    virtual OscResponse processOscMessage(OscMessage *m) {
        return OscResponse(*m);
    };
};

static bool
test_OscServer_T1() {
    bool result=true;

    OscServer s=OscServer("17820");

    if (!s.init()) {
        printf("failed to init server");
        return false;
    }

    OscNode n1=OscNode("base1");
    OscNode n2=OscNode("child1");
    OscNode n3=OscNode("child2");
    OscNode n4=OscNode("subchild1");
    OscNode n5=OscNode("subchild2");

    OscNode n6=OscNode("subsubchild1");
    OscNode n7=OscNode("subsubchild2");

    OscNode n8=OscNode("auto1");
    OscNode n9=OscNode("auto2");

    n1.addChildOscNode(&n2);
    n1.addChildOscNode(&n3);
    n2.addChildOscNode(&n4);
    n2.addChildOscNode(&n5);

    n1.addChildOscNode(&n6,"base1/child1/");
    n1.addChildOscNode(&n7,"base1/child1/");

    n1.addChildOscNode(&n8,"auto1/test/");
    n1.addChildOscNode(&n9,"auto1/test2/");

    OscTestNode n10=OscTestNode("base2");

    if (!s.registerAtRootNode(&n1)) {
        printf("failed to register base1 at server");
        return false;
    }
    if (!s.registerAtRootNode(&n10)) {
        printf("failed to register base2 at server");
        return false;
    }

    if (!s.start()) {
        printf("failed to start server");
        return false;
    }

    printf("server started\n");
    printf("press ctrl-c to stop it & continue\n");

    signal (SIGINT, sighandler);

    run=1;
    while(run) {
        sleep(1);
        fflush(stdout);
        fflush(stderr);
    }
    signal (SIGINT, SIG_DFL);

    if (!s.stop()) {
        printf("failed to stop server");
        return false;
    }

    if (!s.unregisterAtRootNode(&n1)) {
        printf("failed to unregister base1 at server");
        return false;
    }
    if (!s.unregisterAtRootNode(&n10)) {
        printf("failed to unregister base2 at server");
        return false;
    }

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
    { "Message test 1",test_OscMessage_T1 },
    { "Node test 1",test_OscNode_T1 },
    { "Server test 1",test_OscServer_T1 },
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
