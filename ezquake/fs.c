/*
    $Id: fs.c,v 1.1 2007-01-10 12:38:13 qqshka Exp $
*/

#include "quakedef.h"

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
char *VFS_GETS(vfsfile_t *vf, char *buffer, int buflen)
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

vfsfile_t *VFSOS_Open(char *osname, char *mode)
{
	FILE *f;
	vfsosfile_t *file;
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

	f = fopen(osname, newmode);
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

//
// some general function to open VFS file
//

vfsfile_t *FS_OpenVFS(char *filename, char *mode, relativeto_t relativeto)
{
	char fullname[MAX_OSPATH];

	switch (relativeto)
	{
	case FS_NONE_OS:	//OS access only, no paks
		snprintf(fullname, sizeof(fullname), "%s", filename); // pass filename as is

		return VFSOS_Open(fullname, mode);

	case FS_GAME_OS:	//OS access only, no paks
		snprintf(fullname, sizeof(fullname), "%s/%s", com_gamedir, filename);

		return VFSOS_Open(fullname, mode);

	case FS_GAME: // any source
		snprintf(fullname, sizeof(fullname), "%s/%s", com_gamedir, filename);

		break;

	default:
		Sys_Error("FS_OpenVFS: Bad relative path (%i)", relativeto);
		break;
	}

	// FIXME: support paks
	
	// check mode then open paks, write mode is incorrect for paks
	
	return VFSOS_Open(fullname, mode);
}

