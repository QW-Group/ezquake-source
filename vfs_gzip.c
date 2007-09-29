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
 * $Id: vfs_gzip.c,v 1.3 2007-09-29 15:04:43 dkure Exp $
 *             
 */

#ifdef WITH_ZLIB
#ifdef WITH_VFS_GZIP

#include <zlib.h>
#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "fs.h"
#include "vfs.h"

//=============================================================================
//                       G Z I P   V F S
//=============================================================================
typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	gzFile handle;
} vfsgzipfile_t;

// FIXME:
// Everything below assumes that the input file was an OS file
// This may not be the case if we are opening a gz file in a gz file...

int VFSGZIP_ReadBytes(vfsfile_t *file, void *buffer, int bytestoread, vfserrno_t *err) 
{
	int r;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;

	if (bytestoread < 0)
		        Sys_Error("VFSOS_ReadBytes: bytestoread < 0");
	
	r = gzread(intfile->handle, buffer, bytestoread);
	// r == -1 on error

	if (err) // if bytestoread <= 0 it will be treated as non error even we read zero bytes
		*err = ((r || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);


	return r;
}

int VFSGZIP_WriteBytes(vfsfile_t *file, const void *buffer, int bytestowrite) 
{
	int r;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;

	r = gzwrite(intfile->handle, buffer, bytestowrite);

	// r == 0 on error
	
	return r;
}

qbool VFSGZIP_Seek(vfsfile_t *file, unsigned long offset, int whence) 
{
	int r;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;

	r = gzseek(intfile->handle, offset, whence);
	return r;
}

unsigned long VFSGZIP_Tell(vfsfile_t *file) 
{
	int r;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;

	r = gztell(intfile->handle);
	return r;
}

unsigned long VFSGZIP_GetLen(vfsfile_t *file) 
{
	int r, currentpos;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;
	
	// VFS-FIXME: Error handling
	currentpos = gztell(intfile->handle);
	r = gzseek(intfile->handle, 0, SEEK_END);
	gzseek(intfile->handle, currentpos, SEEK_SET);

	return r;
}

void VFSGZIP_Close(vfsfile_t *file) 
{
	int r;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;

	r = gzclose(intfile->handle);
}

void VFSGZIP_Flush(vfsfile_t *file) 
{
	int r;
	vfsgzipfile_t *intfile = (vfsgzipfile_t *)file;

	//r = gzflush(intfile->handle, Z_NO_FLUSH); 	// <-- Allows better compression
	r = gzflush(intfile->handle, Z_SYNC_FLUSH); 	// <-- All pending output is flushed
}

// TODO: Should add a IO mode (read/write) for the function
vfsfile_t *VFSGZIP_Open(vfsfile_t *file) 
{
	vfsosfile_t *intfile = (vfsosfile_t*)file; // FIXME: <-- ASSUMTPTION!
	vfsgzipfile_t *gzipfile = Q_calloc(1, sizeof(*gzipfile));

	int fd = fileno(intfile->handle);

	gzipfile->handle = gzdopen(dup(fd), "r");
	if (gzipfile->handle == NULL) {
		free(gzipfile);
		return NULL;
	}

	// TODO: Use mode to determine NULL read/write VFS calls
	gzipfile->funcs.ReadBytes  = VFSGZIP_ReadBytes;
	gzipfile->funcs.WriteBytes = VFSGZIP_WriteBytes;
	gzipfile->funcs.Seek       = VFSGZIP_Seek;
	gzipfile->funcs.Tell       = VFSGZIP_Tell;
	gzipfile->funcs.GetLen     = VFSGZIP_GetLen;
	gzipfile->funcs.Close      = VFSGZIP_Close;
	gzipfile->funcs.Flush      = VFSGZIP_Flush;

	return (vfsfile_t *)gzipfile;
}

#endif // WITH_VFS_GZIP
#endif // WITH_ZLIB
