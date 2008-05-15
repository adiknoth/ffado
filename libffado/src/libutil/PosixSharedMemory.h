/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __UTIL_POSIX_SHARED_MEMORY__
#define __UTIL_POSIX_SHARED_MEMORY__

#include "debugmodule/debugmodule.h"

#include <string>

namespace Util {

/**
 * @brief A POSIX Shared Memory Wrapper.
 */

class PosixSharedMemory
{
public:
    enum eDirection {
        eD_None,
        eD_ReadOnly,
        eD_WriteOnly,
        eD_ReadWrite
    };

    enum eResult {
        eR_OK,
        eR_Again,
        eR_Error,
    };

public:
    PosixSharedMemory(std::string name, unsigned int len);
    virtual ~PosixSharedMemory();
    
    // mlock interface
    /**
     * Tries to (un)lock the segment to stay in memory
     * @param lock 
     * @return 
     */
    bool LockInMemory(bool lock);

    virtual bool Create(enum eDirection d=eD_ReadWrite);
    virtual bool Open(enum eDirection d=eD_ReadWrite);
    virtual bool Close();

    virtual enum eResult Write(unsigned int offset, void * buff, unsigned int len);
    virtual enum eResult Read(unsigned int offset, void * buff, unsigned int len);

    virtual void* requestBlock(unsigned int offset, unsigned int len);
    virtual void commitBlock(unsigned int offset, unsigned int len);

    virtual void show();
    virtual void setVerboseLevel(int l) {setDebugLevel(l);};

protected:
    DECLARE_DEBUG_MODULE;

private:
    std::string     m_name;
    unsigned int    m_size;
    bool            m_owner;
    char *          m_access;
};

} // namespace Util

#endif // __UTIL__POSIX_SHARED_MEMORY__
