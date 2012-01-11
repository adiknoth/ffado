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

#include "PosixSharedMemory.h"

#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

namespace Util {

IMPL_DEBUG_MODULE( PosixSharedMemory, PosixSharedMemory, DEBUG_LEVEL_NORMAL );

PosixSharedMemory::PosixSharedMemory(std::string name, unsigned int size)
: m_name( "/" + name )
, m_size( size )
, m_owner( false )
, m_access( NULL )
{

}

PosixSharedMemory::~PosixSharedMemory()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) destroy\n",
                this, m_name.c_str());
    Close();
    if(m_owner) {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "(%p, %s) unlink\n",
                    this, m_name.c_str());
        shm_unlink(m_name.c_str());
    }
}

bool
PosixSharedMemory::Open(enum eDirection d)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) open\n",
                this, m_name.c_str());
    if(m_access != NULL) {
        debugError("(%p, %s) already attached to segment\n", this, m_name.c_str());
    }

    int flags=0;
    switch(d) {
        case eD_ReadOnly: flags |= O_RDONLY; break;
        case eD_WriteOnly: flags |= O_WRONLY; break;
        case eD_ReadWrite: flags |= O_RDWR; break;
        default:
            debugError("bad direction\n");
            return false;
    }

    // open the shared memory segment
    int fd = shm_open(m_name.c_str(), flags, S_IRWXU);
    if (fd < 0) {
        if (errno != ENOENT) {
            debugError("(%p, %s) Cannot open shared memory: %s\n",
                       this, m_name.c_str(), strerror (errno));
        } else {
            debugError("(%p, %s) shared memory segment does not exist: %s\n",
                       this, m_name.c_str(), strerror (errno));
        }
        close(fd);
        return false;
    }

    // mmap the memory
    flags=0;
    switch(d) {
        case eD_ReadOnly: flags |= PROT_READ; break;
        case eD_WriteOnly: flags |= PROT_WRITE; break;
        case eD_ReadWrite: flags |= PROT_READ|PROT_WRITE; break;
        default:
            debugError("bad direction\n");
            shm_unlink(m_name.c_str());
            return false;
    }

    m_access = (char*)mmap(0, m_size, flags, MAP_SHARED, fd, 0);
    if (m_access == MAP_FAILED) {
        debugError("(%p, %s) Cannot mmap shared memory: %s\n",
                    this, m_name.c_str(), strerror (errno));
        close(fd);
        m_access = NULL;
        shm_unlink(m_name.c_str());
        return false;
    }

    // close the fd
    close(fd);
    return true;
}

bool
PosixSharedMemory::Create(enum eDirection d)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) create dir: %d, size: %u \n",
                this, m_name.c_str(), d, m_size);
    if(m_access != NULL) {
        debugError("(%p, %s) already attached to segment\n", this, m_name.c_str());
    }

    // open the shared memory segment
    // always create it readwrite, if not, the other side can't map
    // it correctly, nor can we truncate it to the right length.
    int fd = shm_open(m_name.c_str(), O_RDWR|O_CREAT, S_IRWXU);
    if (fd < 0) {
        debugError("(%p, %s) Cannot open shared memory: %s\n",
                    this, m_name.c_str(), strerror (errno));
        close(fd);
        return false;
    }

    // set size
    if (ftruncate (fd, m_size) < 0) {
        debugError("(%p, %s) Cannot set shared memory size: %s\n",
                    this, m_name.c_str(), strerror (errno));
        close(fd);
        return false;
    }

    // mmap the memory
    int flags=0;
    switch(d) {
        case eD_ReadOnly: flags |= PROT_READ; break;
        case eD_WriteOnly: flags |= PROT_WRITE; break;
        case eD_ReadWrite: flags |= PROT_READ|PROT_WRITE; break;
        default:
            debugError("bad direction\n");
            return false;
    }

    m_access = (char*)mmap(0, m_size, flags, MAP_SHARED, fd, 0);
    if (m_access == MAP_FAILED) {
        debugError("(%p, %s) Cannot mmap shared memory: %s\n",
                    this, m_name.c_str(), strerror (errno));
        close(fd);
        m_access = NULL;
        return false;
    }

    // close the fd
    close(fd);

    m_owner = true;
    return true;
}

bool
PosixSharedMemory::Close()
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) close\n",
                this, m_name.c_str());
    if(m_access) {
        if(munmap(m_access, m_size)) {
            debugError("(%p, %s) Cannot munmap shared memory: %s\n",
                       this, m_name.c_str(), strerror (errno));
            return false;
        }
        m_access = NULL;
    } else {
        debugOutput(DEBUG_LEVEL_VERBOSE, 
                    "(%p, %s) not open\n",
                    this, m_name.c_str());
    }
    return true;
}

enum PosixSharedMemory::eResult
PosixSharedMemory::Write(unsigned int offset, void * buff, unsigned int len)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) write\n",
                this, m_name.c_str());
    if(offset+len <= m_size) {
        char * addr = m_access + offset;
        memcpy(addr, buff, len);
        // if(msync(addr, len, MS_SYNC | MS_INVALIDATE)) {
        //     debugError("Could not sync written block\n");
        //     return eR_Error;
        // }
        return eR_OK;
    } else {
        debugError("Requested block (%u) out of range (%u)\n", offset+len, m_size);
        return eR_Error;
    }
}

enum PosixSharedMemory::eResult
PosixSharedMemory::Read(unsigned int offset, void * buff, unsigned int len)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) read\n",
                this, m_name.c_str());
    if(offset+len <= m_size) {
        char * addr = m_access + offset;
        memcpy(buff, addr, len);
        return eR_OK;
    } else {
        debugError("Requested block (%u) out of range (%u)\n", offset+len, m_size);
        return eR_Error;
    }
}

void*
PosixSharedMemory::requestBlock(unsigned int offset, unsigned int len)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) getBlock\n",
                this, m_name.c_str());
    if(offset+len <= m_size) {
        return (void*)(m_access + offset);
    } else {
        debugError("Requested block (%u) out of range (%u)\n", offset+len, m_size);
        return NULL;
    }
}

void
PosixSharedMemory::commitBlock(unsigned int offset, unsigned int len)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) commitBlock\n",
                this, m_name.c_str());
    if(offset+len >= m_size) {
        debugError("Committed block (%u) out of range (%u)\n", offset+len, m_size);
    }
}

bool
PosixSharedMemory::LockInMemory(bool lock)
{
    debugOutput(DEBUG_LEVEL_VERBOSE, 
                "(%p, %s) LockInMemory\n",
                this, m_name.c_str());
    if(lock) {
        if(mlock(m_access, m_size)) {
            debugError("(%p, %s) Cannot mlock shared memory: %s\n",
                       this, m_name.c_str(), strerror (errno));
            return false;
        } else return true;
    } else {
        if(munlock(m_access, m_size)) {
            debugError("(%p, %s) Cannot munlock shared memory: %s\n",
                       this, m_name.c_str(), strerror (errno));
            return false;
        } else return true;
    }
    return false;
}

void
PosixSharedMemory::show()
{
    debugOutput(DEBUG_LEVEL_NORMAL, "(%p) PosixSharedMemory %s\n", this, m_name.c_str());
}

} // namespace Util
