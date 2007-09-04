/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: fs.c,v 1.19 2007-09-04 12:19:58 dkure Exp $
*/



#include "quakedef.h"
#include "fs.h"
#include "common.h"

//==================================================================================================
#ifdef WITH_FTE_VFS

#include "q_shared.h"
#include "hash.h"

#ifndef CLIENTONLY
#include "server.h"
#endif // CLIENTONLY

// To include pak3 support add this define
//#define WITH_PK3

static hashtable_t filesystemhash;
static qbool com_fschanged = true;

// FTE-FIXME: Give this a better name
// FTE-FIXME: This is not added anywhere???
cvar_t com_fs_cache = {"com_fs_cahce", "0"};

// FTE-FIXME: Move this somewhere better
void COM_Path_f (void);

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

typedef enum {
	FSLFRT_IFFOUND,
	FSLFRT_LENGTH,
	FSLFRT_DEPTH_OSONLY,
	FSLFRT_DEPTH_ANYPATH
} FSLF_ReturnType_e;

vfsfile_t *FS_OpenVFSLoc(flocation_t *loc, char *mode);
void FS_CreatePath(char *pname, int relativeto);
void FS_ForceToPure(char *str, char *crcs, int seed);
int FS_FLocateFile(char *filename, FSLF_ReturnType_e returntype, flocation_t *loc); 
void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm);
int COM_FileOpenRead (char *path, FILE **hndl);

typedef struct
{
	int		filepos, filelen;
	char	name[8];
} dwadfile_t;

typedef struct
{
	char	id[4];
	int		dirlen;
	int		dirofs;
} dwadheader_t;

typedef enum {
	FS_LOAD_FILE_PAK = 2,
	FS_LOAD_FILE_PK3 = 4,
	FS_LOAD_FILE_PK4 = 8,
	FS_LOAD_FILE_DOOMWAD = 16,
	FS_LOAD_FILE_ALL = FS_LOAD_FILE_PAK | FS_LOAD_FILE_PK3 
		| FS_LOAD_FILE_PK4 | FS_LOAD_FILE_DOOMWAD,
} FS_Load_File_Types;

//FTE-FIXME: This seems like com_homedir
//char	com_configdir[MAX_OSPATH];	//homedir/fte/configs

int fs_hash_dups;
int fs_hash_files;
#endif /* WITH_FTE_VFS */
//==================================================================================================

/**
  File System related code
  - old Quake FS - declarations in common.h
  - Virtual Quake System - fs.h
  - GZIP/ZIP support - fs.h
*/

#include "quakedef.h"
#include "fs.h"
#include "utils.h"
#ifdef _WIN32
#include <errno.h>
#include <Shlobj.h>
#include <Shfolder.h>
#else
#include <unistd.h>
#endif

char *com_filesearchpath;
char *com_args_original;

/*
=============================================================================
                        QUAKE FILESYSTEM
=============================================================================
*/

int		fs_filesize, fs_filepos;
char	fs_netpath[MAX_OSPATH];
#ifdef WITH_FTE_VFS
qbool com_file_copyprotected;
#endif


// in memory
typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;

#ifdef WITH_FTE_VFS
	bucket_t bucket;
#endif
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
#ifndef WITH_FTE_VFS
	FILE	*handle;
#else
	vfsfile_t	 *handle;
	unsigned int filepos;	// the pos the subfiles left it at 
							// (to optimize calls to vfs_seek)
#endif
	int		numfiles;
	packfile_t	*files;
#ifdef WITH_FTE_VFS
	int references;			// seeing as all vfiles from a pak file use the 
							// parent's vfsfile, we need to keep the parent 
							// open until all subfiles are closed.
#endif
} pack_t;


// on disk
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

#define	MAX_FILES_IN_PACK	2048

//
// WARNING: if u add some FS related global variable then made appropriate change to FS_ShutDown() too, if required.
//

char	com_gamedirfile[MAX_QPATH]; // qw tf ctf and etc. In other words single dir name without path
char	com_gamedir[MAX_OSPATH];    // c:/quake/qw
char	com_basedir[MAX_OSPATH];	// c:/quake
char	com_homedir[MAX_PATH];		// something really long C:/Documents and Settings/qqshka

#ifndef SERVERONLY
char	userdirfile[MAX_OSPATH] = {0};
char	com_userdir[MAX_OSPATH] = {0};
int		userdir_type = -1;
#endif

typedef struct searchpath_s
{
#ifndef WITH_FTE_VFS
	char	filename[MAX_OSPATH];
	pack_t	*pack; // only one of filename / pack will be used
#else

	searchpathfuncs_t *funcs;
	qbool copyprotected;	// don't allow downloads from here.
	qbool istemporary;
	void *handle;

	int crc_check;	// client sorts packs according to this checksum
	int crc_reply;	// client sends a different crc back to the server, 
	                // for the paks it's actually loaded.

	struct searchpath_s *nextpure;
#endif
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*com_searchpaths = NULL;
searchpath_t	*com_base_searchpaths = NULL;	// without gamedirs

/*
================
COM_FileLength
================
*/
int COM_FileLength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

#ifndef WITH_FTE_VFS
int COM_FileOpenRead (char *path, FILE **hndl)
{
	FILE *f;

	if (!(f = fopen(path, "rb"))) {
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_FileLength(f);
}
#endif // WITH_FTE_VFS

#ifndef WITH_FTE_VFS
void COM_Path_f (void)
{
	searchpath_t *search;

	Com_Printf ("Current search path:\n");
	for (search = com_searchpaths; search; search = search->next) {
		if (search == com_base_searchpaths)
			Com_Printf ("----------\n");
		if (search->pack)
			Com_Printf ("%s (%i files)\n", search->pack->filename, search->pack->numfiles);
		else
			Com_Printf ("%s\n", search->filename);
	}
}
#endif// WITH_FTE_VFS

// FTE-FIXME: D-Kure This removes a sanity check
int COM_FCreateFile (char *filename, FILE **file, char *path, char *mode)
{
	searchpath_t *search;
	char fullpath[MAX_OSPATH];

	if (path == NULL)
		path = com_gamedir;
#ifndef WITH_FTE_VFS
	else {
		// check if given path is in one of mounted filesystem
		// we do not allow others
		for (search = com_searchpaths ; search ; search = search->next) {
			if (search->pack != NULL)
				continue;   // no writes to pak files

			if (strlen(search->filename) > strlen(path)  &&
			        !strcmp(search->filename + strlen(search->filename) - strlen(path), path) &&
			        *(search->filename + strlen(search->filename) - strlen(path) - 1) == '/') {
				break;
			}
		}
		if (search == NULL)
			Sys_Error("COM_FCreateFile: out of Quake filesystem\n");
	}
#endif

	if (mode == NULL)
		mode = "wb";

	// try to create
	snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", com_basedir, path, filename);
	COM_CreatePath(fullpath);
	*file = fopen(fullpath, mode);

	if (*file == NULL) {
		// no Sys_Error, quake can be run from read-only media like cd-rom
		return 0;
	}

	Sys_Printf ("FCreateFile: %s\n", filename);

	return 1;
}

//The filename will be prefixed by com_basedir
#ifndef WITH_FTE_VFS
qbool COM_WriteFile (char *filename, void *data, int len)
{
	FILE *f;
	char name[MAX_OSPATH];

	snprintf (name, sizeof(name), "%s/%s", com_basedir, filename);

	if (!(f = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(f = fopen (name, "wb")))
			return false;
	}
	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);
	return true;
}
#endif // WITH_FTE_VFS

//The filename used as is
qbool COM_WriteFile_2 (char *filename, void *data, int len)
{
	FILE *f;
	char name[MAX_PATH];

	snprintf (name, sizeof(name), "%s", filename);

	if (!(f = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(f = fopen (name, "wb")))
			return false;
	}
	Sys_Printf ("COM_WriteFile_2: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);
	return true;
}


//Only used for CopyFile and download


#ifndef WITH_FTE_VFS
void COM_CreatePath(char *path)
{
	char *s, save;

	if (!*path)
		return;

	for (s = path + 1; *s; s++) {
#ifdef _WIN32
		if (*s == '/' || *s == '\\') {
#else
		if (*s == '/') {
#endif
			save = *s;
			*s = 0;
			Sys_mkdir(path);
			*s = save;
		}
	}
}
#endif /* FS_FTE */

int FS_FOpenPathFile (char *filename, FILE **file) {

	*file = NULL;
	fs_filesize = -1;
	fs_filepos = 0;
	fs_netpath[0] = 0;

	if ((*file = fopen (filename, "rb"))) {

		if (developer.value)
			Sys_Printf ("FindFile: %s\n", fs_netpath);

		fs_filesize = COM_FileLength (*file);
		return fs_filesize;
	}

	if (developer.value)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	return -1;
}

//Finds the file in the search path.
//Sets fs_filesize, fs_netpath and one of handle or file
//Sets fs_filepos to 0 for non paks, and to beging of file in pak file
qbool	file_from_pak;		// global indicating file came from a packfile
qbool	file_from_gamedir;	// global indicating file came from a gamedir (and gamedir wasn't id1/qw)

// FTE-FIXME: D-Kure: This function will be removed once we have the VFS layer
#ifndef WITH_FTE_VFS
int FS_FOpenFile (char *filename, FILE **file) {
	searchpath_t *search;
#ifndef WITH_FTE_VFS 
	pack_t *pak;
	int i;
#endif

	*file = NULL;
	file_from_pak = false;
	file_from_gamedir = true;
	fs_filesize = -1;
	fs_filepos = 0;
	fs_netpath[0] = 0;

	// search through the path, one element at a time
	for (search = com_searchpaths; search; search = search->next) {
		if (search == com_base_searchpaths && com_searchpaths != com_base_searchpaths)
			file_from_gamedir = false;

		// is the element a pak file?
#ifndef WITH_FTE_VFS
		if (search->pack) {
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++) {
				if (!strcmp (pak->files[i].name, filename)) {	// found it!
					if (developer.integer >= 3)
						Com_DPrintf ("PackFile: %s : %s\n", pak->filename, filename);
					// open a new file on the pakfile
					if (!(*file = fopen (pak->filename, "rb")))
						Sys_Error ("Couldn't reopen %s\n", pak->filename);
					fseek (*file, pak->files[i].filepos, SEEK_SET);
					fs_filepos = pak->files[i].filepos;
					fs_filesize = pak->files[i].filelen;
					com_filesearchpath = search->filename;

					file_from_pak = true;
					snprintf (fs_netpath, sizeof(fs_netpath), "%s#%i", pak->filename, i);
					return fs_filesize;
				}
			}
		} else {
			snprintf (fs_netpath, sizeof(fs_netpath), "%s/%s", search->filename, filename);

			if (!(*file = fopen (fs_netpath, "rb")))
				continue;

			if (developer.value)
				Sys_Printf ("FindFile: %s\n", fs_netpath);

			fs_filepos = 0;
			fs_filesize = COM_FileLength (*file);
			return fs_filesize;
		}
#endif // WITH_FTE_VFS
	}

	if (developer.value)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	return -1;
}
#endif

// Filename are relative to the quake directory.
// Always appends a 0 byte to the loaded data.
static cache_user_t *loadcache;
static byte			*loadbuf;
static int			loadsize;
byte *FS_LoadFile (char *path, int usehunk)
{
#ifndef WITH_FTE_VFS
	FILE *h;
#else
	vfsfile_t *f;
	vfserrno_t err;
	flocation_t loc;
#endif
	byte *buf;
	char base[32];
	int len;

	// Look for it in the filesystem or pack files.
#ifndef WITH_FTE_VFS
	len = fs_filesize = FS_FOpenFile (path, &h);
	if (!h)
		return NULL;
#else
    FS_FLocateFile(path, FSLFRT_LENGTH, &loc);
	if (!loc.search)
		        return NULL;    //wasn't found

	f = loc.search->funcs->OpenVFS(loc.search->handle, &loc, "rb");
	if (!f)
		return NULL;
	fs_filesize = len = VFS_GETLEN(f);
#endif

	// Extract the filename base name for hunk tag.
	COM_FileBase (path, base);

	// TODO: Make these into defines.
	if (usehunk == 1)
	{
		buf = (byte *) Hunk_AllocName (len + 1, base);
	}
	else if (usehunk == 2)
	{
		buf = (byte *) Hunk_TempAlloc (len + 1);
	}
	else if (usehunk == 3)
	{
		buf = (byte *) Cache_Alloc (loadcache, len + 1, base);
	}
	else if (usehunk == 4)
	{
		buf = ((len+1 > loadsize) ? (byte *) Hunk_TempAlloc (len + 1) : loadbuf);
	}
	else if (usehunk == 5)
	{
		buf = Q_malloc (len + 1);
	}
	else
	{
		Sys_Error ("FS_LoadFile: bad usehunk\n");
		return NULL;
	}

	if (!buf) {
		Sys_Error ("FS_LoadFile: not enough space for %s\n", path);
		return NULL;
	}

	((byte *)buf)[len] = 0;

	#ifndef SERVERONLY
	Draw_BeginDisc ();
	#endif

#ifndef WITH_FTE_VFS
	fread (buf, 1, len, h);
	fclose (h);
#else
	VFS_READ(f, buf, len, &err);
	VFS_CLOSE(f);
#endif

	#ifndef SERVERONLY
	Draw_EndDisc ();
	#endif

	return buf;
}

byte *FS_LoadHunkFile (char *path) {
	return FS_LoadFile (path, 1);
}

byte *FS_LoadTempFile (char *path) {
	return FS_LoadFile (path, 2);
}

void FS_LoadCacheFile (char *path, struct cache_user_s *cu) {
	loadcache = cu;
	FS_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
byte *FS_LoadStackFile (char *path, void *buffer, int bufsize) {
	byte *buf;

	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = FS_LoadFile (path, 4);

	return buf;
}

// use Q_malloc, do not forget Q_free when no needed more
byte *FS_LoadHeapFile (char *path)
{
	return FS_LoadFile (path, 5);
}


/*
Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
*/
pack_t *FS_LoadPackFile (char *packfile) {
	dpackheader_t header;
	int i;
	packfile_t *newfiles;
	pack_t *pack;
#ifndef WITH_FTE_VFS
	FILE *packhandle;
#else
	vfsfile_t *packhandle;
	vfserrno_t err;
#endif
	dpackfile_t *info;

#ifndef WITH_FTE_VFS
	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;
#else
	if (!(packhandle = FS_OpenVFS(packfile, "rb", FS_ANY))) 
		return NULL;
#endif

#ifndef WITH_FTE_VFS
	fread (&header, 1, sizeof(header), packhandle);
#else
	VFS_READ(packhandle, &header, sizeof(header), &err);
#endif
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error ("%s is not a packfile\n", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	pack = (pack_t *) Q_malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = header.dirlen / sizeof(dpackfile_t);

	pack->files = newfiles = (packfile_t *) Q_malloc (pack->numfiles * sizeof(packfile_t));
	info = (dpackfile_t *) Q_malloc (header.dirlen);

#ifndef WITH_FTE_VFS
	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (info, 1, header.dirlen, packhandle);
#else
	VFS_SEEK(packhandle, header.dirofs);
	VFS_READ(packhandle, info, header.dirlen, &err);
#endif

	// parse the directory
	for (i = 0; i < pack->numfiles; i++) {
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	Q_free(info);
	return pack;
}

// QW262 -->
#ifndef SERVERONLY
/*
================
COM_SetUserDirectory
================
*/
void COM_SetUserDirectory (char *dir, char *type) {
	char tmp[sizeof(com_gamedirfile)];

	if (strstr(dir, "..") || strstr(dir, "/")
	        || strstr(dir, "\\") || strstr(dir, ":") ) {
		Com_Printf ("Userdir should be a single filename, not a path\n");
		return;
	}
	strlcpy(userdirfile, dir, sizeof(userdirfile));
	userdir_type = Q_atoi(type);

	strlcpy(tmp, com_gamedirfile, sizeof(tmp)); // save
	com_gamedirfile[0]='\0'; // force reread
	FS_SetGamedir(tmp); // restore
}
#endif

qbool FS_AddPak (char *pakfile) {
	searchpath_t *search;
	pack_t *pak;

	pak = FS_LoadPackFile (pakfile);
	if (!pak)
		return false;

	//search = Hunk_Alloc (sizeof(searchpath_t));
	search = (searchpath_t *) Q_malloc(sizeof(searchpath_t));
#ifndef WITH_FTE_VFS // FTE-FIXME: D-Kure: This should do something.....
	search->pack = pak;
#endif
	search->next = com_searchpaths;
	com_searchpaths = search;
	return true;
}

qbool FS_RemovePak (const char *pakfile) {
	searchpath_t *prev = NULL;
	searchpath_t *cur = com_searchpaths;
#ifndef WITH_FTE_VFS // unused with the VFS stuff
	searchpath_t *temp;
#endif
	qbool ret = false;

	while (cur) {
#ifndef WITH_FTE_VFS
		if (cur->pack) {
			if (!strcmp(cur->pack->filename, pakfile)) {
				if (!fclose(cur->pack->handle)) {
					if (prev)
						prev->next = cur->next;
					else
						com_searchpaths = cur->next;

					temp = cur;
					cur = cur->next;
					Q_free(temp);
					ret = true;
				} else Com_Printf("Couldn't close file handler to %s\n", cur->pack->filename);
			}
		}
#endif // WITH_FTE_VFS
		prev = cur;
		cur = cur->next;
	}

	return ret;
}

#ifndef SERVERONLY

void FS_AddUserPaks (char *dir) {
	FILE	*f;
	char	pakfile[MAX_OSPATH];
	char	userpak[32];

	// add paks listed in paks.lst
	sprintf (pakfile, "%s/pak.lst", dir);
	f = fopen(pakfile, "r");
	if (f) {
		int len;
		while (1) {
			if (!fgets(userpak, 32, f))
				break;
			len = strlen(userpak);
			// strip endline
			if (userpak[len-1] == '\n') {
				userpak[len-1] = '\0';
				--len;
			}
			if (userpak[len-1] == '\r') {
				userpak[len-1] = '\0';
				--len;
			}
			if (len < 5)
				continue;
#ifdef GLQUAKE
			if (!strncasecmp(userpak,"soft",4))
				continue;
#else
			if (!strncasecmp(userpak,"gl", 2))
				continue;
#endif
			sprintf (pakfile, "%s/%s", dir, userpak);
			FS_AddPak(pakfile);
		}
		fclose(f);
	}
	// add userdir.pak
	if (UserdirSet) {
		sprintf (pakfile, "%s/%s.pak", dir, userdirfile);
		FS_AddPak(pakfile);
	}
}
#endif

// <-- QW262

//Sets com_gamedir, adds the directory to the head of the path, then loads and adds pak1.pak pak2.pak ...
#ifndef WITH_FTE_VFS
void FS_AddGameDirectory (char *path_to_dir, char *dir) {
	int i;
	searchpath_t *search;
	char pakfile[MAX_OSPATH];

	strlcpy(com_gamedirfile, dir, sizeof(com_gamedirfile));
	snprintf(com_gamedir, sizeof(com_gamedir), "%s/%s", path_to_dir, dir);

	// add the directory to the search path
	search = (searchpath_t *) Q_malloc (sizeof(searchpath_t));
	strcpy (search->filename, com_gamedir);
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0; ; i++) {
		snprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", com_gamedir, i);
		if(!FS_AddPak(pakfile))
			break;
	}
#ifndef SERVERONLY
	// other paks
	FS_AddUserPaks (com_gamedir);
#endif
}
#endif // WITH_FTE_VFS

#ifndef SERVERONLY
void FS_AddUserDirectory ( char *dir ) {
	int i;
	searchpath_t *search;
	char pakfile[MAX_OSPATH];

	if ( !UserdirSet )
		return;
	switch (userdir_type) {
			case 0:	snprintf (com_userdir, sizeof(com_userdir), "%s/%s", com_gamedir, userdirfile); break;
			case 1:	snprintf (com_userdir, sizeof(com_userdir), "%s/%s/%s", com_basedir, userdirfile, dir); break;
			case 2: snprintf (com_userdir, sizeof(com_userdir), "%s/qw/%s/%s", com_basedir, userdirfile, dir); break;
			case 3: snprintf (com_userdir, sizeof(com_userdir), "%s/qw/%s", com_basedir, userdirfile); break;
			case 4: snprintf (com_userdir, sizeof(com_userdir), "%s/%s", com_basedir, userdirfile); break;
			case 5: {
				char* homedir = getenv("HOME");
				if (homedir)
					snprintf (com_userdir, sizeof(com_userdir), "%s/qw/%s", homedir, userdirfile);
				break;
			}
			default:
			return;
	}

	// add the directory to the search path
	search = (searchpath_t *) Q_malloc (sizeof(searchpath_t));
#ifndef WITH_FTE_VFS
	// FTE-FIXME: D-Kure: What is this search->filename & pack used for??
	strcpy (search->filename, com_userdir);
	search->pack = NULL;
#endif
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0; ; i++) {
		snprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", com_userdir, i);
		if(!FS_AddPak(pakfile))
			break;
	}

	// other paks
	FS_AddUserPaks (com_userdir);
}
#endif /* SERVERONLY */

void Draw_InitConback(void);

// Sets the gamedir and path to a different directory.
void FS_SetGamedir (char *dir)
{
	searchpath_t  *next;
	if (strstr(dir, "..") || strstr(dir, "/")
	 || strstr(dir, "\\") || strstr(dir, ":") ) 
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp(com_gamedirfile, dir))
		return;		// Still the same.
	
	strlcpy (com_gamedirfile, dir, sizeof(com_gamedirfile));

	// Free up any current game dir info.
#ifndef WITH_FTE_VFS
	while (com_searchpaths != com_base_searchpaths)	
	{
		if (com_searchpaths->pack) 
		{
			fclose (com_searchpaths->pack->handle);
			Q_free (com_searchpaths->pack->files);
			Q_free (com_searchpaths->pack);
		}

		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;
	}
#else
	// free up any current game dir info
	while (com_searchpaths != com_base_searchpaths)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;
	}

#endif

	// Flush all data, so it will be forced to reload.
	Cache_Flush ();

	sprintf (com_gamedir, "%s/%s", com_basedir, dir);

	if (strcmp(dir, "id1") && strcmp(dir, "qw") && strcmp(dir, "ezquake"))
	{
#ifndef WITH_FTE_VFS
		FS_AddGameDirectory(com_basedir, dir);
#else
		FS_AddGameDirectory(va("%s%s", com_basedir, dir), FS_LOAD_FILE_ALL);
		if (*com_homedir)
			FS_AddGameDirectory(va("%s%s", com_homedir, dir), FS_LOAD_FILE_ALL);
#endif
	}

#ifdef GLQUAKE
	// Reload gamedir specific conback as its not flushed
	Draw_InitConback();
#endif // GLQUAKE

#ifndef SERVERONLY
	FS_AddUserDirectory(dir);
#endif
}

void FS_ShutDown( void ) {

	// free data
	while (com_searchpaths)	{
		searchpath_t  *next;

#ifndef WITH_FTE_VFS
		// FTE-FIXME: D-Kure: Need to add some VFS Cleanup here
		if (com_searchpaths->pack) {
			fclose (com_searchpaths->pack->handle); // close pack file handler
			Q_free (com_searchpaths->pack->files);
			Q_free (com_searchpaths->pack);
		}
#endif
		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;
	}

	// flush all data, so it will be forced to reload
	Cache_Flush ();

	// reset globals

	com_base_searchpaths = com_searchpaths = NULL;

	com_gamedirfile[0]	= 0;
	com_gamedir[0]		= 0;
	com_basedir[0]		= 0;
	com_homedir[0]		= 0;

#ifndef SERVERONLY
	userdirfile[0]		= 0;
	com_userdir[0]		= 0;
	userdir_type		= -1;
#endif
}

void FS_InitFilesystemEx( qbool guess_cwd ) {
	int i;
#ifndef _WIN32
	char *ev;
#endif

	FS_ShutDown();

	if (guess_cwd) { // so, com_basedir directory will be where ezquake*.exe located
		char *e;

#if defined(_WIN32)
		if(!(i = GetModuleFileName(NULL, com_basedir, sizeof(com_basedir)-1)))
			Sys_Error("FS_InitFilesystemEx: GetModuleFileName failed");
		com_basedir[i] = 0; // ensure null terminator
#elif defined(__linux__)
		if (!Sys_fullpath(com_basedir, "/proc/self/exe", sizeof(com_basedir)))
			Sys_Error("FS_InitFilesystemEx: Sys_fullpath failed");
#else
		com_basedir[0] = 0; // FIXME: MAC / FreeBSD
#endif

		// strip ezquake*.exe, we need only path
		for (e = com_basedir+strlen(com_basedir)-1; e >= com_basedir; e--)
			if (*e == '/' || *e == '\\')
			{
				*e = 0;
				break;
			}
	}
	else if ((i = COM_CheckParm ("-basedir")) && i < com_argc - 1) {
		// -basedir <path>
		// Overrides the system supplied base directory (under id1)
		strlcpy (com_basedir, com_argv[i + 1], sizeof(com_basedir));
	}
 	else { // made com_basedir equa to cwd
//#ifdef __FreeBSD__
//		strlcpy(com_basedir, DATADIR, sizeof(com_basedir) - 1);
//#else

		Sys_getcwd(com_basedir, sizeof(com_basedir) - 1); // FIXME strlcpy (com_basedir, ".", sizeof(com_basedir)); ?
//#endif
	}

	for (i = 0; i < (int) strlen(com_basedir); i++)
		if (com_basedir[i] == '\\')
			com_basedir[i] = '/';

	i = strlen(com_basedir) - 1;
	if (i >= 0 && com_basedir[i] == '/')
		com_basedir[i] = 0;

#ifdef _WIN32
    // gets "C:\documents and settings\johnny\my documents" path
    if (!SHGetSpecialFolderPath(0, com_homedir, CSIDL_PERSONAL, 0))
    {
        *com_homedir = 0;
    }
#else
	ev = getenv("HOME");
	if (ev)
		strlcpy(com_homedir, ev, sizeof(com_homedir));
	else
		com_homedir[0] = 0;
#endif

	if (COM_CheckParm("-nohome"))
		com_homedir[0] = 0;

	if (com_homedir[0])
	{
#ifdef _WIN32
		strlcat(com_homedir, "/ezQuake", sizeof(com_homedir));
#else
		strlcat(com_homedir, "/.ezquake", sizeof(com_homedir));
#endif
		Com_Printf("Using home directory \"%s\"\n", com_homedir);
	}
	else
	{
#ifndef WITH_FTE_VFS
		// if homedir not used set it equal to basedir
		strlcpy(com_homedir, com_basedir, sizeof(com_homedir));
#endif // WITH_FTE_VFS
	}

	// start up with id1 by default
#ifndef WITH_FTE_VFS
	FS_AddGameDirectory(com_basedir, "id1");
	FS_AddGameDirectory(com_basedir, "ezquake");
	FS_AddGameDirectory(com_basedir, "qw");
#else
	FS_AddGameDirectory(va("%s/%s", com_basedir, "id1"), (unsigned int)-1);
	FS_AddGameDirectory(va("%s/%s", com_basedir, "ezquake"), (unsigned int)-1);
	FS_AddGameDirectory(va("%s/%s", com_basedir, "qw"), (unsigned int)-1);
	if (*com_homedir)
	        FS_AddGameDirectory(com_homedir, (unsigned int)-1);
#endif

	//
	// -data <datadir>
	// Adds datadirs similar to "-game"
	//
    //Tei: original code from qbism.
	i = 1;
	while((i = COM_CheckParmOffset ("-data", i)))
	{
		if (i && i < com_argc-1)
		{
#ifndef WITH_FTE_VFS
			FS_AddGameDirectory(com_basedir,com_argv[i+1]);
#else
			FS_AddGameDirectory(va("%s%s", com_basedir, com_argv[i+1]), (unsigned int)-1);
#endif
		}
		i++;
	}


	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

#ifndef SERVERONLY
	if ((i = COM_CheckParm("-userdir")) && i < com_argc - 2)
		COM_SetUserDirectory(com_argv[i+1], com_argv[i+2]);
#endif

	// the user might want to override default game directory
	if (!(i = COM_CheckParm ("-game")))
		i = COM_CheckParm ("+gamedir");
	if (i && i < com_argc - 1)
		FS_SetGamedir (com_argv[i + 1]);
}

void FS_InitFilesystem( void ) {
#ifndef WITH_FTE_VFS
	FILE *f;
#else
	vfsfile_t *vfs;
#endif

	FS_InitModuleFS();

	FS_InitFilesystemEx( false ); // first attempt, simplified

#ifndef WITH_FTE_VFS
	if ( FS_FOpenFile( "gfx.wad", &f ) >= 0 ) { // we found gfx.wad, seems we have proper com_basedir
		fclose( f );
		return;
	}
#else
	vfs = FS_OpenVFS("gfx.wad", "rb", FS_ANY); 
	if (vfs) { // // we found gfx.wad, seems we have proper com_basedir
		VFS_CLOSE(vfs);
		return;
	}
#endif // WITH_FTE_VFS

	FS_InitFilesystemEx( true );  // second attempt
}

// allow user select differet "style" how/where open/save different media files.
// so user select media_dir is relative to quake base dir or some system HOME dir or may be full path
// NOTE: using static buffer, use with care
char *COM_LegacyDir(char *media_dir)
{
	static char dir[MAX_PATH];

	// dir empty, return gamedir
	if (!media_dir || !media_dir[0]) {
		strlcpy(dir, cls.gamedir, sizeof(dir));
		return dir;
	}

	switch (cl_mediaroot.integer) {
		case 1:  //			/home/qqshka/ezquake/<demo_dir>
			while(media_dir[0] == '/' || media_dir[0] == '\\')
				media_dir++; // skip precending / probably smart

			snprintf(dir, sizeof(dir), "%s/%s", com_homedir, media_dir);
			return dir;
		case 2:  //			/fullpath
		  snprintf(dir, sizeof(dir), "%s", media_dir);
			return dir;

		default: //     /basedir/<demo_dir>
			while(media_dir[0] == '/' || media_dir[0] == '\\')
				media_dir++; // skip precending / probably smart

		  snprintf(dir, sizeof(dir), "%s/%s", com_basedir, media_dir);
			return dir;
	}
}


//======================================================================================================
// VFS

void VFS_CHECKCALL (struct vfsfile_s *vf, void *fld, char *emsg) {
	if (!fld)
		Sys_Error("%s", emsg);
}

void VFS_CLOSE (struct vfsfile_s *vf) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->Close, "VFS_CLOSE");
	vf->Close(vf);
}

unsigned long VFS_TELL (struct vfsfile_s *vf) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->Tell, "VFS_TELL");
	return vf->Tell(vf);
}

unsigned long VFS_GETLEN (struct vfsfile_s *vf) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->GetLen, "VFS_GETLEN");
	return vf->GetLen(vf);
}

qbool VFS_SEEK (struct vfsfile_s *vf, unsigned long pos) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->Seek, "VFS_SEEK");
	return vf->Seek(vf, pos);
}

int VFS_READ (struct vfsfile_s *vf, void *buffer, int bytestoread, vfserrno_t *err) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->ReadBytes, "VFS_READ");
	return vf->ReadBytes(vf, buffer, bytestoread, err);
}

int VFS_WRITE (struct vfsfile_s *vf, void *buffer, int bytestowrite) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->WriteBytes, "VFS_WRITE");
	return vf->WriteBytes(vf, buffer, bytestowrite);
}

void VFS_FLUSH (struct vfsfile_s *vf) {
	assert(vf);
	if(vf->Flush)
		vf->Flush(vf);
}

// return null terminated string
char *VFS_GETS(struct vfsfile_s *vf, char *buffer, int buflen)
{
	char in;
	char *out = buffer;
	int len = buflen-1;

	assert(vf);
	VFS_CHECKCALL(vf, vf->ReadBytes, "VFS_GETS");

//	if (len == 0)
//		return NULL;

// FIXME: I am not sure how to handle this better
	if (len <= 0)
		Sys_Error("VFS_GETS: len <= 0");

	while (len > 0)
	{
		if (!VFS_READ(vf, &in, 1, NULL))
		{
			if (len == buflen-1)
				return NULL;
			*out = '\0';
			return buffer;
		}
		if (in == '\n')
			break;
		*out++ = in;
		len--;
	}
	*out = '\0';

	return buffer;
}

//******************************************************************************************************
//STDIO files (OS)

typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	FILE *handle;

} vfsosfile_t;

int VFSOS_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err)
{
	int r;
	vfsosfile_t *intfile = (vfsosfile_t*)file;

	if (bytestoread < 0)
		Sys_Error("VFSOS_ReadBytes: bytestoread < 0"); // ffs

	r = fread(buffer, 1, bytestoread, intfile->handle);

	if (err) // if bytestoread <= 0 it will be treated as non error even we read zero bytes
		*err = ((r || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);

	return r;
}

int VFSOS_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestowrite)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fwrite(buffer, 1, bytestowrite, intfile->handle);
}

qbool VFSOS_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fseek(intfile->handle, pos, SEEK_SET) == 0;
}

unsigned long VFSOS_Tell (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return ftell(intfile->handle);
}

unsigned long VFSOS_GetSize (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;

	unsigned long curpos, maxlen;

	// FIXME: add error checks here?

	curpos = ftell(intfile->handle);
	fseek(intfile->handle, 0, SEEK_END);
	maxlen = ftell(intfile->handle);
	fseek(intfile->handle, curpos, SEEK_SET);

	return maxlen;
}

void VFSOS_Close(vfsfile_t *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	fclose(intfile->handle);
	Q_free(file);
}

vfsfile_t *FS_OpenTemp(void)
{
	FILE *f;
	vfsosfile_t *file;

	f = tmpfile();
	if (!f)
		return NULL;

	file = Q_malloc(sizeof(vfsosfile_t));

	file->funcs.ReadBytes	= VFSOS_ReadBytes;
	file->funcs.WriteBytes	= VFSOS_WriteBytes;
	file->funcs.Seek		= VFSOS_Seek;
	file->funcs.Tell		= VFSOS_Tell;
	file->funcs.GetLen		= VFSOS_GetSize;
	file->funcs.Close		= VFSOS_Close;

	file->handle = f;

	return (vfsfile_t*)file;
}

// if f == NULL then use fopen(name, ...);
#ifndef WITH_FTE_VFS
// Slightly different version below were we don't take in a FILE *
vfsfile_t *VFSOS_Open(char *name, FILE *f, char *mode)
{
	vfsosfile_t *file;

	if (!strchr(mode, 'r') && !strchr(mode, 'w'))
		return NULL; // hm, no read and no write mode?

	if (!f) {
		qbool read  = !!strchr(mode, 'r');
		qbool write = !!strchr(mode, 'w');
		qbool text  = !!strchr(mode, 't');
		char newmode[10];
		int modec = 0;

		if (read)
			newmode[modec++] = 'r';
		if (write)
			newmode[modec++] = 'w';
		if (text)
			newmode[modec++] = 't';
		else
			newmode[modec++] = 'b';
		newmode[modec++] = '\0';
    
		f = fopen(name, newmode);
	}

	if (!f)
		return NULL;

	file = Q_malloc(sizeof(vfsosfile_t));

	file->funcs.ReadBytes	= (strchr(mode, 'r') ? VFSOS_ReadBytes  : NULL);
	file->funcs.WriteBytes	= (strchr(mode, 'w') ? VFSOS_WriteBytes : NULL);
	file->funcs.Seek		= VFSOS_Seek;
	file->funcs.Tell		= VFSOS_Tell;
	file->funcs.GetLen		= VFSOS_GetSize;
	file->funcs.Close		= VFSOS_Close;

	file->handle = f;

	return (vfsfile_t*)file;
}
#endif // WTIH_FTE_VFS

//STDIO files (OS)
//******************************************************************************************************
// PAK files

typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

#ifdef WITH_FTE_VFS
	pack_t *parentpak;
#else
	FILE *handle;
#endif

	unsigned long startpos;
	unsigned long length;
	unsigned long currentpos;
} vfspack_t;

#ifndef WITH_FTE_VFS
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread, vfserrno_t *err)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	unsigned long have = vfsp->length - (vfsp->currentpos - vfsp->startpos);
	unsigned long r;

	if (bytestoread < 0)
		Sys_Error("VFSPAK_ReadBytes: bytestoread < 0"); // ffs

	have = min((unsigned long)bytestoread, have); // mixing sign and unsign types is STUPID and dangerous

// all must work without this, if not then somewhere bug
//	if (ftell(vfsp->handle) != vfsp->currentpos)
//		fseek(vfsp->handle, vfsp->currentpos);

	r = fread(buffer, 1, have, vfsp->handle);
	vfsp->currentpos += r;

	if (err) // if bytestoread <= 0 it will be treated as non error even we read zero bytes
		*err = ((r || bytestoread <= 0) ? VFSERR_NONE : VFSERR_EOF);

	return r;
}
#endif

int VFSPAK_WriteBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("Cannot write to pak files");
	return 0;
}

#ifndef WITH_FTE_VFS
qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	if (pos < 0 || pos > vfsp->length)
		return false;

	vfsp->currentpos = pos + vfsp->startpos;
	return fseek(vfsp->handle, vfsp->currentpos, SEEK_SET) == 0;
}
#endif /* FS_FTE */

unsigned long VFSPAK_Tell (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->currentpos - vfsp->startpos;
}

unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->length;
}

#ifndef WITH_FTE_VFS
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	fclose(vfsp->handle);
	Q_free(vfsp);	//free ourselves.
}
#endif /* WITH_FTE_VFS */

#ifndef WITH_FTE_VFS
vfsfile_t *FSPAK_OpenVFS(FILE *handle, int fsize, int fpos, char *mode)
{
	vfspack_t *vfs;

	if (strcmp(mode, "rb") || !handle || fsize < 0 || fpos < 0)
		return NULL; // support only "rb" mode

	vfs = Q_malloc(sizeof(vfspack_t));

	vfs->handle = handle;

	vfs->startpos   = fpos;
	vfs->length     = fsize;
	vfs->currentpos = vfs->startpos;

	vfs->funcs.Close	  = VFSPAK_Close;
	vfs->funcs.GetLen	  = VFSPAK_GetLen;
	vfs->funcs.ReadBytes  = VFSPAK_ReadBytes;
	vfs->funcs.Seek		  = VFSPAK_Seek;
	vfs->funcs.Tell		  = VFSPAK_Tell;
	vfs->funcs.WriteBytes = VFSPAK_WriteBytes;	//not supported

	return (vfsfile_t *)vfs;
}
#endif /* FS_FTE */

// PAK files
//******************************************************************************************************

//
// some general function to open VFS file, except VFSTCP
//

#ifndef WITH_FTE_VFS
vfsfile_t *FS_OpenVFS(char *filename, char *mode, relativeto_t relativeto)
{
	vfsfile_t *vf;
	FILE *file;
	char fullname[MAX_PATH];

	switch (relativeto)
	{
	case FS_NONE_OS:	//OS access only, no paks, open file as is
		snprintf(fullname, sizeof(fullname), "%s", filename);

		return VFSOS_Open(fullname, NULL, mode);

	case FS_GAME_OS:	//OS access only, no paks, open file in gamedir
		snprintf(fullname, sizeof(fullname), "%s/%s", com_gamedir, filename);

		return VFSOS_Open(fullname, NULL, mode);

	case FS_ANY: // any source on quake fs: paks, gamedir etc..
		if (strcmp(mode, "rb"))
			return NULL; // "rb" mode required

		snprintf(fullname, sizeof(fullname), "%s", filename);

		FS_FOpenFile(filename, &file); // search file in paks gamedir etc..

		if (file) { // we open stdio FILE, probably that point in pak file as well

			if (file_from_pak) // yea, that a pak
				vf = FSPAK_OpenVFS(file, fs_filesize, fs_filepos, mode);
			else // no, just ordinar file
				vf = VFSOS_Open(fullname, file, mode);

			if (!vf) // hm, we in troubles, do not forget close stdio FILE
				fclose(file);

			return vf;
		}

		return NULL;

	default:
		Sys_Error("FS_OpenVFS: Bad relative path (%i)", relativeto);
		break;
	}

	return NULL;
}
#endif /* FS_FTE */


//******************************************************************************************************
//TCP file

typedef struct tcpfile_s {
	vfsfile_t funcs; // <= must be at top/begining of struct

	int		sock;

	char	readbuffer [5 * 65536];
	int		readbuffered;

	char	writebuffer[5 * 65536];
	int		writebuffered;

	struct tcpfile_s *next;

} tcpfile_t;


tcpfile_t *vfs_tcp_list = NULL;


void VFSTCP_Error(tcpfile_t *f)
{
	if (f->sock != INVALID_SOCKET)
	{
		closesocket(f->sock);
		f->sock = INVALID_SOCKET;
	}
}

int VFSTCP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err)
{
	tcpfile_t *tf = (tcpfile_t*)file;

	if (bytestoread < 0)
		Sys_Error("VFSTCP_ReadBytes: bytestoread < 0"); // ffs

	if (tf->sock != INVALID_SOCKET)
	{
		int len;

		if (sizeof(tf->readbuffer) == tf->readbuffered)
			len = -2; // full buffer
		else
			len = recv(tf->sock, tf->readbuffer + tf->readbuffered, sizeof(tf->readbuffer) - tf->readbuffered, 0);

		if (len == -2) {
			; // full buffer
		}
		else if (len == -1)
		{
			int e = qerrno;
	
			if (e == EWOULDBLOCK) {
				; // no data yet, nothing to do
			} else {

				if (e == ECONNABORTED || e == ECONNRESET)
					Com_Printf ("VFSTCP: Connection lost or aborted\n"); //server died/connection lost.
				else
					Com_Printf ("VFSTCP: Error (%i): %s\n", e, strerror(e));

				VFSTCP_Error(tf);
			}
		}
		else if (len == 0) {
			Com_Printf ("VFSTCP: EOF on socket\n");
			VFSTCP_Error(tf);
		}
		else
			tf->readbuffered += len;
	}

	if (bytestoread <= tf->readbuffered)
	{ // we have all required bytes
		if (bytestoread > 0) {
			memcpy(buffer, tf->readbuffer, bytestoread);
			tf->readbuffered -= bytestoread;
			memmove(tf->readbuffer, tf->readbuffer+bytestoread, tf->readbuffered);
		}

		if (err)
			*err = VFSERR_NONE; // we got required bytes somehow, so no error

		return bytestoread;
	}
	else
	{ // lack of data, but probably have at least something in buffer
		if (tf->readbuffered > 0) {
			bytestoread = tf->readbuffered;
			memcpy(buffer, tf->readbuffer, tf->readbuffered);
			tf->readbuffered = 0;
		}
		else
			bytestoread = 0; // this help guess EOF

		if (err) // if socket is not closed we still have chance to get data, so no error
			*err = ((bytestoread || tf->sock != INVALID_SOCKET) ? VFSERR_NONE : VFSERR_EOF);

		return bytestoread;
	}
}

int VFSTCP_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestowrite)
{
	tcpfile_t *tf = (tcpfile_t*)file;
	int len;

	if (bytestowrite < 0)
		Sys_Error("VFSTCP_WriteBytes: bytestowrite < 0"); // ffs

	if (tf->sock == INVALID_SOCKET)
		return 0;

	if (bytestowrite > (int)sizeof(tf->writebuffer) - tf->writebuffered)
	{
		Com_Printf("VFSTCP: failed to write %d bytes, buffer overflow, closing link\n", bytestowrite);
		VFSTCP_Error(tf);
		return 0;
	}

	// append data
	memmove(tf->writebuffer + tf->writebuffered, buffer, bytestowrite);
	tf->writebuffered += bytestowrite;

	len = send(tf->sock, tf->writebuffer, tf->writebuffered, 0);

	if (len < 0)
	{
		int err = qerrno;

		if (err != EWOULDBLOCK/* FIXME: this is require winsock2.h on windows && err != EAGAIN && err != ENOTCONN */)
		{
			Com_Printf("VFSTCP: failed to write %d bytes, closing link, socket error %d\n", bytestowrite, err);
			VFSTCP_Error(tf);
			return 0;
		}
	}
	else if (len > 0)
	{
		tf->writebuffered -= len;
		memmove(tf->writebuffer, tf->writebuffer + len, tf->writebuffered);
	}
	else
	{
		// hm, zero bytes was sent
	}

	return bytestowrite; // well at least we put something in buffer, sure if bytestowrite not zero
}

qbool VFSTCP_Seek (struct vfsfile_s *file, unsigned long pos)
{
	Com_Printf("VFSTCP: seek is illegal, closing link\n");
	VFSTCP_Error((tcpfile_t*)file);
	return false;
}

unsigned long VFSTCP_Tell (struct vfsfile_s *file)
{
	Com_Printf("VFSTCP: tell is illegal, closing link\n");
	VFSTCP_Error((tcpfile_t*)file);
	return 0;
}

unsigned long VFSTCP_GetLen (struct vfsfile_s *file)
{
	Com_Printf("VFSTCP: GetLen non functional\n");
	return 0;
}

void VFSTCP_Close (struct vfsfile_s *file)
{
	tcpfile_t *tmp, *prev;

	VFSTCP_Error((tcpfile_t*)file); // close socket

	// link out
	for (tmp = prev = vfs_tcp_list; tmp; tmp = tmp->next)
	{
		if (tmp == (tcpfile_t*)file)
		{
			if (tmp == vfs_tcp_list)
				vfs_tcp_list = tmp->next; // we remove from head a bit different
			else
				prev->next   = tmp->next; // removing not from head

			break;
		}

		prev = tmp;
	}

	if (!tmp)
		Com_Printf("VFSTCP: Close: not found in list\n");

	Q_free(file);
}

void VFSTCP_Tick(void)
{
	tcpfile_t *tmp;
	vfserrno_t err;

	for (tmp = vfs_tcp_list; tmp; tmp = tmp->next)
	{
		VFSTCP_ReadBytes  ((vfsfile_t*)tmp, NULL, 0, &err); // perform read in our internal buffer
		VFSTCP_WriteBytes ((vfsfile_t*)tmp, NULL, 0);		// perform write from our internall buffer

//		Com_Printf("r %d, w %d\n", tmp->readbuffered, tmp->writebuffered);
	}
}

vfsfile_t *FS_OpenTCP(char *name)
{
	tcpfile_t *newf;
	int sock;
//	int _true = true;
	netadr_t adr = {0};

	if (NET_StringToAdr(name, &adr))
	{
		sock = TCP_OpenStream(adr);
		if (sock == INVALID_SOCKET)
			return NULL;

//		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true)) == -1)
//			Com_Printf ("FS_OpenTCP: setsockopt: (%i): %s\n", qerrno, strerror(qerrno));

		newf = Q_malloc(sizeof(tcpfile_t));
		memset(newf, 0, sizeof(tcpfile_t));
		newf->sock				= sock;
		newf->funcs.Close		= VFSTCP_Close;
		newf->funcs.Flush		= NULL;
		newf->funcs.GetLen		= VFSTCP_GetLen;
		newf->funcs.ReadBytes	= VFSTCP_ReadBytes;
		newf->funcs.Seek		= VFSTCP_Seek;
		newf->funcs.Tell		= VFSTCP_Tell;
		newf->funcs.WriteBytes	= VFSTCP_WriteBytes;
		newf->funcs.seekingisabadplan = true;

		// link in
		newf->next   = vfs_tcp_list;
		vfs_tcp_list = newf;

		return &newf->funcs;
	}
	else
		return NULL;
}

//TCP file
//******************************************************************************************************

void VFS_TICK(void)
{
	VFSTCP_Tick(); // fill in/out our internall buffers (do read/write on socket)
}

// VFS
//======================================================================================================

typedef enum { PAKOP_ADD, PAKOP_REM } pak_operation_t;

static qbool FS_PakOperation(char* pakfile, pak_operation_t op)
{
	switch (op) {
	case PAKOP_REM: return FS_RemovePak(pakfile);
	case PAKOP_ADD: return FS_AddPak(pakfile);
	}

	return false;
}

static qbool FS_PakOper_NoPath(char* pakfile, pak_operation_t op)
{
	char pathbuf[MAX_PATH];
	
	if (op != PAKOP_REM) // do not allow removing e.g. "pak"
		if (FS_PakOperation(pakfile, op)) return true;

	// This is nonstandard, therefore should be discussed first
	// snprintf(pathbuf, sizeof(pathbuf), "addons/%s.pak", pakfile);
	// if (FS_PakOperation(pathbuf, op)) return true;

	snprintf(pathbuf, sizeof(pathbuf), "ezquake/%s.pak", pakfile);
	if (FS_PakOperation(pathbuf, op)) return true;

	snprintf(pathbuf, sizeof(pathbuf), "qw/%s.pak", pakfile);
	if (FS_PakOperation(pathbuf, op)) return true;

	snprintf(pathbuf, sizeof(pathbuf), "id1/%s.pak", pakfile);
	if (FS_PakOperation(pathbuf, op)) return true;

	return false;
}

static void FS_PakOper_Process(pak_operation_t op)
{
	int i;
	int c = Cmd_Argc();

	if (cls.state != ca_disconnected && !cls.demoplayback && !cls.mvdplayback) {
		Com_Printf("This command cannot be used while connected\n");
		return;
	}
	if (c < 2) {
		Com_Printf("Usage: %s <pakname> [<pakname> [<pakname> ...]\n", Cmd_Argv(0));
		return;
	}

	for (i = 1; i < c; i++)
	{
		if (FS_PakOper_NoPath(Cmd_Argv(i), op)) {
			Com_Printf("Pak %s has been %s\n", Cmd_Argv(i), op == PAKOP_ADD ? "added" : "removed");
			Cache_Flush();
		}
		else Com_Printf("Pak not %s\n", op == PAKOP_ADD ? "added" : "removed");
	}
}

void FS_PakAdd_f(void) { FS_PakOper_Process(PAKOP_ADD); }
void FS_PakRem_f(void) { FS_PakOper_Process(PAKOP_REM); }

#ifdef WITH_ZLIB

#define CHUNK 16384

int COM_GZipPack (char *source_path,
				  char *destination_path,
				  qbool overwrite)
{
	FILE *source			= NULL;
	gzFile gzip_destination = NULL;

	// Open source file.
	source = fopen (source_path, "rb");

	// Failed to open source.
	if (!source_path)
	{
		return 0;
	}

	// Check if the destination file exists and
	// if we're allowed to overwrite it.
	if (COM_FileExists (destination_path) && !overwrite)
	{
		return 0;
	}

	// Create the path for the destination.
	COM_CreatePath (COM_SkipPath (destination_path));

	// Open destination file.
	gzip_destination = gzopen (destination_path, "wb");

	// Failed to open destination.
	if (!gzip_destination)
	{
		return 0;
	}

	// Pack.
	{
		unsigned char inbuf[CHUNK];
		int bytes_read = 0;

		while ((bytes_read = fread (inbuf, 1, sizeof(inbuf), source)) > 0)
		{
			gzwrite (gzip_destination, inbuf, bytes_read);
		}

		fclose (source);
		gzclose (gzip_destination);
	}

	return 1;
}

//
// Unpack a .gz file.
//
int COM_GZipUnpack (char *source_path,		// The path to the compressed source file.
					char *destination_path, // The destination file path.
					qbool overwrite)		// Overwrite the destination file if it exists?
{
	FILE *dest		= NULL;
	int retval		= 0;

	// Check if the destination file exists and
	// if we're allowed to overwrite it.
	if (COM_FileExists (destination_path) && !overwrite)
	{
		return 0;
	}

	// Create the path for the destination.
	COM_CreatePath (COM_SkipPath (destination_path));

	// Open destination.
	dest = fopen (destination_path, "wb");

	// Failed to open the file for writing.
	if (!dest)
	{
		return 0;
	}

	// Unpack.
	{
		unsigned char out[CHUNK];
		gzFile gzip_file = gzopen (source_path, "rb");

		while ((retval = gzread (gzip_file, out, CHUNK)) > 0)
		{
			fwrite (out, 1, retval, dest);
		}

		gzclose (gzip_file);
		fclose (dest);
	}

	return 1;
}

//
// Unpack a .gz file to a temp file.
//
int COM_GZipUnpackToTemp (char *source_path,		// The compressed source file.
						  char *unpack_path,		// A buffer that will contain the path to the unpacked file.
						  int unpack_path_size,		// The size of the buffer.
						  char *append_extension)	// The extension if any that should be appended to the filename.
{
	// Get a unique temp filename.
	if (!COM_GetUniqueTempFilename (NULL, unpack_path, unpack_path_size, true))
	{
		return 0;
	}

	// Delete the existing temp file (it is created when the filename is received above).
	if (unlink (unpack_path))
	{
		return 0;
	}

	// Append the extension if any.
	if (append_extension != NULL)
	{
		strlcpy (unpack_path, va("%s%s", unpack_path, append_extension), unpack_path_size);
	}

	// Unpack the file.
	if (!COM_GZipUnpack (source_path, unpack_path, true))
	{
		unpack_path[0] = 0;
		return 0;
	}

	return 1;
}

//
// Inflates source file into the dest file. (Stolen from a zlib example :D) ... NOT the same as gzip!
//
int COM_ZlibInflate(FILE *source, FILE *dest)
{
	int ret = 0;
	unsigned have = 0;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	// Allocate inflate state.
	strm.zalloc		= Z_NULL;
	strm.zfree		= Z_NULL;
	strm.opaque		= Z_NULL;
	strm.avail_in	= 0;
	strm.next_in	= Z_NULL;
	ret				= inflateInit(&strm);

	if (ret != Z_OK)
	{
		return ret;
	}

	// Decompress until deflate stream ends or end of file.
	do
	{
		strm.avail_in = fread(in, 1, CHUNK, source);

		if (ferror(source))
		{
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		}

		if (strm.avail_in == 0)
		{
			break;
		}

		strm.next_in = in;

		// Run inflate() on input until output buffer not full.
		do
		{
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);

			// State not clobbered.
			assert(ret != Z_STREAM_ERROR);

			switch (ret)
			{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR; // Fall through.
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&strm);
					return ret;
			}

			have = CHUNK - strm.avail_out;

			if (fwrite(out, 1, have, dest) != have || ferror(dest))
			{
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);

		// Done when inflate() says it's done
	} while (ret != Z_STREAM_END);

	// Clean up and return.
	(void)inflateEnd(&strm);

	return (ret == Z_STREAM_END) ? Z_OK : Z_DATA_ERROR;
}

//
// Unpack a zlib file. ... NOT the same as gzip!
//
int COM_ZlibUnpack (char *source_path,		// The path to the compressed source file.
					char *destination_path, // The destination file path.
					qbool overwrite)		// Overwrite the destination file if it exists?
{
	FILE *source	= NULL;
	FILE *dest		= NULL;
	int retval		= 0;

	// Open source.
	if (FS_FOpenPathFile (source_path, &source) < 0 || !source)
	{
		return 0;
	}

	// Check if the destination file exists and
	// if we're allowed to overwrite it.
	if (COM_FileExists (destination_path) && !overwrite)
	{
		fclose (source);
		return 0;
	}

	// Create the path for the destination.
	COM_CreatePath (COM_SkipPath (destination_path));

	// Open destination.
	dest = fopen (destination_path, "wb");

	// Failed to open the file for writing.
	if (!dest)
	{
		return 0;
	}

	// Unpack.
	retval = COM_ZlibInflate (source, dest);

	fclose (source);
	fclose (dest);

	return (retval != Z_OK) ? 0 : 1;
}

//
// Unpack a zlib file to a temp file... NOT the same as gzip!
//
int COM_ZlibUnpackToTemp (char *source_path,		// The compressed source file.
						  char *unpack_path,		// A buffer that will contain the path to the unpacked file.
						  int unpack_path_size,		// The size of the buffer.
						  char *append_extension)	// The extension if any that should be appended to the filename.
{
	// Get a unique temp filename.
	if (!COM_GetUniqueTempFilename (NULL, unpack_path, unpack_path_size, true))
	{
		return 0;
	}

	// Delete the existing temp file (it is created when the filename is received above).
	if (unlink (unpack_path))
	{
		return 0;
	}

	// Append the extension if any.
	if (append_extension != NULL)
	{
		strlcpy (unpack_path, va("%s%s", unpack_path, append_extension), unpack_path_size);
	}

	// Unpack the file.
	if (!COM_ZlibUnpack (source_path, unpack_path, true))
	{
		unpack_path[0] = 0;
		return 0;
	}

	return 1;
}

#endif // WITH_ZLIB

#ifdef WITH_ZIP

#define ZIP_WRITEBUFFERSIZE (8192)

/*
[19:23:40] <@disconnect|bla> Cokeman: how do you delete temporary files on windows? =:-)
[19:23:51] <@Cokeman> I don't :D
[19:23:52] Cokeman hides
[19:23:55] <@disconnect|bla> zomfg :E
[19:24:04] <@disconnect|bla> OK. Linux have same behavior now.
*/
int COM_ZipUnpackOneFileToTemp (unzFile zip_file,
						  const char *filename_inzip,
						  qbool case_sensitive,
						  qbool keep_path,
						  const char *password,
						  char *unpack_path,			// The path where the file was unpacked.
						  int unpack_path_size)			// The size of the buffer for "unpack_path", MAX_PATH is a goode idea.
{
	int retval;


	// Get a unique temp filename.
	if (!COM_GetUniqueTempFilename (NULL, unpack_path, unpack_path_size, true))
	{
		return UNZ_ERRNO;
	}

	// Delete the temp file if it exists (it is created when the filename is received above).
	retval = unlink (unpack_path);
	if (retval == -1 && qerrno != ENOENT)
	{
		return UNZ_ERRNO;
	}

	// Make sure we create a directory for the destination path.
	#ifdef WIN32
	strlcat (unpack_path, "\\", unpack_path_size);
	#else
	strlcat (unpack_path, "/", unpack_path_size);
	#endif

	// Unpack the file
	retval = COM_ZipUnpackOneFile (zip_file, filename_inzip, unpack_path, case_sensitive, keep_path, true, password);

	if (retval == UNZ_OK)
	{
		strlcpy (unpack_path, va("%s%s", unpack_path, filename_inzip), unpack_path_size);
	}
	else
	{
		unpack_path[0] = 0;
	}

	return retval;
}

int COM_ZipBreakupArchivePath (char *archive_extension,			// The extension of the archive type we're looking fore "zip" for example.
							   char *path,						// The path that should be broken up into parts.
							   char *archive_path,				// The buffer that should contain the archive path after the breakup.
							   int archive_path_size,			// The size of the archive path buffer.
							   char *inzip_path,				// The buffer that should contain the inzip path after the breakup.
							   int inzip_path_size)				// The size of the inzip path buffer.
{
	char *archive_path_found = NULL;
	char *inzip_path_found = NULL;
	char regexp[MAX_PATH];
	int result_length = 0;

	strlcpy (regexp, va("(.*?\\.%s)(\\\\|/)(.*)", archive_extension), sizeof(regexp));

	// Get the archive path.
	if (Utils_RegExpGetGroup (regexp, path, (const char **) &archive_path_found, &result_length, 1))
	{
		strlcpy (archive_path, archive_path_found, archive_path_size);

		// Get the path of the demo in the zip.
		if (Utils_RegExpGetGroup (regexp, path, (const char **) &inzip_path_found, &result_length, 3))
		{
			strlcpy (inzip_path, inzip_path_found, inzip_path_size);
			Q_free (archive_path_found);
			Q_free (inzip_path_found);
			return 1;
		}
	}

	Q_free (archive_path_found);
	Q_free (inzip_path_found);

	return -1;
}

//
// Does the given path point to a zip file?
//
qbool COM_ZipIsArchive (char *zip_path)
{
	return (!strcmp (COM_FileExtension (zip_path), "zip"));
}

unzFile COM_ZipUnpackOpenFile (const char *zip_path)
{
	return unzOpen (zip_path);
}

int COM_ZipUnpackCloseFile (unzFile zip_file)
{
	if (zip_file == NULL)
	{
		return UNZ_OK;
	}

	return unzClose (zip_file);
}

//
// Creates a directory entry from a unzip fileinfo struct.
//
static void COM_ZipMakeDirent (sys_dirent *ent, char *filename_inzip, unz_file_info *unzip_fileinfo)
{
	// Save the name.
    strlcpy(ent->fname, filename_inzip, sizeof(ent->fname));
    ent->fname[MAX_PATH_LENGTH-1] = 0;

	// TODO : Zip size is unsigned long, dir entry unsigned int, data loss possible for really large files.
	ent->size = (unsigned int)unzip_fileinfo->uncompressed_size;

    // Get the filetime.
	{
		// FIXME: This gets the wrong date...
		#ifdef WIN32
		FILETIME filetime;
		FILETIME local_filetime;
		DosDateTimeToFileTime (unzip_fileinfo->dosDate, 0, &filetime);
		FileTimeToLocalFileTime (&filetime, &local_filetime);
		FileTimeToSystemTime(&local_filetime, &ent->time);
		#else
		// FIXME: Dunno how to do this in *nix.
		memset (&ent->time, 0, sizeof (ent->time));
		#endif // WIN32
	}

	// FIXME: There is no directory structure inside of zip files, but the files are named as if there is.
	// that is, if the file is in the root it will be named "file" in the zip file info. If it's in a directory
	// it will be named "dir/file". So we could find out if it's a directory or not by checking the filename here.
	ent->directory = 0;
	ent->hidden = 0;
}

int COM_ZipUnpack (unzFile zip_file,
				   char *destination_path,
				   qbool case_sensitive,
				   qbool keep_path,
				   qbool overwrite,
				   const char *password)
{
	int error = UNZ_OK;
	unsigned long file_num = 0;
	unz_global_info global_info;

	// Get the number of files in the zip archive.
	error = unzGetGlobalInfo (zip_file, &global_info);

	if (error != UNZ_OK)
	{
		return error;
	}

	for (file_num = 0; file_num < global_info.number_entry; file_num++)
	{
		if (COM_ZipUnpackCurrentFile (zip_file, destination_path, case_sensitive, keep_path, overwrite, password) != UNZ_OK)
		{
			// We failed to extract a file, so there must be something wrong.
			break;
		}

		if ((file_num + 1) < global_info.number_entry)
		{
			error = unzGoToNextFile (zip_file);

			if (error != UNZ_OK)
			{
				// Either we're at the end or something went wrong.
				break;
			}
		}
	}

	return error;
}

int COM_ZipUnpackToTemp (unzFile zip_file,
				   qbool case_sensitive,
				   qbool keep_path,
				   const char *password,
				   char *unpack_path,			// The path where the file was unpacked.
				   int unpack_path_size)		// The size of the buffer for "unpack_path", MAX_PATH is a goode idea.)
{
	int	retval = UNZ_OK;

	// Get a unique temp filename.
	if (!COM_GetUniqueTempFilename (NULL, unpack_path, unpack_path_size, true))
	{
		return UNZ_ERRNO;
	}

	// Delete the existing temp file (it is created when the filename is received above).
	if (unlink (unpack_path))
	{
		return UNZ_ERRNO;
	}

	// Make sure we create a directory for the destination path since we're unpacking an entire zip.
	#ifdef WIN32
	strlcat (unpack_path, "\\", unpack_path_size);
	#else
	strlcat (unpack_path, "/", unpack_path_size);
	#endif

	// Unpack the file.
	retval = COM_ZipUnpack (zip_file, unpack_path, case_sensitive, keep_path, true, password);

	if (retval != UNZ_OK)
	{
		unpack_path[0] = 0;
	}

	return retval;
}

int COM_ZipUnpackOneFile (unzFile zip_file,				// The zip file opened with COM_ZipUnpackOpenFile(..)
						  const char *filename_inzip,	// The name of the file to unpack inside the zip.
						  const char *destination_path, // The destination path where to extract the file to.
						  qbool case_sensitive,			// Should we look for the filename case sensitivly?
						  qbool keep_path,				// Should the path inside the zip be preserved when unpacking?
						  qbool overwrite,				// Overwrite any existing file with the same name when unpacking?
						  const char *password)			// The password to use when extracting the file.
{
	int	retval = UNZ_OK;

	if (filename_inzip == NULL || zip_file == NULL)
	{
		return UNZ_ERRNO;
	}

	// Locate the file.
	retval = unzLocateFile (zip_file, filename_inzip, case_sensitive);

	if (retval == UNZ_END_OF_LIST_OF_FILE || retval != UNZ_OK)
	{
		return retval;
	}

	// Unpack the file.
	COM_ZipUnpackCurrentFile (zip_file, destination_path, case_sensitive, keep_path, overwrite, password);

	return retval;
}

int COM_ZipUnpackCurrentFile (unzFile zip_file,
							  const char *destination_path,
							  qbool case_sensitive,
							  qbool keep_path,
							  qbool overwrite,
							  const char *password)
{
	int				error = UNZ_OK;
	void			*buf = NULL;
	unsigned int	size_buf = ZIP_WRITEBUFFERSIZE;
	char			filename[MAX_PATH_LENGTH];
	unz_file_info	file_info;

	// Nothing to extract from.
	if (zip_file == NULL)
	{
		return UNZ_ERRNO;
	}

	// Get the file info (including the filename).
	error = unzGetCurrentFileInfo (zip_file, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0);

	if (error != UNZ_OK)
	{
		goto finish;
	}

	// Should the path in the zip file be preserved when extracting or not?
	if (!keep_path)
	{
		// Only keep the filename.
		strlcpy (filename, COM_SkipPath (filename), sizeof(filename));
	}

	//
	// Check if the file already exists and create the path if needed.
	//
	{
		// Check if the file exists.
		if (COM_FileExists (va("%s/%s", destination_path, filename)) && !overwrite)
		{
			error = UNZ_ERRNO;
			goto finish;
		}

		// Create the destination dir if it doesn't already exist.
		COM_CreatePath (va("%s%c", destination_path, PATH_SEPARATOR));

		// Create the relative path before extracting.
		if (keep_path)
		{
			COM_CreatePath (va("%s%c%s", destination_path, PATH_SEPARATOR, filename));
		}
	}

	//
	// Extract the file.
	//
	{
		#define	EXPECTED_BYTES_READ	1
		int	bytes_read	= 0;
		FILE *fout		= NULL;

		error = UNZ_OK;

		//
		// Open the zip file.
		//
		{
			error = unzOpenCurrentFilePassword (zip_file, password);

			// Failed opening the zip file.
			if (error != UNZ_OK)
			{
				goto finish;
			}
		}

		//
		// Open the destination file for writing.
		//
		{
			fout = fopen (va("%s/%s", destination_path, filename), "wb");

			// Failure.
			if (fout == NULL)
			{
				error = UNZ_ERRNO;
				goto finish;
			}
		}

		//
		// Write the decompressed file to the destination.
		//
		buf = Q_malloc (size_buf);
		do
		{
			// Read the decompressed data from the zip file.
			bytes_read = unzReadCurrentFile (zip_file, buf, size_buf);

			if (bytes_read > 0)
			{
				// Write the decompressed data to the destination file.
				if (fwrite (buf, bytes_read, EXPECTED_BYTES_READ, fout) != EXPECTED_BYTES_READ)
				{
					// We failed to write the specified number of bytes.
					error = UNZ_ERRNO;
					fclose (fout);
					unzCloseCurrentFile (zip_file);
					goto finish;
				}
			}
		}
		while (bytes_read > 0);

		//
		// Close the zip file + destination file.
		//
		{
			if (fout)
			{
				fclose (fout);
			}

			unzCloseCurrentFile (zip_file);
		}

		// TODO : Change file date for the file.
	}

finish:
	Q_free (buf);

	return error;
}

// Gets the details about the file and save them in the sys_dirent struct.
static int COM_ZipGetDetails (unzFile zip_file, sys_dirent *ent)
{
	int error = UNZ_OK;
	char filename_inzip[MAX_PATH_LENGTH];
	unz_file_info unzip_fileinfo;

	error = unzGetCurrentFileInfo (zip_file,
		&unzip_fileinfo,						// File info.
		filename_inzip, sizeof(filename_inzip), // The files name will be copied to this.
		NULL, 0, NULL, 0);						// Extras + comment stuff. We don't care about this.

	if (error != UNZ_OK)
	{
		return error;
	}

	// Populate the directory entry object.
	COM_ZipMakeDirent (ent, filename_inzip, &unzip_fileinfo);

	return error;
}

int COM_ZipGetFirst (unzFile zip_file, sys_dirent *ent)
{
	int error = UNZ_OK;

	// Go to the first file in the zip.
	if ((error = unzGoToFirstFile (zip_file)) != UNZ_OK)
	{
		return error;
	}

	// Get details.
	if ((error = COM_ZipGetDetails(zip_file, ent)) != UNZ_OK)
	{
		return error;
	}

	return 1;
}

int COM_ZipGetNextFile (unzFile zip_file, sys_dirent *ent)
{
	int error = UNZ_OK;

	// Get the next file.
	error = unzGoToNextFile (zip_file);

	if (error == UNZ_END_OF_LIST_OF_FILE || error != UNZ_OK)
	{
		return error;
	}

	// Get details.
	if ((error = COM_ZipGetDetails(zip_file, ent)) != UNZ_OK)
	{
		return error;
	}

	return 1;
}

#endif // WITH_ZIP

//============================================================================

void FS_InitModuleFS (void)
{
	Cmd_AddCommand("loadpak", FS_PakAdd_f);
	Cmd_AddCommand("removepak", FS_PakRem_f);
	Cmd_AddCommand("path", COM_Path_f);
}

//================================================================================================
//                                START OF FTE VFS LAYER
// Some of the below code uses the functions above
//================================================================================================
#ifdef WITH_FTE_VFS

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
 * $Id: fs.c,v 1.19 2007-09-04 12:19:58 dkure Exp $
 *             
 */

/* 
 * Author:      
 * (I think) Spike from FTE, ported across by D-Kure
 * 
 * Description: 
 * This provides a virtual file system (VFS). This allows a standard way of 
 * accessing different types of files. These include pak, pak3, pak4, zip, 
 * and q4 wad files using a standard interface, FS_OpenVFS, VFSCLOSE, 
 * VFSREAD & VFSWRITE (along with other standard file IO functions). The 
 * standard interface is achieved by using opening files and then using
 * providing different function pointers for the different file types
 *              
 * Most of this file is providing functions for different types of files 
 * supported. The rest is the initalisation, shutdown, reloading & helper
 * functions for the VFS
 */

/******************************************************************************
 *     TODO:
 ****************************
 * 1) Need to add some functions FTE-FIXME's
 * 		D-Kure: Mostly finished, some functions aren't needed as we have other
 * 		        ways of doing it.
 *
 * 2) Need to check all the FTE-FIXME's
 * 		D-Kure: I have made comment on the ones I unsure of,
 * 		        others just need the time to do what the comment says.
 *
 * 3) Need to add Sys_EnumerateFiles for sys_mac.c
 * 		D-Kure: I have noooo idea what the mac eqivalent functions are.
 *
 * 4) Replace some of the functions (marked with XXX) with the ezquake equivlant
 * 		D-Kure: This functions seem to be in common.c and are marked 
 * 		        FS_* instead of COM_*.
 *
 * 5) Replace all strncpy calls with strlcpy
 *      <Cokeman> allthough strncpy isn't safe
 *      <Cokeman> strlcpy is
 *
 * 6) pak3 support does not work yet, firstly we need to disable it to avoid
 *    seg faults in ezQuake, then we need to get it working
 *
 * 7) Fix the rendering bug...
 *    D-Kure: I think this is only to do with png/24bit files as lmp's seem to work
 *    Fixed: This was a fs_filesize not being set correctly on FS_OpenVFS
 *
 * 8) EX_browser*.c need to be updated to use VFS_GETS parsing rather then 
 *    fscanf() & feof()
 *
 * 9) Renaming of lots of the COM_* functions to FS_*
 *
 ****************************
 *     No particular order
 ****************************
 *  - Add Ezquake TCP VFS to the supported files (copy & paste from fs.c)
 *  - Replace standard open/close/read/write pak io with this VFS stuff
 *  - Add tar.gz support
 *  - Replace zipped/tar/tar.gz demo opening with VFS calls
 *  - Replace current fs.c with this file (fte_fs.c)
 *
 *****************************************************************************/

//======================
//   STDIO files (OS)
//======================
// VFSOS_ReadBytes  is above
// VFSOS_WriteBytes is above
// VFSOS_Seek       is above
// VFSOS_Tell       is above
// VFSOS_GetSize    is above
// VFSOS_Close      is above

// XXX: This is slightly different to fs.c version in that we don't take in a FILE *f
vfsfile_t *VFSOS_Open(char *osname, char *mode)
{
	FILE *f;
	vfsosfile_t *file;
	qbool read   = !!strchr(mode, 'r');
	qbool write  = !!strchr(mode, 'w');
	qbool append = !!strchr(mode, 'a');
	qbool text   = !!strchr(mode, 't');
	char newmode[3];
	int modec = 0;
	fs_filesize = -1;

	if (read)
		newmode[modec++] = 'r';
	if (write)
		newmode[modec++] = 'w';
	if (append)
		newmode[modec++] = 'a';
	if (text)
		newmode[modec++] = 't';
	else
		newmode[modec++] = 'b';
	newmode[modec++] = '\0';

	f = fopen(osname, newmode);
	if (!f)
		return NULL;

	file = Q_malloc(sizeof(vfsosfile_t));

	file->funcs.ReadBytes  = ( strchr(mode, 'r')                      ? VFSOS_ReadBytes  : NULL);
	file->funcs.WriteBytes = ((strchr(mode, 'w') || strchr(mode, 'a'))? VFSOS_WriteBytes : NULL);
	file->funcs.Seek       = VFSOS_Seek;
	file->funcs.Tell       = VFSOS_Tell;
	file->funcs.GetLen     = VFSOS_GetSize;
	file->funcs.Close      = VFSOS_Close;

	file->handle = f;

	fs_filesize = VFS_GETLEN((vfsfile_t *)file);

	return (vfsfile_t*)file;
}

vfsfile_t *FSOS_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	char diskname[MAX_OSPATH];

	snprintf(diskname, sizeof(diskname), "%s/%s", (char*)handle, loc->rawname);

	return VFSOS_Open(diskname, mode);
}

void FSOS_PrintPath(void *handle)
{
	Com_Printf("%s\n", handle);
}

void FSOS_ClosePath(void *handle)
{
	Q_free(handle);
}

int FSOS_RebuildFSHash(char *filename, int filesize, void *data)
{
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		sprintf(childpath, "%s*", filename);
		Sys_EnumerateFiles((char*)data, childpath, FSOS_RebuildFSHash, data);
		return true;
	}
	if (!Hash_GetInsensative(&filesystemhash, filename))
	{
		bucket_t *bucket = (bucket_t*)Q_malloc(sizeof(bucket_t) + (size_t)strlen(filename)+1);
		strcpy((char *)(bucket+1), filename);
#ifdef _WIN32
		Q_strlwr((char *)(bucket+1));
#endif
		Hash_AddInsensative(&filesystemhash, (char *)(bucket+1), data, bucket);

		fs_hash_files++;
	}
	else
		fs_hash_dups++;
	return true;
}

void FSOS_BuildHash(void *handle)
{
	Sys_EnumerateFiles(handle, "*", FSOS_RebuildFSHash, handle);
}

qbool FSOS_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	FILE *f;
	int len;
	char netpath[MAX_OSPATH];


	if (hashedresult && (void *)hashedresult != handle)
		return false;

/*
	if (!static_registered)
	{	// if not a registered version, don't ever go beyond base
		if ( strchr (filename, '/') || strchr (filename,'\\'))
			continue;
	}
*/

// check a file in the directory tree
	snprintf (netpath, sizeof(netpath)-1, "%s/%s",(char*)handle, filename);

	f = fopen(netpath, "rb");
	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fclose(f);
	if (loc)
	{
		loc->len = len;
		loc->offset = 0;
		loc->index = 0;
		strncpy(loc->rawname, filename, sizeof(loc->rawname));
	}

	return true;
}

void FSOS_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	FILE *f;
	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	fread(buffer, 1, loc->len, f);
	fclose(f);
}

int FSOS_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	return Sys_EnumerateFiles(handle, match, func, parm);
}

searchpathfuncs_t osfilefuncs = {
	FSOS_PrintPath,
	FSOS_ClosePath,
	FSOS_BuildHash,
	FSOS_FLocate,
	FSOS_ReadFile,
	FSOS_EnumerateFiles,
	NULL,
	NULL,
	FSOS_OpenVFS
};


//=====================
//PACK files (*.pak)
//=====================
void FSPAK_PrintPath(void *handle)
{
	pack_t *pak = handle;

	if (pak->references != 1)
		Com_Printf("%s (%i)\n", pak->filename, pak->references-1);
	else
		Com_Printf("%s\n", pak->filename);
}
void FSPAK_ClosePath(void *handle)
{
	pack_t *pak = handle;

	pak->references--;
	if (pak->references > 0)
		return;	//not free yet


	VFS_CLOSE (pak->handle);
	if (pak->files)
		Q_free(pak->files);
	Q_free(pak);
}
void FSPAK_BuildHash(void *handle)
{
	pack_t *pak = handle;
	int i;

	for (i = 0; i < pak->numfiles; i++)
	{
		if (!Hash_GetInsensative(&filesystemhash, pak->files[i].name))
		{
			fs_hash_files++;
			Hash_AddInsensative(&filesystemhash, pak->files[i].name, &pak->files[i], &pak->files[i].bucket);
		}
		else
			fs_hash_dups++;
	}
}
qbool FSPAK_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	int i, len;
	pack_t		*pak = handle;

// look through all the pak file elements

	if (pf)
	{	//is this a pointer to a file in this pak?
		if (pf < pak->files || pf > pak->files + pak->numfiles)
			return false;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<pak->numfiles ; i++)	//look for the file
		{
			if (!strcmp (pak->files[i].name, filename))
			{
				pf = &pak->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		len = pf->filelen;
		if (loc)
		{
			loc->index = pf - pak->files;
			snprintf(loc->rawname, sizeof(loc->rawname), "%s/%s", pak->filename, filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return true;
	}
	return false;
}
int FSPAK_EnumerateFiles (void *handle, char *match, int (*func)(char *, int, void *), void *parm)
{
	pack_t	*pak = handle;
	int		num;

	for (num = 0; num<(int)pak->numfiles; num++)
	{
		if (wildcmp(match, pak->files[num].name))
		{
			if (!func(pak->files[num].name, pak->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}
/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
void *FSPAK_LoadPackFile (vfsfile_t *file, char *desc)
{
	dpackheader_t	header;
	int				i;
//	int				j;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	vfsfile_t		*packhandle;
	dpackfile_t		info;
	int read;
	vfserrno_t err;
//	unsigned short		crc;

	packhandle = file;
	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header), &err);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return NULL;
//		Sys_Error ("%s is not a packfile", packfile);
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

//	if (numpackfiles > MAX_FILES_IN_PACK)
//		Sys_Error ("%s has %i files", packfile, numpackfiles);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true;	// not the original file

	newfiles = (packfile_t*)Q_malloc (numpackfiles * sizeof(packfile_t));

	VFS_SEEK(packhandle, header.dirofs);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = QCRC_Block((qbyte *)info, header.dirlen);


//	QCRC_Init (&crc);

	pack = (pack_t*)Q_malloc (sizeof (pack_t));
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		*info.name = '\0';
		read = VFS_READ(packhandle, &info, sizeof(info), &err);
/*
		for (j=0 ; j<sizeof(info) ; j++)
			CRC_ProcessByte(&crc, ((qbyte *)&info)[j]);
*/
		strcpy (newfiles[i].name, info.name);
		// FTE-FIXME: I think this safe to remove as its for a q2 tank skin...
		//COM_CleanUpPath(newfiles[i].name);	//blooming tanks.
		//
		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);
	}
/*
	if (crc != PAK0_CRC)
		com_modified = true;
*/
	strcpy (pack->filename, desc);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos);

	pack->references++;

	// FTE-FIXME: Replace this with a Com_Printf
	//Con_TPrintf (TL_ADDEDPACKFILE, desc, numpackfiles);
	return pack;
}

// XXX: This differs with the fs.c version above
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread, vfserrno_t *err)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	int read;

	if (vfsp->currentpos - vfsp->startpos + bytestoread > vfsp->length)
		bytestoread = vfsp->length - (vfsp->currentpos - vfsp->startpos);
	if (bytestoread <= 0)
		return -1;

	if (vfsp->parentpak->filepos != vfsp->currentpos) {
		VFS_SEEK(vfsp->parentpak->handle, vfsp->currentpos);
	}

	read = VFS_READ(vfsp->parentpak->handle, buffer, bytestoread, err);
	vfsp->currentpos += read;
	vfsp->parentpak->filepos = vfsp->currentpos;

	return read;
}

// VFSPAK_WriteBytes is above

// XXX: This function doesn't actually do the fseek... however the above version does
qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	if (pos < 0 || pos > vfsp->length)
		return false;
	vfsp->currentpos = pos + vfsp->startpos;

	return true;
}

// VFSPAK_Tell      is above
// VFSPAK_GetLen    is above

// XXX: This function is different as we close a reference to our parent file
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	FSPAK_ClosePath(vfsp->parentpak);	// tell the parent that we don't need it open any 
										// more (reference counts)
	Q_free(vfsp);
}

// XXX: This is different from the above version that we pass a pack handle in, rather then a 
//      file handle. The parent pak then is updated etc
vfsfile_t *FSPAK_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	pack_t *pack = (pack_t*)handle;
	vfspack_t *vfs;
	fs_filesize = -1;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfs = Q_malloc(sizeof(vfspack_t));

	vfs->parentpak = pack;
	vfs->parentpak->references++;

	vfs->startpos = loc->offset;
	vfs->length = loc->len;
	vfs->currentpos = vfs->startpos;

	vfs->funcs.Close      = VFSPAK_Close;
	vfs->funcs.GetLen     = VFSPAK_GetLen;
	vfs->funcs.ReadBytes  = VFSPAK_ReadBytes;
	vfs->funcs.Seek       = VFSPAK_Seek;
	vfs->funcs.Tell       = VFSPAK_Tell;
	vfs->funcs.WriteBytes = VFSPAK_WriteBytes;	//not supported

	fs_filesize = VFS_GETLEN((vfsfile_t *)vfs);

	return (vfsfile_t *)vfs;
}

searchpathfuncs_t packfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadPackFile,
	NULL,
	FSPAK_OpenVFS
};



#ifdef DOOMWADS
void *FSPAK_LoadDoomWadFile (vfsfile_t *packhandle, char *desc)
{
	dwadheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	dwadfile_t		info;

	int section=0;
	char sectionname[MAX_QPATH];
	char filename[52];
	char neatwadname[52];

	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header));
	if (header.id[1] != 'W'	|| header.id[2] != 'A' || header.id[3] != 'D')
		return NULL;	//not a doom wad

	//doom wads come in two sorts. iwads and pwads.
	//iwads are the master wads, pwads are meant to replace parts of the master wad.
	//this is awkward, of course.
	//we ignore the i/p bit for the most part, but with maps, pwads are given a prefixed name.
	if (header.id[0] == 'I')
		*neatwadname = '\0';
	else if (header.id[0] == 'P')
	{
		COM_FileBase(desc, neatwadname, sizeof(neatwadname));
		strcat(neatwadname, "#");
	}
	else
		return NULL;

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen;
	newfiles = (packfile_t*)Q_malloc (numpackfiles * sizeof(packfile_t));
	VFS_SEEK(packhandle, header.dirofs);

	//doom wads are awkward.
	//they have no directory structure, except for start/end 'files'.
	//they follow along the lines of lumps after the parent name.
	//a map is the name of that map, and then a squence of the lumps that form that map (found by next-with-that-name).
	//this is a problem for a real virtual filesystem, so we add a hack to recognise special names and expand them specially.
	for (i=0 ; i<numpackfiles ; i++)
	{
		VFS_READ (packhandle, &info, sizeof(info));

		strcpy (filename, info.name);
		filename[8] = '\0';
		Q_strlwr(filename);

		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);

		switch(section)	//be prepared to remap filenames.
		{
newsection:
		case 0:
			if (info.filelen == 0)
			{	//marker for something...

				if (!strcmp(filename, "s_start"))
				{
					section = 2;
					sprintf (newfiles[i].name, "sprites/%s", filename);	//the model loader has a hack to recognise .dsp
					break;
				}
				if (!strcmp(filename, "p_start"))
				{
					section = 3;
					sprintf (newfiles[i].name, "patches/%s", filename); //the map loader will find these.
					break;
				}
				if (!strcmp(filename, "f_start"))
				{
					section = 4;
					sprintf (newfiles[i].name, "flats/%s", filename);	//the map loader will find these
					break;
				}
				if ((filename[0] == 'e' && filename[2] == 'm') || !strncmp(filename, "map", 3))
				{	//this is the start of a beutiful new map
					section = 1;
					strcpy(sectionname, filename);
					sprintf (newfiles[i].name, "maps/%s%s.bsp", neatwadname, filename);	//generate fake bsps to allow the server to find them
					newfiles[i].filepos = 0;
					newfiles[i].filelen = 4;
					break;
				}
				if (!strncmp(filename, "gl_", 3) && ((filename[4] == 'e' && filename[5] == 'm') || !strncmp(filename+3, "map", 3)))
				{	//this is the start of a beutiful new map
					section = 5;
					strcpy(sectionname, filename+3);
					break;
				}
			}

			sprintf (newfiles[i].name, "wad/%s", filename);	//but there are many files that we don't recognise/know about. archive them off to keep the vfs moderatly clean.
			break;
		case 1:	//map section
			if (strcmp(filename, "things") &&
				strcmp(filename, "linedefs") &&
				strcmp(filename, "sidedefs") &&
				strcmp(filename, "vertexes") &&
				strcmp(filename, "segs") &&
				strcmp(filename, "ssectors") &&
				strcmp(filename, "nodes") &&
				strcmp(filename, "sectors") &&
				strcmp(filename, "reject") &&
				strcmp(filename, "blockmap"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 5:	//glbsp output section
			if (strcmp(filename, "gl_vert") &&
				strcmp(filename, "gl_segs") &&
				strcmp(filename, "gl_ssect") &&
				strcmp(filename, "gl_pvs") &&
				strcmp(filename, "gl_nodes"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 2:	//sprite section
			if (!strcmp(filename, "s_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "sprites/%s", filename);
			break;
		case 3:	//patches section
			if (!strcmp(filename, "p_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "patches/%s", filename);
			break;
		case 4:	//flats section
			if (!strcmp(filename, "f_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "flats/%s", filename);
			break;
		}
	}

	pack = (pack_t*)Q_malloc (sizeof (pack_t));
	strcpy (pack->filename, desc);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos);

	pack->references++;

	Con_TPrintf (TL_ADDEDPACKFILE, desc, numpackfiles);
	return pack;
}
searchpathfuncs_t doomwadfilefuncs = {
	FSPAK_PrintPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSOS_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadDoomWadFile,
	NULL,
	FSPAK_OpenVFS
};
#endif

#ifdef WITH_PK3
//========================
//ZIP files (*.zip *.pk3)
//========================

void *com_pathforfile;	//fread and stuff is preferable if null

#ifndef ZEXPORT
#define ZEXPORT VARGS
#endif

#ifdef WITH_ZIP
#ifdef _WIN32
#pragma comment( lib, "../libs/zlib.lib" )
#endif

//#define uShort ZLIBuShort
//#define uLong ZLIBuLong

#include <zlib.h>
//#include "unzip.c"	// Modified version of the unzip library

typedef struct zipfile_s
{
	char filename[MAX_QPATH];
	unzFile handle;
	int		numfiles;
	packfile_t	*files;

#ifdef HASH_FILESYSTEM
	hashtable_t hash;
#endif

	vfsfile_t *raw;
	vfsfile_t *currentfile;	//our unzip.c can only handle one active file at any one time
							//so we have to keep closing and switching.
							//slow, but it works. most of the time we'll only have a single file open anyway.
	int references;	//and a reference count
} zipfile_t;

//=================================================================================================
// XXX: Grabbed from unzip.c from fte
/* unz_file_info_interntal contain internal info about a file in zipfile*/
typedef struct unz_file_info_internal_s {
	unsigned long offset_curfile;/* relative offset of local header 4 bytes */
} unz_file_info_internal;

// XXX: Grabbed from unzip.c from fte
/* file_in_zip_read_info_s contain internal information about a file in zipfile,
    when reading and decompress it */
typedef struct {
	char  *read_buffer;         /* internal buffer for compressed data */
	z_stream stream;            /* zLib stream structure for inflate */

	unsigned long pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
	unsigned long stream_initialised;   /* flag set if stream structure is initialised*/

	unsigned long offset_local_extrafield;/* offset of the local extra field */
	unsigned int  size_local_extrafield;  /* size of the local extra field */
	unsigned long pos_local_extrafield;   /* position in the local extra field in read*/

	unsigned long crc32;                  /* crc32 of all data uncompressed            */
	unsigned long crc32_wait;             /* crc32 we must obtain after decompress all */
	unsigned long rest_read_compressed;   /* number of byte to be decompressed         */
	unsigned long rest_read_uncompressed; /*number of byte to be obtained after decomp */
	vfsfile_t* file;                      /* io structore of the zipfile               */
	unsigned long compression_method;     /* compression method (0==store)             */
	unsigned long byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)     */
} file_in_zip_read_info_s;


// XXX: Grabbed from unzip.c from fte
/* unz_s contain internal information about the zipfile
*/
typedef struct {
	vfsfile_t* file;                 	  /* io structore of the zipfile */
	unz_global_info gi;       			  /* public global information */
	unsigned long byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
	unsigned long num_file;               /* number of the current file in the zipfile*/
	unsigned long pos_in_central_dir;     /* pos of the current file in the central dir*/
	unsigned long current_file_ok;        /* flag about the usability of the current file*/
	unsigned long central_pos;            /* position of the beginning of the central dir*/

	unsigned long size_central_dir;       /* size of the central directory  */
	unsigned long offset_central_dir;     /* offset of start of central directory with
								   		     respect to the starting disk number */

	unz_file_info cur_file_info; 					 /* public info about the current file in zip*/
	unz_file_info_internal   cur_file_info_internal; /* private info about it*/
    file_in_zip_read_info_s *pfile_in_zip_read; 	 /* structure about the current
	                                    				file if we are decompressing it */
} unz_s;

/*int unzLocateFileMy (unzFile file, unsigned long num, unsigned long pos);
int unzlocal_GetCurrentFileInfoInternal(unzFile file,
                                                  unz_file_info *pfile_info,
                                                  unz_file_info_internal 
                                                  *pfile_info_internal,
                                                  char *szFileName,
						  unsigned long fileNameBufferSize,
                                                  void *extraField,
						  unsigned long extraFieldBufferSize,
                                                  char *szComment,
						  unsigned long commentBufferSize);
*/

int unzlocal_getShortSane(vfsfile_t *fin, unsigned short *pi)
{
	unsigned short c;
	vfserrno_t err;
	int bytesRead;

	bytesRead = VFS_READ(fin, &c, 2, &err);
	if (bytesRead == 2)
	{
		*pi = LittleShort(c);
		return UNZ_OK;
	}
	else
	{
		*pi = 0;
		if (VFS_TELL(fin) != VFS_GETLEN(fin))
			return UNZ_ERRNO;
		else
			return UNZ_EOF;
	}
}


// XXX: Grabbed from unzip.c from fte
int unzlocal_getShort(vfsfile_t *fin,unsigned long *pi) {
    unsigned short c;
	vfserrno_t err;
	int bytesRead;

	bytesRead = VFS_READ(fin, &c, 2, &err);
    if (bytesRead == 2) {
	    *pi = LittleShort(c);
        return UNZ_OK;
    } else {
        if (VFS_TELL(fin) != VFS_GETLEN(fin)) return UNZ_ERRNO;
        else return UNZ_EOF;
    }
}

// XXX: Grabbed from unzip.c from fte
int unzlocal_getLong(vfsfile_t *fin,unsigned long *pi) {
	unsigned int c;
	vfserrno_t err;
	int bytesRead;

	bytesRead = VFS_READ(fin, &c, 4, &err);
    if (bytesRead == 4) {
	    *pi = LittleLong(c);
        return UNZ_OK;
    } else {
        if (VFS_TELL(fin) != VFS_GETLEN(fin)) return UNZ_ERRNO;
        else return UNZ_EOF;
    }
}


// XXX: Grabbed from unzip.c from fte
int unzlocal_GetCurrentFileInfoInternal (unzFile file,
                                              unz_file_info *pfile_info,
                                              unz_file_info_internal *pfile_info_internal,
					      char *szFileName, unsigned long fileNameBufferSize,
					      void *extraField, unsigned long extraFieldBufferSize,
					      char *szComment,  unsigned long commentBufferSize) {
	unz_s *s;
	unz_file_info file_info;
	unz_file_info_internal file_info_internal = {0};
	vfserrno_t vfserr;
	int err=UNZ_OK;
	unsigned short pi;
	unsigned long uMagic = 0;
	long lSeek=0;

	if (!file) return UNZ_PARAMERROR;
	s = (unz_s *)file;
	if (!VFS_SEEK(s->file,s->pos_in_central_dir+s->byte_before_the_zipfile)) err=UNZ_ERRNO;


	/* we check the magic */
	if (err==UNZ_OK) 
	{
		if (unzlocal_getLong(s->file,&uMagic) != UNZ_OK) err=UNZ_ERRNO;
		else if (uMagic!=0x02014b50) err=UNZ_BADZIPFILE;
	}

	unzlocal_getShortSane(s->file, &pi);
	file_info.version = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.version_needed = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.flag = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.compression_method = pi;
	unzlocal_getLong(s->file, &file_info.dosDate);
	unzlocal_getLong(s->file, &file_info.crc);
	unzlocal_getLong(s->file, &file_info.compressed_size);
	unzlocal_getLong(s->file, &file_info.uncompressed_size);
	unzlocal_getShortSane(s->file, &pi);
	file_info.size_filename = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.size_file_extra = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.size_file_comment = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.disk_num_start = pi;
	unzlocal_getShortSane(s->file, &pi);
	file_info.internal_fa = pi;
	unzlocal_getLong(s->file, &file_info.external_fa);
/*
	VFS_READ(s->file, &file_info, sizeof(file_info)-2*4); // 2*4 is the size of 2 my vars
	file_info.version = LittleShort(file_info.version);
	file_info.version_needed = LittleShort(file_info.version_needed);
	file_info.flag = LittleShort(file_info.flag);
	file_info.compression_method = LittleShort(file_info.compression_method);
	file_info.dosDate = LittleLong(file_info.dosDate);
	file_info.crc = LittleLong(file_info.crc);
	file_info.compressed_size = LittleLong(file_info.compressed_size);
	file_info.uncompressed_size = LittleLong(file_info.uncompressed_size);
	file_info.size_filename = LittleShort(file_info.size_filename);
	file_info.size_file_extra = LittleShort(file_info.size_file_extra);
	file_info.size_file_comment = LittleShort(file_info.size_file_comment);
	file_info.disk_num_start = LittleShort(file_info.disk_num_start);
	file_info.internal_fa = LittleShort(file_info.internal_fa);
	file_info.external_fa = LittleLong(file_info.external_fa);
*/
	if (unzlocal_getLong(s->file,&file_info_internal.offset_curfile) != UNZ_OK) err=UNZ_ERRNO;

	lSeek+=file_info.size_filename;
	if ((err==UNZ_OK) && (szFileName))
	{
		unsigned long uSizeRead ;
		if (file_info.size_filename<fileNameBufferSize)
		{
			*(szFileName+file_info.size_filename)='\0';
			uSizeRead = file_info.size_filename;
		}
		else uSizeRead = fileNameBufferSize;

		if ((file_info.size_filename>0) && (fileNameBufferSize>0))
			if (VFS_READ(s->file, szFileName,(unsigned int)uSizeRead, &vfserr) != uSizeRead) 
				err=UNZ_ERRNO;
		lSeek -= uSizeRead;
	}

	
	if ((err==UNZ_OK) && (extraField))
	{
		unsigned long uSizeRead ;
		if (file_info.size_file_extra<extraFieldBufferSize) uSizeRead = file_info.size_file_extra;
		else uSizeRead = extraFieldBufferSize;

		if (lSeek!=0) 
		{
			if (VFS_SEEK(s->file, VFS_TELL(s->file)+lSeek)) lSeek=0;
			else err=UNZ_ERRNO;
		}
		if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
			if (VFS_READ(s->file, extraField,(unsigned int)uSizeRead, &vfserr)!=uSizeRead) 
				err=UNZ_ERRNO;
		lSeek += file_info.size_file_extra - uSizeRead;
	}
	else lSeek+=file_info.size_file_extra; 

	
	if ((err==UNZ_OK) && (szComment))
	{
		unsigned long uSizeRead ;
		if (file_info.size_file_comment<commentBufferSize) {
			*(szComment+file_info.size_file_comment)='\0';
			uSizeRead = file_info.size_file_comment;
		} else uSizeRead = commentBufferSize;

		if (lSeek!=0)
		{
			if (VFS_SEEK(s->file, VFS_TELL(s->file)+lSeek)) lSeek=0;
			else err=UNZ_ERRNO;
		}
		if ((file_info.size_file_comment>0) && (commentBufferSize>0))
			if (VFS_READ(s->file, szComment,(unsigned int)uSizeRead, &vfserr)!=uSizeRead) 
				err=UNZ_ERRNO;
		lSeek+=file_info.size_file_comment - uSizeRead;
	} else lSeek+=file_info.size_file_comment;

	if ((err==UNZ_OK) && (pfile_info)) *pfile_info=file_info;

	if ((err==UNZ_OK) && (pfile_info_internal)) *pfile_info_internal=file_info_internal;

	return err;
}

// XXX: Grabbed from unzip.c from fte
int unzLocateFileMy (unzFile file, unsigned long num, unsigned long pos) {
	unz_s* s;	
	s = (unz_s *)file;
	s->pos_in_central_dir = pos;
	s->num_file = num;
	unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,&s->cur_file_info_internal,NULL,0,NULL,0,NULL,0);
	return 1;
}

/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
#define SIZEZIPLOCALHEADER (0x1e)
int unzlocal_CheckCurrentFileCoherencyHeader (unz_s *s, unsigned int *piSizeVar,
		unsigned long *poffset_local_extrafield,
		unsigned int *psize_local_extrafield) {
										unsigned long uMagic = 0,uData = 0,uFlags = 0;
										unsigned long size_filename = 0;
										unsigned long size_extra_field = 0;
	int err=UNZ_OK;

	*piSizeVar = 0;
	*poffset_local_extrafield = 0;
	*psize_local_extrafield = 0;

	if (!VFS_SEEK(s->file,s->cur_file_info_internal.offset_curfile + s->byte_before_the_zipfile)) return UNZ_ERRNO;


	if (err==UNZ_OK)
	{
		if (unzlocal_getLong(s->file,&uMagic) != UNZ_OK) err=UNZ_ERRNO;
		else if (uMagic!=0x04034b50) err=UNZ_BADZIPFILE;
	}

	if (unzlocal_getShort(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;

	if (unzlocal_getShort(s->file,&uFlags) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compression_method)) err=UNZ_BADZIPFILE;

    if ((err==UNZ_OK) && (s->cur_file_info.compression_method!=0) && (s->cur_file_info.compression_method!=Z_DEFLATED)) err=UNZ_BADZIPFILE;

	/* date/time */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;

	/* crc */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.crc) && ((uFlags & 8)==0)) err=UNZ_BADZIPFILE;

	/* size compr */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compressed_size) && ((uFlags & 8)==0)) err=UNZ_BADZIPFILE;

	 /* size uncompr */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.uncompressed_size) && ((uFlags & 8)==0)) err=UNZ_BADZIPFILE;


	if (unzlocal_getShort(s->file,&size_filename) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (size_filename!=s->cur_file_info.size_filename)) err=UNZ_BADZIPFILE;

	*piSizeVar += (unsigned int)size_filename;

	if (unzlocal_getShort(s->file,&size_extra_field) != UNZ_OK)	err=UNZ_ERRNO;
	*poffset_local_extrafield= s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + size_filename;
	*psize_local_extrafield = (unsigned int)size_extra_field;

	*piSizeVar += (unsigned int)size_extra_field;

	return err;
}


int unzGetCurrentFileUncompressedPos (unzFile file) {
//	int err=UNZ_OK;
//	int Store;
	unsigned int iSizeVar;
	unsigned int pos;
	unz_s* s;
//	file_in_zip_read_info_s* pfile_in_zip_read_info;
	unsigned long offset_local_extrafield;  /* offset of the local extra field */
	unsigned int  size_local_extrafield;    /* size of the local extra field */

	if (!file) return -1;
	s=(unz_s*)file;
	if (!s->current_file_ok) return -1;

    if (s->pfile_in_zip_read) unzCloseCurrentFile(file);

	if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK) return -1;



	if (s->cur_file_info.compression_method!=0) return -1;	

	//opens a file into the uncompressed steam.
	pos = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + iSizeVar;

	return pos;
}

#define BUFREADCOMMENT (0x400)

/*
  Locate the Central directory of a zipfile (at the end, just before
    the global comment)
*/
unsigned long unzlocal_SearchCentralDir(vfsfile_t *fin) {
	unsigned char* buf;
	unsigned long uSizeFile;
	unsigned long uBackRead;
	unsigned long uMaxBack=0xffff; /* maximum size of global comment */
	unsigned long uPosFound=0;
	vfserrno_t err;
	
	uSizeFile = VFS_GETLEN(fin);
	
	if (uMaxBack>uSizeFile) uMaxBack = uSizeFile;

	buf = (unsigned char*)Q_malloc(BUFREADCOMMENT+4);
	if (!buf) return 0;

	uBackRead = 4;
	while (uBackRead<uMaxBack) {
		unsigned long uReadSize,uReadPos ;
		int i;
		if (uBackRead+BUFREADCOMMENT>uMaxBack) uBackRead = uMaxBack;
		else uBackRead+=BUFREADCOMMENT;
		uReadPos = uSizeFile-uBackRead ;
		
		uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ? 
                     (BUFREADCOMMENT+4) : (uSizeFile-uReadPos);

		if (!VFS_SEEK(fin, uReadPos))
			break;

		if (VFS_READ(fin,buf,(unsigned int)uReadSize, &err) != uReadSize) break;

		for (i=(int)uReadSize-3; (i--)>0;)
			if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) && 
				((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06)) {
				uPosFound = uReadPos+i;
				break;
			}

		if (uPosFound!=0) break;
	}
	Q_free(buf);
	return uPosFound;
}
/*
 * FTE-FIXME: Fix this comment
  Open a Zip file. path contain the full pathname (by example,
     on a Windows NT computer "c:\\test\\zlib109.zip" or on an Unix computer
	 "zlib/zlib109.zip".
	 If the zipfile cannot be opened (file don't exist or in not valid), the
	   return value is NULL.
     Else, the return value is a unzFile Handle, usable with other function
	   of this unzip package.
*/


unzFile unzOpenVFS(vfsfile_t *fin) {
	unz_s us = {0};
	unz_s *s;
	unsigned long central_pos,uL;

	unsigned long number_disk = 0;          /* number of the current dist, used for 
								   spaning ZIP, unsupported, always 0*/
	unsigned long number_disk_with_CD = 0;  /* number the the disk with central dir, used
								   for spaning ZIP, unsupported, always 0*/
	unsigned long number_entry_CD = 0;      /* total number of entries in
	                               the central dir 
	                               (same than number_entry on nospan) */

	int err=UNZ_OK;


	if (!fin) return NULL;

	central_pos = unzlocal_SearchCentralDir(fin);
	if (!central_pos) err=UNZ_ERRNO;

	if (!VFS_SEEK(fin,central_pos)) err=UNZ_ERRNO;

	/* the signature, already checked */
	if (unzlocal_getLong(fin,&uL)!=UNZ_OK) err=UNZ_ERRNO;

	/* number of this disk */
	if (unzlocal_getShort(fin,&number_disk)!=UNZ_OK) err=UNZ_ERRNO;

	/* number of the disk with the start of the central directory */
	if (unzlocal_getShort(fin,&number_disk_with_CD)!=UNZ_OK) err=UNZ_ERRNO;

	/* total number of entries in the central dir on this disk */
	if (unzlocal_getShort(fin,&us.gi.number_entry)!=UNZ_OK) err=UNZ_ERRNO;

	/* total number of entries in the central dir */
	if (unzlocal_getShort(fin,&number_entry_CD)!=UNZ_OK) err=UNZ_ERRNO;

	if ((number_entry_CD!=us.gi.number_entry) || (number_disk_with_CD!=0) || (number_disk!=0)) err=UNZ_BADZIPFILE;

	/* size of the central directory */
	if (unzlocal_getLong(fin,&us.size_central_dir)!=UNZ_OK) err=UNZ_ERRNO;

	/* offset of start of central directory with respect to the 
	      starting disk number */
	if (unzlocal_getLong(fin,&us.offset_central_dir)!=UNZ_OK) err=UNZ_ERRNO;

	/* zipfile comment length */
	if (unzlocal_getShort(fin,&us.gi.size_comment)!=UNZ_OK) err=UNZ_ERRNO;

	if ((central_pos<us.offset_central_dir+us.size_central_dir) && (err==UNZ_OK)) err=UNZ_BADZIPFILE;

	if (err!=UNZ_OK) {
		VFS_CLOSE(fin);
		return NULL;
	}

	us.file=fin;
	us.byte_before_the_zipfile = central_pos - (us.offset_central_dir+us.size_central_dir);
	us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;
	

	s=(unz_s*)Q_malloc(sizeof(unz_s));
	*s=us;
	// FTE-FIXME: I should never know what a unz_s is!
	unzGoToFirstFile((unzFile)s);	
	return (unzFile)s;	
}



//=================================================================================================


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
		if (!Hash_GetInsensative(&filesystemhash, zip->files[i].name))
		{
			fs_hash_files++;
			Hash_AddInsensative(&filesystemhash, zip->files[i].name, &zip->files[i], &zip->files[i].bucket);
		}
		else
			fs_hash_dups++;
	}
}
static qbool FSZIP_FLocate(void *handle, flocation_t *loc, char *filename, void *hashedresult)
{
	packfile_t *pf = hashedresult;
	int i, len;
	zipfile_t	*zip = handle;

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
		len = pf->filelen;
		if (loc)
		{
			loc->index = pf - zip->files;
			strcpy(loc->rawname, zip->filename);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;

			unzLocateFileMy (zip->handle, loc->index, zip->files[loc->index].filepos);
			loc->offset = unzGetCurrentFileUncompressedPos(zip->handle);
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

	unzLocateFileMy (zip->handle, loc->index, zip->files[loc->index].filepos);

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

	for (num = 0; num<(int)zip->numfiles; num++)
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
static void *FSZIP_LoadZipFile (vfsfile_t *packhandle, char *desc)
{
	int i;

	zipfile_t *zip;
	packfile_t		*newfiles;

	unz_global_info	globalinf = {0};
	unz_file_info	file_info;

	zip = Q_malloc(sizeof(zipfile_t));
	strncpy(zip->filename, desc, sizeof(zip->filename));
	zip->handle = unzOpenVFS ((zip->raw = packhandle));
	if (!zip->handle)
	{
		Q_free(zip);
		// FTE-FIXME: Implement this
		// Con_TPrintf (TL_COULDNTOPENZIP, desc);
		return NULL;
	}

	unzGetGlobalInfo (zip->handle, &globalinf);

	zip->numfiles = globalinf.number_entry;

	zip->files = newfiles = Q_malloc (zip->numfiles * sizeof(packfile_t));
	for (i = 0; i < zip->numfiles; i++)
	{
		if (unzGetCurrentFileInfo (zip->handle, &file_info, newfiles[i].name, sizeof(newfiles[i].name), NULL, 0, NULL, 0) != UNZ_OK)
			Com_Printf("Zip Error\n");
		Q_strlwr(newfiles[i].name);
		newfiles[i].filelen = file_info.uncompressed_size;
		//newfiles[i].filepos = file_info.c_offset;
		if (unzGoToNextFile (zip->handle) != UNZ_OK)
			Com_Printf("Zip Error\n");
	}

	zip->references = 1;
	zip->currentfile = NULL;

	// FTE-FIXME: Implement Con_TPrintf
	// Con_TPrintf (TL_ADDEDZIPFILE, desc, zip->numfiles);
	return zip;
}

int FSZIP_GeneratePureCRC(void *handle, int seed, int crctype)
{
	zipfile_t *zip = handle;
	unz_file_info	file_info;

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

typedef struct {
	vfsfile_t funcs;

	vfsfile_t *defer;

	//in case we're forced away.
	zipfile_t *parent;
	qbool iscompressed;
	int pos;
	int length;	//try and optimise some things
	int index;
	int startpos;
} vfszip_t;

void VFSZIP_MakeActive(vfszip_t *vfsz)
{
	int i;
	char buffer[8192];	//must be power of two

	if ((vfszip_t*)vfsz->parent->currentfile == vfsz)
		return;	//already us
	if (vfsz->parent->currentfile)
		unzCloseCurrentFile(vfsz->parent->handle);

	unzLocateFileMy(vfsz->parent->handle, vfsz->index, vfsz->startpos);
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

int VFSZIP_ReadBytes (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err)
{
	int read;
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_READ(vfsz->defer, buffer, bytestoread, err);

	if (vfsz->iscompressed)
	{
		VFSZIP_MakeActive(vfsz);
		read = unzReadCurrentFile(vfsz->parent->handle, buffer, bytestoread);
	}
	else
	{
		if (vfsz->parent->currentfile != file)
		{
			unzCloseCurrentFile(vfsz->parent->handle);
			VFS_SEEK(vfsz->parent->raw, vfsz->pos+vfsz->startpos);
			vfsz->parent->currentfile = file;
		}
		read = VFS_READ(vfsz->parent->raw, buffer, bytestoread, err);
	}

	vfsz->pos += read;
	return read;
}
int VFSZIP_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestoread)
{
	Sys_Error("VFSZIP_WriteBytes: Not supported\n");
	return 0;
}

qbool VFSZIP_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_SEEK(vfsz->defer, pos);

	//This is *really* inefficient
	if (vfsz->parent->currentfile == file)
	{
		if (vfsz->iscompressed)
		{	//if they're going to seek on a file in a zip, let's just copy it out
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
		}

		unzCloseCurrentFile(vfsz->parent->handle);
		vfsz->parent->currentfile = NULL;	//make it not us

		if (vfsz->defer)
			return VFS_SEEK(vfsz->defer, pos);
	}



	if (pos < 0 || pos > vfsz->length)
		return false;
	vfsz->pos = pos;

	return true;
}

unsigned long VFSZIP_Tell (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->defer)
		return VFS_TELL(vfsz->defer);

	return vfsz->pos;
}

unsigned long VFSZIP_GetLen (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;
	return vfsz->length;
}

void VFSZIP_Close (struct vfsfile_s *file)
{
	vfszip_t *vfsz = (vfszip_t*)file;

	if (vfsz->parent->currentfile == file)
		vfsz->parent->currentfile = NULL;	//make it not us

	if (vfsz->defer)
		VFS_CLOSE(vfsz->defer);

	FSZIP_ClosePath(vfsz->parent);
	Q_free(vfsz);
}

vfsfile_t *FSZIP_OpenVFS(void *handle, flocation_t *loc, char *mode)
{
	int rawofs;
	zipfile_t *zip = handle;
	vfszip_t *vfsz;
	fs_filesize = -1;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfsz = Q_malloc(sizeof(vfszip_t));

	vfsz->parent = zip;
	vfsz->index = loc->index;
	vfsz->startpos = zip->files[loc->index].filepos;
	vfsz->length = loc->len;

	vfsz->funcs.Close = VFSZIP_Close;
	vfsz->funcs.GetLen = VFSZIP_GetLen;
	vfsz->funcs.ReadBytes = VFSZIP_ReadBytes;
	vfsz->funcs.Seek = VFSZIP_Seek;
	vfsz->funcs.Tell = VFSZIP_Tell;
	vfsz->funcs.WriteBytes = NULL;
	vfsz->funcs.seekingisabadplan = true;

	unzLocateFileMy(vfsz->parent->handle, vfsz->index, vfsz->startpos);
	rawofs = unzGetCurrentFileUncompressedPos(zip->handle);
	vfsz->iscompressed = rawofs<0;
	if (!vfsz->iscompressed)
	{
		vfsz->startpos = rawofs;
		VFS_SEEK(zip->raw, vfsz->startpos);
		vfsz->parent->currentfile = (vfsfile_t*)vfsz;
	}

	zip->references++;

	fs_filesize = VFS_GETLEN((vfsfile_t *)vfsz);

	return (vfsfile_t*)vfsz;
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

#endif
#endif // WITH_PK3


//=============================================================================

searchpath_t	*com_searchpaths;
searchpath_t	*com_purepaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs

static void COM_AddDataFiles(char *pathto, searchpath_t *search, char *extension, searchpathfuncs_t *funcs);

searchpath_t *COM_AddPathHandle(char *probablepath, searchpathfuncs_t *funcs, void *handle, qbool copyprotect, qbool istemporary, FS_Load_File_Types loadstuff)
{
	searchpath_t *search;

	search = (searchpath_t*)Q_malloc (sizeof(searchpath_t));
	search->copyprotected = copyprotect;
	search->istemporary = istemporary;
	search->handle = handle;
	search->funcs = funcs;

	search->next = com_searchpaths;
	com_searchpaths = search;

	com_fschanged = true;

	//add any data files too
	if (loadstuff & FS_LOAD_FILE_PAK)
		COM_AddDataFiles(probablepath, search, "pak", &packfilefuncs);//q1/hl/h2/q2
	//pk2s never existed.
#ifdef WITH_ZIP
#ifdef WITH_PK3
	if (loadstuff & FS_LOAD_FILE_PK3)
		COM_AddDataFiles(probablepath, search, "pk3", &zipfilefuncs);	//q3 + offspring
	if (loadstuff & FS_LOAD_FILE_PK4)
		COM_AddDataFiles(probablepath, search, "pk4", &zipfilefuncs);	//q4
	//we could easily add zip, but it's friendlier not to
#endif
#endif

#ifdef DOOMWADS
	if (loadstuff & FS_LOAD_FILE_DOOMWAD)
		COM_AddDataFiles(probablepath, search, "wad", &doomwadfilefuncs);	//q4
#endif

	return search;
}

/*
================
COM_filelength
================
*/
int COM_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}
int COM_FileOpenRead (char *path, FILE **hndl)
{
	FILE	*f;

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_filelength(f);
}

int COM_FileSize(char *path)
{
	int len;
	flocation_t loc;
	len = FS_FLocateFile(path, FSLFRT_LENGTH, &loc);
	return len;
}

/*
============
COM_Path_f
============
*/
void COM_Path_f (void)
{
	searchpath_t	*s;

	// FTE-FIXME: Need to implement this in Com_Printf
	// Con_TPrintf (TL_CURRENTSEARCHPATH);

	if (com_purepaths)
	{
		for (s=com_purepaths ; s ; s=s->nextpure)
		{
			s->funcs->PrintPath(s->handle);
		}
		Com_Printf ("----------\n");
	}


	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Com_Printf ("----------\n");

		s->funcs->PrintPath(s->handle);
	}
}


/*
============
COM_Dir_f
============
*/
static int COM_Dir_List(char *name, int size, void *parm)
{
	Com_Printf("%s  (%i)\n", name, size);
	return 1;
}

void COM_Dir_f (void)
{
	char match[MAX_QPATH];

	strncpy(match, Cmd_Argv(1), sizeof(match));
	if (Cmd_Argc()>2)
	{
		strncat(match, "/*.", sizeof(match)-1);
		match[sizeof(match)-1] = '\0';
		strncat(match, Cmd_Argv(2), sizeof(match)-1);
		match[sizeof(match)-1] = '\0';
	}
	else
		strncat(match, "/*", sizeof(match)-1);

	COM_EnumerateFiles(match, COM_Dir_List, NULL);
}

/*
============
COM_Locate_f
============
*/
void COM_Locate_f (void)
{
	flocation_t loc;
	if (FS_FLocateFile(Cmd_Argv(1), FSLFRT_LENGTH, &loc)>=0)
	{
		if (!*loc.rawname)
		{
			Com_Printf("File is compressed inside ");
			loc.search->funcs->PrintPath(loc.search->handle);
		}
		else
		{
			Com_Printf("Inside %s\n", loc.rawname);
			loc.search->funcs->PrintPath(loc.search->handle);
		}
	}
	else
		Com_Printf("Not found\n");
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
qbool COM_WriteFile (char *filename, void *data, int len)
{
	vfsfile_t *vfs;

	Sys_Printf ("COM_WriteFile: %s\n", filename);

	FS_CreatePath(filename, FS_GAME_OS);
	vfs = FS_OpenVFS(filename, "wb", FS_GAME_OS);
	if (vfs)
	{
		VFS_WRITE(vfs, data, len);
		VFS_CLOSE(vfs);
	} else {
		return false;
	}

	com_fschanged=true;
	return true;
}

FILE *COM_WriteFileOpen (char *filename)	//like fopen, but based around quake's paths.
{
	FILE	*f;
	char	name[MAX_OSPATH];

	sprintf (name, "%s/%s", com_gamedir, filename);

	COM_CreatePath(name);

	f = fopen (name, "wb");

	return f;
}


/*
============
COM_CreatePath

Only used for CopyFile and download
============
*/
void	COM_CreatePath (char *path)
{
	char	*ofs;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	*in, *out;
	int		remaining, count;
	char	buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = fopen(cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}

int fs_hash_dups;
int fs_hash_files;

void FS_FlushFSHash(void)
{
	if (filesystemhash.numbuckets)
	{
		int i;
		bucket_t *bucket, *next;

		for (i = 0; i < filesystemhash.numbuckets; i++)
		{
			bucket = filesystemhash.bucket[i];
			filesystemhash.bucket[i] = NULL;
			while(bucket)
			{
				next = bucket->next;
				if (bucket->keystring == (char*)(bucket+1))
					Q_free(bucket);
				bucket = next;
			}
		}
	}

	com_fschanged = true;
}

void FS_RebuildFSHash(void)
{
	searchpath_t	*search;
	if (!filesystemhash.numbuckets)
	{
		filesystemhash.numbuckets = 1024;
		filesystemhash.bucket = (bucket_t**)Q_malloc(Hash_BytesForBuckets(filesystemhash.numbuckets));
	}
	else
	{
		FS_FlushFSHash();
	}
	Hash_InitTable(&filesystemhash, filesystemhash.numbuckets, filesystemhash.bucket);

	fs_hash_dups = 0;
	fs_hash_files = 0;

	if (com_purepaths)
	{	//go for the pure paths first.
		for (search = com_purepaths; search; search = search->nextpure)
		{
			search->funcs->BuildHash(search->handle);
		}
	}
	for (search = com_searchpaths ; search ; search = search->next)
	{
		search->funcs->BuildHash(search->handle);
	}

	com_fschanged = false;

	Com_Printf("%i unique files, %i duplicates\n", fs_hash_files, fs_hash_dups);
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns -1 if couldn't find.
int FS_FLocateFile(char *filename, FSLF_ReturnType_e returntype, flocation_t *loc)
{
	int depth=0, len;
	searchpath_t	*search;

	void *pf;
//Com_Printf("Finding %s: ", filename);

 	if (com_fs_cache.value)
	{
		if (com_fschanged)
			FS_RebuildFSHash();
		pf = Hash_GetInsensative(&filesystemhash, filename);
		if (!pf)
			goto fail;
	}
	else
		pf = NULL;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			if (search->funcs->FindFile(search->handle, loc, filename, pf))
			{
				if (loc)
				{
					loc->search = search;
					len = loc->len;
				}
				else
					len = 0;
				goto out;
			}
			depth += (search->funcs != &osfilefuncs || returntype == FSLFRT_DEPTH_ANYPATH);
		}
	}

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (search->funcs->FindFile(search->handle, loc, filename, pf))
		{
			if (loc)
			{
				loc->search = search;
				len = loc->len;
			}
			else
				len = 1;
			goto out;
		}
		depth += (search->funcs != &osfilefuncs || returntype == FSLFRT_DEPTH_ANYPATH);
	}
	
fail:
	if (loc)
		loc->search = NULL;
	depth = 0x7fffffff;
	len = -1;
out:

/*	Debug printing removed
 *	if (len>=0)
	{
		if (loc)
			Com_Printf("Found %s:%i\n", loc->rawname, loc->len);
		else
			Com_Printf("Found %s\n", filename);
	}
	else
		Com_Printf("Failed\n");
*/
	if (returntype == FSLFRT_IFFOUND)
		return len != -1;
	else if (returntype == FSLFRT_LENGTH)
		return len;
	else
		return depth;
}


char *FS_GetPackHashes(char *buffer, int buffersize, qbool referencedonly)
{
	searchpath_t	*search;
	buffersize--;
	*buffer = 0;

	if (com_purepaths)
	{
		for (search = com_purepaths ; search ; search = search->nextpure)
		{
			strncat(buffer, va("%i ", search->crc_check), buffersize);
		}
		return buffer;
	}
	else
	{
		for (search = com_searchpaths ; search ; search = search->next)
		{
			if (!search->crc_check && search->funcs->GeneratePureCRC)
				search->crc_check = search->funcs->GeneratePureCRC(search->handle, 0, 0);
			if (search->crc_check)
			{
				strncat(buffer, va("%i ", search->crc_check), buffersize);
			}
		}
		return buffer;
	}
}
char *FS_GetPackNames(char *buffer, int buffersize, qbool referencedonly)
{
	return "";
}


//true if protection kicks in
qbool Sys_PathProtection(char *pattern)
{
	if (strchr(pattern, '\\'))
	{
		char *s;
		Com_Printf("Warning: \\ charactures in filename %s\n", pattern);
		while((s = strchr(pattern, '\\')))
			*s = '/';
	}

	if (strstr(pattern, ".."))
		Com_Printf("Error: '..' charactures in filename %s\n", pattern);
	else if (pattern[0] == '/')
		Com_Printf("Error: absolute path in filename %s\n", pattern);
	else if (strstr(pattern, ":")) //win32 drive seperator (or mac path seperator, but / works there and they're used to it)
		Com_Printf("Error: absolute path in filename %s\n", pattern);
	else
		return false;
	return true;
}

#ifdef WITH_ZIP
typedef struct {
	unsigned char ident1;
	unsigned char ident2;
	unsigned char cm;
	unsigned char flags;
	unsigned int mtime;
	unsigned char xflags;
	unsigned char os;
} gzheader_t;
#define sizeofgzheader_t 10

#define	GZ_FTEXT	1
#define	GZ_FHCRC	2
#define GZ_FEXTRA	4
#define GZ_FNAME	8
#define GZ_FCOMMENT	16
#define GZ_RESERVED (32|64|128)

#include <zlib.h>

vfsfile_t *FS_DecompressGZip(vfsfile_t *infile, gzheader_t *header)
{
	char inchar;
	unsigned short inshort;
	vfsfile_t *temp;
	vfserrno_t err;

	if (header->flags & GZ_RESERVED)
	{	//reserved bits should be 0
		//this is probably static, so it's not a gz. doh.
		VFS_SEEK(infile, 0);
		return infile;
	}

	if (header->flags & GZ_FEXTRA)
	{
		VFS_READ(infile, &inshort, sizeof(inshort), &err);
		inshort = LittleShort(inshort);
		VFS_SEEK(infile, VFS_TELL(infile) + inshort);
	}

	if (header->flags & GZ_FNAME)
	{
		Com_Printf("gzipped file name: ");
		do {
			if (VFS_READ(infile, &inchar, sizeof(inchar), &err) != 1)
				break;
			Com_Printf("%c", inchar);
		} while(inchar);
		Com_Printf("\n");
	}

	if (header->flags & GZ_FCOMMENT)
	{
		Com_Printf("gzipped file comment: ");
		do {
			if (VFS_READ(infile, &inchar, sizeof(inchar), &err) != 1)
				break;
			Com_Printf("%c", inchar);
		} while(inchar);
		Com_Printf("\n");
	}

	if (header->flags & GZ_FHCRC)
	{
		VFS_READ(infile, &inshort, sizeof(inshort), &err);
	}



	temp = FS_OpenTemp();
	if (!temp)
	{
		VFS_SEEK(infile, 0);	//doh
		return infile;
	}


	{
		Bytef inbuffer[16384];
		Bytef outbuffer[16384];
		int ret;

		z_stream strm = {
			inbuffer,
			0,
			0,

			outbuffer,
			sizeof(outbuffer),
			0,

			NULL,
			NULL,

			NULL,
			NULL,
			NULL,

			Z_UNKNOWN,
			0,
			0
		};

		strm.avail_in = VFS_READ(infile, inbuffer, sizeof(inbuffer), &err);
		strm.next_in = inbuffer;

		inflateInit2(&strm, -MAX_WBITS);

		while ((ret=inflate(&strm, Z_SYNC_FLUSH)) != Z_STREAM_END)
		{
			if (strm.avail_in == 0 || strm.avail_out == 0)
			{
				if (strm.avail_in == 0)
				{
					strm.avail_in = VFS_READ(infile, inbuffer, sizeof(inbuffer), &err);
					strm.next_in = inbuffer;
				}

				if (strm.avail_out == 0)
				{
					strm.next_out = outbuffer;
					VFS_WRITE(temp, outbuffer, strm.total_out);
					strm.total_out = 0;
					strm.avail_out = sizeof(outbuffer);
				}
				continue;
			}

			//doh, it terminated for no reason
			inflateEnd(&strm);
			if (ret != Z_STREAM_END)
			{
				Com_Printf("Couldn't decompress gz file\n");
				VFS_CLOSE(temp);
				VFS_CLOSE(infile);
				return NULL;
			}
		}
		//we got to the end
		VFS_WRITE(temp, outbuffer, strm.total_out);

		inflateEnd(&strm);

		VFS_SEEK(temp, 0);
	}
	VFS_CLOSE(infile);

	return temp;
}
#endif

vfsfile_t *VFS_Filter(char *filename, vfsfile_t *handle)
{
	vfserrno_t err;
//	char *ext;

	if (!handle || handle->WriteBytes || handle->seekingisabadplan)	//only on readonly files
		return handle;
//	ext = COM_FileExtension (filename);
#ifdef WITH_ZIP
//	if (!stricmp(ext, ".gz"))
	{
		gzheader_t gzh;
		if (VFS_READ(handle, &gzh, sizeofgzheader_t, &err) == sizeofgzheader_t)
		{
			if (gzh.ident1 == 0x1f && gzh.ident2 == 0x8b && gzh.cm == 8)
			{	//it'll do
				return FS_DecompressGZip(handle, &gzh);
			}
		}
		VFS_SEEK(handle, 0);
	}
#endif
	return handle;
}

/*
================
FS_OpenVFS

This should be how all files are opened, will either give a valid vfsfile_t
or NULL. We first check the home directory for the file first, if not found 
we try the basedir.
================
*/
// XXX: This is very similar to FS_OpenVFS in fs.c
// FTE-FIXME: Clean up this function to reduce the relativeto options 
vfsfile_t *FS_OpenVFS(char *filename, char *mode, relativeto_t relativeto)
{
	char fullname[MAX_OSPATH];
	flocation_t loc;
	vfsfile_t *vfs;


	// FTE-FIXME: Need to find the extension of the file so we know what function to use to open it

	//blanket-bans
	//if (Sys_PathProtection(filename) )
	//	return NULL;

	if (strcmp(mode, "rb"))
		if (strcmp(mode, "wb"))
			if (strcmp(mode, "ab"))
				return NULL; //urm, unable to write/append

	switch (relativeto)
	{
	case FS_NONE_OS: 	//OS access only, no paks, open file as is
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}

		snprintf(fullname, sizeof(fullname), "%s", filename);
		return VFSOS_Open(fullname, mode);

	case FS_GAME_OS:	//OS access only, no paks
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homedir, com_gamedirfile, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs) {
				fs_filesize = VFS_GETLEN(vfs);
				return vfs;
			}
		}

		snprintf(fullname, sizeof(fullname), "%s%s/%s", com_basedir, com_gamedirfile, filename);
		return VFSOS_Open(fullname, mode);

/* XXX: Removed as we don't really use this
 *	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_homedir, filename);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_basedir, filename);
		break;
 */

	case FS_BASE:
//		if (*com_homedir)
//		{
//			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, filename);
//			vfs = VFSOS_Open(fullname, mode);
//			if (vfs)
//				return vfs;
//		}
		snprintf(fullname, sizeof(fullname), "%s%s", com_basedir, filename);
		return VFSOS_Open(fullname, mode);

/* XXX: Removed as we don't really use this
 * case FS_CONFIGONLY:
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s.ezquake/%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%sezquake/%s", com_basedir, filename);
		return VFSOS_Open(fullname, mode);
 */
	case FS_HOME:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homedir, com_gamedirfile, filename);
		else
			return NULL;
		return VFSOS_Open(fullname, mode);

	case FS_PAK:
		snprintf(fullname, sizeof(fullname), "%s%s/%s", com_basedir, com_gamedirfile, filename);
		break;

	case FS_ANY:
		vfs = FS_OpenVFS(filename, mode, FS_HOME);
		if (vfs)
			return vfs;

		vfs = FS_OpenVFS(filename, mode, FS_GAME_OS);
		if (vfs)
			return vfs;

		return FS_OpenVFS(filename, mode, FS_PAK);

	default:
		Sys_Error("FS_OpenVFS: Bad relative path (%i)", relativeto);
		break;
	}

	FS_FLocateFile(filename, FSLFRT_IFFOUND, &loc);

	if (loc.search)
	{
		com_file_copyprotected = loc.search->copyprotected;
		return VFS_Filter(filename, loc.search->funcs->OpenVFS(loc.search->handle, &loc, mode));
	}

	//if we're meant to be writing, best write to it.
	if (strchr(mode , 'w') || strchr(mode , 'a'))
		return VFSOS_Open(fullname, mode);
	return NULL;
}

int FS_Rename2(char *oldf, char *newf, relativeto_t oldrelativeto, relativeto_t newrelativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	switch (oldrelativeto)
	{
	case FS_GAME_OS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_homedir, com_gamedirfile);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_basedir, com_gamedirfile);
		break;
/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_basedir);
		break;
 */

	case FS_BASE:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_basedir);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	switch (newrelativeto)
	{
	case FS_GAME_OS:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%s%s/", com_homedir, com_gamedirfile);
		else
			snprintf(newfullname, sizeof(newfullname), "%s%s/", com_basedir, com_gamedirfile);
		break;

/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%sqw/skins/", com_homedir);
		else
			snprintf(newfullname, sizeof(newfullname), "%sqw/skins/", com_basedir);
		break;
 */
	case FS_BASE:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%s", com_homedir);
		else
			snprintf(newfullname, sizeof(newfullname), "%s", com_basedir);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	strncat(oldfullname, oldf, sizeof(oldfullname));
	strncat(newfullname, newf, sizeof(newfullname));

	FS_CreatePath(newf, newrelativeto);
	return rename(oldfullname, newfullname);
}
int FS_Rename(char *oldf, char *newf, relativeto_t relativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	switch (relativeto)
	{
	case FS_GAME_OS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_homedir, com_gamedirfile);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_basedir, com_gamedirfile);
		break;

/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%sqw/skins/", com_basedir);
		break;
 */

	case FS_BASE:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_homedir);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s", com_basedir);
		break;

	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	strncpy(newfullname, oldfullname, sizeof(newfullname));
	strncat(oldfullname, oldf, sizeof(oldfullname));
	strncat(newfullname, newf, sizeof(newfullname));

	return rename(oldfullname, newfullname);
}

int FS_Remove(char *fname, int relativeto)
{
	char fullname[MAX_OSPATH];

	switch (relativeto)
	{
	case FS_GAME_OS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_homedir, com_gamedirfile, fname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_basedir, com_gamedirfile, fname);
		break;

/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_homedir, fname);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_basedir, fname);
		break;
 */
	case FS_BASE:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, fname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s", com_basedir, fname);
		break;
	default:
		Sys_Error("FS_Rename case not handled\n");
	}

	return unlink (fullname);
}

void FS_CreatePath(char *pname, int relativeto)
{
	char fullname[MAX_OSPATH];
	switch (relativeto)
	{
	case FS_NONE_OS:
		snprintf(fullname, sizeof(fullname), "%s", pname);
		break;

	case FS_GAME_OS:
		snprintf(fullname, sizeof(fullname), "%s/%s", com_gamedir, pname);
		break;

	case FS_BASE:
	case FS_ANY:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s", com_basedir, pname);
		break;
/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_basedir, pname);
		break;
	case FS_CONFIGONLY:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%sfte/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%sfte/%s", com_basedir, pname);
		break;*/
	default:
		Sys_Error("FS_CreatePath: Bad relative path (%i)", relativeto);
		break;
	}
	COM_CreatePath(fullname);
}

qbool FS_WriteFile (char *filename, void *data, int len, int relativeto)
{
	vfsfile_t *f;
	FS_CreatePath(filename, relativeto);
	f = FS_OpenVFS(filename, "wb", relativeto);
	if (!f)
		return false;
	VFS_WRITE(f, data, len);
	VFS_CLOSE(f);

	return true;
}

void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm)
{
	searchpath_t    *search;
	for (search = com_searchpaths; search ; search = search->next)
	{
	// is the element a pak file?
		if (!search->funcs->EnumerateFiles(search->handle, match, func, parm))
			break;
	}
}

void COM_FlushTempoaryPacks(void)
{
	searchpath_t *next;
	while (com_searchpaths && com_searchpaths->istemporary)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;

		com_fschanged = true;
	}
}

typedef struct {
	searchpathfuncs_t *funcs;
	searchpath_t *parentpath;
	char *parentdesc;
} wildpaks_t;

static int COM_AddWildDataFiles (char *descriptor, int size, void *vparam)
{
	wildpaks_t *param = vparam;
	vfsfile_t *vfs;
	searchpathfuncs_t *funcs = param->funcs;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	flocation_t loc;

	sprintf (pakfile, "%s%s", param->parentdesc, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != funcs)
			continue;
		if (!strcasecmp((char*)search->handle, pakfile))	//assumption: first member of structure is a char array
			return true; //already loaded (base paths?)
	}

	search = param->parentpath;

	if (!search->funcs->FindFile(search->handle, &loc, descriptor, NULL))
		return true;	//not found..
	vfs = search->funcs->OpenVFS(search->handle, &loc, "rb");
	pak = funcs->OpenNew (vfs, pakfile);
	if (!pak)
		return true;

	sprintf (pakfile, "%s%s/", param->parentdesc, descriptor);
	COM_AddPathHandle(pakfile, funcs, pak, true, false, FS_LOAD_FILE_ALL);

	return true;
}

static void COM_AddDataFiles(char *pathto, searchpath_t *search, char *extension, searchpathfuncs_t *funcs)
{
	//search is the parent
	int				i;
	void			*handle;
	char			pakfile[MAX_OSPATH];
	vfsfile_t *vfs;
	flocation_t loc;
	wildpaks_t wp;

	for (i=0 ; ; i++)
	{
		snprintf (pakfile, sizeof(pakfile), "pak%i.%s", i, extension);
		if (!search->funcs->FindFile(search->handle, &loc, pakfile, NULL))
			break;	//not found..
		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s", pathto, i, extension);
		vfs = search->funcs->OpenVFS(search->handle, &loc, "r");
		if (!vfs)
			break;
		Com_Printf("Opened %s\n", pakfile);
		handle = funcs->OpenNew (vfs, pakfile);
		if (!handle)
			break;
		snprintf (pakfile, sizeof(pakfile), "%spak%i.%s/", pathto, i, extension);
		COM_AddPathHandle(pakfile, funcs, handle, true, false, FS_LOAD_FILE_ALL);
	}

	sprintf (pakfile, "*.%s", extension);
	wp.funcs = funcs;
	wp.parentdesc = pathto;
	wp.parentpath = search;
	search->funcs->EnumerateFiles(search->handle, pakfile, COM_AddWildDataFiles, &wp);
}

void COM_RefreshFSCache_f(void)
{
	com_fschanged=true;
}

void COM_FlushFSCache(void)
{
	if (com_fs_cache.value != 2)
		com_fschanged=true;
}
/*
================
FS_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
// XXX: This seems very similar to FS_AddUserDirectory, but AddUserDirectory 
// has some modifications
void FS_AddGameDirectory (char *dir, FS_Load_File_Types loadstuff)
{
	searchpath_t	*search;

	char			*p;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(com_gamedirfile, ++p);
	else
		strcpy(com_gamedirfile, dir);
	strcpy (com_gamedir, dir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != &osfilefuncs)
			continue;
		if (!strcasecmp(search->handle, com_gamedir))
			return; //already loaded (base paths?)
	}

//
// add the directory to the search path
//

	p = Q_malloc(strlen(dir)+1);
	strcpy(p, dir);
	COM_AddPathHandle(va("%s/", dir), &osfilefuncs, p, false, false, loadstuff);
}

char *COM_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return com_gamedir;

	prev = com_gamedir;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->funcs != &osfilefuncs)
			continue;

		if (prevpath == prev)
			return s->handle;
		prev = s->handle;
	}

	return NULL;
}

#ifndef CLIENTONLY
char *COM_GetPathInfo (int i, int *crc)
{
#ifdef WEBSERVER
	extern cvar_t httpserver;
#endif

	searchpath_t	*s;
	static char name[MAX_OSPATH];
	char			*protocol;

	for (s=com_searchpaths ; s ; s=s->next)
	{
		i--;
		if (!i)
			break;
	}
	if (i)	//too high.
		return NULL;

#ifdef WEBSERVER
	if (httpserver.value)
		protocol = va("http://%s/", NET_AdrToString (net_local_sv_ipadr));
	else
#endif
		protocol = "qw://";

	*crc = 0;//s->crc;
	strcpy(name, "FIXME");
//	Q_strncpyz(name, va("%s%s", protocol, COM_SkipPath(s->filename)), sizeof(name));
	return name;
}
#endif


/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
// XXX: This is pretty much FS_Gamedir, this could be removed and replaced with FS_Gamedir
void COM_Gamedir (char *dir)
{
	searchpath_t	*next;

	if (!*dir || strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		// FTE-FIXME: Implement this with Com_Printf
		// Con_TPrintf (TL_GAMEDIRAINTPATH);
		return;
	}

	if (!strcmp(com_gamedirfile, dir))
		return;		// still the same

	FS_ForceToPure(NULL, NULL, 0);

#ifndef SERVERONLY
// FTE-FIXME: Add this function
//Host_WriteConfiguration();	//before we change anything.
#endif

	strcpy (com_gamedirfile, dir);

#ifndef CLIENTONLY
	// FTE-FIXME: 'server_t' has no member named 'gamedirchanged'
	// FTE-FIXME: Find out what gamedirchanged does in fte
	//sv.gamedirchanged = true;
#endif
#ifndef SERVERONLY
	// FTE-FIXME: 'clientState_t' has no member named 'gamedirchanged'
	// FTE-FIXME: Find out what gamedirchanged does in fte
	//cl.gamedirchanged = true;
#endif

	FS_FlushFSHash();

	//
	// free up any current game dir info
	//
	while (com_searchpaths != com_base_searchpaths)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;

	//
	// flush all data, so it will be forced to reload
	//
	Cache_Flush ();

	FS_AddGameDirectory(va("%s%s", com_basedir, dir), FS_LOAD_FILE_ALL);
	if (*com_homedir)
		FS_AddGameDirectory(va("%s%s", com_homedir, dir), FS_LOAD_FILE_ALL);


#ifndef SERVERONLY
	{
		char	fn[MAX_OSPATH];
		FILE *f;

//		if (qrenderer)	//only do this if we have already started the renderer
//			Cbuf_InsertText("vid_restart\n", RESTRICT_LOCAL);

		sprintf(fn, "%s/%s", com_gamedir, "config.cfg");
		if ((f = fopen(fn, "r")) != NULL)
		{
			fclose(f);
//			FTE-FIXME: find what RESTRICT_LOCAL refers to
//			FTE-FIXME: Change this to an ezquake version
//			Cbuf_InsertText("cl_warncmd 0\n"
//							"exec config.cfg\n"
//							"exec fte.cfg\n"
//							"exec frontend.cfg\n"
//							"cl_warncmd 1\n", RESTRICT_LOCAL, false);
		}
	}

// FTE-FIXME: Does ezquake have Q3SHADERS??? i dont think so
#ifdef Q3SHADERS
	{
		extern void Shader_Init(void);
		Shader_Init();	// FIXME!
	}
#endif

// FTE-FIXME: Add Validation_FlushFileList
// Validation_FlushFileList();	//prevent previous hacks from making a difference.

	// FIXME: load new palette, if different cause a vid_restart.

#endif
}

#define NEXCFG "set sv_maxairspeed \"400\"\nset sv_mintic \"0.01\"\ncl_nolerp 0\n"

// FTE-FIXME: These shouldn't be needed, these are only for extra games 
//        and searching of directories
typedef struct {
	char *gamename;	//sent to the master server when this is the current gamemode.
	char *exename;	//used if the exe name contains this
	char *argname;	//used if this was used as a parameter.
	char *auniquefile;	//used if this file is relative from the gamedir

	char *customexec;

	char *dir1;
	char *dir2;
	char *dir3;
	char *dir4;
} gamemode_info_t;
gamemode_info_t gamemode_info[] = {
//note that there is no basic 'fte' gamemode, this is because we aim for network compatability. Darkplaces-Quake is the closest we get.
//this is to avoid having too many gamemodes anyway.

// FTE-FIXME: These shouldn't be needed, these are only for extra games 
//        and searching of directories
//rogue/hipnotic have no special files - the detection conflicts and stops us from running regular quake
	{"Darkplaces-Quake",	"darkplaces",	"-quake",		"id1/pak0.pak",		NULL,	"id1",		"qw",				"fte"},
	{"Darkplaces-Hipnotic",	"hipnotic",		"-hipnotic",	NULL,				NULL,	"id1",		"qw",	"hipnotic",	"fte"},
	{"Darkplaces-Rogue",	"rogue",		"-rogue",		NULL,				NULL,	"id1",		"qw",	"rogue",	"fte"},
	{"Nexuiz",				"nexuiz",		"-nexuiz",		"nexuiz.exe",		NEXCFG,	"id1",		"qw",	"data",		"fte"},

	//supported commercial mods (some are currently only partially supported)
	{"FTE-Hexen2",			"hexen",		"-hexen2",		"data1/pak0.pak",	NULL,	"data1",						"fte"},
	{"FTE-Quake2",			"q2",			"-q2",			"baseq2/pak0.pak",	NULL,	"baseq2",						"fte"},
	{"FTE-Quake3",			"q3",			"-q3",			"baseq3/pak0.pk3",	NULL,	"baseq3",						"fte"},

	{"FTE-JK2",				"jk2",			"-jk2",			"base/assets0.pk3",	NULL,	"base",							"fte"},

	{NULL}
};

//space-seperate pk3 names followed by space-seperated crcs
//note that we'll need to reorder and filter out files that don't match the crc.
void FS_ForceToPure(char *str, char *crcs, int seed)
{
	//pure files are more important than non-pure.

	searchpath_t *sp;
	searchpath_t *lastpure = NULL;
	int crc;

	if (!str)
	{	//pure isn't in use.
		com_purepaths = NULL;
		FS_FlushFSHash();
		return;
	}

	for (sp = com_searchpaths; sp; sp = sp->next)
	{
		if (sp->funcs->GeneratePureCRC)
		{
			sp->nextpure = (void*)0x1;
			sp->crc_check = sp->funcs->GeneratePureCRC(sp->handle, seed, 0);
			sp->crc_reply = sp->funcs->GeneratePureCRC(sp->handle, seed, 1);
		}
		else
		{
			sp->crc_check = 0;
			sp->crc_reply = 0;
		}
	}

	while(crcs)
	{
		crcs = COM_Parse(crcs);
		crc = atoi(com_token);

		if (!crc)
			continue;

		for (sp = com_searchpaths; sp; sp = sp->next)
		{
			if (sp->nextpure == (void*)0x1)	//don't add twice.
				if (sp->crc_check == crc)
				{
					if (lastpure)
						lastpure->nextpure = sp;
					else
						com_purepaths = sp;
					sp->nextpure = NULL;
					lastpure = sp;
					break;
				}
		}
		if (!sp)
			Com_Printf("Pure crc %i wasn't found\n", crc);
	}

/* don't add any extras.
	for (sp = com_searchpaths; sp; sp = sp->next)
	{
		if (sp->nextpure == (void*)0x1)
		{
			if (lastpure)
				lastpure->nextpure = sp;
			sp->nextpure = NULL;
			lastpure = sp;
		}
	}
*/

	FS_FlushFSHash();
}

char *FS_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum)
{
	flocation_t loc;
	int numpaks = 0;
	searchpath_t *sp;

	FS_FLocateFile("vm/cgame.qvm", FSLFRT_LENGTH, &loc);
	strncat(buffer, va("%i ", loc.search->crc_reply), maxlen);
	basechecksum ^= loc.search->crc_reply;

	FS_FLocateFile("vm/ui.qvm", FSLFRT_LENGTH, &loc);
	strncat(buffer, va("%i ", loc.search->crc_reply), maxlen);
	basechecksum ^= loc.search->crc_reply;

	strncat(buffer, "@ ", maxlen);

	for (sp = com_purepaths; sp; sp = sp->nextpure)
	{
		if (sp->crc_reply)
		{
			strncat(buffer, va("%i ", sp->crc_reply), maxlen);
			basechecksum ^= sp->crc_reply;
			numpaks++;
		}
	}

	basechecksum ^= numpaks;
	strncat(buffer, va("%i ", basechecksum), maxlen);

	return buffer;
}

/*
================
FS_ReloadPackFiles
================

Called when the client has downloaded a new pak/pk3 file
*/
void FS_ReloadPackFilesFlags(FS_Load_File_Types reloadflags)
{
	searchpath_t	*oldpaths;
	searchpath_t	*oldbase;
	searchpath_t	*next;


	//a lame way to fix pure paks
#ifndef SERVERONLY
	if (cls.state)
	{
		CL_Disconnect_f();
		CL_Reconnect_f();
	}
#endif

	FS_FlushFSHash();

	oldpaths = com_searchpaths;
	com_searchpaths = NULL;
	com_purepaths = NULL;
	oldbase = com_base_searchpaths;
	com_base_searchpaths = NULL;

	//invert the order
	next = NULL;
	while(oldpaths)
	{
		oldpaths->nextpure = next;
		next = oldpaths;
		oldpaths = oldpaths->next;
	}
	oldpaths = next;

	com_base_searchpaths = NULL;

	while(oldpaths)
	{
		next = oldpaths->nextpure;

		if (oldbase == oldpaths)
			com_base_searchpaths = com_searchpaths;

		if (oldpaths->funcs == &osfilefuncs)
			FS_AddGameDirectory(oldpaths->handle, reloadflags);

		oldpaths->funcs->ClosePath(oldpaths->handle);
		Q_free(oldpaths);
		oldpaths = next;
	}

	if (!com_base_searchpaths)
		com_base_searchpaths = com_searchpaths;
}

void FS_UnloadPackFiles(void)
{
	FS_ReloadPackFilesFlags(1);
}

void FS_ReloadPackFiles(void)
{
	FS_ReloadPackFilesFlags(FS_LOAD_FILE_ALL);
}

void FS_ReloadPackFiles_f(void)
{
	if (atoi(Cmd_Argv(1)))
		FS_ReloadPackFilesFlags(atoi(Cmd_Argv(1)));
	else
		FS_ReloadPackFilesFlags(FS_LOAD_FILE_ALL);
}

/*
================
COM_InitFilesystem
================
*/
// XXX: This seems to be the same as FS_InitFilesystem && FS_InitFilesystemEx
void COM_InitFilesystem (void)
{
	FILE *f;
	int		i;

	char *ev;


	int gamenum=-1;

	Cmd_AddCommand("fs_restart", FS_ReloadPackFiles_f);

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1) {
		strcpy(com_basedir, com_argv[i+1]);
	} else {
		// FTE-FIXME: Find what host_parms is for ezquake
		//strcpy(com_basedir, host_parms.basedir);
	}

	if (*com_basedir)
	{
		if (com_basedir[strlen(com_basedir)-1] == '\\')
			com_basedir[strlen(com_basedir)-1] = '/';
		else if (com_basedir[strlen(com_basedir)-1] != '/')
		{
			com_basedir[strlen(com_basedir)+1] = '\0';
			com_basedir[strlen(com_basedir)] = '/';
		}
	}



	// FTE-FIXME: com_gamename??? i think we can ignore this
	//Cvar_Register(&com_gamename, "evil hacks");
	//
	// XXX: TODO: FTE-FIXME: Remove thie whole game name stuff
	//identify the game from a telling file
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (!gamemode_info[i].auniquefile)
			continue;	//no more
		f = fopen(va("%s%s", com_basedir, gamemode_info[i].auniquefile), "rb");
		if (f)
		{
			fclose(f);
			gamenum = i;
			break;
		}
	}
	//use the game based on an exe name over the filesystem one.
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (strstr(com_argv[0], gamemode_info[i].exename))
			gamenum = i;
	}
	//use the game based on an parameter over all else.
	for (i = 0; gamemode_info[i].gamename; i++)
	{
		if (COM_CheckParm(gamemode_info[i].argname))
			gamenum = i;
	}

	if (gamenum<0)
	{
		for (i = 0; gamemode_info[i].gamename; i++)
		{
			if (!strcmp(gamemode_info[i].argname, "-quake"))
				gamenum = i;
		}
	}
	// FTE-FIXME: I think we can ignore this
	//Cvar_Set(&com_gamename, gamemode_info[gamenum].gamename);

	//if (gamemode_info[gamenum].customexec)
	//	Cbuf_AddText(gamemode_info[gamenum].customexec, RESTRICT_LOCAL);

#ifdef _WIN32
	{	//win32 sucks.
		ev = getenv("HOMEDRIVE");
		if (ev)
			strcpy(com_homedir, ev);
		else
			strcpy(com_homedir, "");
		ev = getenv("HOMEPATH");
		if (ev)
			strcat(com_homedir, ev);
		else
			strcat(com_homedir, "/");
	}
#else
	//yay for unix!.
	ev = getenv("HOME");
	if (ev)
		strncpy(com_homedir, ev, sizeof(com_homedir));
	else
		*com_homedir = '\0';
#endif

	if (!COM_CheckParm("-usehome"))
		*com_homedir = '\0';

	if (COM_CheckParm("-nohome"))
		*com_homedir = '\0';

	if (*com_homedir)
	{
		strcat(com_homedir, "/.fte/");
		Com_Printf("Using home directory \"%s\"\n", com_homedir);
	}

//
// start up with id1 by default
//
	i = COM_CheckParm ("-basegame");
	if (i && i < com_argc-1)
	{
		do	//use multiple -basegames
		{
			FS_AddGameDirectory (va("%s%s", com_basedir, com_argv[i+1]), FS_LOAD_FILE_ALL);

			i = COM_CheckParmOffset("-basegame", i);
		}
		while (i && i < com_argc-1);
	}
	else
	{
		if (gamemode_info[gamenum].dir1)
			FS_AddGameDirectory (va("%s%s", com_basedir, gamemode_info[gamenum].dir1), FS_LOAD_FILE_ALL);
		if (gamemode_info[gamenum].dir2)
			FS_AddGameDirectory (va("%s%s", com_basedir, gamemode_info[gamenum].dir2), FS_LOAD_FILE_ALL);
		if (gamemode_info[gamenum].dir3)
			FS_AddGameDirectory (va("%s%s", com_basedir, gamemode_info[gamenum].dir3), FS_LOAD_FILE_ALL);
		if (gamemode_info[gamenum].dir4)
			FS_AddGameDirectory (va("%s%s", com_basedir, gamemode_info[gamenum].dir4), FS_LOAD_FILE_ALL);
	}

	if (*com_homedir)
		FS_AddGameDirectory (va("%sfte", com_homedir), FS_LOAD_FILE_ALL);

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	i = COM_CheckParm ("-game");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		FS_AddGameDirectory (va("%s%s", com_basedir, com_argv[i+1]), FS_LOAD_FILE_ALL);

#ifndef CLIENTONLY
		Info_SetValueForStarKey (svs.info, "*gamedir", com_argv[i+1], MAX_SERVERINFO_STRING);
#endif
	}
}

#endif /* WITH_FTE_VFS */
