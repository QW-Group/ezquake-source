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

$Id: fs.c,v 1.30 2007-09-28 05:25:10 dkure Exp $
*/

#include "quakedef.h"
#include "common.h"
#include "hash.h"
#include "fs.h"
#include "vfs.h"

//==================================================================================================
#ifdef WITH_FTE_VFS

#include "q_shared.h"

#ifndef CLIENTONLY
#include "server.h"
#endif // CLIENTONLY

// To include pak3 support add this define
//#define WITH_PK3

hashtable_t filesystemhash;
static qbool com_fschanged = true;

// VFS-FIXME: Give this a better name
cvar_t com_fs_cache = {"com_fs_cache", "1"};

// VFS-FIXME: Move this somewhere better
void COM_Path_f (void);

typedef enum {
	FSLFRT_IFFOUND,
	FSLFRT_LENGTH,
	FSLFRT_DEPTH_OSONLY,
	FSLFRT_DEPTH_ANYPATH
} FSLF_ReturnType_e;

typedef enum {
	FS_LOAD_FILE_PAK = 2,
	FS_LOAD_FILE_PK3 = 4,
	FS_LOAD_FILE_PK4 = 8,
	FS_LOAD_FILE_DOOMWAD = 16,
	FS_LOAD_FILE_ALL = FS_LOAD_FILE_PAK | FS_LOAD_FILE_PK3 
		| FS_LOAD_FILE_PK4 | FS_LOAD_FILE_DOOMWAD,
} FS_Load_File_Types;

vfsfile_t *FS_OpenVFSLoc(flocation_t *loc, char *mode);
void FS_CreatePath(char *pname, int relativeto);
void FS_ForceToPure(char *str, char *crcs, int seed);
int FS_FLocateFile(char *filename, FSLF_ReturnType_e returntype, flocation_t *loc); 
void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm);
int COM_FileOpenRead (char *path, FILE **hndl);
void FS_ReloadPackFiles_f(void);
qbool FS_WriteFile (char *filename, void *data, int len, int relativeto);
void FS_FlushFSHash(void);
void FS_AddHomeDirectory(char *dir, FS_Load_File_Types loadstuff);

static vfsfile_t *VFS_Filter(char *filename, vfsfile_t *handle);

// VFS-FIXME: Debug file for trying to open files
static void FS_OpenFile_f(void);

//VFS-FIXME: This seems like com_homedir
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

// VFS-FIXME: D-Kure This removes a sanity check
int COM_FCreateFile (char *filename, FILE **file, char *path, char *mode)
{
#ifndef WITH_FTE_VFS
	searchpath_t *search;
#endif
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
#endif // WITH_FTE_VFS


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

// VFS-FIXME: D-Kure: This function will be removed once we have the VFS layer
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
	// VFS-FIXME: This only checks the pak files, not the base dir's
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
	VFS_SEEK(packhandle, header.dirofs, SEEK_SET);
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
#ifndef WITH_FTE_VFS // VFS-FIXME: D-Kure: This should do something.....
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
	snprintf (pakfile, sizeof (pakfile), "%s/pak.lst", dir);
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
			snprintf (pakfile, sizeof (pakfile), "%s/%s", dir, userpak);
			FS_AddPak(pakfile);
		}
		fclose(f);
	}
	// add userdir.pak
	if (UserdirSet) {
		snprintf (pakfile, sizeof (pakfile), "%s/%s.pak", dir, userdirfile);
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
	// VFS-FIXME: D-Kure: What is this search->filename & pack used for??
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
	FS_FlushFSHash();

	// free up any current game dir info
	while (com_searchpaths != com_base_searchpaths)
	{
		com_searchpaths->funcs->ClosePath(com_searchpaths->handle);
		next = com_searchpaths->next;
		Q_free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged=true;
#endif

	// Flush all data, so it will be forced to reload.
	Cache_Flush ();

	snprintf (com_gamedir, sizeof (com_gamedir), "%s/%s", com_basedir, dir);

#ifndef WITH_FTE_VFS
	if (strcmp(dir, "id1") && strcmp(dir, "qw") && strcmp(dir, "ezquake"))
	{
		FS_AddGameDirectory(com_basedir, dir);
	}
#else
	FS_AddGameDirectory(va("%s/%s", com_basedir, dir), FS_LOAD_FILE_ALL);
	if (*com_homedir) {
		FS_AddHomeDirectory(va("%s/%s", com_homedir, dir), FS_LOAD_FILE_ALL);
	}
#endif

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
		// VFS-FIXME: D-Kure: Need to add some VFS Cleanup here
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

	// <Cokeman> yea, but it shouldn't be in My Documents
	// <Cokeman> it should be in the application data dir
	// c:\documents and settings\<user>\application data
    //if (!SHGetSpecialFolderPath(0, com_homedir, CSIDL_APPDATA, 0))
    //{
    //    *com_homedir = 0;
    //}
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
	FS_AddGameDirectory(va("%s/%s", com_basedir, "id1"),     FS_LOAD_FILE_ALL);
	FS_AddGameDirectory(va("%s/%s", com_basedir, "ezquake"), FS_LOAD_FILE_ALL);
	FS_AddGameDirectory(va("%s/%s", com_basedir, "qw"),      FS_LOAD_FILE_ALL);
	if (*com_homedir)
	        FS_AddHomeDirectory(com_homedir, FS_LOAD_FILE_ALL);
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
			FS_AddGameDirectory(va("%s%s", com_basedir, com_argv[i+1]), FS_LOAD_FILE_ALL);
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

qbool VFS_SEEK (struct vfsfile_s *vf, unsigned long pos, int whence) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->Seek, "VFS_SEEK");
	return vf->Seek(vf, pos, whence);
}

int VFS_READ (struct vfsfile_s *vf, void *buffer, int bytestoread, vfserrno_t *err) {
	assert(vf);
	VFS_CHECKCALL(vf, vf->ReadBytes, "VFS_READ");
	return vf->ReadBytes(vf, buffer, bytestoread, err);
}

int VFS_WRITE (struct vfsfile_s *vf, const void *buffer, int bytestowrite) {
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
#else 

/*
================
FS_OpenVFS

This should be how all files are opened, will either give a valid vfsfile_t
or NULL. We first check the home directory for the file first, if not found 
we try the basedir.
================
*/
// VFS-XXX: This is very similar to FS_OpenVFS in fs.c
// VFS-FIXME: Clean up this function to reduce the relativeto options 
vfsfile_t *FS_OpenVFS(char *filename, char *mode, relativeto_t relativeto)
{
	char fullname[MAX_OSPATH];
	flocation_t loc;
	vfsfile_t *vfs;


	// VFS-FIXME: Need to find the extension of the file so we know what function to use to open it

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
			snprintf(fullname, sizeof(fullname), "%s/%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}

		snprintf(fullname, sizeof(fullname), "%s", filename);
		return VFSOS_Open(fullname, mode);

	case FS_GAME_OS:	//OS access only, no paks
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s/%s/%s", com_homedir, com_gamedirfile, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs) {
				fs_filesize = VFS_GETLEN(vfs);
				return vfs;
			}
		}

		snprintf(fullname, sizeof(fullname), "%s/%s/%s", com_basedir, com_gamedirfile, filename);
		return VFSOS_Open(fullname, mode);

/* VFS-XXX: Removed as we don't really use this
 *	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/qw/skins/%s", com_homedir, filename);
		else
			snprintf(fullname, sizeof(fullname), "%s/qw/skins/%s", com_basedir, filename);
		break;
 */

	case FS_BASE:
//		if (*com_homedir)
//		{
//			snprintf(fullname, sizeof(fullname), "%s/%s", com_homedir, filename);
//			vfs = VFSOS_Open(fullname, mode);
//			if (vfs)
//				return vfs;
//		}
		snprintf(fullname, sizeof(fullname), "%s/%s", com_basedir, filename);
		return VFSOS_Open(fullname, mode);

/* VFS-XXX: Removed as we don't really use this
 * case FS_CONFIGONLY:
		if (*com_homedir)
		{
			snprintf(fullname, sizeof(fullname), "%s/%s", com_homedir, filename);
			vfs = VFSOS_Open(fullname, mode);
			if (vfs)
				return vfs;
		}
		snprintf(fullname, sizeof(fullname), "%s/ezquake/%s", com_basedir, filename);
		return VFSOS_Open(fullname, mode);
 */
	case FS_HOME:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/%s/%s", com_homedir, com_gamedirfile, filename);
		else
			return NULL;
		return VFSOS_Open(fullname, mode);

	case FS_PAK:
		snprintf(fullname, sizeof(fullname), "%s/%s/%s", com_basedir, com_gamedirfile, filename);
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
#endif /* WITH_FTE_VFS */



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
#ifndef WITH_FTE_VFS
	Com_Printf("Initialising standard quake filesystem\n");
#else
	Cmd_AddCommand("fs_restart", FS_ReloadPackFiles_f);
	Cmd_AddCommand("fs_openfile", FS_OpenFile_f); // VFS-FIXME <-- Only a debug function
	Cvar_Register(&com_fs_cache);
	Com_Printf("Initialising quake VFS filesystem\n");
#endif
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
 * $Id: fs.c,v 1.30 2007-09-28 05:25:10 dkure Exp $
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
 * 2) Need to check all the VFS-FIXME's
 * 		D-Kure: I have made comment on the ones I unsure of,
 * 		        others just need the time to do what the comment says.
 *
 * 3) Need to add Sys_EnumerateFiles for sys_mac.c
 * 		D-Kure: I have noooo idea what the mac eqivalent functions are.
 *
 * 4) Replace some of the functions (marked with VFS-XXX) with the ezquake equivlant
 * 		D-Kure: This functions seem to be in common.c and are marked 
 * 		        FS_* instead of COM_*.
 *
 * 5) Replace all strncpy calls with strlcpy
 *      <Cokeman> allthough strncpy isn't safe
 *      <Cokeman> strlcpy is
 *
 * 9) Renaming of lots of the COM_* functions to FS_*
 *
 ****************************
 *     No particular order
 ****************************
 *  - Add tar.gz support
 *  - Replace zipped/tar/tar.gz demo opening with VFS calls
 *
 *****************************************************************************/
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
	if (loadstuff & FS_LOAD_FILE_PK3)
		COM_AddDataFiles(probablepath, search, "pk3", &zipfilefuncs);	//q3 + offspring
	if (loadstuff & FS_LOAD_FILE_PK4)
		COM_AddDataFiles(probablepath, search, "pk4", &zipfilefuncs);	//q4
	//we could easily add zip, but it's friendlier not to
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

	// VFS-FIXME: Need to implement this in Com_Printf
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
	Sys_Printf ("COM_WriteFile: %s\n", filename);
	return FS_WriteFile(filename, data, len, FS_GAME_OS);
}

//The filename used as is
qbool COM_WriteFile_2 (char *filename, void *data, int len)
{
	Sys_Printf ("COM_WriteFile_2: %s\n", filename);
	return FS_WriteFile(filename, data, len, FS_NONE_OS);
}


#if 0
FILE *COM_WriteFileOpen (char *filename)	//like fopen, but based around quake's paths.
{
	FILE	*f;
	char	name[MAX_OSPATH];

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, filename);

	COM_CreatePath(name);

	f = fopen (name, "wb");

	return f;
}
#endif


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
		filesystemhash.bucket = (bucket_t**)Q_calloc(1, Hash_BytesForBuckets(filesystemhash.numbuckets));
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
		VFS_SEEK(infile, 0, SEEK_SET);
		return infile;
	}

	if (header->flags & GZ_FEXTRA)
	{
		VFS_READ(infile, &inshort, sizeof(inshort), &err);
		inshort = LittleShort(inshort);
		VFS_SEEK(infile, VFS_TELL(infile) + inshort, SEEK_SET);
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
		VFS_SEEK(infile, 0, SEEK_SET);	//doh
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

		VFS_SEEK(temp, 0, SEEK_SET);
	}
	VFS_CLOSE(infile);

	return temp;
}
#endif

static vfsfile_t *VFS_Filter(char *filename, vfsfile_t *handle)
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
		VFS_SEEK(handle, 0, SEEK_SET);
	}
#endif
	return handle;
}

int FS_Rename2(char *oldf, char *newf, relativeto_t oldrelativeto, relativeto_t newrelativeto)
{
	char oldfullname[MAX_OSPATH];
	char newfullname[MAX_OSPATH];

	switch (oldrelativeto)
	{
	case FS_GAME_OS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s/%s/", com_homedir, com_gamedirfile);
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
			snprintf(newfullname, sizeof(newfullname), "%s/%s/", com_homedir, com_gamedirfile);
		else
			snprintf(newfullname, sizeof(newfullname), "%s%s/", com_basedir, com_gamedirfile);
		break;

/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(newfullname, sizeof(newfullname), "%s/qw/skins/", com_homedir);
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
			snprintf(oldfullname, sizeof(oldfullname), "%s/%s/", com_homedir, com_gamedirfile);
		else
			snprintf(oldfullname, sizeof(oldfullname), "%s%s/", com_basedir, com_gamedirfile);
		break;

/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(oldfullname, sizeof(oldfullname), "%s/qw/skins/", com_homedir);
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
			snprintf(fullname, sizeof(fullname), "%s/%s/%s", com_homedir, com_gamedirfile, fname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s/%s", com_basedir, com_gamedirfile, fname);
		break;

/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/qw/skins/%s", com_homedir, fname);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_basedir, fname);
		break;
 */
	case FS_BASE:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/%s", com_homedir, fname);
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
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%s", pname);
		break;

	case FS_GAME_OS:
		snprintf(fullname, sizeof(fullname), "%s/%s", com_gamedir, pname);
		break;

	case FS_BASE:
	case FS_ANY:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%s%s", com_basedir, pname);
		break;
/*	case FS_SKINS:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/qw/skins/%s", com_homedir, pname);
		else
			snprintf(fullname, sizeof(fullname), "%sqw/skins/%s", com_basedir, pname);
		break;
	case FS_CONFIGONLY:
		if (*com_homedir)
			snprintf(fullname, sizeof(fullname), "%s/fte/%s", com_homedir, pname);
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
	char name[MAX_PATH];
	vfsfile_t *f;

	snprintf (name, sizeof(name), "%s", filename);

	FS_CreatePath(filename, relativeto);
	f = FS_OpenVFS(filename, "wb", relativeto);
	if (f) 
	{
		VFS_WRITE(f, data, len);
		VFS_CLOSE(f);
	} else {
		return false;
	}

	com_fschanged=true;

	return true;
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

	snprintf (pakfile, sizeof (pakfile), "%s%s", param->parentdesc, descriptor);

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

	snprintf (pakfile, sizeof (pakfile), "%s%s/", param->parentdesc, descriptor);
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

	/* VFS-FIXME: Use pak.lst instead of COM_AddWildDataFiles */
	snprintf (pakfile, sizeof (pakfile), "*.%s", extension);
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

	// add the directory to the search path
	p = Q_malloc(strlen(dir)+1);
	strcpy(p, dir);
	COM_AddPathHandle(va("%s/", dir), &osfilefuncs, p, false, false, loadstuff);
}

/*
================
FS_AddHomeDirectory

Adds the home directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddHomeDirectory(char *dir, FS_Load_File_Types loadstuff) {
	searchpath_t	*search;
	char			*p;

	for (search = com_searchpaths; search; search = search->next)
	{
		if (search->funcs != &osfilefuncs)
			continue;
		if (!strcasecmp(search->handle, com_homedir))
			return; //already loaded (base paths?)
	}

	// add the directory to the search path
	p = Q_malloc(strlen(dir)+1);
	strcpy(p, dir);
	COM_AddPathHandle(va("%s/", dir), &osfilefuncs, p, false, false, loadstuff);
}

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

// DEBUG FUNCTION
#ifdef WITH_FTE_VFS
static void FS_OpenFile_f(void) {
	int i;
	int c = Cmd_Argc();

	if (c < 2) {
		Com_Printf("Usage: %s filename [<filename> [<filename> ...]\n", Cmd_Argv(0));
	}

	for (i = 1; i < c; i++) {
		char *filename = Cmd_Argv(i);
		vfsfile_t *f;
		f = FS_OpenVFS(filename, "rb", FS_ANY);
		if (f) {
			char *ext = COM_FileExtension (filename);
			Com_Printf("Successfully opened file %s\n", filename);
#ifdef WITH_VFS_GZIP
			if (strcasecmp(ext, "gz") == 0) {
				gzFile f_gz = Gzip_Open(f);
				if (f_gz) {
					Com_Printf("Successfully opened gzipped file %s\n", filename);
					Gzip_Close(f_gz);
				} else {
					Com_Printf("Was unable to open %s as a gzipped file\n", filename);
				}
			} else {
#endif // WITH_VFS_GZIP
#ifdef WITH_ZIP
				if (strcasecmp(ext, "zip") == 0) {
					if (FSZIP_LoadZipFile(f, filename) != NULL) {
						Com_Printf("Successfully opened zipped file %s\n", filename);
					} else {
						Com_Printf("Was unable to open %s as a zip file\n", filename);
					}
				}
#endif // WITH_ZIP
#ifdef WITH_VFS_GZIP
			}
#endif // WITH_VFS_GZIP

			VFS_CLOSE(f);
		} else {
			Com_Printf("Unable to open %s\n", Cmd_Argv(i));
		}
	}
}
#endif // WITH_FTE_VFS DEBUG_FUNCTION


#endif /* WITH_FTE_VFS */
