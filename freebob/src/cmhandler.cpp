/* cmhandler.cpp
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

#include "cmhandler.h"
#include "ieee1394service.h"
#include "debugmodule.h"

CMHandler* CMHandler::m_pInstance = 0;

CMHandler::CMHandler()
    : m_bInitialised( false )
{
}

CMHandler::~CMHandler()
{
}

FBReturnCodes
CMHandler::initialize()
{
    if ( !m_bInitialised ) {
        m_pIeee1394Service = Ieee1394Service::instance();

        if ( !m_pIeee1394Service ) {
            debugError( "Could not get an valid instance of 1394 service.\n" );
            return eFBRC_InitializeCMHandlerFailed;
        }
        FBReturnCodes eStatus = m_pIeee1394Service->initialize();
        if ( eStatus != eFBRC_Success ) {
            debugError( "Initialising of 1394 service failed.\n" );
            return eStatus;
        }
        m_bInitialised = true;
    }
    return eFBRC_Success;
}

CMHandler*
CMHandler::instance()
{
    if ( !m_pInstance ) {
        m_pInstance = new CMHandler;
    }
    return m_pInstance;
}
