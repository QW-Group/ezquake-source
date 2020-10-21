/*
Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __FS_H__
#define __FS_H__

#ifdef WITH_ZLIB
#include "zlib.h"
#endif
#ifdef WITH_ZIP
#include "unzip.h"
#endif

void FS_InitModuleFS (void);

// ====================================================================
// Quake File System addons
// original common FS functions are declared in common.h

// QW262 -->
#define UserdirSet (userdirfile[0] != '\0')
extern	char userdirfile[MAX_OSPATH];
extern	char com_userdir[MAX_OSPATH];
void FS_SetUserDirectory (char *dir, char *type);
// <-- QW262

// ====================================================================
// Virtual File System

typedef enum {
	VFSERR_NONE,
	VFSERR_EOF
} vfserrno_t;

typedef struct vfsfile_s {
	int (*ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err);
	int (*WriteBytes) (struct vfsfile_s *file, const void *buffer, int bytestowrite);
	int (*Seek) (struct vfsfile_s *file, unsigned long pos, int whence);	// Returns 0 on sucess, -1 otherwise
	unsigned long (*Tell) (struct vfsfile_s *file);
	unsigned long (*GetLen) (struct vfsfile_s *file);	// Could give some lag
	void (*Close) (struct vfsfile_s *file);
	void (*Flush) (struct vfsfile_s *file);
	qbool seekingisabadplan;
	qbool copyprotected;							// File found was in a pak
} vfsfile_t;

// VFS-FIXME: D-Kure Clean up this structure
typedef enum {
	FS_NONE_OS, // file name used as is, opened with OS functions (no paks)
	FS_GAME_OS, // file used as com_basedir/filename, opened with OS functions 
				// (no paks)
	FS_BASE,	// file is relative to the com_basedir/com_homedir
	FS_HOME,
	FS_PAK,
	FS_ANY      // file searched on quake file system even in paks, u may use
				// only "rb" mode for file since u can't write to pak
} relativeto_t;

// mostly analogs for stdio functions
void 			VFS_CLOSE  (struct vfsfile_s *vf);
unsigned long	VFS_TELL   (struct vfsfile_s *vf);
unsigned long	VFS_GETLEN (struct vfsfile_s *vf);
int				VFS_SEEK   (struct vfsfile_s *vf, unsigned long pos, int whence);
int				VFS_READ   (struct vfsfile_s *vf, void *buffer, int bytestoread, vfserrno_t *err);
int				VFS_WRITE  (struct vfsfile_s *vf, const void *buffer, int bytestowrite);
void			VFS_FLUSH  (struct vfsfile_s *vf);
char		   *VFS_GETS   (struct vfsfile_s *vf, char *buffer, int buflen); 
				// return null terminated string

void			VFS_TICK   (void);  // fill in/out our internall buffers 
									// (do read/write on socket)
vfsfile_t      *VFS_Filter(const char *filename, vfsfile_t *handle);
qbool			VFS_COPYPROTECTED(struct vfsfile_s *vf);

// some general function to open VFS file, except TCP
vfsfile_t *FS_OpenVFS(const char *filename, char *mode,relativeto_t relativeto);

// TCP VFS file
vfsfile_t *FS_OpenTCP(char *name);

typedef enum {
	FS_LOAD_NONE     = 1,
	FS_LOAD_FILE_PAK = 2,
	FS_LOAD_FILE_PK3 = 4,
	FS_LOAD_FILE_PK4 = 8,
	FS_LOAD_FILE_DOOMWAD = 16,
	FS_LOAD_FROM_PAKLST = 32,
	FS_LOAD_FILE_ALL = FS_LOAD_FILE_PAK | FS_LOAD_FILE_PK3 
		| FS_LOAD_FILE_PK4 | FS_LOAD_FILE_DOOMWAD | FS_LOAD_FROM_PAKLST,
} FS_Load_File_Types;

void FS_AddGameDirectory (char *dir, unsigned int loadstuff);

char *FS_NextPath (char *prevpath);

extern cvar_t fs_cache;
extern qbool filesystemchanged;

// ====================================================================
// GZIP & ZIP De/compression

#ifdef WITH_ZLIB
int FS_GZipPack (char *source_path,
				  char *destination_path,
				  qbool overwrite);

int FS_GZipUnpack (char *source_path,		// The path to the compressed source file.
					char *destination_path, // The destination file path.
					qbool overwrite);		// Overwrite the destination file if it exists?

void* FS_GZipUnpackToMemory(char* source_path, size_t* unpacked_length);

int FS_ZlibInflate(FILE *source, FILE *dest);

int FS_ZlibUnpack (char *source_path,		// The path to the compressed source file.
					char *destination_path, // The destination file path.
					qbool overwrite);		// Overwrite the destination file if it exists?

int FS_ZlibUnpackToTemp (char *source_path,		// The compressed source file.
						  char *unpack_path,		// A buffer that will contain the path to the unpacked file.
						  int unpack_path_size,		// The size of the buffer.	
						  char *append_extension);	// The extension if any that should be appended to the filename.
#endif // WITH_ZLIB

#ifdef WITH_ZIP
qbool FS_IsArchive (char *path);

int FS_ZipBreakupArchivePath (char *archive_extension,			// The extension of the archive type we're looking fore "zip" for example.
							   char *path,						// The path that should be broken up into parts.
							   char *archive_path,				// The buffer that should contain the archive path after the breakup.
							   int archive_path_size,			// The size of the archive path buffer.
							   char *inzip_path,				// The buffer that should contain the inzip path after the breakup.
							   int inzip_path_size);			// The size of the inzip path buffer.

unzFile FS_ZipUnpackOpenFile (const char *zip_path);

int FS_ZipUnpackCloseFile (unzFile zip_file);

int FS_ZipUnpack (unzFile zip_file, 
				   char *destination_path, 
				   qbool case_sensitive, 
				   qbool keep_path, 
				   qbool overwrite, 
				   const char *password);

int FS_ZipUnpackToTemp (unzFile zip_file, 
				   qbool case_sensitive, 
				   qbool keep_path, 
				   const char *password,
				   char *unpack_path,					// The path where the file was unpacked.
				   int unpack_path_size);				// The size of the buffer for "unpack_path", MAX_PATH is a goode idea.)

int FS_ZipUnpackOneFile (unzFile zip_file,				// The zip file opened with FS_ZipUnpackOpenFile(..)
						  const char *filename_inzip,	// The name of the file to unpack inside the zip.
						  const char *destination_path, // The destination path where to extract the file to.
						  qbool case_sensitive,			// Should we look for the filename case sensitivly?
						  qbool keep_path,				// Should the path inside the zip be preserved when unpacking?
						  qbool overwrite,				// Overwrite any existing file with the same name when unpacking?
						  const char *password);		// The password to use when extracting the file.

void* FS_ZipUnpackOneFileToMemory(
	unzFile zip_file,
	const char* filename_inzip,
	qbool case_sensitive,
	qbool keep_path,
	const char* password,
	size_t* unpacked_length
);

int FS_ZipUnpackCurrentFile (unzFile zip_file, 
							  const char *destination_path, 
							  qbool case_sensitive, 
							  qbool keep_path, 
							  qbool overwrite, 
							  const char *password);

int FS_ZipGetFirst (unzFile zip_file, sys_dirent *ent);

int FS_ZipGetNextFile (unzFile zip_file, sys_dirent *ent);

#endif // WITH_ZIP

qbool FS_UnsafeFilename(const char* name);

#endif // __FS_H__
