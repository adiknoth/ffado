/* freebob.cpp
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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "freebob.h"
#include "ieee1394io.h"
#include "busmgr.h"

#define FB_DEFAULT_PORT 0

static void 
sigpipe_handler (int value)
{
  printf ("SIGPIPE Received (%d)\n", value);
}

static void 
sigusr2_handler (int value)
{
  printf ("SIGUSR2 Received (%d)\n",  value);
}

/**
 * Segmentation fault handler.
 *
 * <p>This handler unwinds the stack and prints the stackframes.</p>
 *
 */
#ifdef HAVE_EXECINFO_H
static void sigsegv_handler()
{
  void *array[ 10 ];
  size_t size;
  char **strings;
  size_t i;
  
  printf( "\nKino experienced a segmentation fault.\n"
	  "Dumping stack from the offending thread\n\n" );
  size = backtrace( array, 10 );
  strings = backtrace_symbols( array, size );
  
  printf( "Obtained %zd stack frames.\n", size );
  
  for ( i = 0; i < size; i++ )
    printf( "%s\n", strings[ i ] );
  
  free( strings );
  
  printf( "\nDone dumping - exiting.\n" );
  exit( 1 );
}
#endif

int reset_handler (raw1394handle_t handle, unsigned int generation)
{
  printf ("Reset Handler received\n");
  raw1394_update_generation (handle, generation);
  return 0;
}


int
main(int argc, char** argv)
{
  EReturnCodes eRC;

  signal (SIGPIPE, sigpipe_handler);
  signal (SIGUSR2, sigusr2_handler);
#ifdef HAVE_EXECINFO_H
  signal (SIGSEGV,  sigsegv_handler);
#endif

  Ieee1394Io* pIeee1394Io = new Ieee1394Io (FB_DEFAULT_PORT);
  if (pIeee1394Io == NULL) {
    fprintf (stderr, "Could not create Ieee1394Io object.\n");
    exit (1);
  }
  eRC = pIeee1394Io->initialize();
  if (eRC != eRC_Success) {
    fprintf (stderr, "Error initializing Ieee1394Io object.\n");
    exit (1);
  }

  BusMgr* pBusMgr = new BusMgr (*pIeee1394Io);
  if (pBusMgr == NULL) {
    fprintf (stderr, "Could not create BusMgr object.\n");
    exit (1);
  }
  eRC = pBusMgr->initialize();
  if (eRC != eRC_Success) {
    fprintf (stderr, "Error initializing Ieee1394Io object.\n");
    exit (1);
  }

  sleep(100);

  /* Remove object.  This is actually not needed but we do it
     all the same in order to check if we handle everything
     correctly.  */
  delete pBusMgr;
  delete pIeee1394Io;

  return 0;
}
