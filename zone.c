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

void Cache_FreeLow(int new_low_hunk);
void Cache_FreeHigh(int new_high_hunk);

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
			Con_Printf("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
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
				Con_Printf("          :%8i %8s (TOTAL)\n", sum, name);
			}
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);
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
void *Hunk_AllocName(int size, char *name)
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

	Cache_FreeLow(hunk_low_used);

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
	Cache_FreeHigh(hunk_high_used);

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

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s {
	int						size; // including this header
	cache_user_t			*user;
	char					name[16];
	struct cache_system_s	*prev, *next;
	struct cache_system_s	*lru_prev, *lru_next; // for LRU flushing
} cache_system_t;

cache_system_t *Cache_TryAlloc(int size, qbool nobottom);

cache_system_t cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move(cache_system_t *c)
{
	cache_system_t *new_block;

	// we are clearing up space at the bottom, so only allocate it late
	new_block = Cache_TryAlloc(c->size, true);
	if (new_block) {
		memcpy(new_block + 1, c + 1, c->size - sizeof(cache_system_t));
		new_block->user = c->user;
		memcpy(new_block->name, c->name, sizeof(new_block->name));
		Cache_Free(c->user);
		new_block->user->data = (void *)(new_block + 1);
	}
	else {
		Cache_Free(c->user); // tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow(int new_low_hunk)
{
	cache_system_t *c;

	while (1) {
		c = cache_head.next;
		if (c == &cache_head) {
			return; // nothing in cache at all
		}
		if ((byte *)c >= hunk_base + new_low_hunk) {
			return; // there is space to grow the hunk
		}
		Cache_Move(c); // reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh(int new_high_hunk)
{
	cache_system_t *c, *prev;

	prev = NULL;
	while (1) {
		c = cache_head.prev;
		if (c == &cache_head) {
			return; // nothing in cache at all
		}
		if ((byte *)c + c->size <= hunk_base + hunk_size - new_high_hunk) {
			return; // there is space to grow the hunk
		}
		if (c == prev) {
			Cache_Free(c->user); // didn't move out of the way
		}
		else {
			Cache_Move(c); // try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU(cache_system_t *cs)
{
	if (!cs->lru_next || !cs->lru_prev) {
		Sys_Error("Cache_UnlinkLRU: NULL link");
	}

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = NULL;
}

void Cache_MakeLRU(cache_system_t *cs)
{
	if (cs->lru_next || cs->lru_prev) {
		Sys_Error("Cache_MakeLRU: active link");
	}

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t *Cache_TryAlloc(int size, qbool nobottom)
{
	cache_system_t *cs, *new_block;

	// is the cache completely empty?
	if (!nobottom && cache_head.prev == &cache_head) {
		if (hunk_size - hunk_high_used - hunk_low_used < size) {
			Sys_Error("Cache_TryAlloc: %i is greater than free hunk", size);
		}

		new_block = (cache_system_t *)(hunk_base + hunk_low_used);
		memset(new_block, 0, sizeof(*new_block));
		new_block->size = size;

		cache_head.prev = cache_head.next = new_block;
		new_block->prev = new_block->next = &cache_head;

		Cache_MakeLRU(new_block);
		return new_block;
	}

	// search from the bottom up for space
	new_block = (cache_system_t *)(hunk_base + hunk_low_used);
	cs = cache_head.next;

	do {
		if (!nobottom || cs != cache_head.next) {
			if ((byte *)cs - (byte *)new_block >= size) {
				// found space
				memset(new_block, 0, sizeof(*new_block));
				new_block->size = size;

				new_block->next = cs;
				new_block->prev = cs->prev;
				cs->prev->next = new_block;
				cs->prev = new_block;

				Cache_MakeLRU(new_block);

				return new_block;
			}
		}

		// continue looking
		new_block = (cache_system_t *)((byte *)cs + cs->size);
		cs = cs->next;
	} while (cs != &cache_head);

	// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - (byte *)new_block >= size) {
		memset(new_block, 0, sizeof(*new_block));
		new_block->size = size;

		new_block->next = &cache_head;
		new_block->prev = cache_head.prev;
		cache_head.prev->next = new_block;
		cache_head.prev = new_block;

		Cache_MakeLRU(new_block);

		return new_block;
	}

	return NULL; // couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush(void)
{
	while (cache_head.next != &cache_head) {
		Cache_Free(cache_head.next->user); // reclaim the space
	}
#ifndef SERVERONLY
	Mod_ClearSimpleTextures();
#endif
}

/*
============
Cache_Print

============
*/
void Cache_Print(void)
{
	cache_system_t *cd;

	for (cd = cache_head.next; cd != &cache_head; cd = cd->next) {
		Con_Printf("%5.1f kB : %s\n", (cd->size / (float)(1024)), cd->name);
	}
}

/*
============
Cache_Report

============
*/
void Cache_Report(void)
{
	Con_Printf("%4.1f of %4.1f megabyte data cache free\n",
		(float)(hunk_size - hunk_high_used - hunk_low_used) / (1024 * 1024),
		(float)hunk_size / (1024 * 1024));
}

/*
============
Cache_Init

============
*/
void Cache_Init(void)
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

#ifndef WITH_DP_MEM
	// If DP mem is used then we can't add commands untill Cmd_Init() executed.
	Cache_Init_Commands();
#endif
}

void Cache_Init_Commands(void)
{
	Cmd_AddCommand("flush", Cache_Flush);
	Cmd_AddCommand("cache_print", Cache_Print);
	Cmd_AddCommand("cache_report", Cache_Report);

	Cmd_AddCommand("hunk_print", Hunk_Print_f);
}

#ifndef WITH_DP_MEM
/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free(cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data) {
		Sys_Error("Cache_Free: not allocated");
	}

	cs = ((cache_system_t *)c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU(cs);
}

/*
==============
Cache_Check
==============
*/
void *Cache_Check(cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data) {
		return NULL;
	}

	cs = ((cache_system_t *)c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU(cs);
	Cache_MakeLRU(cs);

	return c->data;
}

/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc(cache_user_t *c, int size, char *name)
{
	cache_system_t *cs;

	if (c->data) {
		Sys_Error("Cache_Alloc: already allocated");
	}

	if (size <= 0) {
		Sys_Error("Cache_Alloc: size %i", size);
	}

	size = (size + sizeof(cache_system_t) + 15) & ~15;

	// find memory for it
	while (1) {
		cs = Cache_TryAlloc(size, false);
		if (cs) {
			strlcpy(cs->name, name, sizeof(cs->name));
			c->data = (void *)(cs + 1);
			cs->user = c;
			break;
		}

		// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head) {
			Sys_Error("Cache_Alloc: out of memory");
		}

		// not enough memory at all
		Cache_Free(cache_head.lru_prev->user);
	}

	return Cache_Check(c);
}
#endif
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

	Cache_Init();
}
