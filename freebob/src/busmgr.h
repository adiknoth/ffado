/* busmgr.h
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

#ifndef BusMgr_h
#define BusMgr_h

#include <libraw1394/raw1394.h>

#include "freebob.h"
#include "ieee1394io.h"

class BusMgr {
 public:
  /**
   * Standard constructor.
   */
  BusMgr (Ieee1394Io& ieee1394Io);

  /**
   * Default destructor.
   */
  virtual ~BusMgr ();

  /**
   * Initialize BusMgr object.
   */
  EReturnCodes initialize ();

  void slotGeneration(GenerationT generation);

 protected:
  /**
   * Scan Bus.
   *
   * Scan the bus for BeBob devices.
   */
  EReturnCodes scanBus();


  EReturnCodes BusMgr::printRomDirectory (int nodeId,
					  rom1394_directory* romDir);
 private:
  Ieee1394Io& m_ieee1394Io;
};

#endif /* BusMgr_h */
