/* 
 * Modifications for FreeBoB by Pieter Palmers
 	Copied from the jackd sources
	function names changed in order to avoid naming problems when using this in
	a jackd backend.
 */
 
/*
 * messagebuffer.h -- realtime-safe message interface for jackd.
 *
 *  This function is included in libjack so backend drivers can use
 *  it, *not* for external client processes.  The VERBOSE() and
 *  MESSAGE() macros are realtime-safe.
 */

/*
 *  Copyright (C) 2004 Rui Nuno Capela, Steve Harris
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __freebob_messagebuffer_h__
#define __freebob_messagebuffer_h__

#ifdef __cplusplus
extern "C" {
#endif

void freebob_messagebuffer_init();
void freebob_messagebuffer_exit();

void freebob_messagebuffer_add(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __freebob_messagebuffer_h__ */
