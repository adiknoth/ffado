/* watchdog.h
  Copyright (C) 2006 Pieter Palmers
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  Thread creation function including workarounds for real-time scheduling
  behaviour on different glibc versions.

*/
#ifndef __FREEBOB_WATCHDOG_H__
#define __FREEBOB_WATCHDOG_H__

void *freebob_streaming_watchdog_thread (void *arg);
int freebob_streaming_start_watchdog (freebob_device_t *dev);
void freebob_streaming_stop_watchdog (freebob_device_t *dev);

#endif
