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
 * $Id: vfs_tar.c,v 1.8 2007-10-13 16:02:51 dkure Exp $
 *             
 */

#ifdef WITH_FTE_VFS

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "fs.h"
#include "vfs.h"
#include "vfs_tar.h"

//=============================================================================
//                       T A R    V F S
//=============================================================================
// These functions give a VFS layer to operations for a file tar file 

typedef struct tarfile_s
{
	char filename[MAX_QPATH];
	hashtable_t hash;
	vfsfile_t *raw;

	int numfiles;
	packfile_t  *files;

	unsigned long filepos;

	int references;
} tarfile_t;

typedef struct 
{
	vfsfile_t funcs; // <= must be at top/begining of struct

	tarfile_t *parent;

	unsigned long startpos;
	unsigned long length;
	unsigned long currentpos;
} vfstarfile_t;

// convert octal digits to int
// on error return -1
static int getoct (char *p,int width)
{
	int result = 0;
	char c;

	while (width--)
	{
		c = *p++;
		if (c == 0)
			break;
		if (c == ' ')
			continue;
		if (c < '0' || c > '7')
			return -1;
		result = result * 8 + (c - '0');
	}
	return result;
}

// =====================
// tarOperationGetFiles
// =====================
// Returns the number of files found
// Placing the names in files if a valid pointer
//
// -1 is returned on error

static int tarOperationIndexFiles(vfsfile_t *in, packfile_t *files) {
	// Tar vars
	union  tar_buffer buffer;
	int    len;
	int    getheader = HEADER_SHORTNAME;
	int    remaining = 0;
	char   fname[BLOCKSIZE];
	// Quake vars
	int numfiles = 0;

	VFS_SEEK(in, 0, SEEK_SET); /* Read the file from the begining */

	while (1)
	{
		len = VFS_READ(in, &buffer, BLOCKSIZE, NULL);
		if (len < 0 || len != BLOCKSIZE) {
			Com_Printf("Invalid tar file\n");
			return -1;
		}

		// We have reached the end of the tar 
		if (len == 0 || buffer.header.name[0] == 0)
			break;

		if (getheader == HEADER_SHORTNAME)
		{
			strncpy(fname,buffer.header.name,SHORTNAMESIZE);
			if (fname[SHORTNAMESIZE-1] != 0)
				fname[SHORTNAMESIZE] = 0;
		}
		else
		{
			/* The file name is longer than SHORTNAMESIZE */
			if (strncmp(fname,buffer.header.name,SHORTNAMESIZE-1) != 0) {
				Com_Printf("bad long name\n");
				return -1;
			}
			getheader = HEADER_SHORTNAME;
		}

		/* Act according to the type flag */
		switch (buffer.header.typeflag)
		{
			case DIRTYPE:
				break;
			case REGTYPE:
			case AREGTYPE:
				remaining = getoct(buffer.header.size,12);
				if (remaining == -1)
				{
					Com_Printf("Invalid tar file\n");
					return -1;
				}

				//Com_Printf(" %9d %s\n",remaining,fname);

				if (files) {
					strlcpy(files[numfiles].name, fname, sizeof(files[numfiles].name));
					files[numfiles].filelen = remaining;
					files[numfiles].filepos = VFS_TELL(in);
				}
				numfiles++;

				getheader = HEADER_NONE;
				break;
			case GNUTYPE_LONGLINK:
			case GNUTYPE_LONGNAME:
				/* FIXME: Handle long names */
				remaining = getoct(buffer.header.size,12);
				if (remaining < 0 || remaining >= BLOCKSIZE)
				{
					printf("Invalid tar file\n");
					return -1;
				}
				len = VFS_READ(in, fname, BLOCKSIZE, NULL);
				if (len < 0) {
					printf("Problem reading blocksize");
					return -1;
				}
				if (fname[BLOCKSIZE-1] != 0 || (int)strlen(fname) > remaining)
				{
					printf("Invalid tar file\n");
					return -1;
				}
				getheader = HEADER_LONGNAME;
				break;
			default:
				printf("      <---> %s\n",fname);
				numfiles++;
				break;
		}

		/* Seek the next valid header which contains file names */
		if (getheader == HEADER_NONE) {
			unsigned int offset; 

			/* Round up to the nearest BLOCKSIZE */
			if (remaining % BLOCKSIZE) {
				offset = remaining + (BLOCKSIZE - (remaining % BLOCKSIZE));
			} else {
				offset = remaining;
			}

			VFS_SEEK(in, offset, SEEK_CUR);
			getheader = HEADER_SHORTNAME;
		}
	}


	return numfiles;
}

//=============================================================================

static int VFSTAR_ReadBytes(vfsfile_t *file, void *buffer, int bytestoread, vfserrno_t *err) 
{
	vfstarfile_t *vfst = (vfstarfile_t *)file;
	int read;
	//unsigned long base, bytes; <-- For reading in multiples of BLOCKSIZE

	// VFS-FIXME: Need to read in chunks of BLOCKSIZE
	if (vfst->currentpos - vfst->startpos + bytestoread > vfst->length)
		bytestoread = vfst->length - (vfst->currentpos - vfst->startpos);

	if (bytestoread < 0)
		Sys_Error("VFSTAR_ReadBytes: bytestoread < 0");

	if (vfst->parent->filepos != vfst->currentpos) {
		VFS_SEEK(vfst->parent->raw, vfst->currentpos, SEEK_SET);
	}

	read = VFS_READ(vfst->parent->raw, buffer, bytestoread, NULL);
	vfst->currentpos += read;
	vfst->parent->filepos = vfst->currentpos;

	// VFS-FIXME: Need to figure out errors
	if (err) 
		*err = bytestoread == 0 ? VFSERR_EOF : VFSERR_NONE;

	return read;
}

static int VFSTAR_WriteBytes(vfsfile_t *file, const void *buffer, int bytestowrite) 
{
	Sys_Error("VFSTAR_WriteBytes: Invalid operatioin\n");
	return 0;
}

static int VFSTAR_Seek(vfsfile_t *file, unsigned long offset, int whence) 
{
	vfstarfile_t *vfst = (vfstarfile_t *)file;

	switch(whence) {
	case SEEK_SET: 
		vfst->currentpos = vfst->startpos + offset; 
		break;
	case SEEK_CUR: 
		vfst->currentpos += offset; 
		break;
	case SEEK_END: 
		vfst->currentpos = vfst->startpos + vfst->length + offset;
		break;
	default:
		Sys_Error("VFSTAR_Seek: Unknown whence value(%d)\n", whence);
		return -1;
	}

	if (vfst->currentpos > vfst->length) {
		Com_Printf("VFSTAR_Seek: Warning seeking past the file's size\n");
	}

	return 0;
}

static unsigned long VFSTAR_Tell(vfsfile_t *file) 
{
	vfstarfile_t *vfst = (vfstarfile_t *)file;

	return vfst->currentpos - vfst->startpos;;
}

static unsigned long VFSTAR_GetLen(vfsfile_t *file) 
{
	vfstarfile_t *vfst = (vfstarfile_t *)file;
	
	return vfst->length;
}

static void FSTAR_ClosePath(void *handle);
static void VFSTAR_Close(vfsfile_t *file) 
{
	vfstarfile_t *vfst = (vfstarfile_t *)file;

	FSTAR_ClosePath(vfst->parent);
	Q_free(vfst);
}

static void VFSTAR_Flush(vfsfile_t *file) 
{
	Sys_Error("VFSTAR_Flush: Invalid operation\n");
}

static vfsfile_t *FSTAR_OpenVFS(void *handle, flocation_t *loc, char *mode) 
{
	tarfile_t *tar = (tarfile_t *) handle;
	vfstarfile_t *vfst;
	
	if (strcmp(mode, "rb"))
		return NULL;

	vfst = Q_calloc(1, sizeof(*vfst));
	vfst->parent = tar;
	vfst->parent->references++;

	vfst->startpos   = loc->offset;
	vfst->length     = loc->len;
	vfst->currentpos = vfst->startpos;

	vfst->funcs.ReadBytes  = VFSTAR_ReadBytes;
	vfst->funcs.WriteBytes = VFSTAR_WriteBytes;
	vfst->funcs.Seek       = VFSTAR_Seek;
	vfst->funcs.Tell       = VFSTAR_Tell;
	vfst->funcs.GetLen     = VFSTAR_GetLen;
	vfst->funcs.Close      = VFSTAR_Close;
	vfst->funcs.Flush      = VFSTAR_Flush;
	if (loc->search)
		vfst->funcs.copyprotected = loc->search->copyprotected;

	return (vfsfile_t *)vfst;
}

//=============================================
// TAR file  (*.tar) - Search Functions
//=============================================
static void FSTAR_PrintPath(void *handle)
{
	tarfile_t *tar = (tarfile_t *)handle;

	if (tar->references != 1)
		Com_Printf("%s (%i)\n", tar->filename, tar->references-1);
	else
		Com_Printf("%s\n", tar->filename);
}

static void FSTAR_ClosePath(void *handle)
{
	tarfile_t *tar = (tarfile_t *)handle;

	if (--tar->references > 0)
		return; //not yet time

	VFS_CLOSE(tar->raw);
	if (tar->files)
		Q_free(tar->files);
	Q_free(tar);
}

static void FSTAR_BuildHash(void *handle)
{
	tarfile_t *tar = (tarfile_t *)handle;
	int i;

	for (i = 0; i < tar->numfiles; i++)
	{
		if (!Hash_GetInsensitive(filesystemhash, tar->files[i].name))
		{
			Hash_AddInsensitive(filesystemhash, tar->files[i].name, &tar->files[i]);
			fs_hash_files++;
		}
		else
			fs_hash_dups++;
	}
}

static qbool FSTAR_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	packfile_t *pf = (packfile_t *) hashedresult;
	tarfile_t *tar = (tarfile_t  *) handle;
	int i;

	// look through all the pak file elements

	if (pf)
	{   //is this a pointer to a file in this pak?
		if (pf < tar->files || pf > tar->files + tar->numfiles)
			return false;   //was found in a different path
	}
	else
	{
		for (i=0 ; i<tar->numfiles ; i++)   //look for the file
		{
			if (!strcmp (tar->files[i].name, filename))
			{
				pf = &tar->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		if (loc)
		{
			loc->index = pf - tar->files;
			snprintf(loc->rawname, sizeof(loc->rawname), "%s/%s", tar->filename, filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return true;
	}
	return false;

}

static void FSTAR_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	tarfile_t *tar = handle;
	int err;

	VFS_SEEK(tar->raw, tar->files[loc->index].filepos, SEEK_SET);

	err = VFS_READ(tar->raw, buffer, tar->files[loc->index].filelen, NULL);

	if (err!=tar->files[loc->index].filelen)
	{
		Com_Printf ("Can't extract file \"%s:%s\" (corrupt)\n", tar->filename, tar->files[loc->index].name);
		return;
	}
	return;
}

static int FSTAR_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	tarfile_t *tar = handle;
	int     num;

	for (num = 0; num < tar->numfiles; num++)
	{
		if (wildcmp(match, tar->files[num].name))
		{
			if (!func(tar->files[num].name, tar->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}

// =================
// FSTAR_LoadTarFile
// =================
// Takes an explicit (not game tree related) path to a tar file.
//
// Loads the header and directory, adding the files at the beginning
// of the list so they override previous pack files.

static void *FSTAR_LoadTarFile(vfsfile_t *tarhandle, const char *desc)
{
	tarfile_t *tar;

	tar = Q_calloc(1, sizeof(*tar));
	strlcpy (tar->filename, desc, sizeof (tar->filename));
	tar->raw = tarhandle;
	if (!tar->raw) goto fail;

	// Get the number of files inside the tar
	tar->numfiles = tarOperationIndexFiles(tar->raw, NULL);
	if (tar->numfiles < 0) goto fail;

	// Create a list of the number of files
	tar->files = (packfile_t *)Q_malloc(tar->numfiles * sizeof(packfile_t));
	tarOperationIndexFiles(tar->raw, tar->files);

	tar->references = 1;

	return tar;

fail:
	// Q_free is safe to call on NULL pointers
	Q_free(tar->files);
	Q_free(tar);
	return NULL;
}

searchpathfuncs_t tarfilefuncs = {
	FSTAR_PrintPath,
	FSTAR_ClosePath,
	FSTAR_BuildHash,
	FSTAR_FLocate,
	FSTAR_ReadFile,
	FSTAR_EnumerateFiles,
	FSTAR_LoadTarFile,
	NULL, // VFS-FIXME: Might be used for f_modification
	FSTAR_OpenVFS 
};

#endif // WITH_FTE_VFS
