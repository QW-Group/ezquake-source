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

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // accept4
#endif

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
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <dirent.h>
#include <sys/resource.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#endif

#ifndef SERVERONLY
#include <SDL.h>
#endif
#include <dlfcn.h>

#include "quakedef.h"
#include "server.h"
#include <pcre2.h>


// BSD only defines FNDELAY:
#ifndef O_NDELAY
#  define O_NDELAY	FNDELAY
#endif


int noconinput = 0;

qbool stdin_ready;
int do_stdin = 1;

void OnChange_sys_highpriority (cvar_t *, char *, qbool *);

cvar_t sys_highpriority = {"sys_highpriority", "", CVAR_NOTFROMSERVER, OnChange_sys_highpriority};
cvar_t sys_nostdout     = {"sys_nostdout", "0" };
cvar_t sys_fontsdir     = {"sys_fontsdir", "/usr/local/share/fonts/"};

void Sys_Printf (char *fmt, ...)
{
#if defined(DEBUG) || defined(SERVERONLY)
	va_list argptr;
#ifdef SERVERONLY
	char text[4096];
#else
	char text[2048];
	unsigned char *p;
#endif

	va_start (argptr,fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (sys_nostdout.value)
		return;

#ifdef SERVERONLY
	fputs(Q_normalizetext(text), stdout);
	fflush(stdout);
#else
	for (p = (unsigned char *) text; *p; p++)
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
#endif
#else
	return;
#endif
}

#ifdef SERVERONLY
static int server_stdin_flags = -1;

static void Sys_RestoreStdin(void)
{
	if (server_stdin_flags != -1) {
		fcntl(STDIN_FILENO, F_SETFL, server_stdin_flags);
	}
}
#endif

void Sys_Quit(void)
{
#ifdef SERVERONLY
	NET_Shutdown();
	Sys_RestoreStdin();
#else
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NDELAY);
#endif
	exit(0);
}

void Sys_Init(void)
{
#if defined(__APPLE__) && !defined(SERVERONLY)
	extern void init_macos_extras(void);
	init_macos_extras();
#endif
	Cvar_Register(&sys_highpriority);
#ifndef SERVERONLY
	Cvar_Register(&sys_fontsdir);
#endif
}

void Sys_Error(char *error, ...)
{
#ifdef SERVERONLY
	static qbool shutting_down;
#else
	extern FILE *qconsole_log;
#endif
	va_list argptr;
	char string[1024];

#ifdef SERVERONLY
	Sys_RestoreStdin();
#else
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NDELAY);
#endif

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);
#ifdef SERVERONLY
	if (!shutting_down) {
		shutting_down = true;
		SV_Shutdown(string);
	}
#else
	if (qconsole_log)
		fprintf(qconsole_log, "Error: %s\n", string);

	Host_Shutdown ();
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", string, NULL);
#endif
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

FILE *Sys_FOpenExclusive(const char *path)
{
	return fopen(path, "wbx");
}

int Sys_RenameNoReplace(const char *source, const char *destination)
{
#if defined(__linux__) && defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
	if (syscall(SYS_renameat2, AT_FDCWD, source, AT_FDCWD, destination, RENAME_NOREPLACE) == 0) {
		return 0;
	}
	if (errno != ENOSYS && errno != EINVAL && errno != EOPNOTSUPP) {
		return errno;
	}
#elif defined(__APPLE__)
	if (renamex_np(source, destination, RENAME_EXCL) == 0) {
		return 0;
	}
	if (errno != ENOTSUP) {
		return errno;
	}
#endif
	// Both names are in the same directory. link() fails if the destination exists.
	if (link(source, destination) == -1 || unlink(source) == -1) {
		return errno;
	}
	return 0;
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
	static file_t list[MAX_DIRFILES]; // FIXME: thread un-safe.
	dir_t dir;
	char pathname[MAX_OSPATH];
	DIR *d;
	DIR *testdir; //bliP: list dir
	struct dirent *oneentry;
	qbool all;

	PCRE2_SIZE error_offset;
	pcre2_code *preg = NULL;
	pcre2_match_data *match_data = NULL;
	int error;

	memset(list, 0, sizeof(list));
	memset(&dir, 0, sizeof(dir));

	dir.files = list;
	all = !strncmp(ext, ".*", 3);
	if (!all)
		if (!(preg = pcre2_compile((PCRE2_SPTR)ext, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &error, &error_offset, NULL)))
		{
			PCRE2_UCHAR error_str[256];
			pcre2_get_error_message(error, error_str, sizeof(error_str));
			Con_Printf("Sys_listdir: pcre2_compile(%s) error: %s at offset %d\n",
			           ext, error_str, error_offset);
			pcre2_code_free(preg);
			return dir;
		}

	if (!(d = opendir(path)))
	{
		if (!all) {
			pcre2_code_free(preg);
		}
		return dir;
	}
	while ((oneentry = readdir(d)))
	{
		if (!strncmp(oneentry->d_name, ".", 2) || !strncmp(oneentry->d_name, "..", 3))
			continue;
		if (!all)
		{
			match_data = pcre2_match_data_create_from_pattern(preg, NULL);
			error = pcre2_match(preg, (PCRE2_SPTR)oneentry->d_name,
				strlen(oneentry->d_name), 0, 0, match_data, NULL);
			pcre2_match_data_free(match_data);
			if (error < 0) {
				if (error != PCRE2_ERROR_NOMATCH) {
					Con_Printf("Sys_listdir: pcre2_match(%s, %s) error code: %d\n",
						ext, oneentry->d_name, error);
				}
				continue;
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
		pcre2_code_free(preg);
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

// macOS exposes clock_gettime independently of _POSIX_TIMERS.
#if defined(__APPLE__) || (_POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK))
#include <time.h>
double Sys_DoubleTime(void)
{
	static unsigned int secbase;
	struct timespec ts;

#if defined(__APPLE__)
	// Like the Linux raw monotonic clock, this stops during suspend.
	clock_gettime(CLOCK_UPTIME_RAW, &ts);
#elif defined(__linux__)
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

#ifdef SERVERONLY
typedef struct {
	int (*func)(void *);
	void *data;
} sys_thread_args_t;

static void *Sys_DetachedThreadMain(void *data)
{
	sys_thread_args_t args = *(sys_thread_args_t *)data;

	free(data);
	args.func(args.data);
	return NULL;
}

int Sys_CreateDetachedThread(int (*func)(void *), void *data)
{
	pthread_t thread;
	pthread_attr_t attr;
	sys_thread_args_t *args;
	int error;

	if (pthread_attr_init(&attr) != 0) {
		return -1;
	}
	error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (error != 0) {
		pthread_attr_destroy(&attr);
		return -1;
	}
	args = malloc(sizeof(*args));
	if (!args) {
		pthread_attr_destroy(&attr);
		return -1;
	}
	args->func = func;
	args->data = data;
	error = pthread_create(&thread, &attr, Sys_DetachedThreadMain, args);
	pthread_attr_destroy(&attr);
	if (error != 0) {
		free(args);
		return -1;
	}
	return 0;
}

qbool Sys_MutexInit(sys_mutex_t *mutex)
{
	return pthread_mutex_init(mutex, NULL) == 0;
}

void Sys_MutexLock(sys_mutex_t *mutex)
{
	pthread_mutex_lock(mutex);
}

void Sys_MutexUnlock(sys_mutex_t *mutex)
{
	pthread_mutex_unlock(mutex);
}

qbool Sys_MutexTryLock(sys_mutex_t *mutex)
{
	return pthread_mutex_trylock(mutex) == 0;
}

static volatile sig_atomic_t server_done;
static char **server_argv;

static void Sys_StopServer(int signal_number)
{
	(void)signal_number;
	server_done = 1;
}

char *Sys_ConsoleInput(void)
{
	static char line[1024];
	static size_t length;
	static qbool too_long;
	char ch;
	ssize_t count;

	while (stdin_ready) {
		count = read(STDIN_FILENO, &ch, 1);
		if (count != 1) {
			stdin_ready = false;
			if (count == 0) {
				do_stdin = 0;
				if (length && !too_long) {
					line[length] = '\0';
					length = 0;
					return line;
				}
			}
			return NULL;
		}
		if (ch == '\n') {
			line[length] = '\0';
			length = 0;
			if (too_long) {
				too_long = false;
				Com_Printf("Console command is too long.\n");
				continue;
			}
			return line;
		}
		if (ch != '\r') {
			if (length < sizeof(line) - 1) {
				line[length++] = ch;
			}
			else {
				too_long = true;
			}
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	extern cvar_t sys_select_timeout;
	double oldtime, newtime, elapsed;
	int flags;

	server_argv = argv;
	COM_InitArgv(argc, argv);
	do_stdin = !COM_CheckParm(cmdline_param_client_nostdinput);
	if (do_stdin) {
		flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		if (flags == -1 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
			do_stdin = 0;
		}
		else {
			server_stdin_flags = flags;
		}
	}
	if (COM_CheckParm(cmdline_param_client_nostdoutput)) {
		sys_nostdout.value = 1;
	}

	signal(SIGINT, Sys_StopServer);
	signal(SIGTERM, Sys_StopServer);
	signal(SIGPIPE, SIG_IGN);

	Host_Init(argc, argv, 64 * 1024 * 1024);
	oldtime = Sys_DoubleTime();
	while (!server_done) {
		stdin_ready = NET_Sleep((int)sys_select_timeout.value / 1000, do_stdin);
		newtime = Sys_DoubleTime();
		elapsed = newtime - oldtime;
		oldtime = newtime;
		curtime += elapsed;
		SV_Frame(elapsed);
	}

	SV_Shutdown("Server quit\n");
	Sys_Quit();
	return 0;
}
#else
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
	// A QTV peer can disconnect between socket readiness and send().
	signal(SIGPIPE, SIG_IGN);

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
#endif

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
		if (realpath(relPath, tmp) && absPath && strlen(tmp) < maxLength) {
			strlcpy(absPath, tmp, maxLength);
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
#ifndef SERVERONLY
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


#endif
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
#ifndef SOCK_CLOEXEC
// On systems without atomic close-on-exec, exclude process launches until
// the new socket is marked. DNS and connect run outside this lock.
static pthread_mutex_t sys_spawn_mutex = PTHREAD_MUTEX_INITIALIZER;

static int Sys_SetSocketCloseOnExec(int sock)
{
	if (sock != -1 && fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
		close(sock);
		return -1;
	}
	return sock;
}
#endif

// A background script can outlive a restart and otherwise keep the port bound.
int Sys_OpenSocket(int family, int type, int protocol)
{
#ifdef SOCK_CLOEXEC
	return socket(family, type | SOCK_CLOEXEC, protocol);
#else
	int sock;

	pthread_mutex_lock(&sys_spawn_mutex);
	sock = Sys_SetSocketCloseOnExec(socket(family, type, protocol));
	pthread_mutex_unlock(&sys_spawn_mutex);
	return sock;
#endif
}

int Sys_AcceptSocket(int listener, struct sockaddr *address, socklen_t *length)
{
#ifdef SOCK_CLOEXEC
	return accept4(listener, address, length, SOCK_CLOEXEC);
#else
	int sock;

	pthread_mutex_lock(&sys_spawn_mutex);
	sock = Sys_SetSocketCloseOnExec(accept(listener, address, length));
	pthread_mutex_unlock(&sys_spawn_mutex);
	return sock;
#endif
}

#ifdef SERVERONLY
void Sys_Restart(void)
{
	int fd, error;
	int maxfd = getdtablesize();

	Sys_RestoreStdin();
#ifndef SOCK_CLOEXEC
	pthread_mutex_lock(&sys_spawn_mutex);
#endif
	// Pack files and QTV connections must not carry over to the new process.
	for (fd = STDERR_FILENO + 1; fd < maxfd; ++fd) {
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	}
	execvp(server_argv[0], server_argv);
	error = errno;
#ifndef SOCK_CLOEXEC
	pthread_mutex_unlock(&sys_spawn_mutex);
#endif
	Sys_Error("Could not restart: %s", strerror(error));
}
#endif

int Sys_RunCommand(const char *command)
{
#ifdef SOCK_CLOEXEC
	return system(command);
#else
	int result;

	pthread_mutex_lock(&sys_spawn_mutex);
	result = system(command);
	pthread_mutex_unlock(&sys_spawn_mutex);
	return result;
#endif
}

int Sys_Script (const char *path, const char *args)
{
	char str[1024];

	snprintf(str, sizeof(str), "cd %s\n./%s.qws %s &\ncd ..", fs_gamedir, path, args);

	if (Sys_RunCommand(str) == -1)
		return 0;

	return 1;
}

DL_t Sys_DLOpen(const char *path)
{
	DL_t ret = dlopen(path,
#ifdef __OpenBSD__
	              DL_LAZY
#else
	              RTLD_NOW
#endif
	             );
	if (!ret)
		Con_DPrintf("Sys_DLOpen: %s\n", dlerror());
	return ret;
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
            "MimeType=x-scheme-handler/qw;\n"
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

	if(Sys_RunCommand("xdg-mime default qw-url-handler.desktop x-scheme-handler/qw") != 0){
        Con_Printf("Failed to register qw-url-handler file with xdg-mime, please make sure this program is installed.");
    }

    if (!quiet) {
        Com_Printf_State(PRINT_WARNING, "qw:// protocol registered\n");
    }

    return;
}

#endif //platforms other than osx

int Sys_SetPriority(int priority)
{
	int ret, i=0;

	switch (priority) {
		case 1:
			for (i=-19;i<0;i++) {
				ret = setpriority(PRIO_PGRP, 0, i);
				if (ret == 0) break;
			}
			break;
		case -1:
			for (i=19;i>0;i--) {
				ret = setpriority(PRIO_PGRP, 0, i);
				if (ret == 0) break;
			}
			break;
		default:
			ret = setpriority(PRIO_PGRP, 0, 0);
	}

	if (ret == 0) return i;
	return 255;
}

void OnChange_sys_highpriority (cvar_t *var, char *s, qbool *cancel)
{
	//do not attempt to change priority if cvar is unset
	if (s[0] == '\0') return;

	int priority, ret;
	char *desc;

	priority = Q_atoi(s);
	if (priority > 0){
		desc = "high";
	} else if (priority < 0) {
		desc = "low";
	} else {
		desc = "normal";
	}

	ret = Sys_SetPriority(priority);
	if (ret != 255) {
		Com_Printf("Process priority set to %s (nice level %d)\n", desc, ret);
	} else {
		Com_Printf("Failed to set process priority set to %s\n", desc);
	}
}
