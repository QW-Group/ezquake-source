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
 * $Id: vfs_gzip.c,v 1.2 2007-09-28 05:21:46 dkure Exp $
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

// FIXME:
// Everything below assumes that the input file was an OS file
// This may not be the case if we are opening a gz file in a gz file...
gzFile Gzip_Open(vfsfile_t *file) 
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	gzFile f_gz;

	int fd = fileno(intfile->handle);

	f_gz = gzdopen(dup(fd), "r");
	if (f_gz == NULL) {
		return NULL;
	}
	return f_gz;
}

int Gzip_Read(gzFile f_gz, void *buffer, int bytestoread) 
{
	int r;
	
	r = gzread(f_gz, buffer, bytestoread);

	// r == -1 on error

	return r;
}

int Gzip_Write(gzFile f_gz, void *buffer, int bytestowrite) 
{
	int r;

	r = gzwrite(f_gz, buffer, bytestowrite);

	// r == 0 on error
	
	return r;
}

int Gzip_Seek(gzFile f_gz, int offset, int whence) 
{
	int r;
	r = gzseek(f_gz, offset, whence);
	return r;
}

int Gzip_Tell(gzFile f_gz) 
{
	int r;
	r = gztell(f_gz);
	return r;
}

int Gzip_GetLen(gzFile f_gz) 
{
	int r, currentpos;
	
	// VFS-FIXME: Error handling
	currentpos = gztell(f_gz);
	r = gzseek(f_gz, 0, SEEK_END);
	gzseek(f_gz, currentpos, SEEK_SET);

	return r;
}

int Gzip_Close(gzFile f_gz) 
{
	int r;

	r = gzclose(f_gz);

	return r;
}

int Gzip_Flush(gzFile f_gz) 
{
	int r;

	//r = gzflush(f_gz, Z_NO_FLUSH); 	// <-- Allows better compression
	r = gzflush(f_gz, Z_SYNC_FLUSH); 	// <-- All pending output is flushed

	return r;
}

#endif // WITH_VFS_GZIP
#endif // WITH_ZLIB
