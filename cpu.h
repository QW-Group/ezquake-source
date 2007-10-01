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
 
	$Id: cpu.h,v 1.2 2007-10-01 18:31:06 disconn3ct Exp $
*/

#ifndef __CPU_H__
#define __CPU_H__

extern unsigned int cpu_features;

#define CPU_IS_INTEL (cpu_features & (1<<8))
#define CPU_IS_AMD (cpu_features & (1<<9))
#define CPY_HAVE_MMX (cpu_features & (1<<23))
#define CPU_HAVE_MMXEXT (cpu_features & (1<<29))
#define CPU_HAVE_3DNOW (cpu_features & (1<<31))
#define CPU_HAVE_3DNOWEXT (cpu_features & (1<<30))
#define CPU_HAVE_SSE (cpu_features & (1<<25))
#define CPU_HAVE_SSE2 (cpu_features & (1<<26))
#define CPU_HAVE_SSE3 (cpu_features & (1<<27))
#define CPU_HAVE_HT (cpu_features & (1<<28))
#define OS_HAVE_SSE (cpu_features & (1<<11))

void CPU_Init (void);
void CPU_Info (void);

#endif /* !__CPU_H__ */
