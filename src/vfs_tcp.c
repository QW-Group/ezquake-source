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
 
 $Id: vfs_tcp.c,v 1.2 2007-09-28 05:21:45 dkure Exp $
*/

#include "quakedef.h"
#include "hash.h"
#include "common.h"
#include "fs.h"
#include "vfs.h"

//==========
// TCP file
//==========

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
		if (tf->readbuffered > 0 && buffer != NULL) {
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

int VFSTCP_WriteBytes (struct vfsfile_s *file, const void *buffer, int bytestowrite)
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

int VFSTCP_Seek (struct vfsfile_s *file, unsigned long pos, int whence)
{
	Com_Printf("VFSTCP: seek is illegal, closing link\n");
	VFSTCP_Error((tcpfile_t*)file);
	return -1;
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

		newf = Q_calloc(1, sizeof(tcpfile_t));
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

void VFS_TICK(void)
{
	VFSTCP_Tick(); // fill in/out our internall buffers (do read/write on socket)
}
