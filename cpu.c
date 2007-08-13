/*
Copyright (C) 2007 Dmitry 'disconnect' Musatov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
See the GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
	$Id: cpu.c,v 1.2 2007-08-13 06:24:04 disconn3ct Exp $
*/

#include "common.h"
#include "cpu.h"

#ifdef id386

static unsigned int cpu_features = 0;
static unsigned int cpu_vendor = 0; // 0 = unknown, 1 = intel, 2 = amd
static unsigned int cpu_mmx = 0;
static unsigned int cpu_mmxext = 0;
static unsigned int cpu_3dnow = 0;
static unsigned int cpu_3dnowext = 0;
static unsigned int cpu_sse = 0;
static unsigned int cpu_sse2 = 0;
static unsigned int cpu_sse3 = 0;
static unsigned int cpu_ht = 0;
static unsigned int os_sse = 0;

void CPU_Init (void)
{
	cpu_features = DetectProcessor ();

	cpu_vendor = (cpu_features & (1<<8)) ? 1 : (cpu_features & (1<<9)) ? 2 : 0;
	cpu_mmx = (cpu_features & (1<<23));
	cpu_mmxext = (cpu_features & (1<<29));
	cpu_3dnow = (cpu_features & (1<<31));
	cpu_3dnowext = (cpu_features & (1<<30));
	cpu_sse = (cpu_features & (1<<25));
	cpu_sse2 = (cpu_features & (1<<26));
	cpu_sse3 = (cpu_features & (1<<27));
	cpu_ht = (cpu_features & (1<<28));
	os_sse = (cpu_features & (1<<11));
}

void CPU_Info (void)
{
	char features[1024];

	memset (features, 0, sizeof (features));
	Com_Printf ("CPU Vendor: %s\n", (cpu_vendor == 1) ? "Intel" : (cpu_vendor == 2) ? "Amd" : "unknown");

	if (cpu_mmx)
		strlcat (features, "MMX,", sizeof (features));
	if (cpu_mmxext)
		strlcat (features, "MMXEXT,", sizeof (features));
	if (cpu_3dnow)
		strlcat (features, "3DNOW,", sizeof (features));
	if (cpu_3dnowext)
		strlcat (features, "3DNOWEXT,", sizeof (features));
	if (cpu_sse)
		strlcat (features, "SSE,", sizeof (features));
	if (cpu_sse2)
		strlcat (features, "SSE2,", sizeof (features));
	if (cpu_sse3)
		strlcat (features, "SSE3,", sizeof (features));
	if (cpu_ht)
		strlcat (features, "HYPERTHREADING,", sizeof (features));

	if (features[strlen (features) - 1] == ',')
		features[strlen (features) - 1] = '\0';

	Com_Printf ("CPU features: %s\n", features);
}
#endif
