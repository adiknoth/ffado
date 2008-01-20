/*
 * Copyright (C) 2005-2008 by Pieter Palmers
 * Copyright (C) 2005-2008 by Daniel Wagner
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

#ifndef __FFADO_ARMHANDLER__
#define __FFADO_ARMHANDLER__

#include "../debugmodule/debugmodule.h"

#include <libraw1394/raw1394.h>

#include "ieee1394service.h"

/**
 * @brief Class to handle AddressRangeMappings
 *
 * This class is intended to help with implementing
 * address range mapping, i.e. implementing handlers
 * that react to reads/writes of certain addresses
 * in 1394 memory space
 *
 * see the _arm_ functions in raw1394.h for more insight
 *
 */

class ARMHandler {
friend class Ieee1394Service;

public:

    ARMHandler(nodeaddr_t start, size_t length,
               unsigned int access_rights,
               unsigned int notification_options,
               unsigned int client_transactions
              );

    virtual ~ARMHandler();

    bool handleRead(struct raw1394_arm_request  *);
    bool handleWrite(struct raw1394_arm_request  *);
    bool handleLock(struct raw1394_arm_request  *);

    struct raw1394_arm_response *getResponse() {return &m_response;};

    nodeaddr_t getStart() {return m_start;};
    nodeaddr_t getLength() {return m_length;};
    unsigned int getAccessRights() {return m_access_rights;};
    unsigned int getNotificationOptions() {return m_notification_options;};
    unsigned int getClientTransactions() {return m_client_transactions;};

    byte_t *getBuffer() {return m_buffer;};

private:
    nodeaddr_t m_start;
    size_t m_length;
    unsigned int m_access_rights;
    unsigned int m_notification_options;
    unsigned int m_client_transactions;

    byte_t *m_buffer;

    struct raw1394_arm_response m_response;

    void printBufferBytes( unsigned int level, size_t length, byte_t* buffer ) const;
    void printRequest(struct raw1394_arm_request *arm_req);

protected:


    DECLARE_DEBUG_MODULE;

};

#endif /* __FFADO_ARMHANDLER__ */


