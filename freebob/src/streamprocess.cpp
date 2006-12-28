/* streamprocess.cpp
 * Copyright (C) 2004 by Daniel Wagner
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

#include <unistd.h>
#include <signal.h>

#include "streamprocess.h"

int g_done = 0;

void sighandler (int sig)
{
	g_done = 1;
}

StreamProcess::StreamProcess()
    : m_pCMHandler( 0 )
{
    setDebugLevel( DEBUG_LEVEL_ALL );
}

StreamProcess::~StreamProcess()
{
}

void
StreamProcess::run( int timeToListen )
{
    m_pCMHandler = CMHandler::instance();
    if ( !m_pCMHandler ) {
        debugError( "Could not get valid CMHandler instance.\n" );
        return;
    }

    FBReturnCodes eStatus = m_pCMHandler->initialize();
    if ( eStatus != eFBRC_Success ) {
        debugError( "Could not initialize CMHandler.\n" );
        return;
    }

    if ( timeToListen == -1 ) {
        printf( "Press Ctrl-C to stop freebob daemon\n" );
        g_done=0;
		signal (SIGINT, sighandler);
		signal (SIGPIPE, sighandler);
		while ( !g_done ) sleep( 1 );
    } else {
        printf( "Waiting for %d seconds: ", timeToListen );
        for ( int i = 0; i < timeToListen; ++i ) {
            printf( "." );
            fflush( stdout );
            sleep( 1 );
        }
        printf( "\n" );
    }

    m_pCMHandler->shutdown();
}
