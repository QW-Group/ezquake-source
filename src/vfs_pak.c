/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *     
 * $Id: vfs_pak.c,v 1.17 2007-10-13 16:02:51 dkure Exp $
 *             
 */

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "vfs.h"

// on disk
typedef struct pack_s
{
	char    filename[MAX_OSPATH];
	vfsfile_t    *handle;
	unsigned int filepos;   // the pos the subfiles left it at 
							// (to optimize calls to vfs_seek)
	int references;         // seeing as all vfiles from a pak file use the 
							// parent's vfsfile, we need to keep the parent 
							// open until all subfiles are closed.

	int     numfiles;
	packfile_t  *files;
} pack_t;

typedef struct
{
	char    name[56];
	int     filepos, filelen;
} dpackfile_t;

typedef struct
{
	char    id[4];
	int     dirofs;
	int     dirlen;
} dpackheader_t;

typedef struct {
    vfsfile_t funcs; // <= must be at top/begining of struct
	pack_t *parentpak;
    unsigned long startpos;
    unsigned long length;
    unsigned long currentpos;
} vfspack_t;

#define	MAX_FILES_IN_PACK	2048

//=====================================
//PACK files (*.pak) - VFS functions
//=====================================
static int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread, vfserrno_t *err)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	int read;

	if (vfsp->currentpos - vfsp->startpos + bytestoread > vfsp->length)
		bytestoread = vfsp->length - (vfsp->currentpos - vfsp->startpos);
	if (bytestoread <= 0)
		return -1;

	if (vfsp->parentpak->filepos != vfsp->currentpos) {
		VFS_SEEK(vfsp->parentpak->handle, vfsp->currentpos, SEEK_SET);
	}

	read = VFS_READ(vfsp->parentpak->handle, buffer, bytestoread, err);
	vfsp->currentpos += read;
	vfsp->parentpak->filepos = vfsp->currentpos;

	// VFS-FIXME: Need to handle errors

	return read;
}
static int VFSPAK_WriteBytes (struct vfsfile_s *vfs, const void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("VFSPAK_WriteBytes: Cannot write to pak files");
	return 0;
}

static int VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long offset, int whence)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	int rel_offset;

	// VFS-FIXME Support other whence types
	switch(whence) {
	case SEEK_SET: 
		vfsp->currentpos = vfsp->startpos + offset; 
		break;
	case SEEK_CUR: 
		vfsp->currentpos += offset; 
		break;
	case SEEK_END: 
		vfsp->currentpos = vfsp->startpos + vfsp->length + offset;
		break;
	default:
		Sys_Error("VFSTAR_Seek: Unknown whence value(%d)\n", whence);
		return -1;
	}

	rel_offset = vfsp->currentpos - vfsp->startpos;
	if (rel_offset < 0 || rel_offset >= vfsp->length) {
		Com_Printf("VFSPAK_Seek: Warning seeking past the file's size\n");
	}

	return 0;
}

static unsigned long VFSPAK_Tell (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->currentpos - vfsp->startpos;
}

static unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->length;
}

static void FSPAK_ClosePath(void *handle);
static void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	FSPAK_ClosePath(vfsp->parentpak);	// tell the parent that we don't need it open any 
										// more (reference counts)
	Q_free(vfsp);
}

static vfsfile_t *FSPAK_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	pack_t *pack = (pack_t*)handle;
	vfspack_t *vfsp;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfsp = Q_calloc(1, sizeof(*vfsp));

	vfsp->parentpak = pack;
	vfsp->parentpak->references++;

	vfsp->startpos   = loc->offset;
	vfsp->length     = loc->len;
	vfsp->currentpos = vfsp->startpos;

	vfsp->funcs.ReadBytes     = strcmp(mode, "rb") ? NULL : VFSPAK_ReadBytes;
	vfsp->funcs.WriteBytes    = strcmp(mode, "wb") ? NULL : VFSPAK_WriteBytes;
	vfsp->funcs.Seek		  = VFSPAK_Seek;
	vfsp->funcs.Tell		  = VFSPAK_Tell;
	vfsp->funcs.GetLen	      = VFSPAK_GetLen;
	vfsp->funcs.Close	      = VFSPAK_Close;
	vfsp->funcs.Flush         = NULL;
	if (loc->search)
		vfsp->funcs.copyprotected = loc->search->copyprotected;

	return (vfsfile_t *)vfsp;
}

//======================================
// PACK files (*.pak) - Search functions
//======================================
static void FSPAK_PrintPath(void *handle)
{
	pack_t *pak = handle;

	if (pak->references != 1)
		Com_Printf("%s (%i)\n", pak->filename, pak->references-1);
	else
		Com_Printf("%s\n", pak->filename);
}

static void FSPAK_ClosePath(void *handle)
{
	pack_t *pak = handle;

	pak->references--;
	if (pak->references > 0)
		return;	//not free yet

	VFS_CLOSE (pak->handle);
	if (pak->files)
		Q_free(pak->files);
	Q_free(pak);
}

static void FSPAK_BuildHash(void *handle)
{
	pack_t *pak = handle;
	int i;

	for (i = 0; i < pak->numfiles; i++)
	{
		if (!Hash_GetInsensitive(filesystemhash, pak->files[i].name))
		{
			Hash_AddInsensitive(filesystemhash, pak->files[i].name, &pak->files[i]);
			fs_hash_files++;
		}
		else
			fs_hash_dups++;
	}
}

static qbool FSPAK_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	int i;
	pack_t		*pak = handle;

// look through all the pak file elements

	if (pf)
	{	//is this a pointer to a file in this pak?
		if (pf < pak->files || pf > pak->files + pak->numfiles)
			return false;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<pak->numfiles ; i++)	//look for the file
		{
			if (!strcmp (pak->files[i].name, filename))
			{
				pf = &pak->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		if (loc)
		{
			loc->index = pf - pak->files;
			snprintf(loc->rawname, sizeof(loc->rawname), "%s/%s", pak->filename, filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return true;
	}
	return false;
}

static int FSPAK_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	pack_t	*pak = handle;
	int		num;

	for (num = 0; num<(int)pak->numfiles; num++)
	{
		if (wildcmp(match, pak->files[num].name))
		{
			if (!func(pak->files[num].name, pak->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}

/*
=================
FSPAK_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static void *FSPAK_LoadPackFile (vfsfile_t *file, const char *desc)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	vfsfile_t		*packhandle;
	dpackfile_t		info;
	vfserrno_t err;

	packhandle = file;
	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header), &err);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return NULL;
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true;	// not the original file

	newfiles = (packfile_t*)Q_malloc (numpackfiles * sizeof(packfile_t));

	VFS_SEEK(packhandle, header.dirofs, SEEK_SET);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = QCRC_Block((qbyte *)info, header.dirlen);


//	QCRC_Init (&crc);

	pack = (pack_t *)Q_calloc(1, sizeof (pack_t));

// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		*info.name = '\0';
		VFS_READ(packhandle, &info, sizeof(info), &err);
/*
		for (j=0 ; j<sizeof(info) ; j++)
			CRC_ProcessByte(&crc, ((qbyte *)&info)[j]);
*/
		strlcpy (newfiles[i].name, info.name, MAX_QPATH);
		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);
	}
/*
	if (crc != PAK0_CRC)
		com_modified = true;
*/
	strlcpy (pack->filename, desc, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos, SEEK_SET);

	pack->references++;

	return pack;
}

extern void FSOS_ReadFile(void *handle, flocation_t *loc, char *buffer);

searchpathfuncs_t packfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadPackFile,
	NULL,
	FSPAK_OpenVFS
};
