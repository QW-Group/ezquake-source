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

$Id: fs.c,v 1.12 2007-09-02 21:59:28 johnnycz Exp $
*/

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

// in memory
typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
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
	char	filename[MAX_OSPATH];
	pack_t	*pack; // only one of filename / pack will be used
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

#ifndef FTE_FS
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
#endif /* FTE_FS */

#ifndef FTE_FS
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
#endif /* FTE_FS */

int COM_FCreateFile (char *filename, FILE **file, char *path, char *mode)
{
	searchpath_t *search;
	char fullpath[MAX_OSPATH];

	if (path == NULL)
		path = com_gamedir;
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
#ifndef FTE_FS
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
#endif /* FTE_FS */

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


#ifndef FTE_FS
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

int FS_FOpenFile (char *filename, FILE **file) {
	searchpath_t *search;
	pack_t *pak;
	int i;

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
	}

	if (developer.value)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	return -1;
}

// Filename are relative to the quake directory.
// Always appends a 0 byte to the loaded data.
static cache_user_t *loadcache;
static byte			*loadbuf;
static int			loadsize;
byte *FS_LoadFile (char *path, int usehunk)
{
	FILE *h;
	byte *buf;
	char base[32];
	int len;

	// Look for it in the filesystem or pack files.
	len = fs_filesize = FS_FOpenFile (path, &h);
	if (!h)
		return NULL;

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

	fread (buf, 1, len, h);
	fclose (h);

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
	FILE *packhandle;
	dpackfile_t *info;

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
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

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (info, 1, header.dirlen, packhandle);

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
	search->pack = pak;
	search->next = com_searchpaths;
	com_searchpaths = search;
	return true;
}

qbool FS_RemovePak (const char *pakfile) {
	searchpath_t *prev = NULL;
	searchpath_t *cur = com_searchpaths;
	searchpath_t *temp;
	qbool ret = false;

	while (cur) {
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
	strcpy (search->filename, com_userdir);
	search->pack = NULL;
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

	// Flush all data, so it will be forced to reload.
	Cache_Flush ();

	sprintf (com_gamedir, "%s/%s", com_basedir, dir);

	if (strcmp(dir, "id1") && strcmp(dir, "qw") && strcmp(dir, "ezquake"))
	{
		FS_AddGameDirectory(com_basedir, dir);
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

		if (com_searchpaths->pack) {
			fclose (com_searchpaths->pack->handle); // close pack file handler
			Q_free (com_searchpaths->pack->files);
			Q_free (com_searchpaths->pack);
		}
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
		// if homedir not used set it equal to basedir
		strlcpy(com_homedir, com_basedir, sizeof(com_homedir));
	}

	// start up with id1 by default
	FS_AddGameDirectory(com_basedir, "id1");

	FS_AddGameDirectory(com_basedir, "ezquake");

	FS_AddGameDirectory(com_basedir, "qw");


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
			FS_AddGameDirectory(com_basedir,com_argv[i+1]);
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
	FILE *f;

	FS_InitModuleFS();

	FS_InitFilesystemEx( false ); // first attempt, simplified

	if ( FS_FOpenFile( "gfx.wad", &f ) >= 0 ) { // we found gfx.wad, seems we have proper com_basedir
		fclose( f );
		return;
	}

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
	VFS_CHECKCALL(vf, vf->Close, "VFS_CLOSE");
	vf->Close(vf);
}

unsigned long VFS_TELL (struct vfsfile_s *vf) {
	VFS_CHECKCALL(vf, vf->Tell, "VFS_TELL");
	return vf->Tell(vf);
}

unsigned long VFS_GETLEN (struct vfsfile_s *vf) {
	VFS_CHECKCALL(vf, vf->GetLen, "VFS_GETLEN");
	return vf->GetLen(vf);
}

qbool VFS_SEEK (struct vfsfile_s *vf, unsigned long pos) {
	VFS_CHECKCALL(vf, vf->Seek, "VFS_SEEK");
	return vf->Seek(vf, pos);
}

int VFS_READ (struct vfsfile_s *vf, void *buffer, int bytestoread, vfserrno_t *err) {
	VFS_CHECKCALL(vf, vf->ReadBytes, "VFS_READ");
	return vf->ReadBytes(vf, buffer, bytestoread, err);
}

int VFS_WRITE (struct vfsfile_s *vf, void *buffer, int bytestowrite) {
	VFS_CHECKCALL(vf, vf->WriteBytes, "VFS_WRITE");
	return vf->WriteBytes(vf, buffer, bytestowrite);
}

void VFS_FLUSH (struct vfsfile_s *vf) {
	if(vf->Flush)
		vf->Flush(vf);
}

// return null terminated string
#ifndef FTE_FS
char *VFS_GETS(struct vfsfile_s *vf, char *buffer, int buflen)
{
	char in;
	char *out = buffer;
	int len = buflen-1;

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
#endif /* FTE_FS */

//******************************************************************************************************
//STDIO files (OS)

typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	FILE *handle;

} vfsosfile_t;

#ifndef FTE_FS
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
#endif /* FTE_FS */

#ifndef FTE_FS
int VFSOS_WriteBytes (struct vfsfile_s *file, void *buffer, int bytestowrite)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fwrite(buffer, 1, bytestowrite, intfile->handle);
}
#endif /* FTE_FS */

#ifndef FTE_FS
qbool VFSOS_Seek (struct vfsfile_s *file, unsigned long pos)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return fseek(intfile->handle, pos, SEEK_SET) == 0;
}
#endif /* FTE_FS */

#ifndef FTE_FS
unsigned long VFSOS_Tell (struct vfsfile_s *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	return ftell(intfile->handle);
}
#endif /* FTE_FS */

#ifndef FTE_FS
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
#endif /* FTE_FS */


#ifndef FTE_FS
void VFSOS_Close(vfsfile_t *file)
{
	vfsosfile_t *intfile = (vfsosfile_t*)file;
	fclose(intfile->handle);
	Q_free(file);
}
#endif /* FTE_FS */

#ifndef FTE_FS
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
#endif /* FTE_FS */

// if f == NULL then use fopen(name, ...);
#ifndef FTE_FS
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
#endif /* FTE_FS */

//STDIO files (OS)
//******************************************************************************************************
// PAK files

typedef struct {
	vfsfile_t funcs; // <= must be at top/begining of struct

	FILE *handle;
	unsigned long startpos;
	unsigned long length;
	unsigned long currentpos;
} vfspack_t;

#ifndef FTE_FS
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

#ifndef FTE_FS
int VFSPAK_WriteBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("Cannot write to pak files");
	return 0;
}
#endif

#ifndef FTE_FS
qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	if (pos < 0 || pos > vfsp->length)
		return false;

	vfsp->currentpos = pos + vfsp->startpos;
	return fseek(vfsp->handle, vfsp->currentpos, SEEK_SET) == 0;
}
#endif /* FS_FTE */

#ifndef FTE_FS
unsigned long VFSPAK_Tell (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->currentpos - vfsp->startpos;
}
#endif /* FS_FTE */

#ifndef FTE_FS
unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->length;
}
#endif /* FS_FTE */

#ifndef FTE_FS
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	fclose(vfsp->handle);
	Q_free(vfsp);	//free ourselves.
}
#endif /* FTE_FS */

#ifndef FTE_FS
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

#ifndef FTE_FS
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
