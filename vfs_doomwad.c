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
 
 $Id: vfs_doomwad.c,v 1.3 2007-09-30 22:59:24 disconn3ct Exp $
*/


#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "vfs.h"

#ifdef DOOMWADS
#ifdef WITH_FTE_VFS
typedef struct
{
	int		filepos, filelen;
	char	name[8];
} dwadfile_t;

typedef struct
{
	char	id[4];
	int		dirlen;
	int		dirofs;
} dwadheader_t;

void *FSPAK_LoadDoomWadFile (vfsfile_t *packhandle, char *desc)
{
	dwadheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	dwadfile_t		info;

	int section=0;
	char sectionname[MAX_QPATH];
	char filename[52];
	char neatwadname[52];

	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header), NULL);
	if (header.id[1] != 'W'	|| header.id[2] != 'A' || header.id[3] != 'D')
		return NULL;	//not a doom wad

	//doom wads come in two sorts. iwads and pwads.
	//iwads are the master wads, pwads are meant to replace parts of the master wad.
	//this is awkward, of course.
	//we ignore the i/p bit for the most part, but with maps, pwads are given a prefixed name.
	if (header.id[0] == 'I')
		*neatwadname = '\0';
	else if (header.id[0] == 'P')
	{
		COM_FileBase(desc, neatwadname);
		strlcat (neatwadname, "#", sizeof (neatwadname));
	}
	else
		return NULL;

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen;
	newfiles = (packfile_t*)Q_malloc (numpackfiles * sizeof(packfile_t));
	VFS_SEEK(packhandle, header.dirofs, SEEK_SET);

	//doom wads are awkward.
	//they have no directory structure, except for start/end 'files'.
	//they follow along the lines of lumps after the parent name.
	//a map is the name of that map, and then a squence of the lumps that form that map (found by next-with-that-name).
	//this is a problem for a real virtual filesystem, so we add a hack to recognise special names and expand them specially.
	for (i=0 ; i<numpackfiles ; i++)
	{
		VFS_READ (packhandle, &info, sizeof(info), NULL);

		strlcpy (filename, info.name, sizeof (filename));
		filename[8] = '\0';
		Q_strlwr(filename);

		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);

		switch(section)	//be prepared to remap filenames.
		{
newsection:
		case 0:
			if (info.filelen == 0)
			{	//marker for something...

				if (!strcmp(filename, "s_start"))
				{
					section = 2;
					snprintf (newfiles[i].name, sizeof (newfiles[i].name), "sprites/%s", filename);	//the model loader has a hack to recognise .dsp
					break;
				}
				if (!strcmp(filename, "p_start"))
				{
					section = 3;
					snprintf (newfiles[i].name, sizeof (newfiles[i].name), "patches/%s", filename); //the map loader will find these.
					break;
				}
				if (!strcmp(filename, "f_start"))
				{
					section = 4;
					snprintf (newfiles[i].name, sizeof (newfiles[i].name), "flats/%s", filename);	//the map loader will find these
					break;
				}
				if ((filename[0] == 'e' && filename[2] == 'm') || !strncmp(filename, "map", 3))
				{	//this is the start of a beutiful new map
					section = 1;
					strlcpy (sectionname, filename, sizeof (sectionname));
					snprintf (newfiles[i].name, sizeof (newfiles[i].name), "maps/%s%s.bsp", neatwadname, filename);	//generate fake bsps to allow the server to find them
					newfiles[i].filepos = 0;
					newfiles[i].filelen = 4;
					break;
				}
				if (!strncmp(filename, "gl_", 3) && ((filename[4] == 'e' && filename[5] == 'm') || !strncmp(filename+3, "map", 3)))
				{	//this is the start of a beutiful new map
					section = 5;
					strlcpy (sectionname, filename+3, sizeof (sectionname));
					break;
				}
			}

			snprintf (newfiles[i].name, sizeof (newfiles[i].name), "wad/%s", filename);	//but there are many files that we don't recognise/know about. archive them off to keep the vfs moderatly clean.
			break;
		case 1:	//map section
			if (strcmp(filename, "things") &&
				strcmp(filename, "linedefs") &&
				strcmp(filename, "sidedefs") &&
				strcmp(filename, "vertexes") &&
				strcmp(filename, "segs") &&
				strcmp(filename, "ssectors") &&
				strcmp(filename, "nodes") &&
				strcmp(filename, "sectors") &&
				strcmp(filename, "reject") &&
				strcmp(filename, "blockmap"))
			{
				section = 0;
				goto newsection;
			}
			snprintf (newfiles[i].name, sizeof (newfiles[i].name), "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 5:	//glbsp output section
			if (strcmp(filename, "gl_vert") &&
				strcmp(filename, "gl_segs") &&
				strcmp(filename, "gl_ssect") &&
				strcmp(filename, "gl_pvs") &&
				strcmp(filename, "gl_nodes"))
			{
				section = 0;
				goto newsection;
			}
			snprintf (newfiles[i].name, sizeof (newfiles[i].name), "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 2:	//sprite section
			if (!strcmp(filename, "s_end"))
			{
				section = 0;
				goto newsection;
			}
			snprintf (newfiles[i].name, sizeof (newfiles[i].name), "sprites/%s", filename);
			break;
		case 3:	//patches section
			if (!strcmp(filename, "p_end"))
			{
				section = 0;
				goto newsection;
			}
			snprintf (newfiles[i].name, sizeof (newfiles[i].name), "patches/%s", filename);
			break;
		case 4:	//flats section
			if (!strcmp(filename, "f_end"))
			{
				section = 0;
				goto newsection;
			}
			snprintf (newfiles[i].name, sizeof (newfiles[i].name), "flats/%s", filename);
			break;
		}
	}

	pack = (pack_t*)Q_malloc (sizeof (pack_t));
	strlcpy (pack->filename, desc, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos, SEEK_SET);

	pack->references++;

	Com_Printf("Added packfile %s (%i files)\n", desc, numpackfiles);
	return pack;
}

searchpathfuncs_t doomwadfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadDoomWadFile,
	NULL,
	FSPAK_OpenVFS
};


#endif // DOOMWADS
#endif // WITH_FTE_VFS


