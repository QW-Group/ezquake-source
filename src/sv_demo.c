/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    
*/

// sv_demo.c - mvd demo related code

#ifndef CLIENTONLY
#include "qwsvdef.h"

// minimal cache which can be used for demos, must be few times greater than DEMO_FLUSH_CACHE_IF_LESS_THAN_THIS
#define DEMO_CACHE_MIN_SIZE 0x1000000

// flush demo cache if we have less than this free bytes
#define DEMO_FLUSH_CACHE_IF_LESS_THAN_THIS	65536


static void sv_demoDir_OnChange(cvar_t *cvar, char *value, qbool *cancel);

cvar_t  sv_demoUseCache     = {"sv_demoUseCache",   "0"};
cvar_t  sv_demoCacheSize    = {"sv_demoCacheSize",  "0", CVAR_ROM};
cvar_t  sv_demoMaxDirSize   = {"sv_demoMaxDirSize", "102400"};
cvar_t  sv_demoClearOld     = {"sv_demoClearOld",   "0"};
cvar_t  sv_demoDir          = {"sv_demoDir",        "demos", 0, sv_demoDir_OnChange};
cvar_t  sv_demoDirAlt       = {"sv_demoDirAlt",     "", 0, sv_demoDir_OnChange };
cvar_t  sv_demofps          = {"sv_demofps",        "30"};
cvar_t  sv_demoIdlefps      = {"sv_demoIdlefps",    "10"};
cvar_t  sv_demoPings        = {"sv_demopings",      "3"};
cvar_t  sv_demoMaxSize      = {"sv_demoMaxSize",    "20480"};
cvar_t  sv_demoExtraNames   = {"sv_demoExtraNames", "0"};

cvar_t	sv_demoPrefix		= {"sv_demoPrefix",		""};
cvar_t	sv_demoSuffix		= {"sv_demoSuffix",		""};
cvar_t	sv_demotxt			= {"sv_demotxt",		"1"};
cvar_t	sv_onrecordfinish	= {"sv_onRecordFinish", ""};

cvar_t	sv_ondemoremove		= {"sv_onDemoRemove",	""};
cvar_t	sv_demoRegexp		= {"sv_demoRegexp",		"\\.mvd(\\.(gz|bz2|rar|zip))?$"};

cvar_t	sv_silentrecord		= {"sv_silentrecord",   "0"};

cvar_t	extralogname		= {"extralogname",		"unset"}; // no sv_ prefix? WTF!

mvddest_t			*singledest;

// only one .. is allowed (security)
static void sv_demoDir_OnChange(cvar_t *cvar, char *value, qbool *cancel)
{
	if (cvar == &sv_demoDir && !value[0]) {
		*cancel = true;
		return;
	}

	if (value[0] == '.' && value[1] == '.') {
		value += 2;
	}

	if (strstr(value, "/..")) {
		*cancel = true;
		return;
	}
}

// { MVD writing functions, just wrappers

void MVD_MSG_WriteChar (const int c)
{
	MSG_WriteChar (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, c);
}

void MVD_MSG_WriteByte (const int c)
{
	MSG_WriteByte (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, c);
}

void MVD_MSG_WriteShort (const int c)
{
	MSG_WriteShort (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, c);
}

void MVD_MSG_WriteLong (const int c)
{
	MSG_WriteLong (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, c);
}

void MVD_MSG_WriteFloat (const float f)
{
	MSG_WriteFloat (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, f);
}

void MVD_MSG_WriteString (const char *s)
{
	MSG_WriteString (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, s);
}

void MVD_MSG_WriteCoord (const float f)
{
	MSG_WriteCoord (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, f);
}

void MVD_MSG_WriteAngle (const float f)
{
	MSG_WriteAngle (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, f);
}

void MVD_SZ_Write (const void *data, int length)
{
	SZ_Write (&demo.frames[demo.parsecount&UPDATE_MASK]._buf_, data, length);
}

// }

mvddest_t *DestByName (char *name)
{
	mvddest_t *d;

	for (d = demo.dest; d; d = d->nextdest)
		if (!strncmp(name, d->name, sizeof(d->name)-1))
			return d;

	return NULL;
}

void DestClose (mvddest_t *d, qbool destroyfiles)
{
	char path[MAX_OSPATH];

	if (d->cache)
		Q_free(d->cache);
	if (d->file)
		fclose(d->file);
	if (d->socket)
		closesocket(d->socket);
	if (d->qtvuserlist)
		QTVsv_FreeUserList(d);

	if (destroyfiles)
	{
		snprintf(path, MAX_OSPATH, "%s/%s/%s", fs_gamedir, d->path, d->name);
		Sys_remove(path);
		strlcpy(path + strlen(path) - 3, "txt", MAX_OSPATH - strlen(path) + 3);
		Sys_remove(path);

		// force cache rebuild.
		FS_FlushFSHash();
	}

	Q_free(d);
}

//
// complete - just force flush for cached dests (dest->desttype == DEST_BUFFEREDFILE)
//
void DestFlush (qbool complete)
{
	int len;
	mvddest_t *d, *t;

	if (!demo.dest)
		return;

	while (demo.dest->error)
	{
		d = demo.dest;
		demo.dest = d->nextdest;

		DestClose(d, false);

		if (!demo.dest)
		{
			SV_MVDStop(3, false);
			return;
		}
	}

	for (d = demo.dest; d; d = d->nextdest)
	{
		switch(d->desttype)
		{
		case DEST_FILE:
			fflush (d->file);
			break;

		case DEST_BUFFEREDFILE:
			if (d->cacheused + DEMO_FLUSH_CACHE_IF_LESS_THAN_THIS > d->maxcachesize || complete)
			{
				len = (int)fwrite(d->cache, 1, d->cacheused, d->file);
				if (len != d->cacheused)
				{
					Sys_Printf("DestFlush: fwrite() error\n");
					d->error = true;
				}
				fflush(d->file);

				d->cacheused = 0;
			}
			break;

		case DEST_STREAM:
			if (d->io_time + qtv_streamtimeout.value <= Sys_DoubleTime())
			{
				// problem what send() have internal buffer, so send() success some time even peer side does't read,
				// this may take some time before internal buffer overflow and timeout trigger, depends of buffer size.
				Sys_Printf("DestFlush: stream timeout\n");
				d->error = true;
			}

			if (d->cacheused && !d->error)
			{
				len = send(d->socket, d->cache, d->cacheused, 0);

				if (len == 0) //client died
				{
//					d->error = true;
					// man says: The calls return the number of characters sent, or -1 if an error occurred.   
					// so 0 is legal or what?
				}
				else if (len > 0) //we put some data through
				{ //move up the buffer
					d->cacheused -= len;
					memmove(d->cache, d->cache+len, d->cacheused);

					d->io_time = Sys_DoubleTime(); // update IO activity
				}
				else
				{ //error of some kind. would block or something
					if (qerrno != EWOULDBLOCK && qerrno != EAGAIN)
					{
						Sys_Printf("DestFlush: error on stream\n");
						d->error = true;
					}
				}
			}
			break;

		case DEST_NONE:
		default:
			Sys_Error("DestFlush: encountered bad dest.");
		}

		if (d->desttype != DEST_STREAM) // no max size for stream
		{
			if ((unsigned int)sv_demoMaxSize.value && d->totalsize > ((unsigned int)sv_demoMaxSize.value * 1024))
			{
				Sys_Printf("DestFlush: sv_demoMaxSize = %db trigger for dest\n", (int)sv_demoMaxSize.value);
				d->error = true;
			}
		}

		while (d->nextdest && d->nextdest->error)
		{
			t = d->nextdest;
			d->nextdest = t->nextdest;

			DestClose(t, false);
		}
	}
}

// if param "mvdonly" == true then close only demos, not QTV's steams
static int DestCloseAllFlush (qbool destroyfiles, qbool mvdonly)
{
	int numclosed = 0;
	mvddest_t *d, **prev, *next;

	DestFlush(true); //make sure it's all written.

	prev = &demo.dest;
	d = demo.dest;
	while (d)
	{
		next = d->nextdest;

		if (!mvdonly || d->desttype != DEST_STREAM)
		{
			desttype_t dt = d->desttype;
			char dest_name[sizeof(d->name)];
			char dest_path[sizeof(d->path)];

			strlcpy(dest_name, d->name, sizeof(dest_name));
			strlcpy(dest_path, d->path, sizeof(dest_path));

			*prev = d->nextdest;
			DestClose(d, destroyfiles); // NOTE: this free dest struck, so we can't use 'd' below
			numclosed++;

			if (dt != DEST_STREAM && dest_name[0]) // ignore stream or empty file name
				Run_sv_demotxt_and_sv_onrecordfinish (dest_name, dest_path, destroyfiles);
		}
		else
			prev = &d->nextdest;

		d = next;
	}

	return numclosed;
}

int DemoWriteDest (void *data, int len, mvddest_t *d)
{
	int ret;

	if (d->error)
		return 0;

	d->totalsize += len;

	switch(d->desttype)
	{
		case DEST_FILE:
			ret = (int)fwrite(data, 1, len, d->file);
			if (ret != len)
			{
				Sys_Printf("DemoWriteDest: fwrite() error\n");
				d->error = true;
				return 0;
			}

			break;
		case DEST_BUFFEREDFILE:	//these write to a cache, which is flushed later
		case DEST_STREAM:
			if (d->cacheused + len > d->maxcachesize)
			{
				Sys_Printf("DemoWriteDest: cache overflow %d > %d\n", d->cacheused + len, d->maxcachesize);
				d->error = true;
				return 0;
			}
			memcpy(d->cache + d->cacheused, data, len);
			d->cacheused += len;

			break;
		case DEST_NONE:
		default:
			Sys_Error("DemoWriteDest: encountered bad dest.");
	}
	return len;
}

static void DemoWrite (void *data, int len) //broadcast to all proxies/mvds
{
	mvddest_t *d;
	for (d = demo.dest; d; d = d->nextdest)
	{
		if (singledest && singledest != d)
			continue;

		DemoWriteDest(data, len, d);
	}
}

/*
====================
MVDWrite_Begin
====================
*/

static qbool MVDWrite_BeginEx (byte type, int to, int size)
{
	qbool new_mvd_msg;

	if (!sv.mvdrecording)
		return false;

	// we alredy overflowed, sorry but it no go
	if (demo.frames[demo.parsecount&UPDATE_MASK]._buf_.overflowed)
		return false; // ERROR

	if (size > MAX_MVD_SIZE)
	{
		Sys_Printf("MVDWrite_Begin: bad demo message size: %d > MAX_MVD_SIZE\n", size);
		return false; // ERROR
	}

	// check we have proper demo message type
	switch (type)
	{
	case dem_all: case dem_multiple: case dem_single: case dem_stats:
		break;
	default:
		Sys_Printf("MVDWrite_Begin: bad demo message type: %d\n", type);
		return false; // ERROR
	}

	new_mvd_msg =    demo.frames[demo.parsecount&UPDATE_MASK].lasttype != type
				  || demo.frames[demo.parsecount&UPDATE_MASK].lastto   != to
				  || demo.frames[demo.parsecount&UPDATE_MASK].lastsize + size > MAX_MVD_SIZE;

	if (new_mvd_msg)
	{
		// we need add mvd header for next message since "type" or "to" differ from previous message,
		// or it first message in this frame.

		MVD_MSG_WriteByte(0); // 0 milliseconds

		demo.frames[demo.parsecount&UPDATE_MASK].lasttype = type;
		demo.frames[demo.parsecount&UPDATE_MASK].lastto   = to;
		demo.frames[demo.parsecount&UPDATE_MASK].lastsize = size; // set initial size

		switch (type)
		{
		case dem_all:
			MVD_MSG_WriteByte(dem_all);
			break;
		case dem_multiple:
			MVD_MSG_WriteByte(dem_multiple);
			MVD_MSG_WriteLong(to);
			break;
		case dem_single:
		case dem_stats:
			MVD_MSG_WriteByte(type | (to << 3)); // msg "type" and "to" incapsulated in one byte
			break;
		default:
			return false; // ERROR: wrong type
		}

		MVD_MSG_WriteLong(size); // msg size

		if (demo.frames[demo.parsecount&UPDATE_MASK]._buf_.overflowed)
			return false; // ERROR: overflow

		// THIS IS TRICKY, SORRY.
		// remember size offset, so latter we can access it
		demo.frames[demo.parsecount&UPDATE_MASK].lastsize_offset = demo.frames[demo.parsecount&UPDATE_MASK]._buf_.cursize - 4;
	}
	else
	{
		// we must alredy have mvd header here of previous message, so just change size in header
		byte *buf;
		int lastsize;

		demo.frames[demo.parsecount&UPDATE_MASK].lastsize += size;

		// THIS IS TRICKY, SORRY.
		// and here we use size offset
		lastsize = demo.frames[demo.parsecount&UPDATE_MASK].lastsize;

		buf = demo.frames[demo.parsecount&UPDATE_MASK]._buf_.data + demo.frames[demo.parsecount&UPDATE_MASK].lastsize_offset;

		buf[0] = lastsize&0xff;
		buf[1] = (lastsize>> 8)&0xff;
		buf[2] = (lastsize>>16)&0xff;
		buf[3] = (lastsize>>24)&0xff;
	}

	return (sv.mvdrecording ? true : false);
}

qbool MVDWrite_Begin (byte type, int to, int size)
{
	if (!sv.mvdrecording)
		return false;

	if (!MVDWrite_BeginEx(type, to, size))
	{
		Con_DPrintf("MVDWrite_Begin: error\n");

		if (sv.mvdrecording)
			SV_MVDStop(4, false); // stop all mvd recording

		return false;
	}

	return (sv.mvdrecording ? true : false);
}

qbool MVDWrite_HiddenBlockBegin(int length)
{
	return MVDWrite_Begin(dem_multiple, 0, length);
}

qbool MVDWrite_HiddenBlock(const void* data, int length)
{
	if (MVDWrite_HiddenBlockBegin(length)) {
		MVD_SZ_Write(data, length);
		return true;
	}
	return false;
}

/*
====================
MVD_FrameDeltaTime

Get frame time mark (delta between two demo frames).

Also advance demo.prevtime.

====================
*/
static byte MVD_FrameDeltaTime (double time1)
{
	int	msec;

	if (!sv.mvdrecording)
		return 0;

	msec = (time1 - demo.prevtime) * 1000;
	demo.prevtime += 0.001 * msec;

	if (msec > 255)
		msec = 255;
	if (msec < 2)
		msec = 0; // uh, why 0 but not 2? 

	return (byte)msec;
}

/*
====================
MVD_WriteMessage
====================
*/
static qbool MVD_WriteMessage (sizebuf_t *msg, byte msec)
{
	int		len;
	byte	c;

	if (!sv.mvdrecording)
		return false;

	if (msg && msg->overflowed)
		return false; // ERROR

	c = msec;
	DemoWrite(&c, sizeof(c));

	c = dem_all;
	DemoWrite (&c, sizeof(c));

	if (msg && msg->cursize)
	{
		len = LittleLong (msg->cursize);
		DemoWrite (&len, 4);
		DemoWrite (msg->data, msg->cursize);
	}
	else
	{
		len = LittleLong (0);
		DemoWrite (&len, 4);
	}

	return (sv.mvdrecording ? true : false);
}

static void SV_MVDWritePausedTimeToStreams(demo_frame_t* frame)
{
	// When writing out a paused frame, send a packet to let QTV keep delay in sync
	if (frame->paused) {
		mvddest_t* d;
		sizebuf_t msg;
		byte msg_buffer[128];
		byte duration = frame->pause_duration;

		SZ_Init(&msg, msg_buffer, sizeof(msg_buffer));
		MSG_WriteByte(&msg, 0);                                           //     0: duration == 0, for demos
		MSG_WriteByte(&msg, dem_multiple);                                //     1: target of the packet
		MSG_WriteLong(&msg, 0);                                           //  2- 5: 0 ... demo_multiple(0) => hidden packet
		MSG_WriteLong(&msg, LittleLong(sizeof(short) + sizeof(byte)));    //  6-10: length = <short> + <byte>
		MSG_WriteShort(&msg, LittleShort(mvdhidden_paused_duration));     // 11-12: tell QTV how much time has really passed
		MSG_WriteByte(&msg, duration);                                    //    13: true ms value, as demo packets will have 0

		for (d = demo.dest; d; d = d->nextdest) {
			if (d->desttype == DEST_STREAM) {
				DemoWriteDest(msg.data, msg.cursize, d);
			}
		}
	}
}

/*
====================
SV_MVDWritePackets

Interpolates to get exact players position for current frame
and writes packets to the disk/memory
====================
*/
static qbool SV_MVDWritePacketsEx (int num)
{
	demo_frame_t	*frame, *nextframe;
	demo_client_t	*cl, *nextcl = NULL, *last_cl;
	int				i, j, flags;
	qbool			valid;
	double			time1, playertime, nexttime;
	vec3_t			origin, angles;
	sizebuf_t		msg;
	byte			msg_buf[MAX_MVD_SIZE]; // data without mvd header
	byte			msec;

	if (!sv.mvdrecording)
		return false;

	// allow overflow, but cancel demo recording in case of overflow
	SZ_InitEx(&msg, msg_buf, sizeof(msg_buf), true);

	if (num > demo.parsecount - demo.lastwritten + 1)
		num = demo.parsecount - demo.lastwritten + 1;

	// 'num' frames to write
	for ( ; num; num--, demo.lastwritten++)
	{
		SZ_Clear(&msg);

		frame = &demo.frames[demo.lastwritten&UPDATE_MASK];
		time1 = frame->time;
		nextframe = frame;

		SV_MVDWritePausedTimeToStreams(frame);

		// find two frames
		// one before the exact time (time - msec) and one after,
		// then we can interpolte exact position for current frame
		for (i = 0, cl = frame->clients, last_cl = demo.clients; i < MAX_CLIENTS; i++, cl++, last_cl++)
		{
			if (cl->parsecount != demo.lastwritten)
				continue; // not valid

			if (!cl->parsecount && svs.clients[i].state != cs_spawned)
				continue; // not valid, occur on first frame

			nexttime = playertime = time1 - cl->sec;

			valid = false;

			for (j = demo.lastwritten+1; nexttime < time1 && j < demo.parsecount; j++)
			{
				nextframe = &demo.frames[j&UPDATE_MASK];
				nextcl = &nextframe->clients[i];

				if (nextcl->parsecount != j)
					break; // disconnected?
				if (nextcl->fixangle)
					break; // respawned, or walked into teleport, do not interpolate!
				if (!(nextcl->flags & DF_DEAD) && (cl->flags & DF_DEAD))
					break; // respawned, do not interpolate

				nexttime = nextframe->time - nextcl->sec;

				if (nexttime >= time1)
				{
					// good, found what we were looking for
					valid = true;
					break;
				}
			}

			if (valid)
			{
				float f = 0;
				float z = nexttime - playertime;

				if ( z )
					f = (time1 - nexttime) / z;

				for (j = 0; j < 3; j++)
				{
					angles[j] = AdjustAngle(cl->angles[j], nextcl->angles[j], 1.0 + f);
					origin[j] = nextcl->origin[j] + f * (nextcl->origin[j] - cl->origin[j]);
				}
			}
			else
			{
				VectorCopy(cl->origin, origin);
				VectorCopy(cl->angles, angles);
			}

			// now write it to buf
			flags = cl->flags;

			for (j = 0; j < 3; j++)
				if (origin[j] != last_cl->origin[j])
					flags |= DF_ORIGIN << j;

			for (j = 0; j < 3; j++)
				if (angles[j] != last_cl->angles[j])
					flags |= DF_ANGLES << j;

			if (cl->model != last_cl->model)
				flags |= DF_MODEL;
			if (cl->effects != last_cl->effects)
				flags |= DF_EFFECTS;
			if (cl->skinnum != last_cl->skinnum)
				flags |= DF_SKINNUM;
			if (cl->weaponframe != last_cl->weaponframe)
				flags |= DF_WEAPONFRAME;

			MSG_WriteByte (&msg, svc_playerinfo);
			MSG_WriteByte (&msg, i);
			MSG_WriteShort (&msg, flags);

			MSG_WriteByte (&msg, cl->frame);

			for (j = 0 ; j < 3 ; j++)
				if (flags & (DF_ORIGIN << j))
					MSG_WriteCoord (&msg, origin[j]);

			for (j = 0 ; j < 3 ; j++)
				if (flags & (DF_ANGLES << j))
					MSG_WriteAngle16 (&msg, angles[j]);

			if (flags & DF_MODEL)
				MSG_WriteByte (&msg, cl->model);

			if (flags & DF_SKINNUM)
				MSG_WriteByte (&msg, cl->skinnum);

			if (flags & DF_EFFECTS)
				MSG_WriteByte (&msg, cl->effects);

			if (flags & DF_WEAPONFRAME)
				MSG_WriteByte (&msg, cl->weaponframe);

			cl->flags = flags;

			// save in last_cl what we wrote to msg so later we can delta from it
			*last_cl = *cl; // struct copy
		}

		// get frame time mark (delta between two frames)
		msec = MVD_FrameDeltaTime(time1);

		if (demo.frames[demo.lastwritten&UPDATE_MASK]._buf_.overflowed)
		{
			Con_DPrintf("SV_MVDWritePackets: error: in _buf_.overflowed\n");
			return false; // ERROR
		}

		// write cumulative data from different sources
		if (demo.frames[demo.lastwritten&UPDATE_MASK]._buf_.cursize)
		{
			// well, first byte must be milliseconds, set it then
			demo.frames[demo.lastwritten&UPDATE_MASK]._buf_.data[0] = msec;
			DemoWrite(demo.frames[demo.lastwritten&UPDATE_MASK]._buf_.data, demo.frames[demo.lastwritten&UPDATE_MASK]._buf_.cursize);

			msec = 0; // That matter, we wrote time mark, so next data(if any) in this frame follow with zero milliseconds offset, since it same frame.
					  // NOTE: demo.frames[demo.lastwritten&UPDATE_MASK]._buf_.cursize possible to be zero, in this case we wrote msec below...
		}

		// Write data about players, if we have data.
		// also if we does't have data and did't wrote msec above, we wrote it here, even packet will be empty, this will break fuh,
		// but I think I correct, also it doubtfull what we did't wrote msec above.
		if (msg.cursize || msec)
		{
			if (!MVD_WriteMessage(&msg, msec))
			{
				Con_DPrintf("SV_MVDWritePackets: error: in msg\n");
				return false; // ERROR
			}
		}

		// { reset frame for future usage
		SZ_Clear(&demo.frames[demo.lastwritten&UPDATE_MASK]._buf_);

		demo.frames[demo.lastwritten&UPDATE_MASK].lastto = 0;
		demo.frames[demo.lastwritten&UPDATE_MASK].lasttype = 0;
		demo.frames[demo.lastwritten&UPDATE_MASK].lastsize = 0;
		demo.frames[demo.lastwritten&UPDATE_MASK].lastsize_offset = 0;
		// }

		if (!sv.mvdrecording)
		{
			Con_DPrintf("SV_MVDWritePackets: error: in sv.mvdrecording\n");
			return false; // ERROR
		}
	}

	if (!sv.mvdrecording)
		return false; // ERROR

	if (demo.lastwritten > demo.parsecount)
		demo.lastwritten = demo.parsecount;

	return true;
}

qbool SV_MVDWritePackets (int num)
{
	if (!sv.mvdrecording)
		return false;

	if (!SV_MVDWritePacketsEx(num))
	{
		Con_DPrintf("SV_MVDWritePackets: error\n");

		if (sv.mvdrecording)
			SV_MVDStop(4, false); // stop all mvd recording

		return false;
	}

	return (sv.mvdrecording ? true : false);
}

/*
====================
SV_InitRecord
====================
*/
static mvddest_t *SV_InitRecordFile (char *name)
{
	char *s;
	mvddest_t *dst;
	FILE *file;

	char path[MAX_OSPATH];

	Con_DPrintf("SV_InitRecordFile: Demo name: \"%s\"\n", name);
	file = fopen (name, "wb");
	if (!file)
	{
		Con_Printf ("ERROR: couldn't open \"%s\"\n", name);
		return NULL;
	}

	dst = (mvddest_t*) Q_malloc (sizeof(mvddest_t));

	if (!(int)sv_demoUseCache.value)
	{
		dst->desttype = DEST_FILE;
		dst->file = file;
		dst->maxcachesize = 0;
	}
	else
	{
		dst->desttype = DEST_BUFFEREDFILE;
		dst->file = file;
		dst->maxcachesize = 1024 * (int) sv_demoCacheSize.value;
		dst->cache = (char *) Q_malloc (dst->maxcachesize);
	}

	s = name + strlen(name);
	while (*s != '/') s--;
	strlcpy(dst->name, s+1, sizeof(dst->name));
	strlcpy(dst->path, sv_demoDir.string, sizeof(dst->path));

	if ( !sv_silentrecord.value )
		SV_BroadcastPrintf (PRINT_CHAT, "Server starts recording (%s):\n%s\n",
		                    (dst->desttype == DEST_BUFFEREDFILE) ? "memory" : "disk", s+1);
	Cvar_SetROM(&serverdemo, dst->name);

	strlcpy(path, name, MAX_OSPATH);
	strlcpy(path + strlen(path) - 3, "txt", MAX_OSPATH - strlen(path) + 3);

	if ((int)sv_demotxt.value)
	{
		FILE *f;
		char *text;

		if (sv_demotxt.value == 2)
		{
			if ((f = fopen (path, "a+t")))
				fclose(f); // at least made empty file
		}
		else if ((f = fopen (path, "w+t")))
		{
			text = SV_PrintTeams();
			fwrite(text, strlen(text), 1, f);
			fflush(f);
			fclose(f);
		}
	}
	else
		Sys_remove(path);

	// force cache rebuild.
	FS_FlushFSHash();

	return dst;
}

static void SV_AddLastDemo(void)
{
	char *name = NULL;
	mvddest_t *d;

	for (d = demo.dest; d; d = d->nextdest)
	{
		if (d->desttype != DEST_STREAM && d->name[0])
		{
			name = d->name;
			break; // we found file dest with non empty name, use it as last demo name
		}
	}

	if (name && name[0])
	{
		size_t name_len = strlen(name) + 1;

		demo.lastdemospos = (demo.lastdemospos + 1) & 0xF;
		Q_free(demo.lastdemosname[demo.lastdemospos]);
		demo.lastdemosname[demo.lastdemospos] = (char *) Q_malloc(name_len);
		strlcpy(demo.lastdemosname[demo.lastdemospos], name, name_len);
		Con_DPrintf("SV_MVDStop: Demo name for 'cmd dl .': \"%s\"\n", demo.lastdemosname[demo.lastdemospos]);
	}
}

/*
====================
SV_MVDStop

stop recording a demo
====================
*/
void SV_MVDStop (int reason, qbool mvdonly)
{
	static qbool instop = false;

	int numclosed;

	if (instop)
	{
		Con_Printf("SV_MVDStop: recursion\n");
		return;
	}

	if (!sv.mvdrecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

	instop = true; // SET TO TRUE, DON'T FORGET SET TO FALSE ON RETURN

	if (reason == 2 || reason == 3 || reason == 4)
	{
		if (reason == 4)
			mvdonly = false; // if serious error, then close all dests including qtv streams

		// stop and remove
		DestCloseAllFlush(true, mvdonly);

		if (!demo.dest)
			sv.mvdrecording = false;

		if (reason == 4)
			SV_BroadcastPrintf (PRINT_CHAT, "Error in MVD/QTV recording, recording stopped\n");
		else if (reason == 3)
			SV_BroadcastPrintf (PRINT_CHAT, "QTV disconnected\n");
		else
		{
			if ( !sv_silentrecord.value )
				SV_BroadcastPrintf (PRINT_CHAT, "Server recording canceled, demo removed\n");
		}

		Cvar_SetROM(&serverdemo, "");

		instop = false; // SET TO FALSE

		return;
	}
	
	// write a disconnect message to the demo file
	// FIXME: qqshka: add clearup to be sure message will fit!!!

	if (MVDWrite_Begin(dem_all, 0, 2+strlen("EndOfDemo")))
	{
		MVD_MSG_WriteByte (svc_disconnect);
		MVD_MSG_WriteString ("EndOfDemo");
	}

	// finish up
	SV_MVDWritePackets(demo.parsecount - demo.lastwritten + 1);

	// last recorded demo's names for command "cmd dl . .." (maximum 15 dots)
	SV_AddLastDemo();

	numclosed = DestCloseAllFlush(false, mvdonly);

	if (!demo.dest)
		sv.mvdrecording = false;

	if (numclosed)
	{
		if (!reason)
		{
			if ( !sv_silentrecord.value )
				SV_BroadcastPrintf (PRINT_CHAT, "Server recording completed\n");
		}
		else
			SV_BroadcastPrintf (PRINT_CHAT, "Server recording stopped\nMax demo size exceeded\n");
	}

	Cvar_SetROM(&serverdemo, "");

	instop = false; // SET TO FALSE
}

/*
====================
SV_MVDStop_f
====================
*/
void SV_MVDStop_f (void)
{
	SV_MVDStop(0, true);
}

/*
====================
SV_MVD_Cancel_f

Stops recording, and removes the demo
====================
*/
void SV_MVD_Cancel_f (void)
{
	SV_MVDStop(2, true);
}

/*
====================
SV_WriteRecordMVDMessage
====================
*/
static void SV_WriteRecordMVDMessage (sizebuf_t *msg)
{
	int len;
	byte c;

	if (!sv.mvdrecording)
		return;

	if (!msg->cursize)
		return;

	c = 0;
	DemoWrite (&c, sizeof(c));

	c = dem_all;
	DemoWrite (&c, sizeof(c));

	len = LittleLong (msg->cursize);
	DemoWrite (&len, 4);

	DemoWrite (msg->data, msg->cursize);
}

/*
====================
SV_WriteRecordMVDStatsMessage
====================
*/
static void SV_WriteRecordMVDStatsMessage (sizebuf_t *msg, int client)
{
	int len;
	byte c;

	if (!sv.mvdrecording)
		return;

	if (!msg->cursize)
		return;

	if (client < 0 || client >= MAX_CLIENTS)
		return;

	c = 0;
	DemoWrite (&c, sizeof(c));

	c = dem_stats | (client << 3) ; // msg "type" and "to" incapsulated in one byte
	DemoWrite (&c, sizeof(c));

	len = LittleLong (msg->cursize);
	DemoWrite (&len, 4);

	DemoWrite (msg->data, msg->cursize);
}


/*
static void SV_WriteSetMVDMessage (void)
{
	int len;
	byte c;

	if (!sv.mvdrecording)
		return;

	c = 0;
	DemoWrite (&c, sizeof(c));

	c = dem_set;
	DemoWrite (&c, sizeof(c));


	len = LittleLong(0);
	DemoWrite (&len, 4);
	len = LittleLong(0);
	DemoWrite (&len, 4);
}
*/

// mvd/qtv related stuff
// Well, here is a chance what player connect after demo recording started,
// so demo.info[edictnum - 1].model == player_model so SV_MVDWritePackets() will not wrote player model index,
// so client during playback this demo will got invisible model, because model index will be 0.
// Fixing that.
// Btw, struct demo contain different client specific structs, may be they need clearing too, not sure.
void MVD_PlayerReset(int player)
{
	if (player < 0 || player >= MAX_CLIENTS) { // protect from lamers
		Con_Printf("MVD_PlayerReset: wrong player num %d\n", player);
		return;
	}

	memset(&(demo.clients[player]), 0, sizeof(demo.clients[0]));
}

qbool SV_MVD_Record (mvddest_t *dest, qbool mapchange)
{
	int i;

	if (mapchange)
	{
		if (dest) // during mapchange dest must be NULL
			return false;
	}
	else
	{
		if (!dest) // in non mapchange dest must be not NULL
			return false;
	}

	// If we initialize MVD recording, then reset some data structs
	if (!sv.mvdrecording)
	{
		// this is either mapchange and we have QTV connected
		// or we just use /record or whatever command first time and here no recording yet

    	// and here we memset() not whole demo_t struct, but part,
    	// so demo.dest and demo.pendingdest is not overwriten
		memset(&demo, 0, (int)((uintptr_t)&(((demo_t *)0)->mem_set_point)));

		for (i = 0; i < UPDATE_BACKUP; i++)
		{
			// set up buffer for record in each frame
			SZ_InitEx(&demo.frames[i]._buf_, demo.frames[i]._buf__data, sizeof(demo.frames[0]._buf__data), true);
		}

		// set up buffer for non releable data
		SZ_InitEx(&demo.datagram, demo.datagram_data, sizeof(demo.datagram_data), true);
	}

	if (mapchange)
	{
		//
		// map change, sent initial stats to all dests
		//
		SV_MVD_SendInitialGamestate(NULL);
	}
	else
	{
		//
		// seems we initializing new dest, sent initial stats only to this dest
		//
		dest->nextdest = demo.dest;
		demo.dest = dest;

		SV_MVD_SendInitialGamestate(dest);
	}

	// done
	return true;
}

void SV_MVD_SendInitialGamestate(mvddest_t* dest)
{
	sizebuf_t	buf;
	unsigned char buf_data[MAX_MSGLEN];
	unsigned int n;
	char* s, info[MAX_EXT_INFO_STRING];

	client_t* player;
	edict_t* ent;
	char* gamedir;
	int i;

	if (!demo.dest)
		return;

	sv.mvdrecording = true; // NOTE:  afaik set to false on map change, so restore it here
	demo.pingtime = demo.time = sv.time;
	singledest = dest;

	/*-------------------------------------------------*/

	// serverdata
	// send the info about the new client to all connected clients
	SZ_Init(&buf, buf_data, sizeof(buf_data));

	// send the serverdata

	gamedir = Info_ValueForKey(svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

	MSG_WriteByte(&buf, svc_serverdata);

#ifdef FTE_PEXT_FLOATCOORDS
	//fix up extensions to match sv_bigcoords correctly. sorry for old clients not working.
	if (msg_coordsize == 4)
		demo.recorder.fteprotocolextensions |= FTE_PEXT_FLOATCOORDS;
	else
		demo.recorder.fteprotocolextensions &= ~FTE_PEXT_FLOATCOORDS;
#endif

#ifdef PROTOCOL_VERSION_FTE
	if (demo.recorder.fteprotocolextensions)
	{
		MSG_WriteLong(&buf, PROTOCOL_VERSION_FTE);
		MSG_WriteLong(&buf, demo.recorder.fteprotocolextensions);
	}
#endif

#ifdef PROTOCOL_VERSION_FTE2
	if (demo.recorder.fteprotocolextensions2)
	{
		MSG_WriteLong(&buf, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong(&buf, demo.recorder.fteprotocolextensions2);
	}
#endif

#ifdef PROTOCOL_VERSION_MVD1
	demo.recorder.mvdprotocolextensions1 |= MVD_PEXT1_HIDDEN_MESSAGES;
	if (demo.recorder.mvdprotocolextensions1)
	{
		MSG_WriteLong(&buf, PROTOCOL_VERSION_MVD1);
		MSG_WriteLong(&buf, demo.recorder.mvdprotocolextensions1);
	}
#endif

	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, svs.spawncount);
	MSG_WriteString (&buf, gamedir);


	MSG_WriteFloat (&buf, sv.time);

	// send full levelname
	MSG_WriteString (&buf, PR_GetEntityString(sv.edicts->v->message));

	// send the movevars
	MSG_WriteFloat(&buf, movevars.gravity);
	MSG_WriteFloat(&buf, movevars.stopspeed);
	MSG_WriteFloat(&buf, movevars.maxspeed);
	MSG_WriteFloat(&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat(&buf, movevars.accelerate);
	MSG_WriteFloat(&buf, movevars.airaccelerate);
	MSG_WriteFloat(&buf, movevars.wateraccelerate);
	MSG_WriteFloat(&buf, movevars.friction);
	MSG_WriteFloat(&buf, movevars.waterfriction);
	MSG_WriteFloat(&buf, movevars.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", svs.info) );

	// flush packet
	SV_WriteRecordMVDMessage (&buf);
	SZ_Clear (&buf);

	// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.sound_precache[n+1];
	while (s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.sound_precache[n+1];
	}

	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.model_precache[n+1];
	while (s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.model_precache[n+1];
	}

	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// static entities
	{
		int i, j;
		entity_state_t from = { 0 };

		for (i = 0; i < sv.static_entity_count; ++i) {
			entity_state_t* s = &sv.static_entities[i];

			if (buf.cursize >= MAX_MSGLEN/2) {
				SV_WriteRecordMVDMessage (&buf);
				SZ_Clear (&buf);
			}

			if (demo.recorder.fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2) {
				MSG_WriteByte(&buf, svc_fte_spawnstatic2);
				SV_WriteDelta(&demo.recorder, &from, s, &buf, true);
			}
			else if (s->modelindex < 256) {
				MSG_WriteByte(&buf, svc_spawnstatic);
				MSG_WriteByte(&buf, s->modelindex);
				MSG_WriteByte(&buf, s->frame);
				MSG_WriteByte(&buf, s->colormap);
				MSG_WriteByte(&buf, s->skinnum);
				for (j = 0; j < 3; ++j) {
					MSG_WriteCoord(&buf, s->origin[j]);
					MSG_WriteAngle(&buf, s->angles[j]);
				}
			}
		}
	}

	// entity baselines
	{
		static entity_state_t empty_baseline = { 0 };
		int i, j;

		for (i = 0; i < sv.num_baseline_edicts; ++i) {
			edict_t* svent = EDICT_NUM(i);
			entity_state_t* s = &svent->e.baseline;

			if (buf.cursize >= MAX_MSGLEN/2) {
				SV_WriteRecordMVDMessage (&buf);
				SZ_Clear (&buf);
			}

			if (!s->number || !s->modelindex || !memcmp(s, &empty_baseline, sizeof(empty_baseline))) {
				continue;
			}

			if (demo.recorder.fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2) {
				MSG_WriteByte(&buf, svc_fte_spawnbaseline2);
				SV_WriteDelta(&demo.recorder, &empty_baseline, s, &buf, true);
			}
			else if (s->modelindex < 256) {
				MSG_WriteByte(&buf, svc_spawnbaseline);
				MSG_WriteShort(&buf, i);
				MSG_WriteByte(&buf, s->modelindex);
				MSG_WriteByte(&buf, s->frame);
				MSG_WriteByte(&buf, s->colormap);
				MSG_WriteByte(&buf, s->skinnum);
				for (j = 0; j < 3; j++) {
					MSG_WriteCoord(&buf, s->origin[j]);
					MSG_WriteAngle(&buf, s->angles[j]);
				}
			}
		}
	}

	// prespawn
	for (n = 0; n < sv.num_signon_buffers; n++)
	{
		if (buf.cursize+sv.signon_buffer_size[n] > MAX_MSGLEN/2)
		{
			SV_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
		SZ_Write (&buf,
		          sv.signon_buffers[n],
		          sv.signon_buffer_size[n]);
	}

	if (buf.cursize > MAX_MSGLEN/2)
	{
		SV_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i 0\n",svs.spawncount) );

	if (buf.cursize)
	{
		SV_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		player = svs.clients + i;

		// there spectators NOT ignored, since this info required, at least userinfo

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->old_frags);

		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, SV_CalcPing(player));

		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->lossage);

		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, SV_ClientGameTime(player));

		Info_ReverseConvert(&player->_userinfoshort_ctx_, info, sizeof(info));
		Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, info);

		if (buf.cursize > MAX_MSGLEN/2)
		{
			SV_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	if (buf.cursize)
	{
		SV_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// this set proper model origin and angles etc for players
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		vec3_t origin, angles;
		int j, flags;

		player = svs.clients + i;
		ent = player->edict;

		if (player->state != cs_spawned)
			continue;

		if (player->spectator)
			continue; // ignore specs

		flags =   (DF_ORIGIN << 0) | (DF_ORIGIN << 1) | (DF_ORIGIN << 2)
				| (DF_ANGLES << 0) | (DF_ANGLES << 1) | (DF_ANGLES << 2)
				| DF_EFFECTS | DF_SKINNUM 
				| (ent->v->health <= 0 ? DF_DEAD : 0)
				| (ent->v->mins[2] != -24 ? DF_GIB : 0)
				| DF_WEAPONFRAME | DF_MODEL;

		VectorCopy(ent->v->origin, origin);
		VectorCopy(ent->v->angles, angles);
		angles[0] *= -3;
#ifdef USE_PR2
		if( player->isBot )
			VectorCopy(ent->v->v_angle, angles);
#endif
		angles[2] = 0; // no roll angle

		if (ent->v->health <= 0)
		{	// don't show the corpse looking around...
			angles[0] = 0;
			angles[1] = ent->v->angles[1];
			angles[2] = 0;
		}

		MSG_WriteByte (&buf, svc_playerinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, flags);

		MSG_WriteByte (&buf, ent->v->frame);

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ORIGIN << j))
				MSG_WriteCoord (&buf, origin[j]);

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ANGLES << j))
				MSG_WriteAngle16 (&buf, angles[j]);

		if (flags & DF_MODEL)
			MSG_WriteByte (&buf, ent->v->modelindex);

		if (flags & DF_SKINNUM)
			MSG_WriteByte (&buf, ent->v->skin);

		if (flags & DF_EFFECTS)
			MSG_WriteByte (&buf, ent->v->effects);

		if (flags & DF_WEAPONFRAME)
			MSG_WriteByte (&buf, ent->v->weaponframe);

		if (buf.cursize > MAX_MSGLEN/2)
		{
			SV_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	// we really need clear buffer before sending stats
	if (buf.cursize)
	{
		SV_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// send stats
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		int		stats[MAX_CL_STATS];
		int		j;

		player = svs.clients + i;
		ent = player->edict;

		if (player->state != cs_spawned)
			continue;

		if (player->spectator)
			continue;

		memset(stats, 0, sizeof(stats));

		stats[STAT_HEALTH]       = ent->v->health;
		stats[STAT_WEAPON]       = SV_ModelIndex(PR_GetEntityString(ent->v->weaponmodel));
		stats[STAT_AMMO]         = ent->v->currentammo;
		stats[STAT_ARMOR]        = ent->v->armorvalue;
		stats[STAT_SHELLS]       = ent->v->ammo_shells;
		stats[STAT_NAILS]        = ent->v->ammo_nails;
		stats[STAT_ROCKETS]      = ent->v->ammo_rockets;
		stats[STAT_CELLS]        = ent->v->ammo_cells;
		stats[STAT_ACTIVEWEAPON] = ent->v->weapon;

		if (ent->v->health > 0) // viewheight for PF_DEAD & PF_GIB is hardwired
			stats[STAT_VIEWHEIGHT] = ent->v->view_ofs[2];

		// stuff the sigil bits into the high bits of items for sbar
		stats[STAT_ITEMS] = (int) ent->v->items | ((int) PR_GLOBAL(serverflags) << 28);

		for (j = 0; j < MAX_CL_STATS; j++)
		{
			if (stats[j] >= 0 && stats[j] <= 255)
			{
				MSG_WriteByte(&buf, svc_updatestat);
				MSG_WriteByte(&buf, j);
				MSG_WriteByte(&buf, stats[j]);
			}
			else
			{
				MSG_WriteByte(&buf, svc_updatestatlong);
				MSG_WriteByte(&buf, j);
				MSG_WriteLong(&buf, stats[j]);
			}
		}

		if (buf.cursize)
		{
			SV_WriteRecordMVDStatsMessage(&buf, i);
			SZ_Clear (&buf);
		}
	}

	// above stats writing must clear buffer
	if (buf.cursize)
	{
		Sys_Error("SV_MVD_SendInitialGamestate: buf.cursize %d", buf.cursize);
	}

	// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, sv.lightstyles[i]);
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, "skins\n");

	SV_WriteRecordMVDMessage (&buf);

//	SV_WriteSetMVDMessage();

	singledest = NULL;
}

/*
====================
SV_MVD_Record_f

record <demoname>
====================
*/
void SV_MVD_Record_f (void)
{
	int c;
	char name[MAX_OSPATH+MAX_DEMO_NAME];
	char newname[MAX_DEMO_NAME];

	c = Cmd_Argc();
	if (c != 2)
	{
		Con_Printf ("record <demoname>\n");
		return;
	}

	if (sv.state != ss_active)
	{
		Con_Printf ("Not active yet.\n");
		return;
	}

	// clear old demos
	if (!SV_DirSizeCheck())
		return;

	strlcpy(newname, va("%s%s%s", sv_demoPrefix.string, SV_CleanName((unsigned char*)Cmd_Argv(1)),
						sv_demoSuffix.string), sizeof(newname) - 4);

	Sys_mkdir(va("%s/%s", fs_gamedir, sv_demoDir.string));

	snprintf (name, sizeof(name), "%s/%s/%s", fs_gamedir, sv_demoDir.string, newname);

	if ((c = strlen(name)) > 3)
		if (strcmp(name + c - 4, ".mvd"))
			strlcat(name, ".mvd", sizeof(name));

	//
	// open the demo file and start recording
	//
	SV_MVD_Record (SV_InitRecordFile(name), false);
}

/*
====================
SV_MVDEasyRecord_f

easyrecord [demoname]
====================
*/

void SV_MVDEasyRecord_f (void)
{
	int		c;
	char	name[MAX_DEMO_NAME];
	char	name2[MAX_OSPATH*7]; // scream
	char	name4[MAX_OSPATH*7]; // scream

	int		i;
	dir_t	dir;
	char	*name3;

	c = Cmd_Argc();
	if (c > 2)
	{
		Con_Printf ("easyrecord [demoname]\n");
		return;
	}

	if (!SV_DirSizeCheck()) // clear old demos
		return;

	if (c == 2)
		strlcpy (name, Cmd_Argv(1), sizeof(name));
	else
	{
		i = Dem_CountPlayers();
		if ((int)teamplay.value >= 1 && i > 2)
		{
			// Teamplay
			snprintf (name, sizeof(name), "%don%d_", Dem_CountTeamPlayers(Dem_Team(1)), Dem_CountTeamPlayers(Dem_Team(2)));
			if ((int)sv_demoExtraNames.value > 0)
			{
				strlcat (name, va("[%s]_%s_vs_[%s]_%s_%s",
				                  Dem_Team(1), Dem_PlayerNameTeam(Dem_Team(1)),
				                  Dem_Team(2), Dem_PlayerNameTeam(Dem_Team(2)),
				                  sv.mapname), sizeof(name));
			}
			else
				strlcat (name, va("%s_vs_%s_%s", Dem_Team(1), Dem_Team(2), sv.mapname), sizeof(name));
		}
		else
		{
			if (i == 2)
			{
				// Duel
				snprintf (name, sizeof(name), "duel_%s_vs_%s_%s",
				          Dem_PlayerName(1),
				          Dem_PlayerName(2),
				          sv.mapname);
			}
			else
			{
				// FFA
				snprintf (name, sizeof(name), "ffa_%s(%d)", sv.mapname, i);
			}
		}
	}

	// <-

	// Make sure the filename doesn't contain illegal characters
	strlcpy(name, va("%s%s%s", sv_demoPrefix.string,
			 SV_CleanName((unsigned char*)name), sv_demoSuffix.string), MAX_DEMO_NAME);
	strlcpy(name2, name, sizeof(name2));
	Sys_mkdir(va("%s/%s", fs_gamedir, sv_demoDir.string));

	// FIXME: very SLOW
	if (!(name3 = quote(name2)))
		return;
	dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string),
					  va("^%s%s", name3, sv_demoRegexp.string), SORT_NO);
	Q_free(name3);
	for (i = 1; dir.numfiles; )
	{
		snprintf(name2, sizeof(name2), "%s_%02i", name, i++);
		if (!(name3 = quote(name2)))
			return;
		dir = Sys_listdir(va("%s/%s", fs_gamedir, sv_demoDir.string),
						  va("^%s%s", name3, sv_demoRegexp.string), SORT_NO);
		Q_free(name3);
	}

	strlcpy(name4, name2, sizeof(name4));
	snprintf(name2, sizeof(name2), "%s", va("%s/%s/%s.mvd", fs_gamedir, sv_demoDir.string, name2));
	snprintf(name4, sizeof(name4), "%s", va("%s/%s.xml", sv_demoDir.string, name4));
	Cvar_Set(&extralogname, name4);

	SV_MVD_Record (SV_InitRecordFile(name2), false);
}

//============================================================

static void MVD_Init (void)
{
	int p, size = DEMO_CACHE_MIN_SIZE;

	memset(&demo, 0, sizeof(demo)); // clear whole demo struct at least once
	
	Cvar_Register (&sv_demofps);
	Cvar_Register (&sv_demoIdlefps);
	Cvar_Register (&sv_demoPings);
	Cvar_Register (&sv_demoUseCache);
	Cvar_Register (&sv_demoCacheSize);
	Cvar_Register (&sv_demoMaxSize);
	Cvar_Register (&sv_demoMaxDirSize);
	Cvar_Register (&sv_demoClearOld); //bliP: 24/9 clear old demos
	Cvar_Register (&sv_demoDir);
	Cvar_Register (&sv_demoDirAlt);
	Cvar_Register (&sv_demoPrefix);
	Cvar_Register (&sv_demoSuffix);
	Cvar_Register (&sv_onrecordfinish);
	Cvar_Register (&sv_ondemoremove);
	Cvar_Register (&sv_demotxt);
	Cvar_Register (&sv_demoExtraNames);
	Cvar_Register (&sv_demoRegexp);
	Cvar_Register (&sv_silentrecord);

	Cvar_Register (&extralogname);

	p = SV_CommandLineDemoCacheArgument();
	if (p)
	{
		if (p < COM_Argc()-1)
			size = Q_atoi (COM_Argv(p+1)) * 1024;
		else
			Sys_Error ("MVD_Init: you must specify a size in KB after -democache");
	}

	if (size < DEMO_CACHE_MIN_SIZE)
	{
		Con_Printf("Minimum memory size for demo cache is %dk\n", DEMO_CACHE_MIN_SIZE / 1024);
		size = DEMO_CACHE_MIN_SIZE;
	}

	Cvar_SetROM(&sv_demoCacheSize, va("%d", size/1024));
	CleanName_Init();
}

void SV_UserCmdTrace_f(void)
{
	const char* user = Cmd_Argv(1);
	const char* option_ = Cmd_Argv(2);
	qbool option = false;
	int uid, i;

	if (Cmd_Argc() != 3) {
		Con_Printf("Usage: %s userid (on | off)\n", Cmd_Argv(0));
		return;
	}

	if (!strcmp(option_, "on")) {
		option = true;
	}
	else if (strcmp(option_, "off")) {
		Con_Printf("Usage: %s userid (on | off)\n", Cmd_Argv(0));
		return;
	}

	uid = atoi(user);
	if (!uid) {
		Con_Printf("Usage: %s userid (on | off)\n", Cmd_Argv(0));
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!svs.clients[i].state) {
			continue;
		}
		if (svs.clients[i].userid == uid) {
			svs.clients[i].mvd_write_usercmds = option;
			return;
		}
	}

	Con_Printf("Couldn't find userid %d\n", uid);
	return;
}

void SV_MVDInit(void)
{
	MVD_Init();

#ifdef SERVERONLY
	// name clashes with client.
	// would be nice to prefix it with sv_demo*,
	// but mods use it like that already, so we keep it for backward compatibility.
	Cmd_AddCommand ("record",			SV_MVD_Record_f);
	Cmd_AddCommand ("easyrecord",		SV_MVDEasyRecord_f);
	Cmd_AddCommand ("stop",				SV_MVDStop_f);
#endif
	// that how thouse commands should be called.
	Cmd_AddCommand ("sv_demorecord",	SV_MVD_Record_f);
	Cmd_AddCommand ("sv_demoeasyrecord",SV_MVDEasyRecord_f);
	Cmd_AddCommand ("sv_demostop",		SV_MVDStop_f);

	// that one does not clashes with client, but keep name for backward compatibility.
	Cmd_AddCommand ("cancel",			SV_MVD_Cancel_f);
	// that how thouse commands should be called.
	Cmd_AddCommand ("sv_democancel",	SV_MVD_Cancel_f);
	// this ones prefixed OK.
	Cmd_AddCommand ("sv_lastscores",	SV_LastScores_f);
	Cmd_AddCommand ("sv_demolist",		SV_DemoList_f);
	Cmd_AddCommand ("sv_demolistr",		SV_DemoListRegex_f);
	Cmd_AddCommand ("sv_demolistregex",	SV_DemoListRegex_f);
	Cmd_AddCommand ("sv_demoremove",	SV_MVDRemove_f);
	Cmd_AddCommand ("sv_demonumremove",	SV_MVDRemoveNum_f);
	Cmd_AddCommand ("sv_demoinfoadd",	SV_MVDInfoAdd_f);
	Cmd_AddCommand ("sv_demoinforemove",SV_MVDInfoRemove_f);
	Cmd_AddCommand ("sv_demoinfo",		SV_MVDInfo_f);
	Cmd_AddCommand ("sv_demoembedinfo", SV_MVDEmbedInfo_f);
	// not prefixed.
#ifdef SERVERONLY
	Cmd_AddCommand ("script",			SV_Script_f);
#endif

	Cmd_AddCommand ("sv_usercmdtrace",  SV_UserCmdTrace_f);

	SV_QTV_Init();
}

const char* SV_MVDDemoName(void)
{
	mvddest_t* d;

	for (d = demo.dest; d; d = d->nextdest) {
		if (d->desttype == DEST_STREAM) {
			continue; // streams are not saved on to HDD, so ignore it...
		}
		if (d->name[0]) {
			return d->name;
		}
	}

	return NULL;
}

#endif // !CLIENTONLY
