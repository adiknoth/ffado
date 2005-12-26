/* ieee1394io.h
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

#ifndef Ieee1394Io_h
#define Ieee1394Io_h

#include <libraw1394/raw1394.h>
#include <libavc1394/rom1394.h>
#include <sigc++/sigc++.h>

#include "freebob.h"

typedef unsigned int GenerationT;
typedef int NodeIdT;

class Ieee1394Io {
 public:
  /**
   * Standard constructor.
   */
  Ieee1394Io (int iPort);

  /**
   * Default destructor.
   */
  virtual ~Ieee1394Io ();

  /**
   * Initialize Ieee1394Io object.
   */
  EReturnCodes initialize ();

  /**
   * Get number of nodes.
   *
   * Returns the number of nodes currently on the bus (including the
   * local node).
   *
   * \param iNodeCount Contains the number of nodes on the bus.
   * \return Error code.  See EReturnCodes.
   */
  EReturnCodes getNodeCount (int* iNodeCount);

  EReturnCodes getRomDirectory (NodeIdT nodeId, rom1394_directory* pRomDir);
  EReturnCodes getBusInfoBlockLength (NodeIdT nodeId, int* pLength);
  EReturnCodes getBusOptions (NodeIdT nodeId, 
			      rom1394_bus_options* pBusOptions);
  EReturnCodes getGuid (NodeIdT nodeId, octlet_t* pGuid);
  EReturnCodes getBusId (NodeIdT nodeId, int* pBusId);

  sigc::signal<void,GenerationT>sigGeneration;

 protected:
  static int resetHandler (raw1394handle_t handle, unsigned int iGeneration);

  bool startThread();
  void stopThread();
  static void* Thread (void* arg);
  
 private:
  /** The interface card to use (typically == 0).  */
  int m_iPort;                        
  /** The handle to the ieee1394 substystem.  */
/*   static raw1394handle_t m_ieee1394Handle; */
  raw1394handle_t m_ieee1394Handle;
  pthread_t m_thread;
/*   static pthread_mutex_t m_mutex; */
  pthread_mutex_t m_mutex;
  bool m_isRunning;
};

#endif /* Ieee1394Io_h */
