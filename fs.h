/*
    $Id: fs.h,v 1.1 2007-01-13 00:00:42 qqshka Exp $
*/

#ifndef __FS_H__
#define __FS_H__

// ====================================================================
// VFS

typedef enum {
	VFSERR_NONE,
	VFSERR_EOF,
	VFSERR_ERR
} vfserrno_t;

typedef struct vfsfile_s {
	int (*ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread, vfserrno_t *err);
	int (*WriteBytes) (struct vfsfile_s *file, void *buffer, int bytestowrite);
	qbool (*Seek) (struct vfsfile_s *file, unsigned long pos);	//returns false for error
	unsigned long (*Tell) (struct vfsfile_s *file);
	unsigned long (*GetLen) (struct vfsfile_s *file);	//could give some lag
	void (*Close) (struct vfsfile_s *file);
	void (*Flush) (struct vfsfile_s *file);
	qbool seekingisabadplan;
} vfsfile_t;

typedef enum {
	FS_NONE_OS,
	FS_GAME_OS,
	FS_GAME
} relativeto_t;

// mostly analogs for stdio functions
void 			VFS_CLOSE  (struct vfsfile_s *vf);
unsigned long	VFS_TELL   (struct vfsfile_s *vf);
unsigned long	VFS_GETLEN (struct vfsfile_s *vf);
qbool			VFS_SEEK   (struct vfsfile_s *vf, unsigned long pos);
int				VFS_READ   (struct vfsfile_s *vf, void *buffer, int bytestoread, vfserrno_t *err);
int				VFS_WRITE  (struct vfsfile_s *vf, void *buffer, int bytestowrite);
void			VFS_FLUSH  (struct vfsfile_s *vf);
char		   *VFS_GETS   (struct vfsfile_s *vf, char *buffer, int buflen); // return null terminated string

// open some temp VFS file, which actually result from tmpfile() at least when i commenting this
vfsfile_t *FS_OpenTemp(void);
// some general function to open VFS file, except TCP
vfsfile_t *FS_OpenVFS(char *filename, char *mode, relativeto_t relativeto);
// TCP VFS file
vfsfile_t *FS_OpenTCP(char *name);

#endif // __FS_H__
