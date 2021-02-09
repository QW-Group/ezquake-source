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
 * $Id: vfs_mmap.c,v 1.2 2007-10-10 17:30:43 dkure Exp $
 *             
 */

//#ifdef WITH_VFS_MMAP

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "fs.h"
#include "vfs.h"

//=============================================================================
//                       M M A P    V F S
//=============================================================================
// These functions give a VFS layer to operations for a file in 
// mapped in memory (aka mmap)

typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	byte *handle;
	size_t position;
	size_t len;
} vfsmmapfile_t;

static int VFSMMAP_ReadBytes(vfsfile_t *file, void *buffer, int bytestoread_, vfserrno_t *err) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;
	size_t bytestoread = bytestoread_;

	if (bytestoread < 0) {
		Sys_Error("VFSMMAP_ReadBytes: bytestoread < 0");
	}

	/* Make sure we don't read past the valid memory */
	if ((bytestoread + intfile->position) > intfile->len) {
		bytestoread = max(0, intfile->len - intfile->position);
	}

	// Read.
	memcpy(buffer, intfile->handle + intfile->position, bytestoread);

	// Advance in the file.
	intfile->position += bytestoread;

	if (err) {
		*err = (bytestoread == 0) ? VFSERR_EOF : VFSERR_NONE;
	}

	// safe cast: will be <= bytestoread_
	return (int)bytestoread;
}

static int VFSMMAP_WriteBytes(vfsfile_t *file, const void *buffer, int bytestowrite) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;

	/* Allocate more memory if we would overflow */
	if (bytestowrite + intfile->position > intfile->len) {
		size_t newlen  = bytestowrite + intfile->position;
		void *p = realloc(intfile->handle, newlen);
		if (!p) {
			Com_Printf("VFSMMAP_WriteBytes: Unable to write to file, memory full\n");
			return 0;
		} 
		intfile->handle = p;
	}

	memcpy(intfile->handle + intfile->position, buffer, bytestowrite);
	intfile->position += bytestowrite;

	return bytestowrite;
}

static int VFSMMAP_Seek(vfsfile_t *file, unsigned long offset, int whence) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;

	switch(whence) 
	{
		case SEEK_SET: 
			intfile->position = offset;
			break;
		case SEEK_CUR: 
			intfile->position += offset;
			break;
		case SEEK_END: 
			intfile->position = intfile->len + offset; 
			break;
		default:
			Sys_Error("VFSMMAP_Seek: Unknown whence value(%d)\n", whence);
			return -1;
	}

	// We seeked outside the bounds.
	if (intfile->position > intfile->len)
	{
		// Be sure we don't have an invalid file position. (Should we do this?)
		clamp(intfile->position, 0, intfile->len);
		return -1;
	}

	return 0;
}

static unsigned long VFSMMAP_Tell(vfsfile_t *file) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;

	return (unsigned long)intfile->position;
}

static unsigned long VFSMMAP_GetLen(vfsfile_t *file) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;
	
	return (unsigned long)intfile->len;
}

static void VFSMMAP_Close(vfsfile_t *file) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;

	Q_free(intfile->handle);
	Q_free(intfile);
}

static void VFSMMAP_Flush(vfsfile_t *file) 
{
	vfsmmapfile_t *intfile = (vfsmmapfile_t *)file;

	(void)intfile;
	Sys_Error("VFSMMAP_Flush: Invalid operation\n");
}

vfsfile_t *FSMMAP_OpenVFS(void *buf, size_t buf_len) 
{
	vfsmmapfile_t *mmapfile = Q_calloc(1, sizeof(*mmapfile));

	mmapfile->handle           = buf;
	mmapfile->len              = buf_len;
	mmapfile->funcs.ReadBytes  = VFSMMAP_ReadBytes;
	mmapfile->funcs.WriteBytes = VFSMMAP_WriteBytes;
	mmapfile->funcs.Seek       = VFSMMAP_Seek;
	mmapfile->funcs.Tell       = VFSMMAP_Tell;
	mmapfile->funcs.GetLen     = VFSMMAP_GetLen;
	mmapfile->funcs.Close      = VFSMMAP_Close;
	mmapfile->funcs.Flush      = VFSMMAP_Flush;

	return (vfsfile_t *)mmapfile;
}

qbool FSMMAP_IsMemoryMapped(vfsfile_t* file)
{
	return file && file->ReadBytes == VFSMMAP_ReadBytes;
}

//#endif // WITH_VFS_MMAP
