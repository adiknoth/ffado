/* freebob.h
 * Copyright (C) 2004 by Daniel Wagner
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

#ifndef FREEBOB_H
#define FREEBOB_H

/**
 * Error Codes
 */
typedef enum {
  eFBRC_Success                      =   0,
  eFBRC_Creating1394HandleFailed     =  -1,
  eFBRC_Setting1394PortFailed        =  -2,
  eFBRC_Scaning1394BusFailed         =  -3,
  eFBRC_AddBusResetObserverFailed    =  -4,
  eFBRC_InitializeCMHandlerFailed    =  -5,
  eFBRC_AvDeviceNotFound             =  -6,
  eFBRC_CreatingAvDeviceFailed       =  -7,
} FBReturnCodes;

#endif
