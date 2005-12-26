/* ieee1394io.cpp
   Copyright (C) 2004 by Daniel Wagner

   This file is part of FreeBob.

   FreeBob is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   FreeBob is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with FreeBob; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
   MA 02111-1307 USA.  */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
 
#include "ieee1394io.h"

/* raw1394handle_t Ieee1394Io::m_ieee1394Handle = NULL; */
/* pthread_mutex_t Ieee1394Io::m_mutex; */

Ieee1394Io::Ieee1394Io (int iPort) 
  : m_iPort (iPort)
    , m_isRunning (false)
{
  pthread_mutex_init (&m_mutex, NULL);
}

Ieee1394Io::~Ieee1394Io ()
{
  stopThread ();
  if (m_ieee1394Handle != NULL) {
    raw1394_destroy_handle (m_ieee1394Handle);
    m_ieee1394Handle = NULL;
  }
}

EReturnCodes 
Ieee1394Io::initialize ()
{
  m_ieee1394Handle = raw1394_new_handle ();
  if (m_ieee1394Handle == NULL) {
    if (!errno) {
      fprintf (stderr, "libraw1394 not compatible.\n");
    } else {
      perror ("Could not get 1394 handle");
      fprintf (stderr, "Is ieee1394 and raw1394 driver loaded?\n");
    }
    return eRC_Creating1394HandleFailed;
  }

  raw1394_set_userdata (m_ieee1394Handle, this);

  if (raw1394_set_port (m_ieee1394Handle, m_iPort) < 0) {
    perror ("Could not set port");
    return eRC_Setting1394PortFailed;
  }

  raw1394_set_bus_reset_handler (m_ieee1394Handle, this->resetHandler);

  startThread ();

  return eRC_Success;
}

int 
Ieee1394Io::resetHandler (raw1394handle_t handle, 
			  unsigned int generation)
{
  printf ("Bus reset received.\n");
  raw1394_update_generation (handle, generation);
  Ieee1394Io* pMe = (Ieee1394Io*) raw1394_get_userdata (handle);
  pMe->sigGeneration(generation);
  return 0;
}

bool 
Ieee1394Io::startThread()
{
  if (m_isRunning) {
    return true;
  }
  pthread_mutex_lock (&m_mutex);
  pthread_create (&m_thread, NULL, Thread, this);
  pthread_mutex_unlock (&m_mutex);
  m_isRunning = true;
  return true;
}

void
Ieee1394Io::stopThread()
{
  if (m_isRunning) {
    pthread_mutex_lock (&m_mutex);
    pthread_cancel (m_thread);
    pthread_join (m_thread, NULL);
    pthread_mutex_unlock (&m_mutex);
  }
  m_isRunning = false;
}

void* Ieee1394Io::Thread(void* arg)
{
  Ieee1394Io* pIeee1394Io = (Ieee1394Io*) arg;

  while (true) {
    raw1394_loop_iterate (pIeee1394Io->m_ieee1394Handle);
    pthread_testcancel ();
  }

  return NULL;
}

EReturnCodes
Ieee1394Io::getNodeCount (int* iNodeCount)
{
  *iNodeCount = raw1394_get_nodecount (m_ieee1394Handle);
  return eRC_Success;
}

EReturnCodes 
Ieee1394Io::getRomDirectory (NodeIdT nodeId, rom1394_directory* pRomDir)
{
  rom1394_get_directory (m_ieee1394Handle, nodeId, pRomDir);
  return eRC_Success;
}

EReturnCodes 
Ieee1394Io::getBusInfoBlockLength (NodeIdT nodeId, int* pLength)
{
  *pLength = rom1394_get_bus_info_block_length (m_ieee1394Handle, nodeId);
  return eRC_Success;
}

EReturnCodes 
Ieee1394Io::getBusOptions (NodeIdT nodeId, rom1394_bus_options* pBusOptions)
{
  rom1394_get_bus_options (m_ieee1394Handle, nodeId, pBusOptions);
  return eRC_Success;
}

EReturnCodes 
Ieee1394Io::getGuid (NodeIdT nodeId, octlet_t* pGuid)
{
  *pGuid = rom1394_get_guid (m_ieee1394Handle, nodeId);
  return eRC_Success;
}

EReturnCodes 
Ieee1394Io::getBusId (NodeIdT nodeId, int* pBusId)
{
  *pBusId = rom1394_get_bus_id (m_ieee1394Handle, nodeId);
  return eRC_Success;
}
