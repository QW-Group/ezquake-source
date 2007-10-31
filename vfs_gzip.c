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
 * $Id: vfs_gzip.c,v 1.12 2007-10-13 17:30:09 borisu Exp $
 *             
 */

#ifdef WITH_FTE_VFS
#ifdef WITH_ZLIB

#include <zlib.h>
#ifdef _WIN32
#include <io.h>
#endif // _WIN32
#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "fs.h"
#include "vfs.h"

//=============================================================================
//                       G Z I P   V F S
//=============================================================================
typedef struct gzipfile_s
{
	char filename[MAX_QPATH];
	vfsfile_t *handle;

	packfile_t file; // Only one file can be stored in a gzip file

	unsigned long filepos;
	vfsfile_t *raw;

	int references;
} gzipfile_t;


typedef struct 
{
	vfsfile_t funcs; // <= must be at top/begining of struct

	gzipfile_t *parent;

	unsigned long startpos;
	unsigned long length;
	unsigned long currentpos;
} vfsgzipfile_t;

// FIXME:
// Everything below assumes that the input file was an OS file
// This may not be the case if we are opening a gz file in a gz file...
// The assumption is due to the minizip library not having hooks for
// f* operations (fread, fwrite, fseek etc)
//
// This assumption may be possible to over come by using streams instead
//
static int VFSGZIP_ReadBytes(vfsfile_t *file, void *buffer, int bytestoread, vfserrno_t *err) 
{
	int r;
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;

	if (bytestoread < 0)
		        Sys_Error("VFSGZIP_ReadBytes: bytestoread < 0");
	
	r = gzread(vfsgz->parent->handle, buffer, bytestoread);
	// r == -1 on error

	if (err) // if bytestoread <= 0 it will be treated as non error even we read zero bytes
		*err = ((r || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);


	return r;
}

static int VFSGZIP_WriteBytes(vfsfile_t *file, const void *buffer, int bytestowrite) 
{
	int r;
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;

	r = gzwrite(vfsgz->parent->handle, buffer, bytestowrite);

	// r == 0 on error
	
	return r;
}

static int VFSGZIP_Seek(vfsfile_t *file, unsigned long offset, int whence) 
{
	int r;
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;

	r = gzseek(vfsgz->parent->handle, offset, whence);

	if (r == -1)
		return -1;
	else
		return 0;
}

static unsigned long VFSGZIP_Tell(vfsfile_t *file) 
{
	int r;
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;

	r = gztell(vfsgz->parent->handle);
	return r;
}

static unsigned long VFSGZIP_GetLen(vfsfile_t *file) 
{
	int r, currentpos;
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;
	
	// VFS-FIXME: Error handling
	currentpos = gztell(vfsgz->parent->handle);
	r = gzseek(vfsgz->parent->handle, 0, SEEK_END);
	gzseek(vfsgz->parent->handle, currentpos, SEEK_SET);

	return r;
}

static void FSGZIP_ClosePath(void *handle);
static void VFSGZIP_Close(vfsfile_t *file) 
{
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;

	FSGZIP_ClosePath(vfsgz->parent);
}

static void VFSGZIP_Flush(vfsfile_t *file) 
{
	int r;
	vfsgzipfile_t *vfsgz = (vfsgzipfile_t *)file;

	//r = gzflush(vfsgz->parent->handle, Z_NO_FLUSH); 	// <-- Allows better compression
	r = gzflush(vfsgz->parent->handle, Z_SYNC_FLUSH); 	// <-- All pending output is flushed
}

static vfsfile_t *FSGZIP_OpenVFS(void *handle, flocation_t *loc, char *mode) 
{
	gzipfile_t *gzip = (gzipfile_t *)handle;
	vfsgzipfile_t *vfsgz;

	if (strcmp(mode, "rb"))
		return NULL;

	vfsgz = Q_calloc(1, sizeof(*vfsgz));
	vfsgz->parent = gzip;
	gzip->references++;

	vfsgz->startpos   = loc->offset;
	vfsgz->length     = loc->len;
	vfsgz->currentpos = vfsgz->startpos;

	vfsgz->funcs.ReadBytes  = strcmp(mode, "rb") ? NULL : VFSGZIP_ReadBytes;
	vfsgz->funcs.WriteBytes = strcmp(mode, "wb") ? NULL : VFSGZIP_WriteBytes;
	vfsgz->funcs.Seek       = VFSGZIP_Seek;
	vfsgz->funcs.Tell       = VFSGZIP_Tell;
	vfsgz->funcs.GetLen     = VFSGZIP_GetLen;
	vfsgz->funcs.Close      = VFSGZIP_Close;
	vfsgz->funcs.Flush      = VFSGZIP_Flush;
	if (loc->search)
		vfsgz->funcs.copyprotected = loc->search->copyprotected;

	return (vfsfile_t *)vfsgz;
}


//=============================================
// GZIP file  (*.gz) - Search Functions
//=============================================
static void FSGZIP_PrintPath(void *handle)
{
	gzipfile_t *gzip = (gzipfile_t *)handle;

	if (gzip->references != 1)
		Com_Printf("%s (%i)\n", gzip->filename, gzip->references-1);
	else
		Com_Printf("%s\n", gzip->filename);
}

static void FSGZIP_ClosePath(void *handle)
{
	gzipfile_t *gzip = (gzipfile_t *)handle;

	if (--gzip->references > 0)
		return; //not yet time


	gzclose(gzip->handle);
	VFS_CLOSE(gzip->raw);
	Q_free(gzip);
}

static void FSGZIP_BuildHash(void *handle)
{
	gzipfile_t *gzip = (gzipfile_t *)handle;

	if (!Hash_GetInsensitive(filesystemhash, gzip->file.name))
	{
		Hash_AddInsensitive(filesystemhash, gzip->file.name, &gzip->file);
		fs_hash_files++;
	}
	else {
		fs_hash_dups++;
	}
}

static qbool FSGZIP_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	gzipfile_t *gzip = (gzipfile_t *)handle;

	// look through all the pak file elements

	//is this a pointer to a file in this pak?
	if (pf && pf != &(gzip->file)) {
		return false;   //was found in a different path
	} else if (!strcmp (gzip->file.name, filename)) {
		pf = &gzip->file;
	}

	if (pf)
	{
		if (loc)
		{
			loc->index = 0;
			snprintf(loc->rawname, sizeof(loc->rawname), "%s/%s", gzip->filename, filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return true;
	}
	return false;

}

static void FSGZIP_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	gzipfile_t *gzip = handle;
	int err;

	VFS_SEEK(gzip->handle, gzip->file.filepos, SEEK_SET);

	err = VFS_READ(gzip->handle, buffer, gzip->file.filelen, NULL);

	if (err!=gzip->file.filelen)
	{
		Com_Printf ("Can't extract file \"%s:%s\" (corrupt)\n", gzip->filename, gzip->file.name);
		return;
	}
	return;
}

static int FSGZIP_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	gzipfile_t *gzip = handle;

	if (wildcmp(match, gzip->file.name))
	{
		if (!func(gzip->file.name, gzip->file.filelen, parm))
			return false;
	}

	return true;
}

// =================
// FSTAR_LoadGZipFile
// =================
// Takes an explicit (not game tree related) path to a gzip file.
//
// Loads the header and directory, adding the files at the beginning
// of the list so they override previous pack files.

static void *FSGZIP_LoadGZipFile(vfsfile_t *gziphandle, const char *desc)
{
	gzipfile_t *gzip;
	const char *base;
	char *ext;
	int fd;

	gzip = Q_calloc(1, sizeof(*gzip));
	strlcpy(gzip->filename, desc, sizeof(gzip->filename));
	if (gziphandle == NULL) goto fail;
	gzip->raw = gziphandle;

	fd = fileno(((vfsosfile_t *)gziphandle)->handle); // <-- ASSUMPTION! that file is OS
	gzip->handle = gzdopen(dup(fd), "r");
	gzip->references = 1;

	/* Remove the .gz from the file.name */
	base = COM_SkipPath(desc);
	ext = COM_FileExtension(desc);
	if (strcmp(ext, "gz") == 0) {
		COM_StripExtension(base, gzip->file.name);
	} else {
		strlcpy(gzip->file.name, base, sizeof(gzip->file.name));
	}

	return gzip;

fail:
	// Q_free is safe to call on NULL pointers
	Q_free(gzip);
	return NULL;
}

searchpathfuncs_t gzipfilefuncs = {
	FSGZIP_PrintPath,
	FSGZIP_ClosePath,
	FSGZIP_BuildHash,
	FSGZIP_FLocate,
	FSGZIP_ReadFile,
	FSGZIP_EnumerateFiles,
	FSGZIP_LoadGZipFile,
	NULL, // VFS-FIXME: Might be used for f_modification
	FSGZIP_OpenVFS 
};

#endif // WITH_ZLIB
#endif // WITH_FTE_VFS
