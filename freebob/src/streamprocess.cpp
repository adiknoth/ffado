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

#include "streamprocess.h"
#include "debugmodule.h"

StreamProcess::StreamProcess()
    : m_pCMHandler( 0 )
{
}

StreamProcess::~StreamProcess()
{
}

void
StreamProcess::run()
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

    printf( "Waiting: " );
    for ( int i = 0; i < 10; ++i ) {
        printf( "." );
        fflush( stdout );
        sleep( 1 );
    }
    printf( "\n" );

    m_pCMHandler->shutdown();
}
