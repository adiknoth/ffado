/*	Time.c
 	FreeBob Streaming API
 	FreeBob = Firewire (pro-)audio for linux

 	http://freebob.sf.net

	Copyright (C) 2005,2006 Pieter Palmers <pieterpalmers@users.sourceforge.net>

	
	Based upon JackTime.c from the jackdmp package.
	Original Copyright:

	Copyright (C) 2001-2003 Paul Davis
	Copyright (C) 2004-2006 Grame


	This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "Time.h"

#ifdef GETCYCLE_TIME

#include <stdio.h>
freebob_microsecs_t GetMhz(void)
{
	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f == 0)
	{
		perror("can't open /proc/cpuinfo\n");
		exit(1);
	}

	for ( ; ; )
	{
		freebob_microsecs_t mhz;
		int ret;
		char buf[1000];

		if (fgets(buf, sizeof(buf), f) == NULL) {
			fprintf (stderr,"FATAL: cannot locate cpu MHz in "
				    "/proc/cpuinfo\n");
			exit(1);
		}

#if defined(__powerpc__)
		ret = sscanf(buf, "clock\t: %" SCNu64 "MHz", &mhz);
#elif defined( __i386__ ) || defined (__hppa__)  || defined (__ia64__) || \
      defined(__x86_64__)
		ret = sscanf(buf, "cpu MHz         : %" SCNu64, &mhz);
#elif defined( __sparc__ )
		ret = sscanf(buf, "Cpu0Bogo        : %" SCNu64, &mhz);
#elif defined( __mc68000__ )
		ret = sscanf(buf, "Clocking:       %" SCNu64, &mhz);
#elif defined( __s390__  )
		ret = sscanf(buf, "bogomips per cpu: %" SCNu64, &mhz);
#else /* MIPS, ARM, alpha */
		ret = sscanf(buf, "BogoMIPS        : %" SCNu64, &mhz);
#endif 
		if (ret == 1)
		{
			fclose(f);
			return (freebob_microsecs_t)mhz;
		}
	}
}

freebob_microsecs_t __freebob_cpu_mhz;

void InitTime()
{
	__freebob_cpu_mhz = GetMhz();
}

#else
void InitTime()
{}

#endif 

