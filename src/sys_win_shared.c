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
*/
// Windows filesystem and dynamic library helpers.

#include "common.h"
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <pcre2.h>
#include <shlobj.h>

void Sys_mkdir (const char *path)
{
	_mkdir (path);
}

int Sys_remove (char *path)
{
	return remove(path);
}

FILE *Sys_FOpenExclusive(const char *path)
{
	// Legacy MSVCRT does not support the exclusive "x" mode in fopen.
	int fd = _open(path, _O_WRONLY | _O_BINARY | _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
	FILE *file;

	if (fd < 0) {
		return NULL;
	}
	file = _fdopen(fd, "wb");
	if (!file) {
		int error = errno;
		_close(fd);
		_unlink(path);
		errno = error;
	}
	return file;
}

int Sys_RenameNoReplace(const char *source, const char *destination)
{
	DWORD error;

	if (MoveFileA(source, destination)) {
		return 0;
	}
	error = GetLastError();
	return (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) ? EEXIST : EIO;
}

int Sys_rmdir (const char *path)
{
	return _rmdir(path);
}

// D-Kure: This is added for FTE vfs
int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
	HANDLE r;
	WIN32_FIND_DATA fd;
	char apath[MAX_OSPATH];
	char apath2[MAX_OSPATH];
	char file[MAX_OSPATH];
	char *s;
	int go;
	if (!gpath)
		return 0;

	snprintf(apath, sizeof(apath), "%s/%s", gpath, match);
	for (s = apath+strlen(apath)-1; s> apath; s--)
	{
		if (*s == '/')
			break;
	}
	*s = '\0';

	// This is what we ask windows for.
	snprintf(file, sizeof(file), "%s/*.*", apath);

	// Keep the directory from match so the callback receives paths relative to gpath.
	strlcpy(apath2, match, sizeof(apath));
	match = s+1;
	for (s = apath2+strlen(apath2)-1; s> apath2; s--)
	{
		if (*s == '/')
			break;
	}
	*s = '\0';
	if (s != apath2)
		strlcat (apath2, "/", sizeof (apath2));

	r = FindFirstFile(file, &fd);
	if (r==(HANDLE)-1)
		return 1;
	go = true;
	do
	{
		if (*fd.cFileName == '.');  // Don't ever find files with a name starting with '.'
		else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)    //is a directory
		{
			if (wildcmp(match, fd.cFileName))
			{
				snprintf(file, sizeof(file), "%s%s/", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm);
			}
		}
		else
		{
			if (wildcmp(match, fd.cFileName))
			{
				snprintf(file, sizeof(file), "%s%s", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm);
			}
		}
	}
	while(FindNextFile(r, &fd) && go);
	FindClose(r);

	return go;
}

#ifndef CLIENTONLY
/*
================
Sys_listdir
================
*/
dir_t Sys_listdir (const char *path, const char *ext, int sort_type)
{
	static file_t	list[MAX_DIRFILES];
	dir_t	dir;
	HANDLE	h;
	WIN32_FIND_DATA fd;
	char	pathname[MAX_DEMO_NAME];
	qbool all;

	PCRE2_SIZE	error_offset;
	pcre2_code	*preg = NULL;
	pcre2_match_data *match_data = NULL;
	int error;

	memset(list, 0, sizeof(list));
	memset(&dir, 0, sizeof(dir));

	dir.files = list;
	all = !strncmp(ext, ".*", 3);
	if (!all) {
		if (!(preg = pcre2_compile((PCRE2_SPTR)ext, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &error, &error_offset, NULL))) {
			Con_Printf("Sys_listdir: pcre2_compile(%s) error: %d at offset %d\n",
				ext, error, error_offset);
			return dir;
		}
	}

	snprintf(pathname, sizeof(pathname), "%s/*.*", path);
	if ((h = FindFirstFile (pathname , &fd)) == INVALID_HANDLE_VALUE)
	{
		if (!all) {
			pcre2_code_free(preg);
		}
		return dir;
	}

	do
	{
		if (!strncmp(fd.cFileName, ".", 2) || !strncmp(fd.cFileName, "..", 3))
			continue;
		if (!all)
		{
			match_data = pcre2_match_data_create_from_pattern(preg, NULL);
			error = pcre2_match(preg, (PCRE2_SPTR)fd.cFileName,
				strlen(fd.cFileName), 0, 0, match_data, NULL);
			pcre2_match_data_free(match_data);
			if (error < 0) {
				if (error != PCRE2_ERROR_NOMATCH) {
					Con_Printf("Sys_listdir: pcre2_match(%s, %s) error code: %d\n",
						ext, fd.cFileName, error);
				}
				continue;
			}
		}

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			//bliP: list dir
			dir.numdirs++;
			list[dir.numfiles].isdir = true;
			list[dir.numfiles].size = list[dir.numfiles].time = 0;
		}
		else {
			ULARGE_INTEGER modified;

			modified.LowPart = fd.ftLastWriteTime.dwLowDateTime;
			modified.HighPart = fd.ftLastWriteTime.dwHighDateTime;
			list[dir.numfiles].isdir = false;
			// FILETIME counts 100 ns intervals since 1601. The demo list uses Unix seconds.
			list[dir.numfiles].time = (int)((int64_t)(modified.QuadPart / 10000000) - 11644473600LL);
			dir.size += (list[dir.numfiles].size = fd.nFileSizeLow);
		}
		strlcpy (list[dir.numfiles].name, fd.cFileName, sizeof(list[0].name));

		if (++dir.numfiles == MAX_DIRFILES - 1) {
			break;
		}
	} while (FindNextFile(h, &fd));

	FindClose (h);
	if (!all) {
		pcre2_code_free(preg);
	}

	switch (sort_type)
	{
	case SORT_NO: break;
	case SORT_BY_DATE:
		qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_date);
		break;
	case SORT_BY_NAME:
		qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_name);
		break;
	}
	return dir;
}
#endif

static void MakeDirent(sys_dirent *ent, WIN32_FIND_DATA *data)
{
    FILETIME ft1;

    strlcpy (ent->fname, data->cFileName, min(strlen(data->cFileName)+1, MAX_PATH_LENGTH));

	ent->size = (data->nFileSizeHigh > 0) ? 0xffffffff : data->nFileSizeLow;

    FileTimeToLocalFileTime(&data->ftLastWriteTime, &ft1);
    FileTimeToSystemTime(&ft1, &ent->time);

	ent->directory = (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
	ent->hidden = (data->dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) ? 1 : 0;
}

SysDirEnumHandle Sys_ReadDirFirst(sys_dirent *ent)
{
    HANDLE search;
    WIN32_FIND_DATA data;

    search = FindFirstFile("*", &data);

    if (search == (HANDLE)ERROR_INVALID_HANDLE)
        return 0;

    MakeDirent(ent, &data);

    return (SysDirEnumHandle) search;
}

int Sys_ReadDirNext(SysDirEnumHandle search, sys_dirent *ent)
{
    WIN32_FIND_DATA data;
    if (!FindNextFile((HANDLE)search, &data))
        return 0;

    MakeDirent(ent, &data);
    return 1;
}

void Sys_ReadDirClose(SysDirEnumHandle search)
{
    FindClose((HANDLE)search);
}

int Sys_chdir (const char *path)
{
    if (SetCurrentDirectory(path))
        return 1;
    else
        return 0;
}

char * Sys_getcwd (char *buf, int bufsize)
{
    if (GetCurrentDirectory(bufsize, buf))
        return buf;
    else
        return NULL;
}

char *Sys_fullpath(char *absPath, const char *relPath, int maxLength)
{
    return _fullpath(absPath, relPath, maxLength);
}

DL_t Sys_DLOpen (const char *path)
{
	return LoadLibrary (path);
}

qbool Sys_DLClose (DL_t dl)
{
	return FreeLibrary (dl);
}

void *Sys_DLProc (DL_t dl, const char *name)
{
	return (void *) GetProcAddress (dl, name);
}

//===========================================================================

void Sys_CloseLibrary(dllhandle_t *lib)
{
	FreeLibrary((HMODULE)lib);
}

dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	HMODULE lib;

	lib = LoadLibrary(name);
	if (!lib)
		return NULL;

	for (i = 0; funcs[i].name; i++)
	{
		*funcs[i].funcptr = GetProcAddress(lib, funcs[i].name);
		if (!*funcs[i].funcptr)
			break;
	}
	if (funcs[i].name)
	{
		Sys_CloseLibrary((dllhandle_t*)lib);
		lib = NULL;
	}

	return (dllhandle_t*)lib;
}

void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname)
{
	if (!module)
		return NULL;
	return GetProcAddress((HINSTANCE)module, exportname);
}

const char* Sys_HomeDirectory(void)
{
	static char path[MAX_OSPATH];

	// Use the system path to the user's Documents folder.
	if (!SHGetSpecialFolderPath(0, path, CSIDL_PERSONAL, 0)) {
		path[0] = 0;
	}

	return path;
}
