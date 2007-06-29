/*
    $Id: fs.c,v 1.6 2007-06-29 23:57:19 johnnycz Exp $
*/

#include "quakedef.h"
#include "fs.h"
#include "common.h"

// fs related things

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

int VFSPAK_WriteBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("Cannot write to pak files");
	return 0;
}

qbool VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	if (pos < 0 || pos > vfsp->length)
		return false;

	vfsp->currentpos = pos + vfsp->startpos;
	return fseek(vfsp->handle, vfsp->currentpos, SEEK_SET) == 0;
}

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

void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;

	fclose(vfsp->handle);
	Q_free(vfsp);	//free ourselves.
}

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

// PAK files
//******************************************************************************************************

//
// some general function to open VFS file, except VFSTCP
//

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


//******************************************************************************************************
//TCP file

typedef struct tcpfile_s {
	vfsfile_t funcs; // <= must be at top/begining of struct

	int sock;

	char readbuffer[65536];
	int readbuffered;

} tcpfile_t;

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

	if (tf->sock == INVALID_SOCKET)
		return 0;

	len = send(tf->sock, buffer, bytestowrite, 0);
	if (len == -1 || len == 0)
	{
		Com_Printf("VFSTCP: failed to write %d bytes, closing link\n", bytestowrite);
		VFSTCP_Error(tf);
		return 0;
	}
	return len;
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
	VFSTCP_Error((tcpfile_t*)file);
	Q_free(file);
}

vfsfile_t *FS_OpenTCP(char *name)
{
	tcpfile_t *newf;
	int sock;
	netadr_t adr = {0};
	if (NET_StringToAdr(name, &adr))
	{
		sock = TCP_OpenStream(adr);
		if (sock == INVALID_SOCKET)
			return NULL;

		newf = Q_malloc(sizeof(*newf));
		newf->sock = sock;
		newf->funcs.Close = VFSTCP_Close;
		newf->funcs.Flush = NULL;
		newf->funcs.GetLen = VFSTCP_GetLen;
		newf->funcs.ReadBytes = VFSTCP_ReadBytes;
		newf->funcs.Seek = VFSTCP_Seek;
		newf->funcs.Tell = VFSTCP_Tell;
		newf->funcs.WriteBytes = VFSTCP_WriteBytes;
		newf->funcs.seekingisabadplan = true;

		return &newf->funcs;
	}
	else
		return NULL;
}

//TCP file
//******************************************************************************************************

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
		Com_Printf("Usage: %s <pakname> [<pakname> [<pakname> ...]\n", op == PAKOP_ADD ? "addpak" : "removepak");
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

void FS_ListPaths_f(void) { FS_ListPaths(); }

void FS_InitModuleFS (void)
{
	Cmd_AddCommand("loadpak", FS_PakAdd_f);
	Cmd_AddCommand("removepak", FS_PakRem_f);
	Cmd_AddCommand("listpaths", FS_ListPaths_f);
}
