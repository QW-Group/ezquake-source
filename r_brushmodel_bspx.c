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

//============================================================
// BSPX loading

#include "quakedef.h"

// lumps immediately follow:
typedef struct {
	char lumpname[24];
	int fileofs;
	int filelen;
} bspx_lump_t;

void* Mod_BSPX_FindLump(bspx_header_t* bspx_header, char *lumpname, int *plumpsize, byte* mod_base)
{
	int i;
	bspx_lump_t *lump;

	if (!bspx_header) {
		return NULL;
	}

	lump = (bspx_lump_t *) (bspx_header + 1);
	for (i = 0; i < bspx_header->numlumps; i++, lump++) {
		if (!strcmp(lump->lumpname, lumpname)) {
			if (plumpsize) {
				*plumpsize = lump->filelen;
			}
			return mod_base + lump->fileofs;
		}
	}

	return NULL;
}

bspx_header_t* Mod_LoadBSPX(int filesize, byte* mod_base)
{
	dheader_t *header;
	bspx_header_t *xheader;
	bspx_lump_t *lump;
	int i;
	int xofs;

	// find end of last lump
	header = (dheader_t *) mod_base;
	xofs = 0;
	for (i = 0; i < HEADER_LUMPS; i++) {
		xofs = max(xofs, header->lumps[i].fileofs + header->lumps[i].filelen);
	}

	if (xofs + sizeof(bspx_header_t) > filesize) {
		return NULL;
	}

	xheader = (bspx_header_t *) (mod_base + xofs);
	xheader->numlumps = LittleLong (xheader->numlumps);

	if (xheader->numlumps < 0 || xofs + sizeof(bspx_header_t) + xheader->numlumps * sizeof(bspx_lump_t) > filesize) {
		return NULL;
	}

	// byte-swap and check sanity
	lump = (bspx_lump_t *) (xheader + 1); // lumps immediately follow the header
	for (i = 0; i < xheader->numlumps; i++, lump++) {
		lump->lumpname[sizeof(lump->lumpname)-1] = '\0'; // make sure it ends with zero
		lump->fileofs = LittleLong(lump->fileofs);
		lump->filelen = LittleLong(lump->filelen);
		if (lump->fileofs < 0 || lump->filelen < 0 || (unsigned)(lump->fileofs + lump->filelen) >(unsigned)filesize) {
			return NULL;
		}
	}

	// success
	return xheader;
}
