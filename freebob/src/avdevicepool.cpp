/* avdevicepool.cpp
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

#include <queue>

#include "avdevice.h"
#include "avdevicepool.h"

AvDevicePool* AvDevicePool::m_pInstance = 0;

AvDevicePool::AvDevicePool()
{
}

AvDevicePool::~AvDevicePool()
{
}

AvDevicePool*
AvDevicePool::instance()
{
    if ( !m_pInstance ) {
        m_pInstance = new AvDevicePool;
    }
    return m_pInstance;
}

FBReturnCodes
AvDevicePool::registerAvDevice(AvDevice* pAvDevice)
{
    m_avDevices.push_back( pAvDevice );
    return eFBRC_Success;
}


FBReturnCodes
AvDevicePool::unregisterAvDevice(AvDevice* pAvDevice)
{
    AvDeviceVector::iterator iter;
    for ( iter = m_avDevices.begin();
          iter != m_avDevices.end();
          ++iter )
    {
        if ( ( *iter ) == pAvDevice ) {
            m_avDevices.erase( iter );
            return eFBRC_Success;
        }
    }

    if ( iter == m_avDevices.end() ) {
        return eFBRC_AvDeviceNotFound;
    }

    return eFBRC_Success;
}

AvDevice*
AvDevicePool::getAvDevice(octlet_t oGuid)
{
    AvDevice* pAvDevice = 0;
    for ( AvDeviceVector::iterator iter = m_avDevices.begin();
          iter != m_avDevices.end();
          ++iter )
    {
        if ( ( *iter )->getGuid() == oGuid ) {
            pAvDevice = *iter;
            break;
        }
    }
    return pAvDevice;
}

FBReturnCodes
AvDevicePool::removeObsoleteDevices( unsigned int iGeneration )
{
    // XXX dw: removing elements can be done more elegant.
    std::queue< AvDevice* > deleteQueue;

    for ( AvDeviceVector::iterator iter = m_avDevices.begin();
          iter != m_avDevices.end();
          ++iter )
    {
        if ( ( *iter )->getGeneration() < iGeneration ) {
            deleteQueue.push( *iter );
        }
    }

    while ( !deleteQueue.empty() ) {
        AvDevice* pAvDevice = deleteQueue.front();
        deleteQueue.pop();
        delete pAvDevice;
    }

    return eFBRC_Success;
}
