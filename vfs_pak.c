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
 * $Id: vfs_pak.c,v 1.3 2007-09-30 22:59:25 disconn3ct Exp $
 *             
 */

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "vfs.h"

typedef struct {
    vfsfile_t funcs; // <= must be at top/begining of struct

#ifdef WITH_FTE_VFS
    pack_t *parentpak;
#else
    FILE *handle;
#endif

    unsigned long startpos;
    unsigned long length;
    unsigned long currentpos;
} vfspack_t;


//=====================================
//PACK files (*.pak) - VFS functions
//=====================================
#ifndef WITH_FTE_VFS
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread, vfserrno_t *err)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	unsigned long have = vfsp->length - (vfsp->currentpos - vfsp->startpos);
	unsigned long r;

	if (bytestoread < 0)
		Sys_Error("VFSPAK_ReadBytes: bytestoread < 0"); // ffs

	have = min((unsigned long)bytestoread, have); // mixing sign and unsign types is STUPID and dangerous

// all must work without this, if not then somewhere bug
//	if (ftell(vfsp->handle) != vfsp->currentpos)
//		fseek(vfsp->handle, vfsp->currentpos);

	r = fread(buffer, 1, have, vfsp->handle);
	vfsp->currentpos += r;

	if (err) // if bytestoread <= 0 it will be treated as non error even we read zero bytes
		*err = ((r || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);

	return r;
}
#else
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread, vfserrno_t *err)
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

	return read;
}
#endif // WITH_FTE_VFS

int VFSPAK_WriteBytes (struct vfsfile_s *vfs, const void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("Cannot write to pak files");
	return 0;
}

#ifndef WITH_FTE_VFS
qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos, int whence)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	if (pos > vfsp->length)
		return false;

	vfsp->currentpos = pos + vfsp->startpos;
	return fseek(vfsp->handle, vfsp->currentpos, whence);
}
#else
qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos, int whence)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	if (pos < 0 || pos > vfsp->length)
		return true;
	vfsp->currentpos = pos + vfsp->startpos;

	return false;
}
#endif /* FS_FTE */

unsigned long VFSPAK_Tell (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->currentpos - vfsp->startpos;
}

unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->length;
}
#ifndef WITH_FTE_VFS
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	fclose(vfsp->handle);
	Q_free(vfsp);	//free ourselves.
}
#else
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	FSPAK_ClosePath(vfsp->parentpak);	// tell the parent that we don't need it open any 
										// more (reference counts)
	Q_free(vfsp);
}
#endif /* WITH_FTE_VFS */

#ifndef WITH_FTE_VFS
vfsfile_t *FSPAK_OpenVFS(FILE *handle, int fsize, int fpos, char *mode)
{
	vfspack_t *vfs;

	if (strcmp(mode, "rb") || !handle || fsize < 0 || fpos < 0)
		return NULL; // support only "rb" mode

	vfs = Q_calloc(1, sizeof(vfspack_t));

	vfs->handle = handle;

	vfs->startpos   = fpos;
	vfs->length     = fsize;
	vfs->currentpos = vfs->startpos;

	vfs->funcs.Close	  = VFSPAK_Close;
	vfs->funcs.GetLen	  = VFSPAK_GetLen;
	vfs->funcs.ReadBytes  = VFSPAK_ReadBytes;
	vfs->funcs.Seek		  = VFSPAK_Seek;
	vfs->funcs.Tell		  = VFSPAK_Tell;
	vfs->funcs.WriteBytes = VFSPAK_WriteBytes;	//not supported

	return (vfsfile_t *)vfs;
}

#else

vfsfile_t *FSPAK_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	pack_t *pack = (pack_t*)handle;
	vfspack_t *vfs;
	fs_filesize = -1;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfs = Q_calloc(1, sizeof(vfspack_t));

	vfs->parentpak = pack;
	vfs->parentpak->references++;

	vfs->startpos = loc->offset;
	vfs->length = loc->len;
	vfs->currentpos = vfs->startpos;

	vfs->funcs.Close      = VFSPAK_Close;
	vfs->funcs.GetLen     = VFSPAK_GetLen;
	vfs->funcs.ReadBytes  = VFSPAK_ReadBytes;
	vfs->funcs.Seek       = VFSPAK_Seek;
	vfs->funcs.Tell       = VFSPAK_Tell;
	vfs->funcs.WriteBytes = VFSPAK_WriteBytes;	//not supported

	fs_filesize = VFS_GETLEN((vfsfile_t *)vfs);

	return (vfsfile_t *)vfs;
}
#endif /* WITH_FTE_VFS */

//======================================
// PACK files (*.pak) - Search functions
//======================================
#ifdef WITH_FTE_VFS
void FSPAK_PrintPath(void *handle)
{
	pack_t *pak = handle;

	if (pak->references != 1)
		Com_Printf("%s (%i)\n", pak->filename, pak->references-1);
	else
		Com_Printf("%s\n", pak->filename);
}

void FSPAK_ClosePath(void *handle)
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
void FSPAK_BuildHash(void *handle)
{
	pack_t *pak = handle;
	int i;

	for (i = 0; i < pak->numfiles; i++)
	{
		if (!Hash_GetInsensative(&filesystemhash, pak->files[i].name))
		{
			fs_hash_files++;
			Hash_AddInsensative(&filesystemhash, pak->files[i].name, &pak->files[i], &pak->files[i].bucket);
		}
		else
			fs_hash_dups++;
	}
}
qbool FSPAK_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	int i, len;
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
		len = pf->filelen;
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
int FSPAK_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
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
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
void *FSPAK_LoadPackFile (vfsfile_t *file, char *desc)
{
	dpackheader_t	header;
	int				i;
//	int				j;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	vfsfile_t		*packhandle;
	dpackfile_t		info;
	int read;
	vfserrno_t err;
//	unsigned short		crc;

	packhandle = file;
	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header), &err);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return NULL;
//		Sys_Error ("%s is not a packfile", packfile);
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

//	if (numpackfiles > MAX_FILES_IN_PACK)
//		Sys_Error ("%s has %i files", packfile, numpackfiles);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true;	// not the original file

	// VFS-FIXME: This probably can be malloc, just being extra safe here
	newfiles = (packfile_t*)Q_calloc (numpackfiles, sizeof(packfile_t));

	VFS_SEEK(packhandle, header.dirofs, SEEK_SET);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = QCRC_Block((qbyte *)info, header.dirlen);


//	QCRC_Init (&crc);

	pack = (pack_t*)Q_malloc (sizeof (pack_t));
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		*info.name = '\0';
		read = VFS_READ(packhandle, &info, sizeof(info), &err);
/*
		for (j=0 ; j<sizeof(info) ; j++)
			CRC_ProcessByte(&crc, ((qbyte *)&info)[j]);
*/
		strlcpy (newfiles[i].name, info.name, MAX_QPATH);
		// VFS-FIXME: I think this safe to remove as its for a q2 tank skin...
		//COM_CleanUpPath(newfiles[i].name);	//blooming tanks.
		//
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

	Com_Printf("Added packfile %s (%i files)\n", desc, numpackfiles);
	return pack;
}

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
#endif // WITH_FTE_VFS

