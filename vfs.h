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
 * $Id: vfs.h,v 1.13 2007-10-13 16:24:50 dkure Exp $
 *             
 */

#ifndef __VFS_H__
#define __VFS_H__

//=================================
// Quake filesystem
//=================================
extern hashtable_t *filesystemhash;
extern int fs_hash_dups;		
extern int fs_hash_files;		

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
	qbool   (*FindFile)(void *handle, flocation_t *loc, const char *name, void *hashedresult);	
		// true if found (hashedresult can be NULL)
		// note that if rawfile and offset are set, many Com_FileOpens will 
		// read the raw file otherwise ReadFile will be called instead.
	void	(*ReadFile)(void *handle, flocation_t *loc, char *buffer);	
		// reads the entire file
	int		(*EnumerateFiles)(void *handle, char *match, int (*func)(char *, int, void *), void *parm);

	void	*(*OpenNew)(vfsfile_t *file, const char *desc);	
		// returns a handle to a new pak/path

	int		(*GeneratePureCRC) (void *handle, int seed, int usepure);

	vfsfile_t *(*OpenVFS)(void *handle, flocation_t *loc, char *mode);
} searchpathfuncs_t;

typedef struct searchpath_s
{
#ifndef WITH_FTE_VFS 	 
	char    filename[MAX_OSPATH]; 	 
	struct pack_s *pack; // only one of filename / pack will be used 	 
#else
	searchpathfuncs_t *funcs;
	qbool copyprotected;	// don't allow downloads from here.
	qbool istemporary;
	void *handle;

	int crc_check;	// client sorts packs according to this checksum
	int crc_reply;	// client sends a different crc back to the server, 
	                // for the paks it's actually loaded.

	struct searchpath_s *nextpure;
#endif // WITH_FTE_VFS
	struct searchpath_s *next;
} searchpath_t;


#ifdef WITH_VFS_ARCHIVE_LOADING
int FS_BreakUpArchivePath(const char *filename, 
		char *archive, size_t archive_len,
		char *inside, size_t inside_len);
searchpathfuncs_t *FS_FileNameToSearchFunctions(const char *filename);
#endif

//=================================
// STDIO Files (OS)
//=================================
typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	FILE *handle;

} vfsosfile_t;

#ifndef WITH_FTE_VFS
vfsfile_t *VFSOS_Open(char *name, FILE *f, char *mode);

#else
vfsfile_t *FS_OpenTemp(void);
vfsfile_t *VFSOS_Open(char *osname, char *mode);

extern searchpathfuncs_t osfilefuncs;
#endif

//====================
// PACK (*pak) Support
//====================

// in memory
// VFS-FIXME: This is a bad name, it really is just a filename + offset....
typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

#ifndef WITH_FTE_VFS
vfsfile_t *FSPAK_OpenVFS(FILE *handle, int fsize, int fpos, char *mode);
#else
extern searchpathfuncs_t packfilefuncs;
#endif // WITH_FTE_VFS

//===========================
// ZIP (*.zip, *.pk3) Support
//===========================
#ifdef WITH_ZIP
extern searchpathfuncs_t zipfilefuncs;
#endif // WITH_ZIP

//=============================
// TCP Support - VFS Functions
//=============================
int VFSTCP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err);
int VFSTCP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite);
int VFSTCP_Seek (struct vfsfile_s *file, unsigned long pos, int whence);
unsigned long VFSTCP_Tell (struct vfsfile_s *file);
unsigned long VFSTCP_GetLen (struct vfsfile_s *file);
void VFSTCP_Close (struct vfsfile_s *file);
void VFSTCP_Tick(void);
vfsfile_t *FS_OpenTCP(char *name);

//=====================
// GZIP (*.gz) Support
//=====================
#ifdef WITH_ZLIB
searchpathfuncs_t gzipfilefuncs;
#endif // WITH_ZLIB

//=====================
// TAR (*.tar) Support
//=====================
extern searchpathfuncs_t tarfilefuncs;

//=====================
// Memory Mapped files
//=====================
vfsfile_t *FSMMAP_OpenVFS(void *buf, size_t buf_len);

//=====================
// Doomwad Support
//=====================
#ifdef DOOMWADS
extern searchpathfuncs_t doomwadfilefuncs;
#endif //DOOMWADS

#endif /* __VFS_H__ */
