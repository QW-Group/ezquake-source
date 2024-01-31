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
 * $Id: vfs_zip.c,v 1.12 2007-10-13 16:02:51 dkure Exp $
 *             
 */

#ifdef WITH_ZIP

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "vfs.h"

//===========================
// Unzip library interfacing
//===========================
// These functions provide IO support for the unzip library using our VFS layer

static void *FSZIP_ZOpenFile(void *fin, const char *filename, int mode) {
	vfsfile_t *vfs_fin = (vfsfile_t *)fin;

	// VFS-TODO Maybe need to increase reference count
	if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ) {
	} else if (mode & ZLIB_FILEFUNC_MODE_EXISTING) {
		Sys_Error("FSZIP_ZOpenFile: Unsupported file mode: MODE_EXISTING");
		return NULL;
	} else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
		Sys_Error("FSZIP_ZOpenFile: Unsupported file mode: write");
		return NULL;
	}

	return vfs_fin;
}

static unsigned long FSZIP_ZReadFile(void *fin, void *stream, void *buf, unsigned long size) {
	vfsfile_t *vfs_fin = (vfsfile_t *)stream;
	return VFS_READ(vfs_fin, buf, size, NULL);
}

static unsigned long FSZIP_ZWriteFile(void *fin, void *stream, const void *buf, unsigned long size) {
	vfsfile_t *vfs_fin = (vfsfile_t *)stream;
	return VFS_WRITE(vfs_fin, buf, size);
}

static long FSZIP_ZTellFile(void *fin, void *stream) {
	vfsfile_t *vfs_fin = (vfsfile_t *)stream;
	return VFS_TELL(vfs_fin);
}

static long FSZIP_ZSeekFile(void *fin, void *stream, unsigned long offset, int origin) {
	vfsfile_t *vfs_fin = (vfsfile_t *)stream;
	int fseek_origin;
	int r;

	switch (origin)
	{
	case ZLIB_FILEFUNC_SEEK_CUR: fseek_origin = SEEK_CUR; break;
	case ZLIB_FILEFUNC_SEEK_END: fseek_origin = SEEK_END; break;
	case ZLIB_FILEFUNC_SEEK_SET: fseek_origin = SEEK_SET; break;
	default: 
		Sys_Error("FSZIP_ZSeekFile: Unsupported seek origin: Unknown");
		return -1;
	}

	r = VFS_SEEK(vfs_fin, offset, fseek_origin);

	return r;
}

static int FSZIP_ZCloseFile(void *fin, void *stream) {
	// vfsfile_t *vfs_fin = (vfsfile_t *)stream;
	// VFS-TODO: Need to decrease references 
	// VFS_CLOSE(vfs_fin);
	return 0; 
}

static int FSZIP_ZErrorFileFile(void *fin, void *stream) {
	// vfsfile_t *vfs_fin = (vfsfile_t *)stream;
	// VFS-TODO: Need to get the error code somehow
	return 0;
}

static void FSZIP_CreteFileFuncs(zlib_filefunc_def *funcs) {
	if (funcs == NULL)
		return;

	funcs->zopen_file  = FSZIP_ZOpenFile;
	funcs->zread_file  = FSZIP_ZReadFile;
	funcs->zwrite_file = FSZIP_ZWriteFile;
	funcs->ztell_file  = FSZIP_ZTellFile;
	funcs->zseek_file  = FSZIP_ZSeekFile;
	funcs->zclose_file = FSZIP_ZCloseFile;
	funcs->zerror_file = FSZIP_ZErrorFileFile;
}

//==========================================
// ZIP file  (*.zip, *.pk3) - VFS Functions
//==========================================
typedef struct zipfile_s
{
	char filename[MAX_QPATH];
	unzFile handle;
	int		numfiles;
	packfile_t	*files;

#ifdef HASH_FILESYSTEM
	hashtable_t hash;
#endif

	zlib_filefunc_def zlib_funcs;

	vfsfile_t *raw;
	vfsfile_t *currentfile;	//our unzip.c can only handle one active file at any one time
							//so we have to keep closing and switching.
							//slow, but it works. most of the time we'll only have a single file open anyway.
	int references;	//and a reference count
} zipfile_t;

typedef struct {
	vfsfile_t funcs;

	vfsfile_t *defer;

	//in case we're forced away.
	zipfile_t *parent;
	int pos;
	int length;	//try and optimise some things
	int index;
	int startpos;
} vfszip_t;

// VFS-FIXME Need to figure what this function is trying to do
static void VFSZIP_MakeActive(vfszip_t *vfsz)
{
	int i;
	char buffer[8192];	//must be power of two

	if ((vfszip_t*)vfsz->parent->currentfile == vfsz)
		return;	//already us
	if (vfsz->parent->currentfile)
		unzCloseCurrentFile(vfsz->parent->handle);

	//unzLocateFileMy(vfsz->parent->handle, vfsz->index, vfsz->startpos);
	unzSetOffset(vfsz->parent->handle, vfsz->startpos);
	unzOpenCurrentFile(vfsz->parent->handle);

	if (vfsz->pos > 0)
	{
		Com_DPrintf("VFSZIP_MakeActive: Shockingly inefficient\n");

		//now we need to seek up to where we had previously gotten to.
		for (i = 0; i < vfsz->pos-sizeof(buffer); i++)
			unzReadCurrentFile(vfsz->parent->handle, buffer, sizeof(buffer));
		unzReadCurrentFile(vfsz->parent->handle, buffer, vfsz->pos - i);
	}

	vfsz->parent->currentfile = (vfsfile_t*)vfsz;
}

static int VFSZIP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err)
{
	int read;
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_READ(vfsz->defer, buffer, bytestoread, err);

	VFSZIP_MakeActive(vfsz);
	read = unzReadCurrentFile(vfsz->parent->handle, buffer, bytestoread);

	if (err)
		*err = ((read || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);

	vfsz->pos += read;
	return read;
}

static int VFSZIP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestoread)
{
	Sys_Error("VFSZIP_WriteBytes: Not supported\n");
	return 0;
}

// VFS-FIXME: What is going on here... why not just unzSetOffset
static int VFSZIP_Seek (struct vfsfile_s *file, unsigned long pos, int whence)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_SEEK(vfsz->defer, pos, whence);

	//This is *really* inefficient
	if (vfsz->parent->currentfile == file)
	{
		char buffer[8192];
		unsigned int chunk;
		unsigned int i;
		unsigned int length;

		vfsz->defer = FS_OpenTemp();
		if (vfsz->defer)
		{
			unzCloseCurrentFile(vfsz->parent->handle);
			vfsz->parent->currentfile = NULL;	//make it not us

			length = vfsz->length;
			i = 0;
			vfsz->pos = 0;
			VFSZIP_MakeActive(vfsz);
			while (1)
			{
				chunk = length - i;
				if (chunk > sizeof(buffer))
					chunk = sizeof(buffer);
				if (chunk == 0)
					break;
				unzReadCurrentFile(vfsz->parent->handle, buffer, chunk);
				VFS_WRITE(vfsz->defer, buffer, chunk);

				i += chunk;
			}
		}

		unzCloseCurrentFile(vfsz->parent->handle);
		vfsz->parent->currentfile = NULL;	//make it not us

		if (vfsz->defer)
			return VFS_SEEK(vfsz->defer, pos, SEEK_SET);
	}



	if (pos > vfsz->length)
		return -1;
	vfsz->pos = pos;

	return 0;
}

static unsigned long VFSZIP_Tell (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_TELL(vfsz->defer);

	return vfsz->pos;
}

static unsigned long VFSZIP_GetLen (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;
	return vfsz->length;
}

static void FSZIP_ClosePath(void *handle);
static void VFSZIP_Close (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->parent->currentfile == file)
		vfsz->parent->currentfile = NULL;	//make it not us

	if (vfsz->defer)
		VFS_CLOSE(vfsz->defer);

	FSZIP_ClosePath(vfsz->parent);
	Q_free(vfsz);
}

static vfsfile_t *FSZIP_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	//int rawofs;
	zipfile_t *zip = handle;
	vfszip_t *vfsz;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfsz = Q_calloc(1, sizeof(vfszip_t));

	vfsz->parent = zip;
	vfsz->index = loc->index;
	vfsz->startpos = zip->files[loc->index].filepos;
	vfsz->length = loc->len;

	vfsz->funcs.ReadBytes  = strcmp(mode, "rb") ? NULL : VFSZIP_ReadBytes;
	vfsz->funcs.WriteBytes = strcmp(mode, "wb") ? NULL : VFSZIP_WriteBytes;
	vfsz->funcs.Seek       = VFSZIP_Seek;
	vfsz->funcs.seekingisabadplan = true;
	vfsz->funcs.Tell       = VFSZIP_Tell;
	vfsz->funcs.GetLen     = VFSZIP_GetLen;
	vfsz->funcs.Close      = VFSZIP_Close;
	if (loc->search)
		vfsz->funcs.copyprotected = loc->search->copyprotected;

	zip->references++;

	return (vfsfile_t*)vfsz;
}

//=============================================
// ZIP file  (*.zip, *.pk3) - Search Functions
//=============================================
static void FSZIP_PrintPath(void *handle)
{
	zipfile_t *zip = handle;

	if (zip->references != 1)
		Com_Printf("%s (%i)\n", zip->filename, zip->references-1);
	else
		Com_Printf("%s\n", zip->filename);
}

static void FSZIP_ClosePath(void *handle)
{
	zipfile_t *zip = handle;

	if (--zip->references > 0)
		return;	//not yet time

	unzClose(zip->handle);
	VFS_CLOSE(zip->raw);
	if (zip->files)
		Q_free(zip->files);
	Q_free(zip);
}
static void FSZIP_BuildHash(void *handle)
{
	zipfile_t *zip = handle;
	int i;

	for (i = 0; i < zip->numfiles; i++)
	{
		if (!Hash_GetInsensitive(filesystemhash, zip->files[i].name))
		{
			Hash_AddInsensitive(filesystemhash, zip->files[i].name, &zip->files[i]);
			fs_hash_files++;
		}
		else
			fs_hash_dups++;
	}
}

static qbool FSZIP_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	packfile_t *pf  = (packfile_t *) hashedresult;
	zipfile_t  *zip = (zipfile_t  *) handle;
	int i;

// look through all the pak file elements

	if (pf)
	{	//is this a pointer to a file in this pak?
		if (pf < zip->files || pf >= zip->files + zip->numfiles)
			return false;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<zip->numfiles ; i++)	//look for the file
		{
			if (!strcasecmp (zip->files[i].name, filename))
			{
				pf = &zip->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		if (loc)
		{
			loc->index = pf - zip->files;
			strlcpy (loc->rawname, zip->filename, sizeof (loc->rawname));
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
			loc->search = NULL;
	
			// VFS-FIXME: What is the purpose of the stuff below....
//			unzLocateFileMy (zip->handle, loc->index, zip->files[loc->index].filepos);
//			loc->offset = unzGetCurrentFileUncompressedPos(zip->handle);
//			if (loc->offset<0)
//			{	//file not found, or is compressed.
//				*loc->rawname = '\0';
//				loc->offset=0;
//			}
		}
		return true;
	}
	return false;
}


static void FSZIP_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	zipfile_t *zip = handle;
	int err;

	unzSetOffset(zip->handle, zip->files[loc->index].filepos);

	unzOpenCurrentFile (zip->handle);
	err = unzReadCurrentFile (zip->handle, buffer, zip->files[loc->index].filelen);
	unzCloseCurrentFile (zip->handle);

	if (err!=zip->files[loc->index].filelen)
	{
		Com_Printf ("Can't extract file \"%s:%s\" (corrupt)\n", zip->filename, zip->files[loc->index].name);
		return;
	}
	return;
}


static int FSZIP_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	zipfile_t *zip = handle;
	int		num;

	for (num = 0; num < zip->numfiles; num++)
	{
		if (wildcmp(match, zip->files[num].name))
		{
			if (!func(zip->files[num].name, zip->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}

/*
=================
COM_LoadZipFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static void *FSZIP_LoadZipFile(vfsfile_t *packhandle, const char *desc)
{
	int i, r;

	zipfile_t *zip;
	packfile_t		*newfiles;
	zlib_filefunc_def *funcs = NULL;
	unz_global_info info;
	
	zip   = (zipfile_t *) Q_calloc(1, sizeof(*zip));
	strlcpy (zip->filename, desc, sizeof (zip->filename));
	FSZIP_CreteFileFuncs(&(zip->zlib_funcs));
	zip->raw = packhandle;
	zip->handle = unzOpen2(desc, funcs);
	if (!zip->handle) goto fail;

	if (unzGetGlobalInfo(zip->handle, &info) != UNZ_OK) goto fail;

	// Get the number of zip files
	zip->numfiles = info.number_entry;

	// Create a list of the number of files
	zip->files = newfiles = Q_malloc (zip->numfiles * sizeof(packfile_t));
	if (unzGoToFirstFile(zip->handle) != UNZ_OK) goto fail;
	for (i = 0; i < zip->numfiles; i++) {
		unz_file_info file_info;
		if (unzGetCurrentFileInfo(zip->handle, &file_info, newfiles[i].name, sizeof(newfiles[i].name), NULL, 0, NULL, 0) != UNZ_OK) goto fail;

		Q_strlwr(newfiles[i].name);
		newfiles[i].filelen = file_info.uncompressed_size;
		newfiles[i].filepos = unzGetOffset(zip->handle); // VFS-FIXME: Need to verify this
		r = unzGoToNextFile (zip->handle);
		if (r == UNZ_END_OF_LIST_OF_FILE) {
			break;
		} else if (r != UNZ_OK) {
			goto fail;
		}

	}
	
	zip->references = 1;
	zip->currentfile = NULL;

	return zip;

fail:
	// Q_free is safe to call on NULL pointers
	Q_free(funcs);
	Q_free(zip->handle);
	Q_free(zip->files);
	Q_free(zip);
	return NULL;
}

// VFS-FIXME: Don't really seem to know what this does
static int FSZIP_GeneratePureCRC(void *handle, int seed, int crctype)
{
	zipfile_t *zip = handle;
	unz_file_info   file_info;

	int *filecrcs;
	int numcrcs=0;
	int i;

	filecrcs = Q_malloc((zip->numfiles+1)*sizeof(int));
	filecrcs[numcrcs++] = seed;

	unzGoToFirstFile(zip->handle);
	for (i = 0; i < zip->numfiles; i++)
	{
		if (zip->files[i].filelen>0)
		{
			unzGetCurrentFileInfo (zip->handle, &file_info, NULL, 0, NULL, 0, NULL, 0);
			filecrcs[numcrcs++] = file_info.crc;
		}
		unzGoToNextFile (zip->handle);
	}

	if (crctype)
		return Com_BlockChecksum(filecrcs, numcrcs*sizeof(int));
	else
		return Com_BlockChecksum(filecrcs+1, (numcrcs-1)*sizeof(int));
}

searchpathfuncs_t zipfilefuncs = {
	FSZIP_PrintPath,
	FSZIP_ClosePath,
	FSZIP_BuildHash,
	FSZIP_FLocate,
	FSZIP_ReadFile,
	FSZIP_EnumerateFiles,
	FSZIP_LoadZipFile,
	FSZIP_GeneratePureCRC,
	FSZIP_OpenVFS
};

#endif // WITH_ZIP
