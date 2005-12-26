/* freebob.h
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

#ifndef Freebob_h_
#define Freebob_h_

/**
 * Error Codes
 */
typedef enum {
  eRC_Success                      = 0,
  eRC_Creating1394HandleFailed     = 1,
  eRC_Setting1394PortFailed        = 2,
  eRC_Scaning1394BusFailed         = 3,
  eRC_AddBusResetObserverFailed    = 4
} EReturnCodes;

#define FAIL_CHECK(func, msg, rc) \
    { EReturnCodes eRC = func; \
      if (eRC != eRC_Success) { \
        fprintf (stderr, msg); \
        return rc; \
    }

#endif /* Freebob_h_ */
