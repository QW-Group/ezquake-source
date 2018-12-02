/*
Copyright (C) 1996-1997 Id Software, Inc.

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

*/
// zone.c - memory management

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "common.h"
#include "gl_model.h"
#endif

//============================================================================

#define HUNK_SENTINEL 0x1df001ed

typedef struct {
	int		sentinel;
	int		size; // including sizeof(hunk_t), -1 = not allocated
	char	name[8];
} hunk_t;

byte	*hunk_base;
int		hunk_size;

int		hunk_low_used;
int		hunk_high_used;

qbool	hunk_tempactive;
int		hunk_tempmark;

/*
==============
Hunk_Check

Run consistency and sentinel trashing checks
==============
*/
void Hunk_Check(void)
{
	hunk_t *h;

	for (h = (hunk_t *)hunk_base; (byte *)h != hunk_base + hunk_low_used;) {
		if (h->sentinel != HUNK_SENTINEL) {
			Sys_Error("Hunk_Check: trashed sentinel");
		}
		if (h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size) {
			Sys_Error("Hunk_Check: bad size");
		}
		h = (hunk_t *)((byte *)h + h->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print(qbool all)
{
	hunk_t  *h, *next, *endlow, *starthigh, *endhigh;
	int     count, sum;
	int     totalblocks;
	char    name[9];

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunk_t *)hunk_base;
	endlow = (hunk_t *)(hunk_base + hunk_low_used);
	starthigh = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *)(hunk_base + hunk_size);

	Con_Printf("          :%8i total hunk size\n", hunk_size);
	Con_Printf("-------------------------\n");

	while (1) {
		// skip to the high hunk if done with low hunk
		if (h == endlow) {
			Con_Printf("-------------------------\n");
			Con_Printf("        :%8ikb REMAINING\n", (hunk_size - hunk_low_used - hunk_high_used) / 1024);
			Con_Printf("-------------------------\n");
			h = starthigh;
		}

		// if totally done, break
		if (h == endhigh) {
			break;
		}

		// run consistency checks
		if (h->sentinel != HUNK_SENTINEL) {
			Sys_Error("Hunk_Print: trashed sentinel");
		}
		if (h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size) {
			Sys_Error("Hunk_Print: bad size");
		}

		next = (hunk_t *)((byte *)h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		// print the single block
		memcpy(name, h->name, 8);
		if (all) {
			Con_Printf("%8p :%8i %8s\n", h, h->size, name);
		}

		// print the total
		if (next == endlow || next == endhigh || strncmp(h->name, next->name, 8)) {
			if (!all) {
				Con_Printf("          :%8ikb %8s (TOTAL)\n", sum / 1024, name);
			}
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);
	Con_Printf("High used %i, low used %i\n", hunk_high_used, hunk_low_used);
}

void Hunk_Print_f(void)
{
	qbool all = Cmd_Argc() != 1;

	Hunk_Print(all);
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName(int size, const char *name)
{
	hunk_t *h;

#ifdef PARANOID
	Hunk_Check();
#endif

	if (size < 0) {
		Sys_Error("Hunk_AllocName: bad size: %i", size);
	}

	size = sizeof(hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size) {
#ifdef SERVERONLY
		Sys_Error("Hunk_AllocName: Not enough RAM allocated. Try starting using \"-mem 64\" (or more) on the command line.");
#else
		Sys_Error("Hunk_AllocName: Not enough RAM allocated. Try starting using \"-mem 128\" (or more) on the command line.");
#endif
	}

	h = (hunk_t *)(hunk_base + hunk_low_used);
	hunk_low_used += size;

	memset(h, 0, size);

	h->size = size;
	h->sentinel = HUNK_SENTINEL;
	strlcpy(h->name, name, sizeof(h->name));

	return (void *)(h + 1);
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

int	Hunk_LowMark(void)
{
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark)
{
	if (mark < 0 || mark > hunk_low_used) {
		Sys_Error("Hunk_FreeToLowMark: bad mark %i, hunk_low_used = %i", mark, hunk_low_used);
	}
	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int	Hunk_HighMark(void)
{
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark(int mark)
{
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used) {
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);
	}
	memset(hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}

/*
===================
Hunk_HighAllocName
===================
*/
void *Hunk_HighAllocName(int size, char *name)
{
	hunk_t *h;

	if (size < 0) {
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);
	}

	if (hunk_tempactive) {
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

#ifdef PARANOID
	Hunk_Check();
#endif

	size = sizeof(hunk_t) + ((size + 15)&~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size) {
#ifdef SERVERONLY
		Sys_Error("Hunk_HighAllocName: Not enough RAM allocated. Try starting using \"-mem 64\" (or more) on the command line.");
#else
		Sys_Error("Hunk_HighAllocName: Not enough RAM allocated. Try starting using \"-mem 128\" (or more) on the command line.");
#endif
	}

	hunk_high_used += size;

	h = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);

	memset(h, 0, size);
	h->size = size;
	h->sentinel = HUNK_SENTINEL;
	strlcpy(h->name, name, sizeof(h->name));

	return (void *)(h + 1);
}

/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void *Hunk_TempAlloc(int size)
{
	void *buf;

	size = (size + 15) & ~15;

	if (hunk_tempactive) {
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

	hunk_tempmark = Hunk_HighMark();

	buf = Hunk_HighAllocName(size, "temp");

	hunk_tempactive = true;

	return buf;
}

//============================================================================

/*
========================
Memory_Init
========================
*/
void Memory_Init(void *buf, int size)
{
	hunk_base = (byte *)buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;
}
