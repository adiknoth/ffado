/* busmgr.cpp
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

#include "ieee1394io.h"
#include "busmgr.h"


BusMgr::BusMgr (Ieee1394Io& ieee1394Io)
  : m_ieee1394Io(ieee1394Io)
{
}

BusMgr::~BusMgr ()
{
}

EReturnCodes 
BusMgr::initialize ()
{
  EReturnCodes eRC;

  m_ieee1394Io.sigGeneration.connect(sigc::mem_fun(this, 
						   &BusMgr::slotGeneration));


  printf ("asdf");
  eRC = scanBus ();
  if (eRC != eRC_Success) {
    fprintf (stderr, "Error scanning 1394 bus for cards\n");
    return  eRC_Scaning1394BusFailed;
  }

  return eRC_Success;
}

void 
BusMgr::slotGeneration (GenerationT generation)
{
  printf ("BusMgr received a generation update (%d)\n", generation);
  printf ("Rescan bus\n");

  EReturnCodes eRC;

  eRC = scanBus ();
  if (eRC != eRC_Success) {
    fprintf (stderr, "Error scanning 1394 bus for cards\n");
  }  
}

EReturnCodes
BusMgr::scanBus ()
{
  EReturnCodes eRC;
  int iNodeCount;
  rom1394_directory romDir;
  
  eRC = m_ieee1394Io.getNodeCount (&iNodeCount);

  for (int nodeId = 0; nodeId < iNodeCount; nodeId++) {
    eRC = m_ieee1394Io.getRomDirectory (nodeId, &romDir);
    if (eRC != eRC_Success) {
      fprintf (stderr, "Error reading config rom directory for node %d\n",
	       nodeId);
      continue;
    }
    
    printRomDirectory (nodeId, &romDir);
  }

  return eRC_Success;
}

EReturnCodes
BusMgr::printRomDirectory (int nodeId,
			   rom1394_directory* romDir)
{
  int length;
  octlet_t guid;
  int busId;

  rom1394_bus_options busOptions;

  printf ( "\nNode %d: \n", nodeId);
  printf ( "-------------------------------------------------\n");
  m_ieee1394Io.getBusInfoBlockLength (nodeId, &length);
  printf ("bus info block length = %d\n", length);
  m_ieee1394Io.getBusId (nodeId, &busId);
  printf ("bus id = 0x%08x\n", busId);
  m_ieee1394Io.getBusOptions (nodeId, &busOptions);
  printf ("bus options:\n");
  printf ("    isochronous resource manager capable: %d\n", busOptions.irmc);
  printf ("    cycle master capable                : %d\n", busOptions.cmc);
  printf ("    isochronous capable                 : %d\n", busOptions.isc);
  printf ("    bus manager capable                 : %d\n", busOptions.bmc);
  printf ("    cycle master clock accuracy         : %d ppm\n", busOptions.cyc_clk_acc);
  printf ("    maximum asynchronous record size    : %d bytes\n", busOptions.max_rec);
  m_ieee1394Io.getGuid (nodeId, &guid);
  printf ("GUID: 0x%08x%08x\n", (quadlet_t) (guid>>32), 
	  (quadlet_t) (guid & 0xffffffff));
  printf ("directory:\n");
  printf ("    node capabilities    : 0x%08x\n", romDir->node_capabilities);
  printf ("    vendor id            : 0x%08x\n", romDir->vendor_id);
  printf ("    unit spec id         : 0x%08x\n", romDir->unit_spec_id);
  printf ("    unit software version: 0x%08x\n", romDir->unit_sw_version);
  printf ("    model id             : 0x%08x\n", romDir->model_id);
  printf ("    textual leaves       : %s\n", romDir->label);
 
  return eRC_Success;
}
