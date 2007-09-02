/*
    $Id: fs.h,v 1.5 2007-09-02 21:59:29 johnnycz Exp $
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
#ifndef SERVERONLY
#define UserdirSet (userdirfile[0] != '\0')
extern	char userdirfile[MAX_OSPATH];
extern	char com_userdir[MAX_OSPATH];
void COM_SetUserDirectory (char *dir, char *type);
#endif
// <-- QW262

qbool FS_AddPak (char *pakfile);
qbool FS_RemovePak (const char *pakfile);

// ====================================================================
// Virtual File System

typedef enum {
	VFSERR_NONE,
	VFSERR_EOF
} vfserrno_t;

typedef struct vfsfile_s {
	int (*ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err);
	int (*WriteBytes) (struct vfsfile_s *file, void *buffer, int bytestowrite);
	qbool (*Seek) (struct vfsfile_s *file, unsigned long pos);	//returns false for error
	unsigned long (*Tell) (struct vfsfile_s *file);
	unsigned long (*GetLen) (struct vfsfile_s *file);	//could give some lag
	void (*Close) (struct vfsfile_s *file);
	void (*Flush) (struct vfsfile_s *file);
	qbool seekingisabadplan;
} vfsfile_t;

typedef enum {
	FS_NONE_OS, // file name used as is, opened with OS functions (no paks)
	FS_GAME_OS, // file used as com_basedir/filename, opened with OS functions (no paks)
	FS_ANY      // file searched on quake file system even in paks, u may use only "rb" mode for file since u can't write to pak
} relativeto_t;

// mostly analogs for stdio functions
void 			VFS_CLOSE  (struct vfsfile_s *vf);
unsigned long	VFS_TELL   (struct vfsfile_s *vf);
unsigned long	VFS_GETLEN (struct vfsfile_s *vf);
qbool			VFS_SEEK   (struct vfsfile_s *vf, unsigned long pos);
int				VFS_READ   (struct vfsfile_s *vf, void *buffer, int bytestoread, vfserrno_t *err);
int				VFS_WRITE  (struct vfsfile_s *vf, void *buffer, int bytestowrite);
void			VFS_FLUSH  (struct vfsfile_s *vf);
char		   *VFS_GETS   (struct vfsfile_s *vf, char *buffer, int buflen); // return null terminated string

void			VFS_TICK   (void);  // fill in/out our internall buffers (do read/write on socket)

// open some temp VFS file, which actually result from tmpfile() at least when i commenting this
vfsfile_t *FS_OpenTemp(void);
// some general function to open VFS file, except TCP
vfsfile_t *FS_OpenVFS(char *filename, char *mode, relativeto_t relativeto);
// TCP VFS file
vfsfile_t *FS_OpenTCP(char *name);

// ====================================================================
// GZIP & ZIP De/compression

#ifdef WITH_ZLIB
int COM_GZipPack (char *source_path,
				  char *destination_path,
				  qbool overwrite);

int COM_GZipUnpack (char *source_path,		// The path to the compressed source file.
					char *destination_path, // The destination file path.
					qbool overwrite);		// Overwrite the destination file if it exists?

int COM_GZipUnpackToTemp (char *source_path,		// The compressed source file.
						  char *unpack_path,		// A buffer that will contain the path to the unpacked file.
						  int unpack_path_size,		// The size of the buffer.	
						  char *append_extension);	// The extension if any that should be appended to the filename.

int COM_ZlibInflate(FILE *source, FILE *dest);

int COM_ZlibUnpack (char *source_path,		// The path to the compressed source file.
					char *destination_path, // The destination file path.
					qbool overwrite);		// Overwrite the destination file if it exists?

int COM_ZlibUnpackToTemp (char *source_path,		// The compressed source file.
						  char *unpack_path,		// A buffer that will contain the path to the unpacked file.
						  int unpack_path_size,		// The size of the buffer.	
						  char *append_extension);	// The extension if any that should be appended to the filename.
#endif // WITH_ZLIB

#ifdef WITH_ZIP
qbool COM_ZipIsArchive (char *zip_path);

int COM_ZipBreakupArchivePath (char *archive_extension,			// The extension of the archive type we're looking fore "zip" for example.
							   char *path,						// The path that should be broken up into parts.
							   char *archive_path,				// The buffer that should contain the archive path after the breakup.
							   int archive_path_size,			// The size of the archive path buffer.
							   char *inzip_path,				// The buffer that should contain the inzip path after the breakup.
							   int inzip_path_size);			// The size of the inzip path buffer.

unzFile COM_ZipUnpackOpenFile (const char *zip_path);

int COM_ZipUnpackCloseFile (unzFile zip_file);

int COM_ZipUnpack (unzFile zip_file, 
				   char *destination_path, 
				   qbool case_sensitive, 
				   qbool keep_path, 
				   qbool overwrite, 
				   const char *password);

int COM_ZipUnpackToTemp (unzFile zip_file, 
				   qbool case_sensitive, 
				   qbool keep_path, 
				   const char *password,
				   char *unpack_path,					// The path where the file was unpacked.
				   int unpack_path_size);				// The size of the buffer for "unpack_path", MAX_PATH is a goode idea.)

int COM_ZipUnpackOneFile (unzFile zip_file,				// The zip file opened with COM_ZipUnpackOpenFile(..)
						  const char *filename_inzip,	// The name of the file to unpack inside the zip.
						  const char *destination_path, // The destination path where to extract the file to.
						  qbool case_sensitive,			// Should we look for the filename case sensitivly?
						  qbool keep_path,				// Should the path inside the zip be preserved when unpacking?
						  qbool overwrite,				// Overwrite any existing file with the same name when unpacking?
						  const char *password);		// The password to use when extracting the file.

int COM_ZipUnpackOneFileToTemp (unzFile zip_file, 
						  const char *filename_inzip,
						  qbool case_sensitive, 
						  qbool keep_path,
						  const char *password,
						  char *unpack_path,			// The path where the file was unpacked.
						  int unpack_path_size);			// The size of the buffer for "unpack_path", MAX_PATH is a goode idea.

int COM_ZipUnpackCurrentFile (unzFile zip_file, 
							  const char *destination_path, 
							  qbool case_sensitive, 
							  qbool keep_path, 
							  qbool overwrite, 
							  const char *password);

int COM_ZipGetFirst (unzFile zip_file, sys_dirent *ent);

int COM_ZipGetNextFile (unzFile zip_file, sys_dirent *ent);

#endif // WITH_ZIP


#endif // __FS_H__
