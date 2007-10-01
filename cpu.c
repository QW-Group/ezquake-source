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
 
	$Id: cpu.c,v 1.3 2007-10-01 18:31:06 disconn3ct Exp $
*/

#include "quakedef.h"
// #include "common.h"
#include "asmlib.h"
#include "cpu.h"

#ifdef WITH_ASMLIB

unsigned int cpu_features = 0;

void CPU_Init (void)
{
	cpu_features = DetectProcessor ();
}

void CPU_Info (void)
{
	char features[1024];

	memset (features, 0, sizeof (features));
	Com_Printf ("CPU Vendor: %s\n", CPU_IS_INTEL ? "Intel" : CPU_IS_AMD ? "Amd" : "unknown");

	if (CPY_HAVE_MMX)
		strlcat (features, "MMX,", sizeof (features));
	if (CPU_HAVE_MMXEXT)
		strlcat (features, "MMXEXT,", sizeof (features));
	if (CPU_HAVE_3DNOW)
		strlcat (features, "3DNOW,", sizeof (features));
	if (CPU_HAVE_3DNOWEXT)
		strlcat (features, "3DNOWEXT,", sizeof (features));
	if (CPU_HAVE_SSE)
		strlcat (features, "SSE,", sizeof (features));
	if (CPU_HAVE_SSE2)
		strlcat (features, "SSE2,", sizeof (features));
	if (CPU_HAVE_SSE3)
		strlcat (features, "SSE3,", sizeof (features));
	if (CPU_HAVE_HT)
		strlcat (features, "HYPERTHREADING,", sizeof (features));

	if (features[strlen (features) - 1] == ',')
		features[strlen (features) - 1] = '\0';

	Com_Printf ("CPU features: %s\n", features);
}

#endif /* !WITH_ASMLIB */
