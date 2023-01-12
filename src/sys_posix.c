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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sched.h>
#include <errno.h>
#include <dirent.h>

#include <SDL.h>
#include <dlfcn.h>

#include "quakedef.h"
#include "server.h"
#include "pcre.h"


// BSD only defines FNDELAY:
#ifndef O_NDELAY
#  define O_NDELAY	FNDELAY
#endif

int noconinput = 0;

qbool stdin_ready;
int do_stdin = 1;

cvar_t sys_nostdout = {"sys_nostdout", "0"};
cvar_t sys_fontsdir = { "sys_fontsdir", "/usr/local/share/fonts/" };

void Sys_Printf (char *fmt, ...)
{
#ifdef DEBUG
	va_list argptr;
	char text[2048];
	unsigned char *p;

	return;

	va_start (argptr,fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (sys_nostdout.value)
		return;

	for (p = (unsigned char *) text; *p; p++)
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
#else
	return;
#endif
}

void Sys_Quit(void)
{
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NDELAY);
	exit(0);
}

void Sys_Init(void)
{
#ifdef __APPLE__
    extern void init_url_handler( void );
    init_url_handler();
#endif

	Cvar_Register(&sys_fontsdir);
}

void Sys_Error(char *error, ...)
{
	extern FILE *qconsole_log;
	va_list argptr;
	char string[1024];

	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NDELAY);	//change stdin to non blocking

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);
	if (qconsole_log)
		fprintf(qconsole_log, "Error: %s\n", string);

	Host_Shutdown ();
	exit(1);
}

void Sys_mkdir (const char *path)
{
	mkdir (path, 0777);
}

/*
================
Sys_remove
================
*/
int Sys_remove(char *path)
{
	return unlink(path);
}

int Sys_rmdir(const char *path)
{
	return rmdir(path);
}

int Sys_FileSizeTime(char *path, int *time1)
{
	struct stat buf;
	if (stat(path, &buf) == -1)
	{
		*time1 = -1;
		return 0;
	}
	else
	{
		*time1 = buf.st_mtime;
		return buf.st_size;
	}
}

/*
================
Sys_listdir
================
*/

dir_t Sys_listdir (const char *path, const char *ext, int sort_type)
{
	static file_t list[MAX_DIRFILES];
	dir_t dir;
	char pathname[MAX_OSPATH];
	DIR *d;
	DIR *testdir; //bliP: list dir
	struct dirent *oneentry;
	qbool all;

	int r;
	pcre *preg = NULL;
	const char *errbuf;

	memset(list, 0, sizeof(list));
	memset(&dir, 0, sizeof(dir));

	dir.files = list;
	all = !strncmp(ext, ".*", 3);
	if (!all)
		if (!(preg = pcre_compile(ext, PCRE_CASELESS, &errbuf, &r, NULL)))
		{
			Con_Printf("Sys_listdir: pcre_compile(%s) error: %s at offset %d\n",
			           ext, errbuf, r);
			pcre_free(preg);
			return dir;
		}

	if (!(d = opendir(path)))
	{
		if (!all) {
			pcre_free(preg);
		}
		return dir;
	}
	while ((oneentry = readdir(d)))
	{
		if (!strncmp(oneentry->d_name, ".", 2) || !strncmp(oneentry->d_name, "..", 3))
			continue;
		if (!all)
		{
			switch (r = pcre_exec(preg, NULL, oneentry->d_name,
			                      strlen(oneentry->d_name), 0, 0, NULL, 0))
			{
			case 0: break;
			case PCRE_ERROR_NOMATCH: continue;
			default:
				Con_Printf("Sys_listdir: pcre_exec(%s, %s) error code: %d\n",
				           ext, oneentry->d_name, r);
				pcre_free(preg);
				return dir;
			}
		}
		snprintf(pathname, sizeof(pathname), "%s/%s", path, oneentry->d_name);
		if ((testdir = opendir(pathname)))
		{
			dir.numdirs++;
			list[dir.numfiles].isdir = true;
			list[dir.numfiles].size = list[dir.numfiles].time = 0;
			closedir(testdir);
		}
		else
		{
			list[dir.numfiles].isdir = false;
			//list[dir.numfiles].time = Sys_FileTime(pathname);
			dir.size +=
				(list[dir.numfiles].size = Sys_FileSizeTime(pathname, &list[dir.numfiles].time));
		}
		strlcpy (list[dir.numfiles].name, oneentry->d_name, MAX_DEMO_NAME);

		if (++dir.numfiles == MAX_DIRFILES - 1)
			break;
	}
	closedir(d);
	if (!all) {
		pcre_free(preg);
	}

	switch (sort_type)
	{
	case SORT_NO: break;
#ifndef CLIENTONLY
	case SORT_BY_DATE:
		qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_date);
		break;
	case SORT_BY_NAME:
		qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_name);
		break;
#endif
	}

	return dir;
}

int Sys_chdir(const char *path)
{
	return (chdir(path) == 0);
}

char *Sys_getcwd(char *buf, int bufsize)
{
	return getcwd(buf, bufsize);
}

#if (_POSIX_TIMERS > 0) && defined(_POSIX_MONOTONIC_CLOCK)
#include <time.h>
double Sys_DoubleTime(void)
{
	static unsigned int secbase;
	struct timespec ts;

#ifdef __linux__
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

	if (!secbase) {
		secbase = ts.tv_sec;
		return ts.tv_nsec / 1000000000.0;
	}

	return (ts.tv_sec - secbase) + ts.tv_nsec / 1000000000.0;
}
#else
double Sys_DoubleTime(void)
{
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
	    secbase = tp.tv_sec;
	    return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}
#endif

int main(int argc, char **argv)
{
	double time, oldtime, newtime;
	int i;

#ifdef __linux__
	extern void InitSig(void);
	InitSig();
#endif

	COM_InitArgv (argc, argv);

	// let me use -condebug C:\condebug.log before Quake FS init, so I get ALL messages before quake fully init
	if ((i = COM_CheckParm(cmdline_param_console_debug)) && i < COM_Argc() - 1) {
		extern FILE *qconsole_log;
		char *s = COM_Argv(i + 1);
		if (*s != '-' && *s != '+')
			qconsole_log = fopen(s, "a");
	}

	signal(SIGFPE, SIG_IGN);

	// we need to check for -noconinput and -nostdout before Host_Init is called
	if (!(noconinput = COM_CheckParm(cmdline_param_client_nostdinput)))
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	if (COM_CheckParm(cmdline_param_client_nostdoutput))
		sys_nostdout.value = 1;

	Host_Init (argc, argv, 256 * 1024 * 1024);

	oldtime = Sys_DoubleTime ();
	while (1) {
		// find time spent rendering last frame
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(time);
	}
}

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length) {
	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize - 1)) - psize;
	r = mprotect((char*) addr, length + startaddr - addr + psize, 7);
	if (r < 0)
    		Sys_Error("Protection change failed");
}

// kazik -->
// directory listing functions
int CopyDirent(sys_dirent *ent, struct dirent *tmpent)
{
    struct stat statbuf;
    struct tm * lintime;
    if (stat(tmpent->d_name, &statbuf) != 0)
        return 0;

    strncpy(ent->fname, tmpent->d_name, MAX_PATH_LENGTH);
    ent->fname[MAX_PATH_LENGTH-1] = 0;

    lintime = localtime(&statbuf.st_mtime);
    UnixtimeToWintime(&ent->time, lintime);

    ent->size = statbuf.st_size;

    ent->directory = (statbuf.st_mode & S_IFDIR);

    return 1;
}

SysDirEnumHandle Sys_ReadDirFirst(sys_dirent *ent)
{
    struct dirent *tmpent;
    DIR *dir = opendir(".");
    if (dir == NULL)
        return 0;

    tmpent = readdir(dir);
    if (tmpent == NULL)
    {
        closedir(dir);
        return 0;
    }

    if (!CopyDirent(ent, tmpent))
    {
        closedir(dir);
        return 0;
    }

    return (SysDirEnumHandle)dir;
}

int Sys_ReadDirNext(SysDirEnumHandle search, sys_dirent *ent)
{
    struct dirent *tmpent;

    tmpent = readdir((DIR*)search);

    if (tmpent == NULL)
        return 0;

    if (!CopyDirent(ent, tmpent))
        return 0;

    return 1;
}

void Sys_ReadDirClose(SysDirEnumHandle search)
{
    closedir((DIR *)search);
}

void _splitpath(const char *path, char *drive, char *dir, char *file, char *ext)
{
    const char *f, *e;

    if (drive)
    drive[0] = 0;

    f = path;
    while (strchr(f, '/'))
    f = strchr(f, '/') + 1;

    if (dir)
    {
    strncpy(dir, path, min(f-path, _MAX_DIR));
        dir[_MAX_DIR-1] = 0;
    }

    e = f;
    while (*e == '.')   // skip dots at beginning
    e++;
    if (strchr(e, '.'))
    {
    while (strchr(e, '.'))
        e = strchr(e, '.')+1;
    e--;
    }
    else
    e += strlen(e);

    if (file)
    {
        strncpy(file, f, min(e-f, _MAX_FNAME));
    file[min(e-f, _MAX_FNAME-1)] = 0;
    }

    if (ext)
    {
    strncpy(ext, e, _MAX_EXT);
    ext[_MAX_EXT-1] = 0;
    }
}

// full path
char *Sys_fullpath(char *absPath, const char *relPath, int maxLength)
{
    // too small buffer, copy in tmp[] and then look is enough space in output buffer aka absPath
    if (maxLength-1 < PATH_MAX)	{
			 char tmp[PATH_MAX+1];
			 if (realpath(relPath, tmp) && absPath && strlen(tmp) < maxLength+1) {
					strlcpy(absPath, tmp, maxLength+1);
          return absPath;
			 }

       return NULL;
		}

    return realpath(relPath, absPath);
}
// kazik <--

int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
	DIR *dir, *dir2;
	char apath[MAX_OSPATH];
	char file[MAX_OSPATH];
	char truepath[MAX_OSPATH];
	char *s;
	struct dirent *ent;

	//printf("path = %s\n", gpath);
	//printf("match = %s\n", match);

	if (!gpath)
		gpath = "";
	*apath = '\0';

	strlcpy(apath, match, sizeof(apath));
	for (s = apath+strlen(apath)-1; s >= apath; s--)
	{
		if (*s == '/')
		{
			s[1] = '\0';
			match += s - apath+1;
			break;
		}
	}
	if (s < apath)  //didn't find a '/'
		*apath = '\0';

	snprintf(truepath, sizeof(truepath), "%s/%s", gpath, apath);


	//printf("truepath = %s\n", truepath);
	//printf("gamepath = %s\n", gpath);
	//printf("apppath = %s\n", apath);
	//printf("match = %s\n", match);
	dir = opendir(truepath);
	if (!dir)
	{
		Com_DPrintf("Failed to open dir %s\n", truepath);
		return true;
	}
	do
	{
		ent = readdir(dir);
		if (!ent)
			break;
		if (*ent->d_name != '.')
			if (wildcmp(match, ent->d_name))
			{
				snprintf(file, sizeof(file), "%s/%s", truepath, ent->d_name);
				//would use stat, but it breaks on fat32.

				if ((dir2 = opendir(file)))
				{
					closedir(dir2);
					snprintf(file, sizeof(file), "%s%s/", apath, ent->d_name);
					//printf("is directory = %s\n", file);
				}
				else
				{
					snprintf(file, sizeof(file), "%s%s", apath, ent->d_name);
					//printf("file = %s\n", file);
				}

				if (!func(file, -2, parm))
				{
					closedir(dir);
					return false;
				}
			}
	} while(1);
	closedir(dir);

	return true;
}

/*************************** INTER PROCESS CALLS *****************************/
#define PIPE_BUFFERSIZE		1024

static int fifo_pipe = -1;

static char *Sys_PipeFile(void) {
	static char pipe[MAX_PATH] = {0};

	if (*pipe)
		return pipe;

	snprintf(pipe, sizeof(pipe), "/tmp/ezquake_fifo_%s", getlogin());
	return pipe;
}

void Sys_InitIPC(void)
{
	mode_t old;

	/* Don't use the user's umask, make sure we set the proper access */
	old = umask(0);
	// This could legitimately fail (pipe exists), but it should be fine.
	mkfifo(Sys_PipeFile(), 0600);
	umask(old); // Reset old mask

	/* Open in non blocking mode */
	if ((fifo_pipe = open(Sys_PipeFile(), O_RDONLY | O_NONBLOCK)) == -1) {
		// We failed ...
		return;
	}
}

void Sys_CloseIPC(void)
{
	if (fifo_pipe != -1) {
		close(fifo_pipe);
		fifo_pipe = -1;
		unlink(Sys_PipeFile());
	}
}

void Sys_ReadIPC(void)
{
	char buf[PIPE_BUFFERSIZE] = {0};
	int num_bytes_read = 0;

	if (fifo_pipe == -1)
		return;

	num_bytes_read = read(fifo_pipe, buf, sizeof(buf) - 1); // nul terminated.
	if (num_bytes_read <= 0) {
		return;
	}
	COM_ParseIPCData(buf, num_bytes_read);
}

unsigned int Sys_SendIPC(const char *buf)
{
	FILE *fifo_out;
	int nelms_written;
	
	if (!(fifo_out = fopen(Sys_PipeFile(), "w")))
		return true;

	nelms_written = fwrite(buf, strlen(buf) + 1, 1, fifo_out);

	fclose(fifo_out);

	if (nelms_written != 1)
		return false;
	
	return true;
}


/********************************** SEMAPHORES *******************************/
/* Sys_Sem*() returns 0 on success; on error, -1 is returned */
int Sys_SemInit(sem_t *sem, int value, int max_value) 
{
	return sem_init(sem, 0, value); // Don't share between processes
}

int Sys_SemWait(sem_t *sem) 
{
	return sem_wait(sem);
}

int Sys_SemPost(sem_t *sem)
{
	return sem_post(sem);
}

int Sys_SemDestroy(sem_t *sem)
{
	return sem_destroy(sem);
}

/*********************************************************************************/
int Sys_Script (const char *path, const char *args)
{
	char str[1024];

	snprintf(str, sizeof(str), "cd %s\n./%s.qws %s &\ncd ..", fs_gamedir, path, args);

	if (system(str) == -1)
		return 0;

	return 1;
}

DL_t Sys_DLOpen(const char *path)
{
	return dlopen(path,
#ifdef __OpenBSD__
	              DL_LAZY
#else
	              RTLD_NOW
#endif
	             );
}

qbool Sys_DLClose(DL_t dl)
{
	return !dlclose(dl);
}

void *Sys_DLProc(DL_t dl, const char *name)
{
	return dlsym(dl, name);
}

/*********************************************************************************/

void Sys_CloseLibrary(dllhandle_t *lib)
{
	dlclose((void*)lib);
}

dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	dllhandle_t lib;

	lib = dlopen (name, RTLD_LAZY);
	if (!lib)
		return NULL;

	for (i = 0; funcs[i].name; i++)
	{
		*funcs[i].funcptr = dlsym(lib, funcs[i].name);
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
	return dlsym(module, exportname);
}

const char* Sys_FontsDirectory(void)
{
	return sys_fontsdir.string;
}

const char* Sys_HomeDirectory(void)
{
	char *ev = getenv("HOME");
	if (ev) {
		return ev;
	}
	return "";
}

#ifdef __MACOSX__

void Sys_RegisterQWURLProtocol_f(void)
{
	Con_Printf("Not yet implemented for this platform.");
	return;
}

#else

void Sys_RegisterQWURLProtocol_f(void)
{
    char open_cmd[MAX_PATH*2+1024] = { 0 };
    char exe_path[MAX_PATH] = { 0 };
    char buf[MAX_PATH] = { 0 };
    const char *homedir=Sys_HomeDirectory();
    int nchar = -1;
    FILE *fptr;

    qbool quiet = Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "quiet");

    nchar = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (nchar < 0 || nchar >= sizeof exe_path) {
        Con_Printf("Failed to get executable path.");
        return;
    }

    snprintf(open_cmd, sizeof(open_cmd), "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=ezQuake\n"
            "GenericName=Handles qw:// protocol links from browsers\n"
            "Comment=Browser uses ezQuake to open qw:// urls.\n"
            "Keywords=qtv,qw://;\n"
            "Categories=quakeworld;qtv;Game;\n"
            "Exec=%s +qwurl %s\n"
            "Path=%s\n"
            "MimeType=x-scheme-handler/qw\n"
            "Terminal=false\n"
            "StartupNotify=false\n"
            , exe_path, "%u", com_basedir);
    snprintf(buf, sizeof(buf), "%s/.local/share/applications/qw-url-handler.desktop", homedir);

    fptr = fopen(buf, "wb+");
    if(!fptr){
        //create directory path to url handler file
        snprintf(buf, sizeof(buf), "%s/.local", homedir);
        Sys_mkdir(buf);
        snprintf(buf, sizeof(buf), "%s/.local/share", homedir);
        Sys_mkdir(buf);
        snprintf(buf, sizeof(buf), "%s/.local/share/applications", homedir);
        Sys_mkdir(buf);
        //attempt opening the file again
        snprintf(buf, sizeof(buf), "%s/.local/share/applications/qw-url-handler.desktop", homedir);
        fptr = fopen(buf, "wb+");
    }
    if (fptr == NULL) {
        Con_Printf("Failed to open qw-url-handler file.");
        return;
    }
    fprintf(fptr, "%s", open_cmd);
    fclose(fptr);

	if(system("xdg-mime default qw-url-handler.desktop x-scheme-handler/qw") != 0){
        Con_Printf("Failed to register qw-url-handler file with xdg-mime, please make sure this program is installed.");
    }

    if (!quiet) {
        Com_Printf_State(PRINT_WARNING, "qw:// protocol registered\n");
    }

    return;
}

#endif //platforms other than osx
