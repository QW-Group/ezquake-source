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
 * $Id: vfs.h,v 1.1 2007-09-28 04:41:37 dkure Exp $
 *             
 */

#ifndef __VFS_H__
#define __VFS_H__

// Quake filesystem
extern hashtable_t filesystemhash;  // VFS-FIXME: Should probably go in fs.h
extern int fs_hash_dups;			// VFS-FIXME: Should probably go in fs.h
extern int fs_hash_files;			// VFS-FIXME: Should probably go in fs.h

typedef struct {
	struct searchpath_s *search;
	int             index;
	char            rawname[MAX_OSPATH];
	int             offset;
	int             len;
} flocation_t;

typedef struct {
	void	(*PrintPath)(void *handle);
	void	(*ClosePath)(void *handle);
	void	(*BuildHash)(void *handle);
	qbool   (*FindFile)(void *handle, flocation_t *loc, char *name, void *hashedresult);	
		// true if found (hashedresult can be NULL)
		// note that if rawfile and offset are set, many Com_FileOpens will 
		// read the raw file otherwise ReadFile will be called instead.
	void	(*ReadFile)(void *handle, flocation_t *loc, char *buffer);	
		// reads the entire file
	int		(*EnumerateFiles)(void *handle, char *match, int (*func)(char *, int, void *), void *parm);

	void	*(*OpenNew)(vfsfile_t *file, char *desc);	
		// returns a handle to a new pak/path

	int		(*GeneratePureCRC) (void *handle, int seed, int usepure);

	vfsfile_t *(*OpenVFS)(void *handle, flocation_t *loc, char *mode);
} searchpathfuncs_t;

//=================
// STDIO Files (OS)
//=================
typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	FILE *handle;

} vfsosfile_t;

// Stdio (OS) Files - VFS functions
int VFSOS_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err);
int VFSOS_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite);
qbool VFSOS_Seek (struct vfsfile_s *file, unsigned long pos, int whence);
unsigned long VFSOS_Tell (struct vfsfile_s *file);
unsigned long VFSOS_GetSize (struct vfsfile_s *file);
void VFSOS_Close(vfsfile_t *file);
vfsfile_t *FS_OpenTemp(void);
#ifndef WITH_FTE_VFS
vfsfile_t *VFSOS_Open(char *name, FILE *f, char *mode);
#else
vfsfile_t *VFSOS_Open(char *osname, char *mode);
#endif // WITH_FTE_VFS

// Stdio (OS) Files - Search functions
#ifdef WITH_FTE_VFS
void FSOS_PrintPath(void *handle);
void FSOS_ClosePath(void *handle);
int FSOS_RebuildFSHash(char *filename, int filesize, void *data);
void FSOS_BuildHash(void *handle);
qbool FSOS_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult);
void FSOS_ReadFile(void *handle, flocation_t *loc, char *buffer);
int FSOS_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm);

extern searchpathfuncs_t osfilefuncs;
#endif

//==================
// PACK files (*pak)
//==================

// in memory
typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;

#ifdef WITH_FTE_VFS
	bucket_t bucket;
#endif
} packfile_t;

// on disk
typedef struct pack_s
{
	char    filename[MAX_OSPATH];
#ifndef WITH_FTE_VFS
	FILE    *handle;
#else
	vfsfile_t    *handle;
	unsigned int filepos;   // the pos the subfiles left it at 
							// (to optimize calls to vfs_seek)
	int references;         // seeing as all vfiles from a pak file use the 
							// parent's vfsfile, we need to keep the parent 
							// open until all subfiles are closed.

#endif // WITH_FTE_VFS
	int     numfiles;
	packfile_t  *files;
} pack_t;

// on disk
typedef struct
{
	char    name[56];
	int     filepos, filelen;
} dpackfile_t;

typedef struct
{
	char    id[4];
	int     dirofs;
	int     dirlen;
} dpackheader_t;

// Pack (*.pak) - VFS Functions
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread, vfserrno_t *err);
int VFSPAK_WriteBytes (struct vfsfile_s *vfs, const void *buffer, int bytestoread);
qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos, int whence);
unsigned long VFSPAK_Tell (struct vfsfile_s *vfs);
unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs);
void VFSPAK_Close(vfsfile_t *vfs);
#ifndef WITH_FTE_VFS
vfsfile_t *FSPAK_OpenVFS(FILE *handle, int fsize, int fpos, char *mode);
#else
vfsfile_t *FSPAK_OpenVFS(void *handle, flocation_t *loc, char *mode);
#endif // WITH_FTE_VFS

// Pack (*.pak) - Search Functions
#ifdef WITH_FTE_VFS
void FSPAK_PrintPath(void *handle);
void FSPAK_ClosePath(void *handle);
void FSPAK_BuildHash(void *handle);
qbool FSPAK_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult);
int FSPAK_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm);
void *FSPAK_LoadPackFile (vfsfile_t *file, char *desc);

extern searchpathfuncs_t packfilefuncs;
#endif // WITH_FTE_VFS

// Zip files (*.zip, *.pk3)
#ifdef WITH_ZIP
// Zip (*.zip, *.pk3) - VFS Functions
int VFSZIP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err);
int VFSZIP_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestoread);
qbool VFSZIP_Seek (struct vfsfile_s *file, unsigned long pos, int whence);
unsigned long VFSZIP_Tell (struct vfsfile_s *file);
unsigned long VFSZIP_GetLen (struct vfsfile_s *file);
void VFSZIP_Close (struct vfsfile_s *file);
vfsfile_t *FSZIP_OpenVFS(void *handle, flocation_t *loc, char *mode);
	
// Zip (*.zip, *.pk3) - Search Functions
void FSZIP_PrintPath(void *handle);
void FSZIP_ClosePath(void *handle);
void FSZIP_BuildHash(void *handle);
qbool FSZIP_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult);
void FSZIP_ReadFile(void *handle, flocation_t *loc, char *buffer);
int FSZIP_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm);

extern searchpathfuncs_t zipfilefuncs;

// Zip (*.zip, *.pk3) - Misc
void *FSZIP_LoadZipFile(vfsfile_t *packhandle, char *desc);
int FSZIP_GeneratePureCRC(void *handle, int seed, int crctype);
#endif // WITH_ZIP

// Gzip
#ifdef WITH_VFS_GZIP
gzFile Gzip_Open(vfsfile_t *file);
int Gzip_Close(gzFile f_gz);
#endif // WITH_VFS_GZIP

// Doomwad
extern searchpathfuncs_t doomwadfilefuncs;

#endif /* __VFS_H__ */
