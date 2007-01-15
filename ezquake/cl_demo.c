/*
Copyright (C) 1996-1997 Id Software, Inc.

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

	$Id: cl_demo.c,v 1.57 2007-01-15 21:21:06 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "winquake.h"
#include "movie.h"
#include "menu_demo.h"
#include "qtv.h"

float olddemotime, nextdemotime;		

//QIZMO
#ifdef _WIN32
static qbool	qwz_unpacking = false;
static qbool	qwz_playback = false;
static qbool	qwz_packing = false;
static qbool OnChange_demo_format(cvar_t*, char*);
cvar_t demo_format = {"demo_format", "qwz", 0, OnChange_demo_format};
static HANDLE hQizmoProcess = NULL;
static char tempqwd_name[256] = {0}; // this file must be deleted after playback is finished
int CL_Demo_Compress(char*);
#endif

static qbool OnChange_demo_dir(cvar_t *var, char *string);
cvar_t demo_dir = {"demo_dir", "", 0, OnChange_demo_dir};

char Demos_Get_Trackname(void);
int FindBestNick(char *s,int use);

//=============================================================================
//								DEMO WRITING
//=============================================================================

static FILE *recordfile = NULL;
static float playback_recordtime;		


#define DEMORECORDTIME	((float) (cls.demoplayback ? playback_recordtime : cls.realtime))
#define DEMOCACHE_MINSIZE	(2 * 1024 * 1024)
#define DEMOCACHE_FLUSHSIZE	(1024 * 1024)

static sizebuf_t democache;
static qbool democache_available = false;

static qbool CL_Demo_Open(char *name) {
	if (democache_available)
		SZ_Clear(&democache);
	recordfile = fopen (name, "wb");
	return recordfile ? true : false;
}

static void CL_Demo_Close(void) {
	if (democache_available)
		fwrite(democache.data, democache.cursize, 1, recordfile);
	fclose(recordfile);
	recordfile = NULL;
}

static void CL_Demo_Write(void *data, int size) {
	if (democache_available) {
		if (size > democache.maxsize) {		
			Sys_Error("CL_Demo_Write: size > democache.maxsize");
		} else if (size > democache.maxsize - democache.cursize) {
			int overflow;

			Com_Printf("Warning: democache overflow...flushing\n");
			overflow = size - (democache.maxsize - democache.cursize);
			
			if (overflow <= DEMOCACHE_FLUSHSIZE)
				overflow = min(DEMOCACHE_FLUSHSIZE, democache.cursize);	
			
			fwrite(democache.data, overflow, 1, recordfile);
			memmove(democache.data, democache.data + overflow, democache.cursize - overflow);	
			democache.cursize -= overflow;
			SZ_Write(&democache, data, size);
		} else {
			SZ_Write(&democache, data, size);
		}
	} else {
		fwrite (data, size, 1, recordfile);
	}
}

static void CL_Demo_Flush(void) {
	fflush(recordfile);
}

//Writes the current user cmd
void CL_WriteDemoCmd (usercmd_t *pcmd) {
	int i;
	float fl, t[3];
	byte c;
	usercmd_t cmd;

	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	c = dem_cmd;
	CL_Demo_Write(&c, sizeof(c));

	// correct for byte order, bytes don't matter
	cmd = *pcmd;

	for (i = 0; i < 3; i++)
		cmd.angles[i] = LittleFloat(cmd.angles[i]);
	cmd.forwardmove = LittleShort(cmd.forwardmove);
	cmd.sidemove    = LittleShort(cmd.sidemove);
	cmd.upmove      = LittleShort(cmd.upmove);

	CL_Demo_Write(&cmd, sizeof(cmd));

	t[0] = LittleFloat (cl.viewangles[0]);
	t[1] = LittleFloat (cl.viewangles[1]);
	t[2] = LittleFloat (cl.viewangles[2]);
	CL_Demo_Write(t, sizeof(t));

	CL_Demo_Flush();
}

//Dumps the current net message, prefixed by the length and view angles
void CL_WriteDemoMessage (sizebuf_t *msg) {
	int len;
	float fl;
	byte c;

	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	c = dem_read;
	CL_Demo_Write(&c, sizeof(c));

	len = LittleLong (msg->cursize);
	CL_Demo_Write(&len, 4);
	CL_Demo_Write(msg->data, msg->cursize);

	CL_Demo_Flush();

    {
        extern void Request_Pings(void);
        extern cvar_t demo_getpings;
        
        if (demo_getpings.value)
            Request_Pings();
    }

}



void CL_WriteDemoEntities (void) {
	int ent_index, ent_total;
	entity_state_t *ent;

	MSG_WriteByte (&cls.demomessage, svc_packetentities);

	ent = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].packet_entities.entities;
	ent_total = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].packet_entities.num_entities;

	for (ent_index = 0; ent_index < ent_total; ent_index++, ent++)
		MSG_WriteDeltaEntity (&cl_entities[ent->number].baseline, ent, &cls.demomessage, true);

	MSG_WriteShort (&cls.demomessage, 0);    // end of packetentities
}


static void CL_WriteStartupDemoMessage (sizebuf_t *msg, int seq) {
	int len, i;
	float fl;
	byte c;

	if (!cls.demorecording)
		return;

	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	c = dem_read;
	CL_Demo_Write(&c, sizeof(c));

	len = LittleLong (msg->cursize + 8);
	CL_Demo_Write(&len, 4);

	i = LittleLong(seq);
	CL_Demo_Write(&i, 4);
	CL_Demo_Write(&i, 4);

	CL_Demo_Write(msg->data, msg->cursize);

	CL_Demo_Flush();
}

static void CL_WriteSetDemoMessage (void) {
	int len;
	float fl;
	byte c;

	if (!cls.demorecording)
		return;

	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	c = dem_set;
	CL_Demo_Write(&c, sizeof(c));

	len = LittleLong(cls.netchan.outgoing_sequence);
	CL_Demo_Write(&len, 4);
	len = LittleLong(cls.netchan.incoming_sequence);
	CL_Demo_Write(&len, 4);

	CL_Demo_Flush();
}

//write startup data to demo (called when demo started and cls.state == ca_active)
static void CL_WriteStartupData (void) {
	sizebuf_t buf;
	char buf_data[MAX_MSGLEN * 2], *s;
	int n, i, j, seq = 1;
	entity_t *ent;
	entity_state_t *es, blankes;
	player_info_t *player;

	// serverdata
	// send the info about the new client to all connected clients
	SZ_Init (&buf, (byte *) buf_data, sizeof(buf_data));

	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, cl.servercount);
	MSG_WriteString (&buf, cls.gamedirfile);

	if (cl.spectator)
		MSG_WriteByte (&buf, cl.playernum | 128);
	else
		MSG_WriteByte (&buf, cl.playernum);

	// send full levelname
	MSG_WriteString (&buf, cl.levelname);

	// send the movevars
	MSG_WriteFloat(&buf, movevars.gravity);
	MSG_WriteFloat(&buf, movevars.stopspeed);
	MSG_WriteFloat(&buf, cl.maxspeed);
	MSG_WriteFloat(&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat(&buf, movevars.accelerate);
	MSG_WriteFloat(&buf, movevars.airaccelerate);
	MSG_WriteFloat(&buf, movevars.wateraccelerate);
	MSG_WriteFloat(&buf, movevars.friction);
	MSG_WriteFloat(&buf, movevars.waterfriction);
	MSG_WriteFloat(&buf, cl.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", cl.serverinfo));

	// flush packet
	CL_WriteStartupDemoMessage (&buf, seq++);
	SZ_Clear (&buf); 

	// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.sound_name[n + 1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.sound_name[n+1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteStartupDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

#ifdef VWEP_TEST
// vwep modellist
	if ((cl.z_ext & Z_EXT_VWEP) && cl.vw_model_name[0][0]) {
		// send VWep precaches
		// pray we don't overflow
		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			s = cl.vw_model_name[i];
			if (!*s)
				continue;
			MSG_WriteByte (&buf, svc_serverinfo);
			MSG_WriteString (&buf, "#vw");
			MSG_WriteString (&buf, va("%i %s", i, TrimModelName(s)));
		}
		// send end-of-list messsage
		MSG_WriteByte (&buf, svc_serverinfo);
		MSG_WriteString (&buf, "#vw");
		MSG_WriteString (&buf, "");
	}
	// don't bother flushing, the vwep list is not that large (I hope)
#endif

	// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.model_name[n + 1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize	> MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteStartupDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

	// spawnstatic
	for (i = 0; i < cl.num_statics; i++) {
		ent = cl_static_entities + i;

		MSG_WriteByte (&buf, svc_spawnstatic);

		for (j = 1; j < MAX_MODELS; j++)
			if (ent->model == cl.model_precache[j])
				break;
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		MSG_WriteByte (&buf, ent->frame);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->skinnum);
		for (j = 0; j < 3; j++) {
			MSG_WriteCoord (&buf, ent->origin[j]);
			MSG_WriteAngle (&buf, ent->angles[j]);
		}

		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

	// static sounds are skipped in demos, life is hard

	// baselines
	memset(&blankes, 0, sizeof(blankes));
	for (i = 0; i < CL_MAX_EDICTS; i++) {
		es = &cl_entities[i].baseline;

		if (memcmp(es, &blankes, sizeof(blankes))) {
			MSG_WriteByte (&buf,svc_spawnbaseline);		
			MSG_WriteShort (&buf, i);

			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);
			for (j = 0; j < 3; j++) {
				MSG_WriteCoord(&buf, es->origin[j]);
				MSG_WriteAngle(&buf, es->angles[j]);
			}

			if (buf.cursize > MAX_MSGLEN / 2) {
				CL_WriteStartupDemoMessage (&buf, seq++);
				SZ_Clear (&buf); 
			}
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i 0\n", cl.servercount));

	if (buf.cursize) {
		CL_WriteStartupDemoMessage (&buf, seq++);
		SZ_Clear (&buf); 
	}

	// send current status of all other players
	for (i = 0; i < MAX_CLIENTS; i++) {
		player = cl.players + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->frags);

		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->ping);

		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->pl);

		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, cls.realtime - player->entertime);

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, player->userinfo);

		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		if (!cl_lightstyle[i].length)
			continue;		// don't send empty lightstyle strings
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, cl_lightstyle[i].map);
	}

	for (i = 0; i < MAX_CL_STATS; i++) {
		if (!cl.stats[i])
			continue;		// no need to send zero values
		if (cl.stats[i] >= 0 && cl.stats[i] <= 255) {
			MSG_WriteByte (&buf, svc_updatestat);
			MSG_WriteByte (&buf, i);
			MSG_WriteByte (&buf, cl.stats[i]);
		} else {
			MSG_WriteByte (&buf, svc_updatestatlong);
			MSG_WriteByte (&buf, i);
			MSG_WriteLong (&buf, cl.stats[i]);
		}
		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf); 
		}
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("skins\n"));

	CL_WriteStartupDemoMessage (&buf, seq++);

	CL_WriteSetDemoMessage();
}

//=============================================================================
//								DEMO READING
//=============================================================================

int CL_Demo_Peek(void *buf, int size);

vfsfile_t *playbackfile = NULL;

char pb_buf[1024*10];
char pb_tmp_buf[sizeof(pb_buf)];
int pb_s = 0, pb_cnt = 0;
qbool pb_eof = false;

void CL_Demo_PB_Init(void *buf, int buflen) {
	if (buflen < 0 || buflen > (int)sizeof(pb_buf))
		Sys_Error("CL_Demo_PB_Init: buflen < 0"); // ffs

	memcpy(pb_buf, buf, buflen);
	pb_s = 0;
	pb_cnt = buflen;
	pb_eof = false;
}

int pb_raw_read(void *buf, int size) {
	vfserrno_t err;
	int r = VFS_READ(playbackfile, buf, size, &err);

	if (size > 0 && !r && err == VFSERR_EOF) // size > 0 mean detect EOF only if we actually trying read some data
		pb_eof = true;

	return r;
}

qbool pb_ensure(void) {
	int need, got = 0;

	if (cl_shownet.value == 3)
		Com_Printf(" %d", pb_cnt);

	if (cls.mvdplayback && pb_cnt > 0 ) { // try decrease pb_buf[]
		pb_raw_read(NULL, 0); // at the same time increase internal TCP buffer
		if (pb_cnt > 2000) // seems theoretical size of one MVD packet is 1400, so 2000 must be safe to parse something
			return true;

		CL_Demo_Peek(pb_tmp_buf, pb_cnt);
		if (ConsistantMVDData(pb_tmp_buf, pb_cnt))
			return true;
	}

	need = sizeof(pb_buf); // we need full buffer
	need -= pb_cnt; // alredy have something
	need = min(need, (int)sizeof(pb_buf) -pb_s - pb_cnt);
	need = max(0, need);
	if (need)
		pb_cnt += (got = pb_raw_read(pb_buf + pb_s + pb_cnt, need));

	if (got == need) {
		int tmp = (pb_s + pb_cnt) % (int)sizeof(pb_buf);
		need = sizeof(pb_buf);
		need -= pb_cnt;
		pb_cnt += pb_raw_read(pb_buf + tmp, need);
	}

	if (pb_cnt == (int)sizeof(pb_buf) || pb_eof)
		return true; // return true if we have full buffer or get EOF

	if (cls.mvdplayback && pb_cnt) { // not enough data in buffer, check do we have at least one message in buffer
		CL_Demo_Peek(pb_tmp_buf, pb_cnt);

		return !!ConsistantMVDData(pb_tmp_buf, pb_cnt);
	}

	return false;
}

int pb_read(void *buf, int size, qbool peek) {
	int need, have = 0;

	if (size < 0)
		Sys_Error("pb_read: size < 0"); // ffs

	need = min(pb_cnt, size);
	need -= have;
	need = min(need, (int)sizeof(pb_buf) - pb_s);
	memcpy((byte*)buf + have, (byte*)pb_buf + pb_s, need);
	have += need;

	need = min(pb_cnt, size);
	need -= have;
	need = min(need, pb_s);
	memcpy((byte*)buf + have, pb_buf, need);
	have += need;

	if (!peek) {
		pb_s += have;
		pb_s = pb_s % (int)sizeof(pb_buf);
		pb_cnt -= have;
	}

	return have;
}

int CL_Demo_Peek(void *buf, int size) {
	if (pb_read(buf, size, true) != size)
		Host_Error("Unexpected end of demo");
	return 1;
}

int CL_Demo_Read(void *buf, int size) {
	if (pb_read(buf, size, false) != size)
		Host_Error("Unexpected end of demo");
	return 1;
}

//When a demo is playing back, all NET_SendMessages are skipped, and NET_GetMessages are read from the demo file.
//Whenever cl.time gets past the last received message, another message is read from the demo file.

qbool CL_GetDemoMessage (void) {
	int i, j, tracknum;
	float demotime;
	byte c, newtime;
	usercmd_t *pcmd;
	static float prevtime = 0;

	#define SIZEOF_DEMOTIME		(cls.mvdplayback ? sizeof(newtime) : sizeof(demotime))

#ifdef _WIN32
	if (qwz_unpacking)
		return false;
#endif

	if (cl.paused & PAUSED_DEMO)
		return false;

	if (cls.mvdplayback) {
		if (prevtime < nextdemotime)
			prevtime = nextdemotime;

		if (cls.demotime + 1.0 < nextdemotime)
			cls.demotime = nextdemotime - 1.0;
	}

readnext:
	if (!pb_ensure())
		return false;

	// read the time from the packet
	if (cls.mvdplayback) {
		CL_Demo_Peek(&newtime, SIZEOF_DEMOTIME); // peek inside, but no read
		demotime =  prevtime + newtime * 0.001;
		if (cls.demotime - nextdemotime > 0.0001 && nextdemotime != demotime) {
			olddemotime = nextdemotime;
			cls.netchan.incoming_sequence++;
			cls.netchan.incoming_acknowledged++;
			cls.netchan.frame_latency = 0;
			cls.netchan.last_received = cls.demotime; // just to happy timeout check
			nextdemotime = demotime;
		}
	} else {
		CL_Demo_Peek(&demotime, SIZEOF_DEMOTIME); // peek inside, but no read
		demotime = LittleFloat(demotime);
	}

	playback_recordtime = demotime;

	// decide if it is time to grab the next message		
	if (cls.timedemo) {
		if (cls.td_lastframe < 0) {
			cls.td_lastframe = demotime;
		} else if (demotime > cls.td_lastframe) {
			cls.td_lastframe = demotime;
//			fseek(playbackfile, ftell(playbackfile) - SIZEOF_DEMOTIME, SEEK_SET);	// rewind back to time
			return false;		// already read this frame's message
		}
		if (!cls.td_starttime && cls.state == ca_active) {
			cls.td_starttime = Sys_DoubleTime();
			cls.td_startframe = cls.framecount;
		}
		cls.demotime = demotime; // warp
	} else if (!(cl.paused & PAUSED_SERVER) && cls.state == ca_active) {	// always grab until fully connected
		if (cls.mvdplayback) {
			if (nextdemotime < demotime) {
//				fseek(playbackfile, ftell(playbackfile) - SIZEOF_DEMOTIME, SEEK_SET);	// rewind back to time
				return false;		// don't need another message yet
			}
		} else {
			if (cls.demotime < demotime) {
				if (cls.demotime + 1.0 < demotime)
					cls.demotime = demotime - 1.0;	// too far back
//				fseek(playbackfile, ftell(playbackfile) - SIZEOF_DEMOTIME, SEEK_SET);	// rewind back to time
				return false;		// don't need another message yet
			}
		}
	} else {
		cls.demotime = demotime; // we're warping
	}

	// actually read the time from the packet, so we does't need fseek
	if (cls.mvdplayback) {
		byte dummy_newtime;
		CL_Demo_Read(&dummy_newtime, SIZEOF_DEMOTIME);
	} else {
		float dummy_demotime;
		CL_Demo_Read(&dummy_demotime, SIZEOF_DEMOTIME);
	}

	if (cls.mvdplayback)
		prevtime = demotime;

	CL_Demo_Read(&c, sizeof(c));	// get the msg type

	switch ((c & 7)) {
	case dem_cmd :
		// user sent input
		i = cls.netchan.outgoing_sequence & UPDATE_MASK;
		pcmd = &cl.frames[i].cmd;
		CL_Demo_Read(pcmd, sizeof(*pcmd));

		for (j = 0; j < 3; j++)
			pcmd->angles[j] = LittleFloat(pcmd->angles[j]);

		pcmd->forwardmove = LittleShort(pcmd->forwardmove);
		pcmd->sidemove = LittleShort(pcmd->sidemove);
		pcmd->upmove  = LittleShort(pcmd->upmove);
		cl.frames[i].senttime = cls.realtime;
		cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet
		cls.netchan.outgoing_sequence++;

		CL_Demo_Read(cl.viewangles, 12);
		for (j = 0; j < 3; j++)
			 cl.viewangles[j] = LittleFloat (cl.viewangles[j]);

        CL_CalcPlayerFPS(&cl.players[cl.playernum], pcmd->msec);

		if (cl.spectator)
			Cam_TryLock();

		if (cls.demorecording)
			CL_WriteDemoCmd(pcmd);

		goto readnext;

	case dem_read:
readit:
		// get the next message
		CL_Demo_Read(&net_message.cursize, 4);
		net_message.cursize = LittleLong (net_message.cursize);
		if (net_message.cursize > net_message.maxsize) {
			Com_DPrintf("CL_GetDemoMessage: net_message.cursize > net_message.maxsize");
			Host_EndGame();
			Host_Abort();
		}

		CL_Demo_Read(net_message.data, net_message.cursize);

		if (cls.mvdplayback) {
			switch(cls.lasttype) {
			case dem_multiple:
				tracknum = Cam_TrackNum();
				if (tracknum == -1 || !(cls.lastto & (1 << tracknum)))
					goto readnext;	
				break;
			case dem_single:
				tracknum = Cam_TrackNum();
				if (tracknum == -1 || cls.lastto != spec_track)
					goto readnext;
				break;
			}
		}

		return true;

	case dem_set :
		CL_Demo_Read(&i, sizeof(i));
		cls.netchan.outgoing_sequence = LittleLong(i);
		CL_Demo_Read(&i, sizeof(i));
		cls.netchan.incoming_sequence = LittleLong(i);
		if (cls.mvdplayback)
			cls.netchan.incoming_acknowledged = cls.netchan.incoming_sequence;
		goto readnext;

	case dem_multiple:
		CL_Demo_Read(&i, sizeof(i));
		cls.lastto = LittleLong(i);
		cls.lasttype = dem_multiple;
		goto readit;

	case dem_single:
		cls.lastto = c >> 3;
		cls.lasttype = dem_single;
		goto readit;

	case dem_stats:
		cls.lastto = c >> 3;
		cls.lasttype = dem_stats;
		goto readit;

	case dem_all:
		cls.lastto = 0;
		cls.lasttype = dem_all;
		goto readit;

	default:
		Host_Error("Corrupted demo");
		return false;
	}
}

//=============================================================================
//								DEMO RECORDING
//=============================================================================

static char demoname[2 * MAX_OSPATH];
static char fulldemoname[_MAX_PATH];
static qbool autorecording = false;
static qbool easyrecording = false;

void CL_AutoRecord_StopMatch(void);
void CL_AutoRecord_CancelMatch(void);

static qbool OnChange_demo_dir(cvar_t *var, char *string) {
	if (!string[0])
		return false;

	Util_Process_Filename(string);
	if (!Util_Is_Valid_Filename(string)) {
		Com_Printf(Util_Invalid_Filename_Msg(var->name));
		return true;
	}

	Menu_Demo_NewHome(string);
	return false;
}

#ifdef _WIN32
static qbool OnChange_demo_format(cvar_t *var, char *string) 
{
	char* allowed_formats[5] = { "qwd", "qwz", "mvd", "mvd.gz", "qwd.gz" };
	int i;

	for (i = 0; i < 5; i++)
		if (!strcmp(allowed_formats[i], string))
			return false;

	Com_Printf("Not valid demo format. Allowed values are: ");
	for (i = 0; i < 5; i++) {
		if (i) Com_Printf(", ");
		Com_Printf(allowed_formats[i]);
	}
	Com_Printf(".\n");

	return true;
}
#endif

static void CL_WriteDemoPimpMessage(void) {
	int i;
	char pimpmessage[256] = {0}, border[64] = {0};

	if (cls.demoplayback)
		return;

	strcat(border, "\x1d");
	for (i = 0; i < 34; i++)
		strcat(border, "\x1e");
	strcat(border, "\x1f");

	snprintf(pimpmessage, sizeof(pimpmessage), "\n%s\n%s\n%s\n",
		border,
		"\x1d\x1e\x1e\x1e\x1e\x1e\x1e Recorded by ezQuake \x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f",
		border		
	);

	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, cls.netchan.incoming_sequence + 1);
	MSG_WriteLong (&net_message, cls.netchan.incoming_acknowledged | (cls.netchan.incoming_reliable_acknowledged << 31));
	MSG_WriteByte (&net_message, svc_print);
	MSG_WriteByte (&net_message, PRINT_HIGH);
	MSG_WriteString (&net_message, pimpmessage);
	CL_WriteDemoMessage (&net_message);
}

//stop recording a demo
static void CL_StopRecording (void) {
	if (!cls.demorecording)
		return;

	CL_WriteDemoPimpMessage();

	// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);	// -1 sequence means out of band
	MSG_WriteByte (&net_message, svc_disconnect);
	MSG_WriteString (&net_message, "EndOfDemo");
	CL_WriteDemoMessage (&net_message);

	// finish up
	CL_Demo_Close();
	cls.demorecording = false;
}

void CL_Stop_f (void) {
	if (!cls.demorecording) {
		Com_Printf ("Not recording a demo\n");
		return;
	}
	if (autorecording) {
		CL_AutoRecord_StopMatch();
#ifdef _WIN32
	} else if (easyrecording) {
		CL_StopRecording();
		CL_Demo_Compress(fulldemoname);
		easyrecording = false;
#endif
	} else {
		CL_StopRecording();
		Com_Printf ("Completed demo\n");
	}
}

static char *CL_DemoDirectory(void) {
	static char dir[MAX_OSPATH * 2];

	strlcpy(dir, demo_dir.string[0] ? va("%s/%s", com_basedir, demo_dir.string) : cls.gamedir, sizeof(dir));
	return dir;
}

#ifdef VWEP_TEST
// FIXME: same as in sv_user.c. Move to common.c?
static char *TrimModelName (char *full)
{
	static char shortn[MAX_QPATH];
	int len;

	if (!strncmp(full, "progs/", 6) && !strchr(full + 6, '/'))
		strlcpy (shortn, full + 6, sizeof(shortn));		// strip progs/
	else
		strlcpy (shortn, full, sizeof(shortn));

	len = strlen(shortn);
	if (len > 4 && !strcmp(shortn + len - 4, ".mdl")
		&& strchr(shortn, '.') == shortn + len - 4)
	{	// strip .mdl
		shortn[len - 4] = '\0';
	}

	return shortn;
}
#endif // VWEP_TEST

void CL_Record_f (void) {
	char nameext[MAX_OSPATH * 2], name[MAX_OSPATH * 2];

	if (cls.state != ca_active) {
		Com_Printf ("You must be connected before using record\n");
		return;
	}

	switch(Cmd_Argc()) {
	case 1:
		if (autorecording)
			Com_Printf("Auto demo recording is in progress\n");
		else if (cls.demorecording)
			Com_Printf("Recording to %s\n", demoname);
		else
			Com_Printf("Not recording\n");

		break;
	case 2:
		if (cls.mvdplayback) {
			Com_Printf ("Cannot record during mvd playback\n");
			return;
		}

		if (cls.state != ca_active && cls.state != ca_disconnected) {
			Com_Printf ("Cannot record whilst connecting\n");
			return;
		}

		if (autorecording) {
			Com_Printf("Auto demo recording must be stopped first!\n");
			return;
		}

		if (cls.demorecording)
			CL_Stop_f();

		if (!Util_Is_Valid_Filename(Cmd_Argv(1))) {
			Com_Printf(Util_Invalid_Filename_Msg(Cmd_Argv(1)));
			return;
		}

		// open the demo file
		strlcpy(nameext, Cmd_Argv(1), sizeof(nameext));
		COM_ForceExtension (nameext, ".qwd");	

		snprintf (name, sizeof(name), "%s/%s", cls.gamedir, nameext);
		if (!CL_Demo_Open(name)) {
			Com_Printf ("Error: Couldn't record to %s. Make sure path exists.\n", name);
			return;
		}

		cls.demorecording = true;
		if (cls.state == ca_active)
			CL_WriteStartupData();	

		strlcpy(demoname, nameext, sizeof(demoname));	

		Com_Printf ("Recording to %s\n", nameext);

		break;
	default:		
		Com_Printf("Usage: %s [demoname]\n", Cmd_Argv(0));
		break;
	}
}

static qbool CL_RecordDemo(char *dir, char *name, qbool autorecord) {
	char extendedname[MAX_OSPATH * 2], strippedname[MAX_OSPATH * 2], *fullname, *exts[] = {"qwd", "qwz", "mvd", NULL};
	int num;

	if (cls.state != ca_active) {
		Com_Printf ("You must be connected before using easyrecord\n");
		return false;
	}

	if (cls.mvdplayback) {
			Com_Printf ("Cannot record during mvd playback\n");
		return false;
	}

	if (autorecording) {	
		Com_Printf("Auto demo recording must be stopped first!\n");
		return false;
	}

	if (cls.demorecording)
		CL_Stop_f();

	if (!Util_Is_Valid_Filename(name)) {
		Com_Printf(Util_Invalid_Filename_Msg(name));
		return false;
	}

	COM_ForceExtension(name, ".qwd");
	if (autorecord) {
		strlcpy (extendedname, name, sizeof(extendedname));
	} else {
		COM_StripExtension(name, strippedname);
		fullname = va("%s/%s", dir, strippedname);
		if ((num = Util_Extend_Filename(fullname, exts)) == -1) {
			Com_Printf("Error: no available filenames\n");
			return false;
		}
		snprintf (extendedname, sizeof(extendedname), "%s_%03i.qwd", strippedname, num);
	}

	fullname = va("%s/%s", dir, extendedname);

	// open the demo file
	if (!CL_Demo_Open(fullname)) {
		COM_CreatePath(fullname);
		if (!CL_Demo_Open(fullname)) {
			Com_Printf("Error: Couldn't open %s\n", fullname);
			return false;
		}
	}

	cls.demorecording = true;
	CL_WriteStartupData ();

	if (!autorecord) {
		Com_Printf ("Recording to %s\n", extendedname);
		strlcpy(demoname, extendedname, sizeof(demoname));	
		strlcpy(fulldemoname, fullname, sizeof(fulldemoname));
	}

	return true;
}

void CL_EasyRecord_f (void) {
	int c;
	char *name;

	if (cls.state != ca_active) {
		Com_Printf("You must be connected to easyrecord\n");
		return;
	}

	switch((c = Cmd_Argc())) {
		case 1:
			name = MT_MatchName(); break;
		case 2:
			name = Cmd_Argv(1);	break;
		default:
			Com_Printf("Usage: %s [demoname]\n", Cmd_Argv(0));
			return;
	}

	easyrecording = CL_RecordDemo(CL_DemoDirectory(), name, false);
}

//=============================================================================
//							DEMO AUTO RECORDING
//=============================================================================



static char	auto_matchname[2 * MAX_OSPATH];
static qbool temp_demo_ready = false;
static float auto_starttime;

char *MT_TempDirectory(void);

extern cvar_t match_auto_record, match_auto_minlength;

#define TEMP_DEMO_NAME "_!_temp_!_.qwd"


void CL_AutoRecord_StopMatch(void) {
	if (!autorecording)
		return;

	autorecording = false;
	CL_StopRecording();
	temp_demo_ready = true;
	if (match_auto_record.value == 2){
		CL_AutoRecord_SaveMatch();
		Com_Printf ("Auto record ok\n");
	}else
		Com_Printf ("Auto demo recording completed\n");
}


void CL_AutoRecord_CancelMatch(void) {
	if (!autorecording)
		return;

	autorecording = false;
	CL_StopRecording();
	temp_demo_ready = true;

	if (match_auto_record.value == 2) {
		if (cls.realtime - auto_starttime > match_auto_minlength.value)
			CL_AutoRecord_SaveMatch();
		else
			Com_Printf("Auto demo recording cancelled\n");
	} else {
		Com_Printf ("Auto demo recording completed\n");
	}
}


void CL_AutoRecord_StartMatch(char *demoname) {
	temp_demo_ready = false;

	if (!match_auto_record.value)
		return;

	if (cls.demorecording) {
		if (autorecording) {			
			
			autorecording = false;
			CL_StopRecording();
		} else {
			Com_Printf("Auto demo recording skipped (already recording)\n");
			return;
		}
	}

	strlcpy(auto_matchname, demoname, sizeof(auto_matchname));

	
	if (!CL_RecordDemo(MT_TempDirectory(), TEMP_DEMO_NAME, true)) {
		Com_Printf ("Auto demo recording failed to start!\n");
		return;
	}

	autorecording = true;
	auto_starttime = cls.realtime;
	Com_Printf ("Auto demo recording commenced\n");
}

qbool CL_AutoRecord_Status(void) {
	return temp_demo_ready ? 2 : autorecording ? 1 : 0;
}

void CL_AutoRecord_SaveMatch(void) {
	int error, num;
	FILE *f;
	char *dir, *tempname, savedname[2 * MAX_OSPATH], *fullsavedname, *exts[] = {"qwd", "qwz", "mvd", NULL};
	if (!temp_demo_ready)
		return;

	temp_demo_ready = false;

	dir = CL_DemoDirectory();
	tempname = va("%s/%s", MT_TempDirectory(), TEMP_DEMO_NAME);

	fullsavedname = va("%s/%s", dir, auto_matchname);
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1) {
		Com_Printf("Error: no available filenames\n");
		return;
	}
	snprintf (savedname, sizeof(savedname), "%s_%03i.qwd", auto_matchname, num);

	fullsavedname = va("%s/%s", dir, savedname);

	
	if (!(f = fopen(tempname, "rb")))
		return;
	fclose(f);

	if ((error = rename(tempname, fullsavedname))) {
		COM_CreatePath(fullsavedname);
		error = rename(tempname, fullsavedname);
	}

#ifdef _WIN32

	if (!strcmp(demo_format.string, "qwz") || !strcmp(demo_format.string, "mvd")) {
		Com_Printf("Converting QWD to %s format.\n", demo_format.string);
		if (CL_Demo_Compress(fullsavedname)) {
			qwz_packing = true;
			return;
		}
		else
			qwz_packing = false;
	}

#endif

	if (!error)
		Com_Printf("Match demo saved to %s\n", savedname);
}

//=============================================================================
//							QIZMO COMPRESSION
//=============================================================================

#ifdef _WIN32

void CL_Demo_RemoveQWD(void)
{
	unlink(tempqwd_name);
}

void CL_Demo_GetCompressedName(char* cdemo_name)
{
	int namelen;
	
	namelen = strlen(tempqwd_name);
	if (strlen(demo_format.string) && strlen(tempqwd_name)) {
		strncpy(cdemo_name, tempqwd_name, namelen - 3);
		strlcpy(cdemo_name + namelen - 3, demo_format.string, 255 - (namelen - 3) - strlen(demo_format.string));
	}
}

void CL_Demo_RemoveCompressed(void)
{
	char cdemo_name[255];
	CL_Demo_GetCompressedName(cdemo_name);
	unlink(cdemo_name);
}

static void StopQWZPlayback (void) {
	if (!hQizmoProcess && tempqwd_name[0]) {
		if (remove (tempqwd_name) != 0)
			Com_Printf ("Error: Couldn't delete %s\n", tempqwd_name);
		tempqwd_name[0] = 0;
	}
	qwz_playback = false;
	qwz_unpacking = false;	
}

void CL_CheckQizmoCompletion (void) {
	DWORD ExitCode;

	if (!hQizmoProcess)
		return;

	if (!GetExitCodeProcess (hQizmoProcess, &ExitCode)) {
		Com_Printf ("WARINING: CL_CheckQizmoCompletion: GetExitCodeProcess failed\n");
		hQizmoProcess = NULL;
		if (qwz_unpacking) {
			qwz_unpacking = false;
			qwz_playback = false;
			cls.demoplayback = cls.timedemo = false;
			StopQWZPlayback();
		} else if (qwz_packing) {
			qwz_packing = false;
			CL_Demo_RemoveCompressed();
		}
		return;
	}

	if (ExitCode == STILL_ACTIVE)
		return;

	hQizmoProcess = NULL;

	if (!qwz_packing && (!qwz_unpacking || !cls.demoplayback)) {
		StopQWZPlayback ();
		return;
	}

	if (qwz_unpacking) {
		qwz_unpacking = false;

		if (!(playbackfile = FS_OpenVFS (tempqwd_name, "rb", FS_NONE_OS))) {
			Com_Printf ("Error: Couldn't open %s\n", tempqwd_name);
			qwz_playback = false;
			cls.demoplayback = cls.timedemo = false;
			tempqwd_name[0] = 0;
			return;
		}
		Com_Printf("Decompression completed...playback starting\n");
	} else if (qwz_packing) {
		FILE* tempfile;
		char newname[255];
		CL_Demo_GetCompressedName(newname);
		qwz_packing = false;

		if ((tempfile = fopen(newname, "rb")) && (COM_FileLength(tempfile) > 0) && fclose(tempfile) != EOF)
		{
			Com_Printf("Demo saved to %s\n", newname + strlen(com_basedir));
			CL_Demo_RemoveQWD();
		} else {
			Com_Printf("Compression failed, demo saved as QWD.\n");
		}
	}
}

static void PlayQWZDemo (void) {
	extern cvar_t qizmo_dir;
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char *name, qwz_name[_MAX_PATH], cmdline[512], *p;

	if (hQizmoProcess) {
		Com_Printf ("Cannot unpack -- Qizmo still running!\n");
		return;
	}

	name = Cmd_Argv(1);

	if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3)) {
		strlcpy (qwz_name, va("%s/%s", com_basedir, name + 3), sizeof(qwz_name));
	} else {
		if (name[0] == '/' || name[0] == '\\')
			strlcpy (qwz_name, va("%s/%s", cls.gamedir, name + 1), sizeof(qwz_name));
		else
			strlcpy (qwz_name, va("%s/%s", cls.gamedir, name), sizeof(qwz_name));
	}

	if (playbackfile = FS_OpenVFS(name, "rb", FS_NONE_OS)) {
		// either we got full system path
		strlcpy(qwz_name, name, sizeof(qwz_name));
	} else {
		// or we have to build it because Qizmo needs an absolute file name
		_fullpath (qwz_name, qwz_name, sizeof(qwz_name) - 1);
		qwz_name[sizeof(qwz_name) - 1] = 0;

		// check if the file exists
		if (!(playbackfile = FS_OpenVFS (qwz_name, "rb", FS_NONE_OS))) {
			Com_Printf ("Error: Couldn't open %s\n", name);
			return;
		}
	}

	VFS_CLOSE (playbackfile);
	playbackfile = NULL;

	strlcpy (tempqwd_name, qwz_name, sizeof(tempqwd_name) - 4);

	// the way Qizmo does it, sigh
	if (!(p = strstr (tempqwd_name, ".qwz")))
		p = strstr (tempqwd_name, ".QWZ");
	if (!p)
		p = tempqwd_name + strlen(tempqwd_name);
	strcpy (p, ".qwd");

	if ((playbackfile = FS_OpenVFS(tempqwd_name, "rb", FS_NONE_OS)))	// if .qwd already exists, ust play it
		return;

	Com_Printf ("Unpacking %s...\n", COM_SkipPath(name));

	// start Qizmo to unpack the demo
	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	strlcpy (cmdline, va("%s/%s/qizmo.exe -q -u -3 -D \"%s\"", com_basedir, qizmo_dir.string, qwz_name), sizeof(cmdline));

	if (!CreateProcess (NULL, cmdline, NULL, NULL,
		FALSE, GetPriorityClass(GetCurrentProcess()),
		NULL, va("%s/%s", com_basedir, qizmo_dir.string), &si, &pi))
	{
		Com_Printf ("Couldn't execute %s/%s/qizmo.exe\n", com_basedir, qizmo_dir.string);
		return;
	}

	hQizmoProcess = pi.hProcess;
	qwz_unpacking = true;
	qwz_playback = true;
}

int CL_Demo_Compress(char* qwdname)
{
	extern cvar_t qizmo_dir;
	extern cvar_t qwdtools_dir;
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char cmdline[1024];
	char* execute, *path, outputpath[255];

	if (hQizmoProcess) 
	{
		Com_Printf ("Cannot compress -- Qizmo still running!\n");
		return 0;
	}

	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	if (!strcmp(demo_format.string, "qwz")) 
	{
		execute = "qizmo.exe -q -C";
		path = qizmo_dir.string;
		outputpath[0] = 0;
	} 
	else if (!strcmp(demo_format.string, "mvd")) 
	{
		execute = "qwdtools.exe -c -o * -od";
		path = qwdtools_dir.string;
		strlcpy(outputpath, qwdname, COM_SkipPath(qwdname) - qwdname);
	} 
	else 
	{
		Com_Printf("%s demo format not yet supported.\n", demo_format.string);
		return 0;
	}

	strlcpy (cmdline, va("%s/%s/%s \"%s\" \"%s\"", com_basedir, path, execute, outputpath, qwdname), sizeof(cmdline));

	if (!CreateProcess (NULL, cmdline, NULL, NULL,
		FALSE, GetPriorityClass(GetCurrentProcess()),
		NULL, va("%s/%s", com_basedir, path), &si, &pi))
	{
		Com_Printf ("Couldn't execute %s/%s/qizmo.exe\n", com_basedir, qizmo_dir.string);
		return 0;
	}

	strlcpy(tempqwd_name, qwdname, sizeof(tempqwd_name));

	hQizmoProcess = pi.hProcess;
	qwz_packing = true;
	return 1;
}

#endif

//=============================================================================
//							DEMO PLAYBACK
//=============================================================================

double		demostarttime;

#ifdef WITH_ZIP
static int CL_GetUnpackedDemoPath (char *play_path, char *unpacked_path, int unpacked_path_size)
{
	//
	// Check if the demo is in a zip file and if so, try to extract it before playing.
	//
	int retval = 0;
	char archive_path[MAX_PATH];
	char inzip_path[MAX_PATH];

	if (!strcmp (COM_FileExtension (play_path), "gz"))
	{
		int i = 0;
		int ext_len = 0;
		char ext[5];

		// Find the extension with .gz removed.
		{
			for (i = strlen(play_path) - 4; i > 0; i--)
			{
				if (play_path[i] == '.' || ext_len >= 4)
				{
					break;
				}

				ext_len++;
			}

			if (ext_len <= 4)
			{
				strlcpy (ext, play_path + strlen(play_path) - 4 - ext_len, sizeof(ext));
			}
			else
			{
				// Default to MVD.
				strlcpy (ext, ".mvd", sizeof(ext));
			}
		}

		// Unpack the file.
		if (!COM_GZipUnpackToTemp (play_path, unpacked_path, unpacked_path_size, ext))
		{
			return 0;
		}

		return 1;
	}
	
	// Check if the path is in the format "c:\quake\bla\demo.zip\some_demo.mvd" and split it up.
	if (COM_ZipBreakupArchivePath ("zip", play_path, archive_path, MAX_PATH, inzip_path, MAX_PATH) < 0)
	{
		return retval;
	}

	//
	// Extract the ZIP file.
	//
	{
		char temp_path[MAX_PATH];

		// Open the zip file.
		unzFile zip_file = COM_ZipUnpackOpenFile (archive_path);
		
		// Try extracting the zip file.
		if(COM_ZipUnpackOneFileToTemp (zip_file, inzip_path, false, false, NULL, temp_path, MAX_PATH) != UNZ_OK)
		{
			Com_Printf ("Failed to unpack the demo file \"%s\" to the temp path \"%s\"\n", inzip_path, temp_path);
			unpacked_path[0] = 0;
		}
		else
		{
			// Successfully unpacked the demo.
			retval = 1;

			// Copy the path of the unpacked file to the return string.
			strlcpy (unpacked_path, temp_path, unpacked_path_size);
		}

		// Close the zip file.
		COM_ZipUnpackCloseFile (zip_file);
	}

	return retval;
}
#endif // WITH_ZIP

void CL_StopPlayback (void) 
{
	extern int demo_playlist_started;
	extern int mvd_demo_track_run;
	
	if (!cls.demoplayback)
		return;

	if (Movie_IsCapturing())	
		Movie_Stop();

	if (playbackfile)
		VFS_CLOSE (playbackfile);

	playbackfile = NULL;
	cls.mvdplayback = cls.demoplayback = cls.nqdemoplayback = false;
	cl.paused &= ~PAUSED_DEMO;

	#ifdef WIN32
	if (qwz_playback)
		StopQWZPlayback ();
	#endif

	if (cls.timedemo) {	
		int frames;
		float time;

		cls.timedemo = false;

		frames = cls.framecount - cls.td_startframe - 1;
		time = Sys_DoubleTime() - cls.td_starttime;
		if (time <= 0)
			time = 1;
		Com_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames / time);
	}

	if (demo_playlist_started)
	{
		CL_Demo_Playlist_f();
		mvd_demo_track_run = 0;
	}

	TP_ExecTrigger("f_demoend");
}

void CL_Play_f (void) 
{
	#ifdef WITH_ZIP
	char unpacked_path[MAX_OSPATH];
	#endif // WITH_ZIP
	
	int i;
	char *real_name;
	char name[2 * MAX_OSPATH], **s;
	static char *ext[] = {".qwd", ".mvd", ".dem", NULL};

	if (Cmd_Argc() != 2) 
	{
		Com_Printf ("Usage: %s <demoname>\n", Cmd_Argv(0));
		return;
	}

	real_name = Cmd_Argv(1);

	Host_EndGame();

	TP_ExecTrigger("f_demostart");

	#ifdef WITH_ZIP
	{
		if (CL_GetUnpackedDemoPath (Cmd_Argv(1), unpacked_path, MAX_OSPATH))
		{
			real_name = unpacked_path;
		}
	}
	#endif // WITH_ZIP

	#ifdef WIN32
	// Decompress QWZ demos to QWD before playing it (using an external app).
	strlcpy (name, real_name, sizeof(name) - 4);

	if (strlen(name) > 4 && !strcasecmp(COM_FileExtension(name), "qwz")) 
	{
		PlayQWZDemo();

		// We failed to extract the QWZ demo.
		if (!playbackfile && !qwz_playback)
		{
			return;	
		}
	} 
	else 
	#endif // WIN32
	{
		// Find the demo path, trying different extensions if needed.
		for (s = ext; *s && !playbackfile; s++) 
		{
			// Strip the extension from the specified filename and append
			// the one we're currently checking for.
			COM_StripExtension (real_name, name);
			strlcpy (name, va("%s%s", name, *s), sizeof(name));

			// Look for the file in the above directory if it has ../ prepended to the filename.
			if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3))
			{
				playbackfile = FS_OpenVFS (va("%s/%s", com_basedir, name + 3), "rb", FS_NONE_OS);
			}
			else
			{
				playbackfile = FS_OpenVFS (name, "rb", FS_ANY); // search demo on quake file system, even in paks
			}

			// Look in the demo dir (user specified).
			if (!playbackfile)
			{
				playbackfile = FS_OpenVFS (va("%s/%s/%s", com_basedir, demo_dir.string, name), "rb", FS_NONE_OS);
			}

			// Check the full system path (Run a demo anywhere on the file system).
			if (!playbackfile)
			{
				playbackfile = FS_OpenVFS (name, "rb", FS_NONE_OS);
			}
		}

		if (!playbackfile) 
		{
			Com_Printf ("Error: Couldn't open %s\n", Cmd_Argv(1));
			return;
		}

		// Reset multiview track slots.
		for(i = 0; i < 4; i++)
		{
			mv_trackslots[i] = -1;
		}
		nTrack1duel = nTrack2duel = 0;
		mv_skinsforced = false;

		Com_Printf ("Playing demo from %s\n", COM_SkipPath(name));
	}

	cls.demoplayback	= true;
	cls.mvdplayback		= !strcasecmp(COM_FileExtension(name), "mvd");	
	cls.nqdemoplayback	= !strcasecmp(COM_FileExtension(name), "dem");

	CL_Demo_PB_Init(NULL, 0); // init some buffers

	// NetQuake demo support.
	if (cls.nqdemoplayback)
	{
		NQD_StartPlayback ();
	}

	cls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, 0);
	cls.demotime = 0;
	demostarttime = -1.0;		

	olddemotime = nextdemotime = 0;	
	cls.findtrack = true;
	cls.lastto = cls.lasttype = 0;
	CL_ClearPredict();
	
	// Recording not allowed during mvdplayback.
	if (cls.mvdplayback && cls.demorecording)
	{
		CL_Stop_f();
	}
}

void CL_TimeDemo_f (void) {
	if (Cmd_Argc() != 2) {
		Com_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_Play_f ();

	if (cls.state != ca_demostart)
		return;

	// cls.td_starttime will be grabbed at the second frame of the demo, so all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_starttime = 0;
	cls.td_startframe = cls.framecount;
	cls.td_lastframe = -1;		// get a new message this frame
}

void CL_QTVPlay (vfsfile_t *newf, void *buf, int buflen);

char qtvrequestbuffer[4096] = {0};
int qtvrequestsize = 0;
vfsfile_t *qtvrequest = NULL;

void QTV_CloseRequest(qbool warn) {
	if (qtvrequest) {
		if (warn)
			Com_Printf("Closing qtv request file\n");

		VFS_CLOSE(qtvrequest);
		qtvrequest = NULL;
	}

	qtvrequestsize = 0;
}

void CL_QTVPoll (void)
{
	vfserrno_t err;
	char *s, *e, *colon;
	int len, need;
	qbool streamavailable = false;
	qbool saidheader = false;

	if (!qtvrequest)
		return;

	need = sizeof(qtvrequestbuffer);
	need -= 1; // for null terminator
	need -= qtvrequestsize; // probably alredy have something
	need = max(0, need);
	len = VFS_READ(qtvrequest, qtvrequestbuffer + qtvrequestsize, need, &err);

	if (!len && err == VFSERR_EOF) {
		QTV_CloseRequest(true); // EOF, end of polling
		return;
	}

	qtvrequestsize += len;
	qtvrequestbuffer[qtvrequestsize] = '\0';
	if (!qtvrequestsize)
		return;

	//make sure it's a compleate chunk.
	for (s = qtvrequestbuffer; *s; s++)
	{
		if (s[0] == '\n' && s[1] == '\n')
			break;
	}

	if (!*s) // we does't find "\n\n"
		return;

	s = qtvrequestbuffer;
	for (e = s; *e; )
	{
		if (*e == '\n')
		{
			*e = '\0';
			colon = strchr(s, ':');
			if (colon)
			{
				*colon++ = '\0';
				if (!strcmp(s, "PERROR"))
				{	//printable error
					Com_Printf("QTV Error:\n%s\n", colon);
				}
				else if (!strcmp(s, "PRINT"))
				{	//printable error
					Com_Printf("QTV:\n%s\n", colon);
				}
				else if (!strcmp(s, "TERROR"))
				{	//printable error
					Com_Printf("QTV Error:\n%s\n", colon);
				}
				else if (!strcmp(s, "ADEMO"))
				{	//printable error
					Com_Printf("Demo%s is available\n", colon);
				}
				else if (!strcmp(s, "ASOURCE"))
				{	//printable source
					if (!saidheader)
					{
						saidheader=true;
						Com_Printf("Available Sources:\n");
					}
					Com_Printf("%s\n", colon);
				}
				else if (!strcmp(s, "BEGIN"))
					streamavailable = true;
			}
			else
			{
				if (!strcmp(s, "BEGIN"))
					streamavailable = true;
			}
			//from e to s, we have a line	
			s = e+1;
		}
		e++;
	}

	if (streamavailable)
	{
		int cnt = qtvrequestsize - (e-qtvrequestbuffer);

		if (cnt >= 0) {
			CL_QTVPlay(qtvrequest, e, cnt);
			qtvrequest = NULL;
			return;
		}
		Com_Printf("Error while parsing qtv request\n");
	}

	QTV_CloseRequest(true);
}

void CL_QTVList_f (void)
{
	char *connrequest;
	vfsfile_t *newf;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: qtvlist hostname[:port]\n");
		return;
	}

	if (!(newf = FS_OpenTCP(Cmd_Argv(1))))
	{
		Com_Printf("Couldn't connect to proxy\n");
		return;
	}

	connrequest =	"QTV\n"
					"VERSION: 1\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"SOURCELIST\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	QTV_CloseRequest(true);
	qtvrequest = newf;
}

void CL_QTVPlay (vfsfile_t *newf, void *buf, int buflen)
{
	int i;

	Host_EndGame();

	if (playbackfile)
		VFS_CLOSE(playbackfile); // just in case
	playbackfile = newf;

	// Reset multiview track slots.
	for(i = 0; i < 4; i++)
	{
		mv_trackslots[i] = -1;
	}
	nTrack1duel = nTrack2duel = 0;
	mv_skinsforced = false;

//	Com_Printf ("Playing demo from %s\n", COM_SkipPath(name));

	cls.demoplayback	= true;
	cls.mvdplayback		= true;	
	cls.nqdemoplayback	= false;

	CL_Demo_PB_Init(buf, buflen); // init some buffers

	// NetQuake demo support.
	if (cls.nqdemoplayback)
	{
		NQD_StartPlayback (); // may be some day qtv will stream NQ demos too...
	}

	cls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, 0);
	cls.demotime = 0;
	demostarttime = -1.0;		

	olddemotime = nextdemotime = 0;	
	cls.findtrack = true;
	cls.lastto = cls.lasttype = 0;
	CL_ClearPredict();
	
	// Recording not allowed during mvdplayback.
	if (cls.mvdplayback && cls.demorecording)
	{
		CL_Stop_f();
	}

	TP_ExecTrigger ("f_demostart");

	Com_Printf("Attempt to stream data\n");
}

char *strchrrev(char *str, char chr)
{
	char *firstchar = str;
	for (str = str + strlen(str)-1; str>=firstchar; str--)
		if (*str == chr)
			return str;

	return NULL;
}

void CL_QTVPlay_f (void)
{
	qbool raw=0;
	char *connrequest;
	vfsfile_t *newf;
	char *host;

//	Com_Printf("QTVPLAY %s\n", Cmd_Argv(1));

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: qtvplay [stream@]hostname[:port] [password]\n");
		return;
	}

	connrequest = Cmd_Argv(1);

	if (*connrequest == '#') // this is parsing a .qtv file
	{
		char buffer[1024];
		char *s;
		FILE *f;
		f = fopen(connrequest+1, "rt");
		if (!f) {
			Com_Printf("qtvplay: can't open file %s\n", connrequest+1);
			return;
		}

		while (!feof(f))
		{
			fgets(buffer, sizeof(buffer)-1, f);
			if (!strncmp(buffer, "Stream=", 7) || !strncmp(buffer, "Stream:", 7))
			{
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
						*s = 0;
					else
						break;
				}
				s = buffer+8;
				while(*s && *s <= ' ')
					s++;
				Cbuf_AddText(va("qtvplay \"%s\"\n", s));
				break;
			}
			if (!strncmp(buffer, "Join=", 7) || !strncmp(buffer, "Join:", 7))
			{
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
						*s = 0;
					else
						break;
				}
				s = buffer+8;
				while(*s && *s <= ' ')
					s++;
				Cbuf_AddText(va("join \"%s\"\n", s));
				break;
			}
			if (!strncmp(buffer, "Observe=", 7) || !strncmp(buffer, "Observe:", 7))
			{
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
						*s = 0;
					else
						break;
				}
				s = buffer+8;
				while(*s && *s <= ' ')
					s++;
				Cbuf_AddText(va("observe \"%s\"\n", s));
				break;
			}
		}
		fclose(f);
		return;
	}

	host = connrequest;

	connrequest = strchrrev(connrequest, '@');
	if (connrequest)
		host = connrequest+1;
	newf = FS_OpenTCP(host);

	if (!newf)
	{
		Com_Printf("Couldn't connect to proxy\n");
		return;
	}

	host = Cmd_Argv(1);
	if (connrequest)
		*connrequest = '\0';
	else
		host = NULL;

	connrequest =	"QTV\n"
					"VERSION: 1\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	if (raw)
	{
		connrequest =	"RAW: 1\n";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
	}
	if (host)
	{
		connrequest =	"SOURCE: ";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
		connrequest =	host;
		VFS_WRITE(newf, connrequest, strlen(connrequest));
		connrequest =	"\n";
	}

	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	if (raw)
	{
		CL_QTVPlay(newf, NULL, 0);
	}
	else
	{
		QTV_CloseRequest(false);
		qtvrequest = newf;
	}
}


//=============================================================================
//								DEMO TOOLS
//=============================================================================

void CL_Demo_SetSpeed_f (void) {
	extern cvar_t cl_demospeed;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s [speed %%]\n", Cmd_Argv(0));
		return;
	}

	Cvar_SetValue(&cl_demospeed, atof(Cmd_Argv(1)) / 100.0);
}

void CL_Demo_Jump_f (void) {
    int seconds = 0, seen_col, relative = 0;
	double newdemotime;
    char *text, *s;
	static char *usage_message = "Usage: %s [+|-][m:]<s> (seconds)\n";

	if (!cls.demoplayback) {
		Com_Printf("Error: not playing a demo\n");
        return;
	}

	if (cls.state < ca_active) {	
		Com_Printf("Error: demo must be active first\n");
		return;
	}

    if (Cmd_Argc() != 2) {
        Com_Printf(usage_message, Cmd_Argv(0));
        return;
    }

    text = Cmd_Argv(1);
	if (text[0] == '-') {
		text++;
		relative = -1;
	} else if (text[0] == '+') {
		text++;
		relative = 1;
	} else if (!isdigit(text[0])){
        Com_Printf(usage_message, Cmd_Argv(0));
        return;
	}

	for (seen_col = 0, s = text; *s; s++) {
		if (*s == ':') {
			seen_col++;
		} else if (!isdigit(*s)) {
			Com_Printf(usage_message, Cmd_Argv(0));
			return;			
		}
		if (seen_col >= 2) {
			Com_Printf(usage_message, Cmd_Argv(0));
			return;			
		}
	}

    if (strchr(text, ':')) {
        seconds += 60 * atoi(text);
        text = strchr(text, ':') + 1;
    }
    seconds += atoi(text);
	cl.gametime += seconds;

	newdemotime = relative ? cls.demotime + relative * seconds : demostarttime + seconds;

	if (newdemotime < cls.demotime) {
		Com_Printf ("Error: cannot demo_jump backwards\n");
		return;
	}
	cls.demotime = newdemotime;
}

void CL_Demo_Init (void) {
	int parm, democache_size;
	byte *democache_buffer;

	
	democache_available = false;
	if ((parm = COM_CheckParm("-democache")) && parm + 1 < com_argc) {
		democache_size = Q_atoi(com_argv[parm + 1]) * 1024;
		democache_size = max(democache_size, DEMOCACHE_MINSIZE);
		if ((democache_buffer = malloc(democache_size))) {
			Com_Printf_State (PRINT_OK, "Democache initialized (%.1f MB)\n", (float) (democache_size) / (1024 * 1024));
			SZ_Init(&democache, democache_buffer, democache_size);
			democache_available = true;
		} else {
			Com_Printf_State (PRINT_FAIL, "Democache allocation failed\n");
		}
	}

	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_Play_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
	Cmd_AddCommand ("easyrecord", CL_EasyRecord_f);

	Cmd_AddCommand("demo_setspeed", CL_Demo_SetSpeed_f);
	Cmd_AddCommand("demo_jump", CL_Demo_Jump_f);

	Cmd_AddCommand ("qtvplay", CL_QTVPlay_f);
	Cmd_AddCommand ("qtvlist", CL_QTVList_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
#ifdef _WIN32
	Cvar_Register(&demo_format);
#endif
	Cvar_Register(&demo_dir);

	Cvar_ResetCurrentGroup();
}
