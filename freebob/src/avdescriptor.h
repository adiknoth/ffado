/* avdescriptor.h
 * Copyright (C) 2004 by Pieter Palmers
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

#ifndef AVDESCRIPTOR_H
#define AVDESCRIPTOR_H

#include "avdevice.h"

class AvDescriptor {
 public:
    AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type);
    virtual ~AvDescriptor();
    
    void Load();
    bool isLoaded();
    bool isPresent();
    unsigned int getLength();
    unsigned char readByte(unsigned int address);
    unsigned int readWord(unsigned int address);
    unsigned int readBuffer(unsigned int address, unsigned int length, unsigned char *buffer);
    void OpenReadWrite();
    void OpenReadOnly();
    void Close();
    bool isOpen();
    bool canWrite();
    
 private:
    AvDevice *cParent;
    unsigned int iType;
    unsigned char *aContents;
    bool bLoaded;
    bool bOpen;
    unsigned int iAccessType;
    unsigned int iLength;
    quadlet_t qTarget;
};

#endif
