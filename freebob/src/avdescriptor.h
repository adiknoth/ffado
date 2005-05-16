/* avdescriptor.h
 * Copyright (C) 2004,05 by Pieter Palmers
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
#include "debugmodule.h"

#define AVC_DESCRIPTOR_SPECIFIER_TYPE_IDENTIFIER                 0x00
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_LIST_BY_ID                 0x10
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_LIST_BY_TYPE               0x11
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_POSITION_IN_LIST  0x20
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_ID_IN_LIST        0x21
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_TYPE              0x22
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_ENTRY_BY_ID                0x23
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_INFOBLOCK_BY_TYPE          0x30
#define AVC_DESCRIPTOR_SPECIFIER_TYPE_INFOBLOCK_BY_POSITION      0x31

#define AVC_DESCRIPTOR_SUBFUNCTION_CLOSE                         0x00
#define AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READONLY                 0x01
#define AVC_DESCRIPTOR_SUBFUNCTION_OPEN_READWRITE                0x03

class AvDescriptor {
 public:
    AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type);
    AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type, unsigned int list_id, unsigned int list_id_size);
    AvDescriptor(AvDevice *parent, quadlet_t target, unsigned int type, unsigned int list_id, unsigned int list_id_size,
                                                     unsigned int object_id, unsigned int object_id_size,
                                                     unsigned int position, unsigned int position_size);
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
    bool isValid();

 protected:
    AvDevice *cParent;
    unsigned int iType;
    unsigned char *aContents;
    bool bLoaded;
    bool bOpen;
    bool bValid;
    unsigned int iAccessType;
    unsigned int iLength;
    quadlet_t qTarget;
    
    unsigned int iListId;
    unsigned int iListIdSize;
    
    unsigned int iObjectId;
    unsigned int iObjectIdSize;
    unsigned int iPosition;
    unsigned int iPositionSize;

    int ConstructDescriptorSpecifier(quadlet_t *request);
    
    DECLARE_DEBUG_MODULE;
};

#endif
