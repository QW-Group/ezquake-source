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
 * $Id: vfs_os.c,v 1.11 2007/10/10 17:30:43 dkure Exp $
 *             
 */

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "vfs.h"

//==================================
// STDIO files (OS) - VFS Functions
//==================================
static int VFSOS_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err)
{
	int r;
	vfsosfile_t *intfile = (vfsosfile_t*)file;

	if (bytestoread < 0)
		Sys_Error("VFSOS_ReadBytes: bytestoread < 0"); // ffs

	r = fread(buffer, 1, bytestoread, intfile->handle);

	if (err) // if bytestoread <= 0 it will be treated as non error even we read zero bytes
		*err = ((r || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);

	return r;
}

static int VFSOS_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fwrite(buffer, 1, bytestowrite, intfile->handle);
}

static int VFSOS_Seek (struct vfsfile_s *file, unsigned long pos, int whence)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fseek(intfile->handle, pos, whence);
}

static unsigned long VFSOS_Tell (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return ftell(intfile->handle);
}

static unsigned long VFSOS_GetSize (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;

	unsigned long curpos, maxlen;

	// FIXME: add error checks here?

	curpos = ftell(intfile->handle);
	fseek(intfile->handle, 0, SEEK_END);
	maxlen = ftell(intfile->handle);
	fseek(intfile->handle, curpos, SEEK_SET);

	return maxlen;
}

static void VFSOS_Close(vfsfile_t *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	fclose(intfile->handle);
	Q_free(file);
}

vfsfile_t *FS_OpenTemp(void)
{
	FILE *f;
	vfsosfile_t *file;

	f = tmpfile();
	if (!f)
		return NULL;

	file = Q_calloc(1, sizeof(vfsosfile_t));

	file->funcs.ReadBytes	= VFSOS_ReadBytes;
	file->funcs.WriteBytes	= VFSOS_WriteBytes;
	file->funcs.Seek		= VFSOS_Seek;
	file->funcs.Tell		= VFSOS_Tell;
	file->funcs.GetLen		= VFSOS_GetSize;
	file->funcs.Close		= VFSOS_Close;

	file->handle = f;

	return (vfsfile_t*)file;
}

// if f == NULL then use fopen(name, ...);
#ifndef WITH_FTE_VFS
// Slightly different version below were we don't take in a FILE *
vfsfile_t *VFSOS_Open(char *name, FILE *f, char *mode)
{
	vfsosfile_t *file;

	if (!strchr(mode, 'r') && !strchr(mode, 'w'))
		return NULL; // hm, no read and no write mode?

	if (!f) {
		qbool read  = !!strchr(mode, 'r');
		qbool write = !!strchr(mode, 'w');
		qbool text  = !!strchr(mode, 't');
		char newmode[10];
		int modec = 0;

		if (read)
			newmode[modec++] = 'r';
		if (write)
			newmode[modec++] = 'w';
		if (text)
			newmode[modec++] = 't';
		else
			newmode[modec++] = 'b';
		newmode[modec++] = '\0';
    
		f = fopen(name, newmode);
	}

	if (!f)
		return NULL;

	file = Q_calloc(1, sizeof(vfsosfile_t));

	file->funcs.ReadBytes	= (strchr(mode, 'r') ? VFSOS_ReadBytes  : NULL);
	file->funcs.WriteBytes	= (strchr(mode, 'w') ? VFSOS_WriteBytes : NULL);
	file->funcs.Seek		= VFSOS_Seek;
	file->funcs.Tell		= VFSOS_Tell;
	file->funcs.GetLen		= VFSOS_GetSize;
	file->funcs.Close		= VFSOS_Close;

	file->handle = f;

	return (vfsfile_t*)file;
}
#else 

// VFS-XXX: This is slightly different to fs.c version in that we don't take in a FILE *f
vfsfile_t *VFSOS_Open(char *osname, char *mode)
{
	FILE *f;
	vfsosfile_t *file;
	qbool read   = !!strchr(mode, 'r');
	qbool write  = !!strchr(mode, 'w');
	qbool append = !!strchr(mode, 'a');
	qbool text   = !!strchr(mode, 't');
	char newmode[10];
	int modec = 0;

	if (read)
		newmode[modec++] = 'r';
	if (write)
		newmode[modec++] = 'w';
	if (append)
		newmode[modec++] = 'a';
	if (text)
		newmode[modec++] = 't';
	else
		newmode[modec++] = 'b';
	newmode[modec++] = '\0';

	f = fopen(osname, newmode);
	if (!f)
		return NULL;

	file = Q_calloc(1, sizeof(vfsosfile_t));

	file->funcs.ReadBytes  = ( strchr(mode, 'r')                      ? VFSOS_ReadBytes  : NULL);
	file->funcs.WriteBytes = ((strchr(mode, 'w') || strchr(mode, 'a'))? VFSOS_WriteBytes : NULL);
	file->funcs.Seek       = VFSOS_Seek;
	file->funcs.Tell       = VFSOS_Tell;
	file->funcs.GetLen     = VFSOS_GetSize;
	file->funcs.Close      = VFSOS_Close;

	file->handle = f;

	return (vfsfile_t*)file;
}

#endif // WTIH_FTE_VFS

//==================================
// STDIO files (OS) - Search functions
//==================================
#ifdef WITH_FTE_VFS
vfsfile_t *FSOS_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	char diskname[MAX_OSPATH];

	snprintf(diskname, sizeof(diskname), "%s/%s", (char*)handle, loc->rawname);

	return VFSOS_Open(diskname, mode);
}

static void FSOS_PrintPath(void *handle)
{
	Com_Printf("%s\n", handle);
}

static void FSOS_ClosePath(void *handle)
{
	Q_free(handle);
}

static int FSOS_RebuildFSHash(char *filename, int filesize, void *data)
{
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		snprintf(childpath, sizeof (childpath), "%s*", filename);
		Sys_EnumerateFiles((char*)data, childpath, FSOS_RebuildFSHash, data);
		return true;
	}
	if (!Hash_GetInsensitive(filesystemhash, filename))
	{
		Hash_AddInsensitive(filesystemhash, filename, data);
		fs_hash_files++;
	}
	else
		fs_hash_dups++;
	return true;
}

static void FSOS_BuildHash(void *handle)
{
	Sys_EnumerateFiles(handle, "*", FSOS_RebuildFSHash, handle);
}

static qbool FSOS_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	FILE *f;
	int len;
	char netpath[MAX_OSPATH];


	if (hashedresult && (void *)hashedresult != handle)
		return false;

/*
	if (!static_registered)
	{	// if not a registered version, don't ever go beyond base
		if ( strchr (filename, '/') || strchr (filename,'\\'))
			continue;
	}
*/

// check a file in the directory tree
	snprintf (netpath, sizeof(netpath)-1, "%s/%s",(char*)handle, filename);

	// VFS-FIXME: This could be optimised to only do this once and save the result!
	f = fopen(netpath, "rb");
	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fclose(f);
	if (loc)
	{
		loc->len = len;
		loc->offset = 0;
		loc->index = 0;
		strlcpy (loc->rawname, filename, sizeof (loc->rawname));
	}

	return true;
}

void FSOS_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	FILE *f;
	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	fread(buffer, 1, loc->len, f);
	fclose(f);
}

static int FSOS_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	return Sys_EnumerateFiles(handle, match, func, parm);
}

searchpathfuncs_t osfilefuncs = {
	FSOS_PrintPath,
	FSOS_ClosePath,
	FSOS_BuildHash,
	FSOS_FLocate,
	FSOS_ReadFile,
	FSOS_EnumerateFiles,
	NULL,
	NULL,
	FSOS_OpenVFS
};

#endif // WITH_FTE_VFS

