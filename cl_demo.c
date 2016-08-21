/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007-2015 ezQuake team

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

#include <time.h>
#include "quakedef.h"
#include "movie.h"
#include "menu_demo.h"
#include "qtv.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "teamplay.h"
#include "pmove.h"
#include "fs.h"
#include "hash.h"
#include "vfs.h"
#include "utils.h"
#include "crc.h"
#include "logging.h"
#include "version.h"
#include "demo_controls.h"
#include "mvd_utils.h"
#ifndef CLIENTONLY
#include "server.h"
#endif

/* FIXME Move these to a proper header file and included that */
void Cam_Unlock(void);

// TODO: Create states for demo_recording, demo_playback, and so on and put all related vars into these.
// Right now with global vars for everything is a mess. Also renaming some of the time vars to be less
// confusing is probably good. demotime, olddemotime, nextdemotime, prevtime...

typedef struct demo_state_s
{
	float		olddemotime;
	float		nextdemotime;

	double		bufferingtime;

	sizebuf_t	democache;
	qbool		democache_available;

	#ifdef _WIN32
	qbool		qwz_unpacking;
	qbool		qwz_playback;
	qbool		qwz_packing;
	char		tempqwd_name[256];
	#endif // _WIN32
} demo_state_t;

float olddemotime, nextdemotime; // TODO: Put in a demo struct.

double bufferingtime; // if we stream from QTV, this is non zero when we trying fill our buffer

//
// Vars related to QIZMO compressed demos.
// (Only available in Win32 since we need to use an external app)
//
#ifdef _WIN32
static qbool	qwz_unpacking = false;
static qbool	qwz_playback = false;
static qbool	qwz_packing = false;

#define QWZ_DECOMPRESSION_TIMEOUT_MS	10000

static void		OnChange_demo_format(cvar_t*, char*, qbool*);
cvar_t			demo_format = {"demo_format", "qwz", 0, OnChange_demo_format};

static HANDLE hQizmoProcess = NULL;
static char tempqwd_name[256] = {0}; // This file must be deleted after playback is finished.
int CL_Demo_Compress(char*);
#endif

static vfsfile_t *CL_Open_Demo_File(char *name, qbool searchpaks, char **fullpath);
static void OnChange_demo_dir(cvar_t *var, char *string, qbool *cancel);
cvar_t demo_dir = {"demo_dir", "", 0, OnChange_demo_dir};
cvar_t demo_benchmarkdumps = {"demo_benchmarkdumps", "1"};
cvar_t cl_startupdemo = {"cl_startupdemo", ""};
cvar_t demo_jump_rewind = { "demo_jump_rewind", "-10" };

// Used to save track status when rewinding.
static int rewind_trackslots[4];
static int rewind_duel_track1 = 0;
static int rewind_duel_track2 = 0;
static int rewind_spec_track = 0;
static vec3_t rewind_angle;
static vec3_t rewind_pos;
static double qtv_demospeed = 1;

char Demos_Get_Trackname(void);
static void CL_DemoPlaybackInit(void);
void CL_ProcessUserInfo(int slot, player_info_t *player, char *key);

char *CL_DemoDirectory(void);
void CL_Demo_Jump_Status_Check (void);

//=============================================================================
//								DEMO WRITING
//=============================================================================

static FILE *recordfile = NULL;		// File used for recording demos. // TODO: Put in a demo struct.
static float playback_recordtime;	// Time when in demo playback and recording. // TODO: Put in a demo struct.

#define DEMORECORDTIME	((float) (cls.demoplayback ? playback_recordtime : cls.realtime))
#define DEMOCACHE_MINSIZE	(2 * 1024 * 1024)
#define DEMOCACHE_FLUSHSIZE	(1024 * 1024)

static sizebuf_t democache; // TODO: Put in a demo struct.
static qbool democache_available = false;	// Has the user opted to use a demo cache? // TODO: Put in a demo struct.

//
// Opens a demo for writing.
//
static qbool CL_Demo_Open(char *name)
{
	// Clear the demo cache and open the demo file for writing.
	if (democache_available)
		SZ_Clear(&democache);
	recordfile = fopen (name, "wb");
	return recordfile ? true : false;
}

//
// Closes a demo.
//
static void CL_Demo_Close(void)
{
	// Flush the demo cache and close the demo file.
	if (democache_available)
		fwrite(democache.data, democache.cursize, 1, recordfile);
	fclose(recordfile);
	recordfile = NULL;
}

//
// Writes a chunk of data to the currently opened demo record file.
//
static void CL_Demo_Write(void *data, int size)
{
	if (democache_available)
	{
		//
		// Write to the demo cache.
		//
		if (size > democache.maxsize)
		{
			// The size of the data to be written is bigger than the demo cache, fatal error.
			Sys_Error("CL_Demo_Write: size > democache.maxsize");
		}
		else if (size > democache.maxsize - democache.cursize)
		{
			//
			// Flushes part of the demo cache (enough to fit the new data) if it has been overflowed.
			//
			int overflow_size = size - (democache.maxsize - democache.cursize);

			Com_Printf("Warning: democache overflow...flushing\n");

			if (overflow_size <= DEMOCACHE_FLUSHSIZE)
				overflow_size = min(DEMOCACHE_FLUSHSIZE, democache.cursize);

			// Write as much data as overflowed from the current
			// contents of the demo cache to the demo file.
			fwrite(democache.data, overflow_size, 1, recordfile);

			// Shift the cache contents (remove what was just written).
			memmove(democache.data, democache.data + overflow_size, democache.cursize - overflow_size);
			democache.cursize -= overflow_size;

			// Write the new data to the demo cache.
			SZ_Write(&democache, data, size);
		}
		else
		{
			// Write to the demo cache.
			SZ_Write(&democache, data, size);
		}
	}
	else
	{
		//
		// Write directly to the file.
		//
		fwrite (data, size, 1, recordfile);
	}
}

//
// Flushes any pending write operations to the recording file.
//
static void CL_Demo_Flush(void)
{
	fflush(recordfile);
}

//
// Writes a user cmd to the open demo file.
//
void CL_WriteDemoCmd (usercmd_t *pcmd)
{
	int i;
	float fl, t[3];
	byte c;
	usercmd_t cmd;

	// Write the current time.
	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	// Write the message type. A user cmd (movement and such).
	c = dem_cmd;
	CL_Demo_Write(&c, sizeof(c));

	// Correct for byte order, bytes don't matter.
	cmd = *pcmd;

	// Convert the movement angles / vectors to the correct byte order.
	for (i = 0; i < 3; i++)
		cmd.angles[i] = LittleFloat(cmd.angles[i]);
	cmd.forwardmove = LittleShort(cmd.forwardmove);
	cmd.sidemove    = LittleShort(cmd.sidemove);
	cmd.upmove      = LittleShort(cmd.upmove);

	// Write the actual user command to the demo.
	CL_Demo_Write(&cmd, sizeof(cmd));

	// Write the view angles with the correct byte order.
	t[0] = LittleFloat (cl.viewangles[0]);
	t[1] = LittleFloat (cl.viewangles[1]);
	t[2] = LittleFloat (cl.viewangles[2]);
	CL_Demo_Write(t, sizeof(t));

	// Flush the changes to file.
	CL_Demo_Flush();
}

//
// Dumps the current net message, prefixed by the length and view angles
//
void CL_WriteDemoMessage (sizebuf_t *msg)
{
	int len;
	float fl;
	byte c;

	// Write time of cmd.
	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	// Write the message type. Net message.
	c = dem_read;
	CL_Demo_Write(&c, sizeof(c));

	// Write the length of the data.
	len = LittleLong (msg->cursize);
	CL_Demo_Write(&len, 4);

	// Write the data.
	CL_Demo_Write(msg->data, msg->cursize);

	// Flush the changes to the recording file.
	CL_Demo_Flush();

	// Request pings from the net chan if the user has set it.
    {
        extern void Request_Pings(void);
        extern cvar_t demo_getpings;

        if (demo_getpings.value)
            Request_Pings();
    }
}


//
// Writes the entities list to a demo.
//
void CL_WriteDemoEntities (void)
{
	int ent_index, ent_total;
	entity_state_t *ent;
	
	// Write the ID byte for a delta entity operation to the demo.
	MSG_WriteByte (&cls.demomessage, svc_packetentities);

	// Get the entities list from the frame.
	ent = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].packet_entities.entities;
	ent_total = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].packet_entities.num_entities;

	// Write all the entity changes since last packet entity message.
	for (ent_index = 0; ent_index < ent_total; ent_index++, ent++)
		MSG_WriteDeltaEntity (&cl_entities[ent->number].baseline, ent, &cls.demomessage, true);

	// End of packetentities.
	MSG_WriteShort (&cls.demomessage, 0);
}

//
// Writes a startup demo message to the demo being recorded.
//
static void CL_WriteStartupDemoMessage (sizebuf_t *msg, int seq)
{
	int len, i;
	float fl;
	byte c;

	// Don't write, we're not recording.
	if (!cls.demorecording)
		return;

	// Write the time.
	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	// Message type = net message.
	c = dem_read;
	CL_Demo_Write(&c, sizeof(c));

	// Write the length of the message.
	len = LittleLong (msg->cursize + 8);
	CL_Demo_Write(&len, 4);

	// Write the sequence number twice. Why?
	i = LittleLong(seq);
	CL_Demo_Write(&i, 4);
	CL_Demo_Write(&i, 4);

	// Write the actual message data to the demo.
	CL_Demo_Write(msg->data, msg->cursize);

	// Flush the message to file.
	CL_Demo_Flush();
}

//
// Writes a set demo message. The outgoing / incoming sequence at the start of the demo.
// This is only written once at startup.
//
static void CL_WriteSetDemoMessage (void)
{
	int len;
	float fl;
	byte c;

	// We're not recording.
	if (!cls.demorecording)
		return;

	// Write the demo time.
	fl = LittleFloat(DEMORECORDTIME);
	CL_Demo_Write(&fl, sizeof(fl));

	// Write the message type.
	c = dem_set;
	CL_Demo_Write(&c, sizeof(c));

	// Write the outgoing / incoming sequence.
	len = LittleLong(cls.netchan.outgoing_sequence);
	CL_Demo_Write(&len, 4);
	len = LittleLong(cls.netchan.incoming_sequence);
	CL_Demo_Write(&len, 4);

	// Flush the demo file to disk.
	CL_Demo_Flush();
}

// FIXME: same as in sv_user.c. Move to common.c?
static char *TrimModelName (const char *full)
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

void CL_WriteServerdata (sizebuf_t *msg)
{
	int ignore_extensions;

	MSG_WriteByte (msg, svc_serverdata);

	// Maintain demo compatibility.
	ignore_extensions = 0;
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	// this is OK, since download data skipped anyway
	ignore_extensions |= FTE_PEXT_CHUNKEDDOWNLOADS;
#endif
#ifdef FTE_PEXT_256PACKETENTITIES
	// this is probably OK, since engine should ignore more than 64 entities if not supported.
	ignore_extensions |= FTE_PEXT_256PACKETENTITIES;
#endif

	#ifdef PROTOCOL_VERSION_FTE
	if (cls.fteprotocolextensions &~ ignore_extensions)
	{
		MSG_WriteLong (msg, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (msg, cls.fteprotocolextensions);
	}
	#endif // PROTOCOL_VERSION_FTE

	// Maintain demo pseudo-compatibility,
	ignore_extensions = 0;

	#ifdef PROTOCOL_VERSION_FTE2
	if (cls.fteprotocolextensions2 & ~ignore_extensions)
	{
		MSG_WriteLong (msg, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (msg, cls.fteprotocolextensions2);
	}
	#endif // PROTOCOL_VERSION_FTE2

	MSG_WriteLong (msg, PROTOCOL_VERSION);
	MSG_WriteLong (msg, cl.servercount);
	MSG_WriteString (msg, cls.gamedirfile);

	// Write if we're a spectator or not.
	if (cl.spectator)
		MSG_WriteByte (msg, cl.playernum | 128);
	else
		MSG_WriteByte (msg, cl.playernum);

	// Send full levelname.
	MSG_WriteString (msg, cl.levelname);

	// Send the movevars.
	MSG_WriteFloat(msg, movevars.gravity);
	MSG_WriteFloat(msg, movevars.stopspeed);
	MSG_WriteFloat(msg, cl.maxspeed);
	MSG_WriteFloat(msg, movevars.spectatormaxspeed);
	MSG_WriteFloat(msg, movevars.accelerate);
	MSG_WriteFloat(msg, movevars.airaccelerate);
	MSG_WriteFloat(msg, movevars.wateraccelerate);
	MSG_WriteFloat(msg, movevars.friction);
	MSG_WriteFloat(msg, movevars.waterfriction);
	MSG_WriteFloat(msg, cl.entgravity);
}

//
// Write startup data to demo (called when demo started and cls.state == ca_active)
//
static void CL_WriteStartupData (void)
{
	sizebuf_t buf;
	char buf_data[MAX_MSGLEN * 2], *s;
	int n, i, j, seq = 1;
	entity_t *ent;
	entity_state_t *es, blankes;
	player_info_t *player;

	//
	// Serverdata
	// Send the info about the new client to all connected clients.
	//

	// Init a buffer that we write to before writing to the file.
	SZ_Init (&buf, (byte *) buf_data, sizeof(buf_data));

	//
	// Send the serverdata.
	//
	CL_WriteServerdata (&buf);

	// Send music.
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // None in demos

	// Send server info string.
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", cl.serverinfo));

	// Flush the buffer to the demo file and then clear it.
	CL_WriteStartupDemoMessage (&buf, seq++);
	SZ_Clear (&buf);

	//
	// Write the soundlist.
	//
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	// Loop through all sounds and write them to the demo.
	n = 0;
	s = cl.sound_name[n + 1];
	while (*s)
	{
		// Write the sound name to the buffer.
		MSG_WriteString (&buf, s);

		// If the buffer is half full, flush it.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			// End of the partial sound list.
			MSG_WriteByte (&buf, 0);

			// Write how many sounds we've listed so far.
			MSG_WriteByte (&buf, n);

			// Flush the buffer to the demo file and clear the buffer.
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf);

			// Start on a new sound list and continue writing the
			// remaining sounds.
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}

		n++;
		s = cl.sound_name[n+1];
	}

	// If the buffer isn't empty flush and clear it.
	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteStartupDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}

	// Vwep modellist
	if (cl.vwep_enabled && cl.vw_model_name[0][0]) 
	{
		// Send VWep precaches
		// pray we don't overflow
		char ss[1024] = "//vwep ";
		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			s = cl.vw_model_name[i];
			if (!*s)
				break;
			if (i > 0)
				strlcat (ss, " ", sizeof(ss));
			strlcat (ss, TrimModelName(s), sizeof(ss));
		}
		strlcat (ss, "\n", sizeof(ss));
		if (strlen(ss) < sizeof(ss)-1)		// Didn't overflow?
		{
			MSG_WriteByte (&buf, svc_stufftext);
			MSG_WriteString (&buf, ss);
		}
	}
	// Don't bother flushing, the vwep list is not that large (I hope).

	//
	// Modellist.
	//
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	// Loop through the models
	n = 0;
	s = cl.model_name[n + 1];
	while (*s)
	{
		// Write the model name to the buffer.
		MSG_WriteString (&buf, s);

		// If the buffer is half full, flush it.
		if (buf.cursize	> MAX_MSGLEN / 2)
		{
			// End of partial model list.
			MSG_WriteByte (&buf, 0);

			// Write how many models we've written so far.
			MSG_WriteByte (&buf, n);

			// Flush the model list to the demo file and clear the buffer.
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf);

			// Start on a new partial model list and continue.
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n + 1];
	}

	// Flush the buffer if it's not empty.
	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteStartupDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}

	//
	// Write static entities.
	//
	for (i = 0; i < cl.num_statics; i++)
	{
		// Get the next static entity.
		ent = cl_static_entities + i;

		// Write ID for static entities.
		MSG_WriteByte (&buf, svc_spawnstatic);

		// Find if the model is precached or not.
		for (j = 1; j < MAX_MODELS; j++)
		{
			if (ent->model == cl.model_precache[j])
				break;
		}

		// Write if the model is precached.
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		// Write the entities frame and skin number.
		MSG_WriteByte (&buf, ent->frame);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->skinnum);

		// Write the coordinate and angles.
		for (j = 0; j < 3; j++)
		{
			MSG_WriteCoord (&buf, ent->origin[j]);
			MSG_WriteAngle (&buf, ent->angles[j]);
		}

		// Flush the buffer if it's half full.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// spawnstaticsound
	for (i = 0; i < cl.num_static_sounds; i++) 
	{
		static_sound_t *ss = &cl.static_sounds[i];
		MSG_WriteByte (&buf, svc_spawnstaticsound);
		for (j = 0; j < 3; j++)
			MSG_WriteCoord (&buf, ss->org[j]);
		MSG_WriteByte (&buf, ss->sound_num);
		MSG_WriteByte (&buf, ss->vol);
		MSG_WriteByte (&buf, ss->atten);

		if (buf.cursize > MAX_MSGLEN/2) 
		{
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	//
	// Write entity baselines.
	//
	memset(&blankes, 0, sizeof(blankes));
	for (i = 0; i < CL_MAX_EDICTS; i++)
	{
		es = &cl_entities[i].baseline;

		//
		// If the entity state isn't blank write it to the buffer.
		//
		if (memcmp(es, &blankes, sizeof(blankes)))
		{
			// Write ID.
			MSG_WriteByte (&buf, svc_spawnbaseline);
			MSG_WriteShort (&buf, i);

			// Write model info.
			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);

			// Write coordinates and angles.
			for (j = 0; j < 3; j++)
			{
				MSG_WriteCoord(&buf, es->origin[j]);
				MSG_WriteAngle(&buf, es->angles[j]);
			}

			// Flush to demo file if buffer is half full.
			if (buf.cursize > MAX_MSGLEN / 2)
			{
				CL_WriteStartupDemoMessage (&buf, seq++);
				SZ_Clear (&buf);
			}
		}
	}

	// Write spawn information into the clients console buffer.
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i 0\n", cl.servercount));

	// Flush buffer to demo file.
	if (buf.cursize)
	{
		CL_WriteStartupDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}

	//
	// Send current status of all other players.
	//
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		player = cl.players + i;

		// Frags.
		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->frags);

		// Ping.
		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->ping);

		// Packet loss.
		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->pl);

		// Entertime.
		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, cls.realtime - player->entertime);

		// User ID and user info.
		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, player->userinfo);

		// Flush buffer to demo file.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// Send all current light styles.
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		// Don't send empty lightstyle strings.
		if (!cl_lightstyle[i].length)
			continue;

		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, cl_lightstyle[i].map);
	}

	for (i = 0; i < MAX_CL_STATS; i++)
	{
		// No need to send zero values.
		if (!cl.stats[i])
			continue;

		// Write the current players user stats.
		if (cl.stats[i] >= 0 && cl.stats[i] <= 255)
		{
			MSG_WriteByte (&buf, svc_updatestat);
			MSG_WriteByte (&buf, i);
			MSG_WriteByte (&buf, cl.stats[i]);
		}
		else
		{
			MSG_WriteByte (&buf, svc_updatestatlong);
			MSG_WriteByte (&buf, i);
			MSG_WriteLong (&buf, cl.stats[i]);
		}

		// Flush buffer to demo file.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			CL_WriteStartupDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// Get the client to check and download skins
	// when that is completed, a begin command will be issued.
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("skins\n"));

	CL_WriteStartupDemoMessage (&buf, seq++);

	CL_WriteSetDemoMessage();
}

//=========================================================
// MVD demo writing
//=========================================================

static FILE *mvdrecordfile = NULL;
static char mvddemoname[2 * MAX_OSPATH] = {0};

static void CL_MVD_DemoWrite (void *data, int len)
{
	if (!mvdrecordfile)
		return;

	fwrite(data, len, 1, mvdrecordfile);
}

// ====================
// CL_WriteRecordMVDMessage
// ====================
static void CL_WriteRecordMVDMessage (sizebuf_t *msg)
{
	int len;
	byte c;

	if (!cls.mvdrecording)
		return;

	if (!msg->cursize)
		return;

	c = 0;
	CL_MVD_DemoWrite (&c, sizeof(c));

	c = dem_all;
	CL_MVD_DemoWrite (&c, sizeof(c));

	len = LittleLong (msg->cursize);
	CL_MVD_DemoWrite (&len, 4);

	CL_MVD_DemoWrite (msg->data, msg->cursize);
}

// ====================
// CL_WriteRecordMVDStatsMessage
// ====================
static void CL_WriteRecordMVDStatsMessage (sizebuf_t *msg, int client)
{
	int len;
	byte c;

	if (!cls.mvdrecording)
		return;

	if (!msg->cursize)
		return;

	if (client < 0 || client >= MAX_CLIENTS)
		return;

	c = 0;
	CL_MVD_DemoWrite (&c, sizeof(c));

	c = dem_stats | (client << 3) ; // msg "type" and "to" incapsulated in one byte
	CL_MVD_DemoWrite (&c, sizeof(c));

	len = LittleLong (msg->cursize);
	CL_MVD_DemoWrite (&len, 4);

	CL_MVD_DemoWrite (msg->data, msg->cursize);
}

// TODO: Split this up?
void CL_WriteMVDStartupData(void)
{
	sizebuf_t	buf;
	unsigned char buf_data[MAX_MSGLEN * 4];
	player_info_t *player;
	entity_state_t *es, blankes;
	entity_t *ent;
	int i, j, n;
	char *s;

	// Serverdata
	// send the info about the new client to all connected clients
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// send the serverdata

	MSG_WriteByte (&buf, svc_serverdata);

	#ifdef PROTOCOL_VERSION_FTE
	if (cls.fteprotocolextensions)
	{
		MSG_WriteLong (&buf, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (&buf, cls.fteprotocolextensions);
	}
	#endif // PROTOCOL_VERSION_FTE

	#ifdef PROTOCOL_VERSION_FTE2
	if (cls.fteprotocolextensions2)
	{
		MSG_WriteLong (&buf, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (&buf, cls.fteprotocolextensions2);
	}
	#endif // PROTOCOL_VERSION_FTE2

	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, cl.servercount);

	//
	// Gamedir.
	//

	s = cls.gamedirfile; // FIXME: or do we need to take a look in serverinfo?
	if (!s[0])
		s = "qw"; // If empty use "qw" gamedir

	MSG_WriteString (&buf, s);

	MSG_WriteFloat (&buf, cls.demotime); // FIXME: not sure

	// Send full levelname.
	MSG_WriteString (&buf, cl.levelname);

	// Send the movevars.
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

	// Send music.
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0); // None in demos.

	// Send server info string.
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("fullserverinfo \"%s\"\n", cl.serverinfo));

	// Flush packet.
	CL_WriteRecordMVDMessage (&buf);
	SZ_Clear (&buf);

	//
	// Soundlist.
	//
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.sound_name[n + 1];
	while (*s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.sound_name[n + 1];
	}

	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// Modellist.
	//
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.model_name[n+1];
	while (*s)
	{
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN/2)
		{
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n+1];
	}

	if (buf.cursize)
	{
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// Write static entities.
	//
	for (i = 0; i < cl.num_statics; i++)
	{
		// Get the next static entity.
		ent = cl_static_entities + i;

		// Write ID for static entities.
		MSG_WriteByte (&buf, svc_spawnstatic);

		// Find if the model is precached or not.
		for (j = 1; j < MAX_MODELS; j++)
		{
			if (ent->model == cl.model_precache[j])
				break;
		}

		// Write if the model is precached.
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		// Write the entities frame and skin number.
		MSG_WriteByte (&buf, ent->frame);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->skinnum);

		// Write the coordinate and angles.
		for (j = 0; j < 3; j++)
		{
			MSG_WriteCoord (&buf, ent->origin[j]);
			MSG_WriteAngle (&buf, ent->angles[j]);
		}

		// Flush the buffer if it's half full.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// Write static sounds
	//
	for (i = 0; i < cl.num_static_sounds; i++)
	{
		static_sound_t *ss = &cl.static_sounds[i];

		MSG_WriteByte (&buf, svc_spawnstaticsound);

		for (j = 0; j < 3; j++)
			MSG_WriteCoord (&buf, ss->org[j]);

		MSG_WriteByte (&buf, ss->sound_num);
		MSG_WriteByte (&buf, ss->vol);
		MSG_WriteByte (&buf, ss->atten);

		if (buf.cursize > MAX_MSGLEN/2)
		{
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// Write entity baselines.
	//
	memset(&blankes, 0, sizeof(blankes));

	for (i = 0; i < CL_MAX_EDICTS; i++)
	{
		es = &cl_entities[i].baseline;

		//
		// If the entity state isn't blank write it to the buffer.
		//
		if (memcmp(es, &blankes, sizeof(blankes)))
		{
			// Write ID.
			MSG_WriteByte (&buf, svc_spawnbaseline);
			MSG_WriteShort (&buf, i);

			// Write model info.
			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);

			// Write coordinates and angles.
			for (j = 0; j < 3; j++)
			{
				MSG_WriteCoord(&buf, es->origin[j]);
				MSG_WriteAngle(&buf, es->angles[j]);
			}

			// Flush to demo file if buffer is half full.
			if (buf.cursize > MAX_MSGLEN / 2)
			{
				CL_WriteRecordMVDMessage (&buf);
				SZ_Clear (&buf);
			}
		}
	}

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// Send all current light styles.
	//
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		// Don't send empty lightstyle strings.
		if (!cl_lightstyle[i].length)
			continue;

		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char)i);
		MSG_WriteString (&buf, cl_lightstyle[i].map);

		// Flush to demo file if buffer is half full.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va("cmd spawn %i 0\n", cl.servercount));

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// Send current status of all other players: frags, ping, pl, enter time, userinfo, player id.
	//
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		player = cl.players + i;

		// Do NOT ignore spectators here, since we need at least userinfo.

		// Frags.
		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->frags);

		// Ping.
		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->ping);

		// Packet loss.
		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->pl);

		// Entertime.
		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, cls.realtime - player->entertime);

		// User ID and user info.
		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, player->userinfo);

		// Flush buffer to demo file.
		if (buf.cursize > MAX_MSGLEN / 2)
		{
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	//
	// This set proper model, origin, angles etc for players.
	//
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		vec3_t origin, angles;
		int j, flags;
		player_state_t *state;

		player = cl.players + i;

		state = cl.frames[cl.validsequence & UPDATE_MASK].playerstate + i;

		if (!player->name[0])
			continue;

		if (player->spectator)
			continue; // Ignore spectators.

		flags =   (DF_ORIGIN << 0) | (DF_ORIGIN << 1) | (DF_ORIGIN << 2)
				| (DF_ANGLES << 0) | (DF_ANGLES << 1) | (DF_ANGLES << 2)
				| DF_EFFECTS | DF_SKINNUM 
				| ((state->flags & PF_DEAD) ? DF_DEAD : 0)
				| ((state->flags & PF_GIB)  ? DF_GIB  : 0)
				| DF_WEAPONFRAME | DF_MODEL;

		VectorCopy(state->origin, origin);
		VectorCopy(state->viewangles, angles);

		MSG_WriteByte (&buf, svc_playerinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, flags);

		MSG_WriteByte (&buf, state->frame);

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ORIGIN << j))
				MSG_WriteCoord (&buf, origin[j]);

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ANGLES << j))
				MSG_WriteAngle16 (&buf, angles[j]);

		if (flags & DF_MODEL)
			MSG_WriteByte (&buf, state->modelindex);

		if (flags & DF_SKINNUM)
			MSG_WriteByte (&buf, state->skinnum);

		if (flags & DF_EFFECTS)
			MSG_WriteByte (&buf, state->effects);

		if (flags & DF_WEAPONFRAME)
			MSG_WriteByte (&buf, state->weaponframe);

		if (buf.cursize > MAX_MSGLEN/2)
		{
			CL_WriteRecordMVDMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	// we really need clear buffer before sending stats
	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// send stats
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		int		*stats;
		int		j;

		player = cl.players + i;

		if (!player->name[0])
			continue;

		if (player->spectator)
			continue; // Ignore spectators.

		stats = cl.players[i].stats;

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
			CL_WriteRecordMVDStatsMessage(&buf, i);
			SZ_Clear (&buf);
		}
	}

	// Above stats writing must clear buffer.
	if (buf.cursize)
	{
		Sys_Error("CL_WriteMVDStartupData: buf.cursize %d", buf.cursize);
	}

	// 
	// Send packetentities.
	//
	{
		int ent_index, ent_total;
		entity_state_t *ent_state;

		// Write the ID byte for a delta entity operation to the demo.
		MSG_WriteByte (&buf, svc_packetentities);

		// Get the entities list from the frame.
		ent_state = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].packet_entities.entities;
		ent_total = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].packet_entities.num_entities;

		// Write all the entity changes since last packet entity message.
		for (ent_index = 0; ent_index < ent_total; ent_index++, ent_state++)
			MSG_WriteDeltaEntity (&cl_entities[ent_state->number].baseline, ent_state, &buf, true);

		// End of packetentities.
		MSG_WriteShort (&buf, 0);
	}

	if (buf.cursize)
	{
		CL_WriteRecordMVDMessage (&buf);
		SZ_Clear (&buf);
	}

	// Get the client to check and download skins
	// when that is completed, a begin command will be issued.
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, "skins\n");

	CL_WriteRecordMVDMessage (&buf);
}

// ==============
// Commands
// ==============

void CL_StopMvd_f(void)
{
	if (!cls.mvdrecording && !mvdrecordfile)
	{
		Com_Printf ("Not recording a demo\n");
		return;
	}

	if (mvdrecordfile)
	{
		char *quotes[] = {
	       " Make love not WarCraft\n"
	       " Get quake at http://nquake.sf.net\n",
	       " ez come ez go\n"
	       " Get ezQuake at http://ezQuake.sf.net\n",
	       " In the name of fun\n"
	       " Visit http://quakeworld.nu\n"
		};

		char str[1024];
		sizebuf_t	buf;
		unsigned char buf_data[MAX_MSGLEN];

		SZ_Init (&buf, buf_data, sizeof(buf_data));

		// Print offensive message.
		snprintf(str, sizeof(str),
				"\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f\n"
		        "%s"
				"\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f\n",
				quotes[i_rnd( 0, sizeof(quotes)/sizeof(quotes[0]) - 1 )]);

		MSG_WriteByte(&buf, svc_print);
		MSG_WriteByte(&buf, 2);
		MSG_WriteString(&buf, str);

		// Add disconnect.

		MSG_WriteByte (&buf, svc_disconnect);
		MSG_WriteString (&buf, "EndOfDemo");

		CL_WriteRecordMVDMessage (&buf);

		fclose(mvdrecordfile);
		mvdrecordfile = NULL;

		Com_Printf ("Completed demo\n");
	}
	else
	{
		Com_Printf ("BUG: Demo alredy completed or something\n");
	}

	cls.mvdrecording = false;
}

void CL_RecordMvd_f(void)
{
	char nameext[MAX_OSPATH * 2], name[MAX_OSPATH * 2];

	switch(Cmd_Argc())
	{
		case 1:
		//
		// Just show if anything is being recorded.
		//
		{
			if (cls.mvdrecording)
				Com_Printf("Recording to %s\n", mvddemoname);
			else
				Com_Printf("Not recording\n");
			break;
		}
		case 2:
		//
		// Start recording to the specified demo name.
		//
		{
			if (cls.state != ca_active && cls.state != ca_disconnected)
			{
				Com_Printf ("Cannot record whilst connecting\n");
				return;
			}

			// Stop any recording in progress.
			if (cls.mvdrecording)
				CL_StopMvd_f();

			// Make sure the filename doesn't contain any invalid characters.
			if (!Util_Is_Valid_Filename(Cmd_Argv(1)))
			{
				Com_Printf(Util_Invalid_Filename_Msg(Cmd_Argv(1)));
				return;
			}

			// Open the demo file for writing.
			strlcpy(nameext, Cmd_Argv(1), sizeof(nameext));
			COM_ForceExtensionEx (nameext, ".mvd", sizeof (nameext));

			// Get the path for the demo and try opening the file for writing.
			snprintf (name, sizeof(name), "%s/%s", CL_DemoDirectory(), nameext);

			mvdrecordfile = fopen(name, "wb");

			if (!mvdrecordfile)
			{
				Com_Printf ("Error: Couldn't record to %s. Make sure path exists.\n", name);
				return;
			}

			// Demo starting has begun.
			cls.mvdrecording = true;

			// Save the demoname for later use.
			strlcpy(mvddemoname, name, sizeof(mvddemoname));

			// If we're active, write startup data right away.
			if (cls.state == ca_active)
				CL_WriteMVDStartupData();

			Com_Printf ("Recording to %s\n", nameext);
			break;
		}
		default:
		{
			Com_Printf("Usage: %s [demoname]\n", Cmd_Argv(0));
			break;
		}
	}
}

//=============================================================================
//								DEMO READING
//=============================================================================

vfsfile_t *playbackfile = NULL;			// The demo file used for playback.
float demo_time_length = 0;				// The length of the demo.

unsigned char pb_buf[1024*32];			// Playback buffer.
int		pb_cnt = 0;						// How many bytes we've have in playback buffer.
qbool	pb_eof = false;					// Have we reached the end of the playback buffer?

//
// Inits the demo playback buffer.
// If we do QTV demo playback, we read data ahead while parsing QTV connection headers,
// so we may have demo data in qtvrequestbuffer[], so we move data from qtvrequestbuffer[] to pb_buf[] with this function.
//
void CL_Demo_PB_Init(void *buf, int buflen)
{
	// The length of the buffer is out of bounds.
	if (buflen < 0 || buflen > (int)sizeof(pb_buf))
		Sys_Error("CL_Demo_PB_Init: buflen out of bounds.");

	// Copy the specified init data into the playback buffer.
	memcpy(pb_buf, buf, buflen);

	// Reset any associated playback buffers.
	pb_cnt = buflen;
	pb_eof = false;
}

//
// This is memory reading(not from file or socket), we just copy data from pb_buf[] to caller buffer,
// sure if we're not peeking we decrease pb_buf[] size (pb_cnt) and move along pb_buf[] itself.
//
int CL_Demo_Read(void *buf, int size, qbool peek)
{
	int need;

	// Size can't be negative.
	if (size < 0)
		Host_Error("pb_read: size < 0");

	need = max(0, min(pb_cnt, size));
	memcpy(buf, pb_buf, need);

	if (!peek)
	{
		// We are not peeking, so move along buffer.
		pb_cnt -= need;
		memmove(pb_buf, pb_buf + need, pb_cnt);

		// We get some data from playback file or qtv stream, dump it to file right now.
		if (need > 0 && cls.mvdplayback && cls.mvdrecording)
			CL_MVD_DemoWrite(buf, need);
	}

	if (need != size)
		Host_Error("Unexpected end of demo");

	return need;
}

//
// Reads a chunk of data from the playback file and returns the number of bytes read.
//
static int pb_raw_read(void *buf, int size)
{
	vfserrno_t err;
	int r = VFS_READ(playbackfile, buf, size, &err);

	// Size > 0 mean detect EOF only if we actually trying read some data.
	if (size > 0 && !r && err == VFSERR_EOF)
		pb_eof = true;

	return r;
}

//
// Ensure we have enough data to parse, it not then return false.
// Function was introduced with QTV. If you read a demo from file and you run out of data
// that's critical, but when streaming an MVD using QTV and don't receive enough data from 
// the network it isn't critical, we just freeze the client for some time while filling
// the playback buffer.
//
qbool pb_ensure(void)
{
	// Increase internal TCP buffer by faking a read to it.
	pb_raw_read(NULL, 0);

	// Show how much we've read.
	if (cl_shownet.value == 3)
		Com_Printf(" %d", pb_cnt);

	// Try to fill the entire buffer with demo data.
	pb_cnt += pb_raw_read(pb_buf + pb_cnt, max(0, (int)sizeof(pb_buf) - pb_cnt));

	if (pb_cnt == (int)sizeof(pb_buf) || pb_eof)
		return true; // Return true if we have full buffer or get EOF.

	// Probably not enough data in buffer, check do we have at least one message in buffer.
	if (cls.mvdplayback && pb_cnt)
	{
		if(ConsistantMVDData((unsigned char*)pb_buf, pb_cnt))
			return true;
	}

	// Set the buffering time if it hasn't been set already.
	if (cls.mvdplayback == QTV_PLAYBACK && !bufferingtime && !cls.qtv_donotbuffer)
	{
		double prebufferseconds = QTVBUFFERTIME;

		bufferingtime = Sys_DoubleTime() + prebufferseconds;

		if (developer.integer >= 2)
			Com_DPrintf("&cF00" "qtv: not enough buffered, buffering for %.1fs\n" "&r", prebufferseconds); // print some annoying message
	}

	return false;
}

//
// Peeks the demo time.
//
static float CL_PeekDemoTime(void)
{
	float demotime = 0.0;

	if (cls.mvdplayback)
	{
		byte mvd_time = 0; // Number of miliseconds since last frame. Can be between 0-255. MVD Only.

		// Peek inside, but don't read.
		// (Since it might not be time to continue reading in the demo
		// we want to be able to check this again later if that's the case).
		CL_Demo_Read(&mvd_time, sizeof(mvd_time), true);

		// Calculate the demo time.
		// (The time in an MVD is saved as a byte with number of miliseconds since the last cmd
		// so we need to multiply it by 0.001 to get it in seconds like normal quake time).
		demotime = cls.demopackettime + (mvd_time * 0.001);

		if ((cls.demotime - nextdemotime) > 0.0001 && (nextdemotime != demotime))
		{
			olddemotime = nextdemotime;
			cls.netchan.incoming_sequence++;
			cls.netchan.incoming_acknowledged++;
			cls.netchan.frame_latency = 0;
			cls.netchan.last_received = cls.demotime; // Make timeout check happy.
			nextdemotime = demotime;
		}
	}
	else 
	{
		// Peek inside, but don't read.
		// (Since it might not be time to continue reading in the demo
		// we want to be able to check this again later if that's the case).
		CL_Demo_Read(&demotime, sizeof(demotime), true);
		demotime = LittleFloat(demotime);

		if (demotime < cls.demotime)
		{
			olddemotime = nextdemotime;

			nextdemotime = demotime;
		}
	}

	return demotime;
}

//
// Consume the demo time.
//
static void CL_ConsumeDemoTime(void)
{
	if (cls.mvdplayback)
	{
		byte dummy_newtime;
		CL_Demo_Read(&dummy_newtime, sizeof(dummy_newtime), false);
	}
	else
	{
		float dummy_demotime;
		CL_Demo_Read(&dummy_demotime, sizeof(dummy_demotime), false);
	}
}

//
// Reads a dem_cmd message from an demo.
//
static void CL_DemoReadDemCmd(void)
{			
	// User cmd read.
	extern int cmdtime_msec;

	// Get which frame we should read the cmd into from the demo.
	int i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	int j;

	// Read the user cmd from the demo.
	usercmd_t *pcmd = &cl.frames[i].cmd;
	CL_Demo_Read(pcmd, sizeof(*pcmd), false);

	// Convert the angles/movement vectors into the correct byte order.
	for (j = 0; j < 3; j++)
		pcmd->angles[j] = LittleFloat(pcmd->angles[j]);
	pcmd->forwardmove = LittleShort(pcmd->forwardmove);
	pcmd->sidemove = LittleShort(pcmd->sidemove);
	pcmd->upmove = LittleShort(pcmd->upmove);
	cmdtime_msec += pcmd->msec;

	// Set the time time this cmd was sent and increase
	// how many net messages have been sent.
	cl.frames[i].senttime = cls.demopackettime;
	cl.frames[i].receivedtime = -1;		// We haven't gotten a reply yet.
	cls.netchan.outgoing_sequence++;

	// Read the viewangles from the demo and convert them to correct byte order.
	CL_Demo_Read(cl.viewangles, 12, false);
	for (j = 0; j < 3; j++)
		cl.viewangles[j] = LittleFloat (cl.viewangles[j]);

	// Calculate the player fps based on the cmd.
	CL_CalcPlayerFPS(&cl.players[cl.playernum], pcmd->msec);

	// Try locking on to a player.
	if (cl.spectator)
		Cam_TryLock();

	// Write the demo to the record file if we're recording.
	if (cls.demorecording)
		CL_WriteDemoCmd(pcmd);
}

//
// Reads a dem_read message from a demo. Returns true if we should continue reading messages.
//
static qbool CL_DemoReadDemRead(void)
{
	// Read the size of next net message in the demo file
	// and convert it into the correct byte order.
	CL_Demo_Read(&net_message.cursize, 4, false);
	net_message.cursize = LittleLong(net_message.cursize);

	// The message was too big, stop playback.
	if (net_message.cursize > net_message.maxsize)
	{
		Com_DPrintf("CL_GetDemoMessage: net_message.cursize > net_message.maxsize");
		Host_EndGame();
		Host_Abort();
	}

	// Read the net message from the demo.
	CL_Demo_Read(net_message.data, net_message.cursize, false);

	return false;
}

//
// Reads a dem_set message from a demo.
//
static void CL_DemoReadDemSet(void)
{
	int i;

	CL_Demo_Read(&i, sizeof(i), false);
	cls.netchan.outgoing_sequence = LittleLong(i);

	CL_Demo_Read(&i, sizeof(i), false);
	cls.netchan.incoming_sequence = LittleLong(i);

	if (cls.mvdplayback)
		cls.netchan.incoming_acknowledged = cls.netchan.incoming_sequence;
}

//
// Returns true if it's time to read the next message, false otherwise.
//
static qbool CL_DemoShouldWeReadNextMessage(float demotime)
{
	if (cls.timedemo)
	{
		// Timedemo playback, grab the next message as quickly as possible.

		if (cls.td_lastframe < 0)
		{
			// This is the first frame of the timedemo.
			cls.td_lastframe = demotime;
		}
		else if (demotime > cls.td_lastframe)
		{
			// We've already read this frame's message so skip it.
			cls.td_lastframe = demotime;
			return false;
		}

		// Did we just start the time demo?
		if (!cls.td_starttime && (cls.state == ca_active))
		{
			// Save the start time (real world time) and current frame number
			// so that we will know how long it took to go through it all
			// and calculate the framerate when it's done.
			cls.td_starttime = Sys_DoubleTime();
			cls.td_startframe = cls.framecount;
		}

		cls.demotime = demotime; // Warp.
	}
	else if (!(cl.paused & PAUSED_SERVER) && (cls.state == ca_active)) // Always grab until fully connected.
	{
		// Not paused and active.

		if (cls.mvdplayback)
		{
			if (nextdemotime < demotime)
			{
				return false; // Don't need another message yet.
			}
		}
		else
		{
			if (cls.demotime < demotime)
			{
				// Don't need another message yet.

				// Adjust the demotime to match what's read from file.
				if (cls.demotime + 1.0 < demotime)
					cls.demotime = demotime - 1.0;

				return false;
			}
		}
	}
	else
	{
		cls.demotime = demotime; // We're warping.
	}

	return true;
}

//
// When a demo is playing back, all NET_SendMessages are skipped, and NET_GetMessages are read from the demo file.
// Whenever cl.time gets past the last received message, another message is read from the demo file.
//
// Summary:
//
// 1. MVD - Make sure we're not more than 1 second behind the next demo time.
// 2. Get the time of the next demo message by peeking at it.
// 3. Is it time to get the next demo message yet? (Always for timedemo) Return false if not.
// 4. Read the time of the next message from the demo (consume it).
// 5. MVD - Save the current time so we have it for the next frame
//    (we need it to calculate the demo time. Time in MVDs is saved
//     as the number of miliseconds since last frame).
// 6. Read the message type from the demo, only the first 3 bits are significant.
//    There are 3 basic message types used by all demos:
//    dem_set = Only appears once at start of demo. Contains sequence numbers for the netchan.
//    dem_cmd = A user movement cmd.
//    dem_Read = An arbitrary network message.
//
//    MVDs also use:
//    dem_multiple, dem_single, dem_stats and dem_all to direct certain messages to
//    specific clients only, and to update stats.
//
// 7. Parse the specific demo messages.
//
qbool CL_GetDemoMessage (void)
{
	float demotime;
	byte c;
	byte message_type;

	// Don't try to play while QWZ is being unpacked.
	#ifdef _WIN32
	if (qwz_unpacking)
		return false;
	#endif

	// Only read packets when in main POV
	if (!CL_Demo_IsPrimaryPointOfView ())
		return false;

	// Demo paused, don't read anything.
	if (cl.paused & PAUSED_DEMO)
	{
		pb_ensure();	// Make sure we keep the QTV connection alive by reading from the socket.
		return false;
	}

	// We're not ready to parse since we're buffering for QTV.
	if (bufferingtime && (bufferingtime > Sys_DoubleTime()))
	{
		extern qbool	host_skipframe;

		pb_ensure();			// Fill the buffer for QTV.
		host_skipframe = true;	// This will force not update cls.demotime.

		return false;
	}

	bufferingtime = 0;

	// DEMO REWIND.
	if (!cls.mvdplayback || cls.mvdplayback != QTV_PLAYBACK)
	{
		CL_Demo_Check_For_Rewind(nextdemotime);
	}

	// Adjust the time for MVD playback.
	if (cls.mvdplayback)
	{
		// Reset the previous time.
		if (cls.demopackettime < nextdemotime)
			cls.demopackettime = nextdemotime;

		// Always be within one second from the next demo time.
		if (cls.demotime + 1.0 < nextdemotime)
			cls.demotime = nextdemotime - 1.0;
	}

	// Check if we need to get more messages for now and if so read it
	// from the demo file and pass it on to the net channel.
	while (true)
	{
		// Make sure we have enough data in the buffer.
		if (!pb_ensure())
			return false;

		// Read the time of the next message in the demo.
		demotime = CL_PeekDemoTime();

		// Keep gameclock up-to-date if we are seeking
		if (cls.demoseeking && demotime > cls.demopackettime)
			cl.gametime += demotime - cls.demopackettime;

		// Keep MVD features such as itemsclock up-to-date during seeking
		if (cls.demoseeking && cls.mvdplayback) {
			double tmp = cls.demotime;
			cls.demotime = demotime;
			MVD_Interpolate();
			MVD_Mainhook();
			cls.demotime = tmp;
		}

		if (cls.demoseeking == DST_SEEKING_STATUS)
			CL_Demo_Jump_Status_Check();

		// If we found demomark, we should stop seeking, so reset time to the proper value.
		if (cls.demoseeking == DST_SEEKING_FOUND) {
			cls.demotime = demotime; // this will trigger seeking stop

			if (demo_jump_rewind.value < 0) {
				CL_Demo_Jump (-demo_jump_rewind.value, -1, DST_SEEKING_NORMAL);
			}
			return false;
		}

		// If we've reached our seek goal, stop seeking.
		if (cls.demoseeking && cls.demotime <= demotime && cls.state >= ca_active)
		{
			cls.demoseeking = DST_SEEKING_NONE;

			if (cls.demorewinding)
			{
				CL_Demo_Stop_Rewinding();
			}
		}

		playback_recordtime = demotime;

		// Decide if it is time to grab the next message from the demo yet.
		if (!CL_DemoShouldWeReadNextMessage(demotime))
		{
			return false;
		}
		
		// Read the time from the packet (we peaked at it earlier),
		// we're ready to get the next message.
		CL_ConsumeDemoTime();

		// Save the previous time for MVD playback (for the next message),
		// it is needed to calculate the demotime since in mvd's the time is
		// saved as the number of miliseconds since last frame message.
		// This is also used when seeking in qwds to keep the gameclock in time.
		cls.demopackettime = demotime;

		// Get the msg type.
		CL_Demo_Read(&c, sizeof(c), false);
		message_type = (c & 7);

		// QWD Only.
		if (message_type == dem_cmd)
		{
			CL_DemoReadDemCmd();
			
			continue; // Get next message.
		}

		// MVD Only. These message types tells to which players the message is directed to.
		if ((message_type >= dem_multiple) && (message_type <= dem_all))
		{
			switch (message_type)
			{
				case dem_multiple:
				//
				// This message should be sent to more than one player, get which players.
				//
				{
					// Read the player bit mask from the demo and convert to the correct byte order.
					// Each bit in this number represents a player, 32-bits, 32 players.
					int i;
					CL_Demo_Read(&i, 4, false);
					cls.lastto = LittleLong(i);
					cls.lasttype = dem_multiple;
					break;
				}
				case dem_stats:
				//
				// The stats for a player has changed. Get the player number of that player.
				//
				case dem_single:
				//
				// Only a single player should receive this message. Get the player number of that player.
				//
				{
					// The first 3 bits contain the message type (so remove that part), the rest contains
					// a 5-bit number which contains the player number of the affected player.
					cls.lastto = (c >> 3);
					cls.lasttype = message_type;
					break;
				}
				case dem_all:
				//
				// This message is directed to all players.
				//
				{
					cls.lastto = 0;
					cls.lasttype = dem_all;
					break;
				}
				default:
				{
					Host_Error("This can't happen (unknown command type) %c.\n", message_type);
				}
			}

			// Fall through to dem_read after we've gotten the affected players.
			message_type = dem_read;
		}

		// Get the next net message from the demo file.
		if (message_type == dem_read)
		{
			if (CL_DemoReadDemRead())
			{
				continue; // Continue reading messages.
			}
			
			return true; // We just read something.
		}

		// Gets the sequence numbers for the netchan at the start of the demo.
		if (message_type == dem_set)
		{
			CL_DemoReadDemSet();
			
			continue;
		}

		// We should never get this far if the demo is ok.
		Host_Error("Corrupted demo");
		return false;
	}
}

//=============================================================================
//								DEMO RECORDING
//=============================================================================

static char demoname[2 * MAX_OSPATH];
static char fulldemoname[MAX_PATH];
static qbool autorecording = false;
static qbool easyrecording = false;

void CL_AutoRecord_StopMatch(void);
void CL_AutoRecord_CancelMatch(void);

static void OnChange_demo_dir(cvar_t *var, char *string, qbool *cancel)
{
	if (!string[0])
		return;

	// Replace \ with /.
	Util_Process_FilenameEx(string, cl_mediaroot.integer == 2);

	// Make sure the filename doesn't have any invalid chars in it.
	if (!Util_Is_Valid_FilenameEx(string, cl_mediaroot.integer == 2))
	{
		Com_Printf(Util_Invalid_Filename_Msg(var->name));
		*cancel = true;
		return;
	}

	// Change to the new folder in the demo browser.
	// FIXME: this did't work
	Menu_Demo_NewHome(string);
}

#ifdef _WIN32
static void OnChange_demo_format(cvar_t *var, char *string, qbool *cancel)
{
	char* allowed_formats[5] = { "qwd", "qwz", "mvd", "mvd.gz", "qwd.gz" };
	int i;

	for (i = 0; i < 5; i++)
		if (!strcmp(allowed_formats[i], string))
			return;

	Com_Printf("Not valid demo format. Allowed values are: ");
	for (i = 0; i < 5; i++)
	{
		if (i)
			Com_Printf(", ");
		Com_Printf(allowed_formats[i]);
	}
	Com_Printf(".\n");

	*cancel = true;
}
#endif

//
// Writes a "pimp message" for ezQuake at the end of a demo.
//
static void CL_WriteDemoPimpMessage(void)
{
	int i;
	char pimpmessage[256], border[64];

	if (cls.demoplayback)
		return;

	strlcpy (border, "\x1d", sizeof (border));

	for (i = 0; i < 34; i++)
		strlcat (border, "\x1e", sizeof (border));

	strlcat (border, "\x1f", sizeof (border));

	snprintf (pimpmessage, sizeof(pimpmessage), "\n%s\n%s\n%s\n",
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

//
// Stop recording a demo.
//
static void CL_StopRecording (void)
{
	// Nothing to stop.
	if (!cls.demorecording)
		return;

	// Write a pimp message to the demo.
	CL_WriteDemoPimpMessage();

	// Write a disconnect message to the demo file.
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);	// -1 sequence means out of band
	MSG_WriteByte (&net_message, svc_disconnect);
	MSG_WriteString (&net_message, "EndOfDemo");
	CL_WriteDemoMessage (&net_message);

	// Finish up by closing the demo file.
	CL_Demo_Close();
	cls.demorecording = false;
}

//
// Stop recording a demo.
//
void CL_Stop_f (void)
{
	if (com_serveractive && strcmp(Cmd_Argv(0), "stop") == 0)
	{
		SV_MVDStop_f();
		return;
	}

	if (!cls.demorecording)
	{
		Com_Printf ("Not recording a demo\n");
		return;
	}
	if (autorecording)
	{
		CL_AutoRecord_StopMatch();
#ifdef _WIN32
	}
	else if (easyrecording)
	{
		CL_StopRecording();
		CL_Demo_Compress(fulldemoname);
		easyrecording = false;
#endif
	}
	else
	{
		CL_StopRecording();
		Com_Printf ("Completed demo\n");
	}
}

//
// Returns the Demo directory. If the user hasn't set the demo_dir var, the gamedir is returned.
//
char *CL_DemoDirectory(void)
{
	static char dir[MAX_PATH];

	strlcpy(dir, FS_LegacyDir(demo_dir.string), sizeof(dir));
	return dir;
}

//
// Start recording a demo.
//
void CL_Record_f (void)
{
	char nameext[MAX_OSPATH * 2], name[MAX_OSPATH * 2];

	if (com_serveractive && strcmp(Cmd_Argv(0), "record") == 0)
	{
		SV_MVD_Record_f();
		return;
	}

	if (cls.fteprotocolextensions &~ (FTE_PEXT_CHUNKEDDOWNLOADS|FTE_PEXT_256PACKETENTITIES))
	{
		Com_Printf ("WARNING: FTE protocol extensions enabled; this demo most likely will be unplayable in older clients. "
			"Use cl_pext 0 for 100%% compatible demos. But do NOT forget set it to 1 later or you will lack useful features!\n");
	}

	switch(Cmd_Argc())
	{
		case 1:
		//
		// Just show if anything is being recorded.
		//
		{
			if (autorecording)
				Com_Printf("Auto demo recording is in progress\n");
			else if (cls.demorecording)
				Com_Printf("Recording to %s\n", demoname);
			else
				Com_Printf("Not recording\n");
			break;
		}
		case 2:
		//
		// Start recording to the specified demo name.
		//
		{
			if (cls.mvdplayback)
			{
				Com_Printf ("Cannot record during mvd playback\n");
				return;
			}

			if (cls.state != ca_active && cls.state != ca_disconnected)
			{
				Com_Printf ("Cannot record whilst connecting\n");
				return;
			}

			if (autorecording)
			{
				Com_Printf("Auto demo recording must be stopped first!\n");
				return;
			}

			// Stop any recording in progress.
			if (cls.demorecording)
				CL_Stop_f();

			// Make sure the filename doesn't contain any invalid characters.
			if (!Util_Is_Valid_Filename(Cmd_Argv(1)))
			{
				Com_Printf(Util_Invalid_Filename_Msg(Cmd_Argv(1)));
				return;
			}

			// Open the demo file for writing.
			strlcpy(nameext, Cmd_Argv(1), sizeof(nameext));
			COM_ForceExtensionEx (nameext, ".qwd", sizeof (nameext));

			// Get the path for the demo and try opening the file for writing.
			snprintf (name, sizeof(name), "%s/%s", CL_DemoDirectory(), nameext);
			if (!CL_Demo_Open(name))
			{
				Com_Printf ("Error: Couldn't record to %s. Make sure path exists.\n", name);
				return;
			}

			// Demo starting has begun.
			cls.demorecording = true;

			// If we're active, write startup data right away.
			if (cls.state == ca_active)
				CL_WriteStartupData();

			// Save the demoname for later use.
			strlcpy(demoname, nameext, sizeof(demoname));

			Com_Printf ("Recording to %s\n", nameext);

			break;
		}
		default:
		{
			Com_Printf("Usage: %s [demoname]\n", Cmd_Argv(0));
			break;
		}
	}
}

//
// Starts recording a demo using autorecord or easyrecord.
//
static qbool CL_MatchRecordDemo(char *dir, char *name, qbool autorecord)
{
	char extendedname[MAX_PATH];
	char strippedname[MAX_PATH];
	char *fullname;
	char *exts[] = {"qwd", "qwz", "mvd", NULL};
	int num;

	if (cls.state != ca_active)
	{
		Com_Printf ("You must be connected before using easyrecord\n");
		return false;
	}

	if (cls.mvdplayback)
	{
		Com_Printf ("Cannot record during mvd playback\n");
		return false;
	}

	if (autorecording)
	{
		Com_Printf("Auto demo recording must be stopped first!\n");
		return false;
	}

	// Stop any old recordings.
	if (cls.demorecording)
		CL_Stop_f();

	// Make sure we don't have any invalid chars in the demo name.
	if (!Util_Is_Valid_Filename(name))
	{
		Com_Printf(Util_Invalid_Filename_Msg(name));
		return false;
	}

	// We always record to qwd. If the user has set some other demo format
	// we convert to that later on.
	COM_ForceExtension(name, ".qwd");

	if (autorecord)
	{
		// Save the final demo name.
		strlcpy (extendedname, name, sizeof(extendedname));
	}
	else
	{
		//
		// Easy recording, file is saved using match_* settings.
		//

		// Get rid of the extension again.
		COM_StripExtension(name, strippedname, sizeof(strippedname));
		fullname = va("%s/%s", dir, strippedname);

		// Find a unique filename in the specified dir.
		if ((num = Util_Extend_Filename(fullname, exts)) == -1)
		{
			Com_Printf("Error: no available filenames\n");
			return false;
		}

		// Save the demo name..
		snprintf (extendedname, sizeof(extendedname), "%s_%03i.qwd", strippedname, num);
	}

	// Get dir + final demo name.
	fullname = va("%s/%s", dir, extendedname);

	// Open the demo file for writing.
	if (!CL_Demo_Open(fullname))
	{
		// Failed to open the file, make sure it exists and try again.
		FS_CreatePath(fullname);
		if (!CL_Demo_Open(fullname))
		{
			Com_Printf("Error: Couldn't open %s\n", fullname);
			return false;
		}
	}

	// Write the demo startup stuff.
	cls.demorecording = true;
	CL_WriteStartupData ();

	// Echo the name of the demo if we're easy recording
	// and save the demo name for later use.
	if (!autorecord)
	{
		Com_Printf ("Recording to %s\n", extendedname);
		strlcpy(demoname, extendedname, sizeof(demoname));		// Just demo name.
		strlcpy(fulldemoname, fullname, sizeof(fulldemoname));  // Demo name including path.
	}

	return true;
}

//
// Starts recording a demo and names it according to your match_ settings.
//
void CL_EasyRecord_f (void)
{
	char *name;

	if ( com_serveractive )
	{
		SV_MVDEasyRecord_f();
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf("You must be connected to easyrecord\n");
		return;
	}

	switch(Cmd_Argc())
	{
		case 1:
		{
			// No name specified by the user, get it from match tools instead.
			name = MT_MatchName();
			break;
		}
		case 2:
		{
			// User specified a demo name, use it.
			name = Cmd_Argv(1);
			break;
		}
		default:
		{
			Com_Printf("Usage: %s [demoname]\n", Cmd_Argv(0));
			return;
		}
	}

	easyrecording = CL_MatchRecordDemo(CL_DemoDirectory(), name, false);
}

//=============================================================================
//							DEMO AUTO RECORDING
//=============================================================================

static char	auto_matchname[MAX_PATH];	// Demoname when recording auto match demo.
static qbool temp_demo_ready = false;	// Indicates if the autorecorded match demo is done recording.
static float auto_starttime;

char *MT_TempDirectory(void);

extern cvar_t match_auto_record, match_auto_minlength;

#define TEMP_DEMO_NAME "_!_temp_!_.qwd"

#define DEMO_MATCH_NORECORD		0 // No autorecord.
#define DEMO_MATCH_MANUALSAVE	1 // Demo will be recorded but requires manuall saving.
#define DEMO_MATCH_AUTOSAVE		2 // Automatically saves the demo after the match is completed.

//
// Stops auto recording of a match.
//
void CL_AutoRecord_StopMatch(void)
{
	// Not doing anything.
	if (!autorecording)
		return;

	// Stop the recording and write end of demo stuff.
	autorecording = false;
	CL_StopRecording();
	temp_demo_ready = true;

	// Automatically save the demo after the match is completed.
	if (match_auto_record.value == DEMO_MATCH_AUTOSAVE)
	{
		CL_AutoRecord_SaveMatch();
		Com_Printf ("Auto record ok\n");
	}
	else
	{
		Com_Printf ("Auto demo recording completed\n");
	}
}

//
// Cancels the match.
//
void CL_AutoRecord_CancelMatch(void)
{
	// Not recording.
	if (!autorecording)
		return;

	// Stop the recording and write end of demo stuff.
	autorecording = false;
	CL_StopRecording();
	temp_demo_ready = true;

	if (match_auto_record.value == DEMO_MATCH_AUTOSAVE)
	{
		// Only save the demo if it's longer than the specified minimum length
		if (cls.realtime - auto_starttime > match_auto_minlength.value)
			CL_AutoRecord_SaveMatch();
		else
			Com_Printf("Auto demo recording cancelled\n");
	}
	else
	{
		Com_Printf ("Auto demo recording completed\n");
	}
}

//
// Starts autorecording a match.
//
void CL_AutoRecord_StartMatch(char *demoname)
{
	temp_demo_ready = false;

	// No autorecording is set.
	if (!match_auto_record.value)
		return;

	// Don't start autorecording if the
	// user already is recording a demo.
	if (cls.demorecording)
	{
		// We're autorecording since before, it's
		// ok to restart the recording then.
		if (autorecording)
		{
			autorecording = false;
			CL_StopRecording();
		}
		else
		{
			Com_Printf("Auto demo recording skipped (already recording)\n");
			return;
		}
	}

	// Save the name of the auto recorded demo for later.
	strlcpy(auto_matchname, demoname, sizeof(auto_matchname));

	// Try starting to record the demo.
	if (!CL_MatchRecordDemo(MT_TempDirectory(), TEMP_DEMO_NAME, true))
	{
		Com_Printf ("Auto demo recording failed to start!\n");
		return;
	}

	// We're now in business.
	autorecording = true;
	auto_starttime = cls.realtime;
	Com_Printf ("Auto demo recording commenced\n");
}

//
//
//
qbool CL_AutoRecord_Status(void)
{
	return temp_demo_ready ? 2 : autorecording ? 1 : 0;
}

//
// Saves an autorecorded demo.
//
void CL_AutoRecord_SaveMatch(void)
{
	//
	// All demos are first recorded in .qwd, and will then be converted to .mvd/.qwz afterwards
	// if those formats are chosen.
	//
	int error, num;
	FILE *f;
	char *dir, *tempname, savedname[MAX_PATH], *fullsavedname, *exts[] = {"qwd", "qwz", "mvd", NULL};

	// The auto recorded demo hasn't finished recording, can't do this yet.
	if (!temp_demo_ready)
		return;

	// Don't try to save it again.
	temp_demo_ready = false;

	// Get the demo dir.
	dir = CL_DemoDirectory();

	// Get the temp name of the file we've recorded.
	tempname = va("%s/%s", MT_TempDirectory(), TEMP_DEMO_NAME);

	// Get the final name where we'll save the final product.
	fullsavedname = va("%s/%s", dir, auto_matchname);

	// Find a unique filename in the final location.
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1)
	{
		Com_Printf("Error: no available filenames\n");
		return;
	}

	// Get the final full path where we'll save the demo. (This is the final name for real now)
	snprintf (savedname, sizeof(savedname), "%s_%03i.qwd", auto_matchname, num);
	fullsavedname = va("%s/%s", dir, savedname);

	// Try opening the temp file to make sure we can read it.
	if (!(f = fopen(tempname, "rb")))
		return;
	fclose(f);

	// Move the temp file to the final location.
	if ((error = rename(tempname, fullsavedname)))
	{
		// Failed to move, make sure the path exists and try again.
		FS_CreatePath(fullsavedname);
		error = rename(tempname, fullsavedname);
	}

#ifdef _WIN32

	// If the file type is not QWD we need to conver it using external apps.
	if (!strcmp(demo_format.string, "qwz") || !strcmp(demo_format.string, "mvd"))
	{
		Com_Printf("Converting QWD to %s format.\n", demo_format.string);

		// Convert the file to either MVD or QWZ.
		if (CL_Demo_Compress(fullsavedname))
		{
			return;
		}
		else
		{
			qwz_packing = false;
		}
	}

#endif

	if (!error)
		Com_Printf("Match demo saved to %s\n", savedname);
}

//=============================================================================
//							QIZMO COMPRESSION
//=============================================================================

#ifdef _WIN32

//
//
//
void CL_Demo_RemoveQWD(void)
{
	unlink(tempqwd_name);
}

//
// cdemo_name is assumed to be 255 chars long
//
void CL_Demo_GetCompressedName (char* cdemo_name)
{
	size_t namelen = strlen (tempqwd_name);

	if (strlen (demo_format.string) && namelen)
	{
		strlcpy (cdemo_name, tempqwd_name, 255);
		strlcpy (cdemo_name + namelen - 3, demo_format.string, 255 - namelen + 3);
	}
}

//
//
//
void CL_Demo_RemoveCompressed(void)
{
	char cdemo_name[255];
	CL_Demo_GetCompressedName (cdemo_name);
	unlink(cdemo_name);
}

//
//
//
static void StopQWZPlayback (void)
{
	if (!hQizmoProcess && tempqwd_name[0])
	{
		if (remove (tempqwd_name) != 0)
			Com_Printf ("Error: Couldn't delete %s\n", tempqwd_name);
		tempqwd_name[0] = 0;
	}
	qwz_playback = false;
	qwz_unpacking = false;
}

//
//
//
void CL_CheckQizmoCompletion (void)
{
	DWORD ExitCode;

	if (!hQizmoProcess)
		return;

	if (!GetExitCodeProcess (hQizmoProcess, &ExitCode))
	{
		Com_Printf ("WARINING: CL_CheckQizmoCompletion: GetExitCodeProcess failed\n");
		hQizmoProcess = NULL;
		if (qwz_unpacking)
		{
			qwz_unpacking = false;
			qwz_playback = false;
			cls.demoplayback = cls.timedemo = false;
			StopQWZPlayback();
		}
		else if (qwz_packing)
		{
			qwz_packing = false;
			CL_Demo_RemoveCompressed();
		}
		return;
	}

	if (ExitCode == STILL_ACTIVE)
		return;

	hQizmoProcess = NULL;

	if (!qwz_packing && (!qwz_unpacking || !cls.demoplayback))
	{
		StopQWZPlayback ();
		return;
	}

	if (qwz_unpacking)
	{
		qwz_unpacking = false;

		if (!(playbackfile = FS_OpenVFS (tempqwd_name, "rb", FS_NONE_OS)))
		{
			Com_Printf ("Error: Couldn't open %s\n", tempqwd_name);
			qwz_playback = false;
			cls.demoplayback = cls.timedemo = false;
			tempqwd_name[0] = 0;
			return;
		}

		Com_Printf("Decompression completed...playback starting\n");
	}
	else if (qwz_packing)
	{
		FILE* tempfile;
		char newname[255];
		CL_Demo_GetCompressedName (newname);
		qwz_packing = false;

		if ((tempfile = fopen(newname, "rb")) && (FS_FileLength(tempfile) > 0) && fclose(tempfile) != EOF)
		{
			Com_Printf("Demo saved to %s\n", newname + strlen(com_basedir));
			CL_Demo_RemoveQWD();
		}
		else
		{
			Com_Printf("Compression failed, demo saved as QWD.\n");
		}
	}
}

//
//
//
static void PlayQWZDemo (void)
{
	extern cvar_t qizmo_dir;
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char *name, qwz_name[MAX_PATH], cmdline[512], *p;
	DWORD res;

	if (hQizmoProcess)
	{
		Com_Printf ("Cannot unpack -- Qizmo still running!\n");
		return;
	}

	name = Cmd_Argv(1);

	char* initialName = NULL;
	if (!(playbackfile = CL_Open_Demo_File(name, false, &initialName)))
	{
		Com_Printf ("Error: Couldn't open %s\n", name);
		return;
	}

	// Convert to system path
	Sys_fullpath(qwz_name, initialName, MAX_PATH);

	VFS_CLOSE (playbackfile);
	playbackfile = NULL;

	strlcpy (tempqwd_name, qwz_name, sizeof(tempqwd_name) - 4);

	// the way Qizmo does it, sigh
	if (!(p = strstr (tempqwd_name, ".qwz")))
		p = strstr (tempqwd_name, ".QWZ");
	if (!p)
		p = tempqwd_name + strlen(tempqwd_name);
	strcpy (p, ".qwd");

	// If .qwd already exists, just play it.
	if ((playbackfile = FS_OpenVFS(tempqwd_name, "rb", FS_NONE_OS)))
		return;

	Com_Printf ("Unpacking %s...\n", COM_SkipPath(name));

	// Start Qizmo to unpack the demo.
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
	
	res = WaitForSingleObject(pi.hProcess, QWZ_DECOMPRESSION_TIMEOUT_MS);
	if (res == WAIT_TIMEOUT) {
		Com_Printf("Decompression took too long, aborting\n");
		return;
	}

	hQizmoProcess = pi.hProcess;
	qwz_unpacking = true;
	qwz_playback = true;
}

//
//
//
int CL_Demo_Compress(char* qwdname)
{
	extern cvar_t qizmo_dir;
	extern cvar_t qwdtools_dir;
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char cmdline[1024];
	char *path, outputpath[255];
	char *appname;
	char *parameters;

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
		appname = "qizmo.exe";
		parameters = "-q -C";
		path = qizmo_dir.string;
		outputpath[0] = 0;
	}
	else if (!strcmp(demo_format.string, "mvd"))
	{
		appname = "qwdtools.exe";
		parameters = "-c -o * -od";
		path = qwdtools_dir.string;
		strlcpy(outputpath, qwdname, COM_SkipPath(qwdname) - qwdname);
	}
	else
	{
		Com_Printf("%s demo format not yet supported.\n", demo_format.string);
		return 0;
	}

	strlcpy (cmdline, va("\"%s/%s/%s\" %s \"%s\" \"%s\"", com_basedir, path, appname, parameters, outputpath, qwdname), sizeof(cmdline));
	Com_DPrintf("Executing ---\n%s\n---\n", cmdline);

	if (!CreateProcess (NULL, cmdline, NULL, NULL,
		FALSE, GetPriorityClass(GetCurrentProcess()),
		NULL, va("%s/%s", com_basedir, path), &si, &pi))
	{
		Com_Printf ("Couldn't execute %s/%s/%s\n", com_basedir, path, appname);
		return 0;
	}

	strlcpy(tempqwd_name, qwdname, sizeof(tempqwd_name));

	hQizmoProcess = pi.hProcess;
	qwz_packing = true;
	return 1;
}

#endif // _WIN32

//=============================================================================
//							DEMO PLAYBACK
//=============================================================================

double		demostarttime;

#ifdef WITH_ZIP
#ifndef WITH_VFS_ARCHIVE_LOADING
//
// [IN]		play_path = The compressed demo file that needs to be extracted to play it.
// [OUT]	unpacked_path = The path to the decompressed file.
//
static int CL_GetUnpackedDemoPath (char *play_path, char *unpacked_path, int unpacked_path_size)
{
	//
	// Check if the demo is in a zip file and if so, try to extract it before playing.
	//
	int retval = 0;
	char archive_path[MAX_PATH];
	char inzip_path[MAX_PATH];

	//
	// Is it a GZip file?
	//
	if (!strcmp (COM_FileExtension (play_path), "gz"))
	{
		int i = 0;
		int ext_len = 0;
		char ext[5];

		//
		// Find the extension with .gz removed.
		//
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
		if (!FS_GZipUnpackToTemp (play_path, unpacked_path, unpacked_path_size, ext))
		{
			return 0;
		}

		return 1;
	}

	//
	// Check if the path is in the format "c:\quake\bla\demo.zip\some_demo.mvd" and split it up.
	//
	if (FS_ZipBreakupArchivePath ("zip", play_path, archive_path, sizeof(archive_path), inzip_path, sizeof(inzip_path)) < 0)
	{
		return retval;
	}

	//
	// Extract the ZIP file.
	//
	{
		char temp_path[MAX_PATH];

		// Open the zip file.
		unzFile zip_file = FS_ZipUnpackOpenFile (archive_path);

		// Try extracting the zip file.
		if(FS_ZipUnpackOneFileToTemp (zip_file, inzip_path, false, false, NULL, temp_path, sizeof(temp_path)) != UNZ_OK)
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
		FS_ZipUnpackCloseFile (zip_file);
	}

	return retval;
}
#endif // WITH_VFS_ARCHIVE_LOADING
#endif // WITH_ZIP

void CL_Demo_DumpBenchmarkResult(int frames, float timet)
{
	char logfile[MAX_PATH];
	char datebuf[32];
	FILE* f;
	time_t t = time(&t);
	struct tm *ptm = localtime(&t);
	int width = 0, height = 0; 

	snprintf(logfile, sizeof(logfile), "%s/timedemo.log", FS_LegacyDir(log_dir.string));
	f = fopen(logfile, "a");
	if (!f) {
		Com_Printf("Can't open %s to dump timedemo result\n", logfile);
		return;
	}

	fputs("<timedemo date=\"", f);
	if (ptm)
		strftime (datebuf, sizeof(datebuf) - 1, "%Y-%m-%dT%H:%M:%S", ptm);
	else
		*datebuf = '\0';
	fputs(datebuf, f); fputs("\">\n", f);

	fputs(va("\t<system>\n\t\t<os>%s</os>\n\t\t<hardware>%s</hardware>\n\t</system>\n", QW_PLATFORM, SYSINFO_GetString()), f);

	fputs(va("\t<client>\n\t\t<name>ezQuake</name><version>%s</version>\n"
		"\t\t<configuration>%s</configuration><rendering>%s</rendering>\n\t</client>\n",
		VersionString(), QW_CONFIGURATION, QW_RENDERER), f);
//FIXME width/height doesnt get set, remove vid_mode/r_mode... Is this function used??
	if (width)
		fputs(va("\t<screen width=\"%d\" height=\"%d\"/>\n", width, height), f);

	fputs(va("\t<demo><name>%s</name></demo>\n", cls.demoname), f);

	fputs(va("\t<result frames=\"%i\" time=\"PT%fS\" fps=\"%f\"/>\n", frames, timet, frames/timet), f);
	
	fputs("</timedemo>\n", f);

	fclose(f);
}

//
// Stops demo playback.
//
void CL_StopPlayback (void)
{
	extern int demo_playlist_started;
	extern int mvd_demo_track_run;

	// Nothing to stop.
	if (!cls.demoplayback)
		return;

	// Capturing to avi/images, stop that.
	if (Movie_IsCapturing())
		Movie_Stop(false);

	// Close the playback file.
	if (playbackfile)
		VFS_CLOSE(playbackfile);

	// Reset demo playback vars.
	playbackfile = NULL;
	cls.mvdplayback = cls.demoplayback = cls.nqdemoplayback = false;
	cl.paused &= ~PAUSED_DEMO;

	cls.qtv_svversion = 0;
	cls.qtv_ezquake_ext = 0;
	cls.qtv_donotbuffer = false;

	// Stop Qizmo demo playback.
	#ifdef WIN32
	if (qwz_playback)
		StopQWZPlayback ();
	#endif

	//
	// Stop timedemo and show the result.
	//
	if (cls.timedemo)
	{
		int frames;
		float time;

		cls.timedemo = false;

		//
		// Calculate the time it took to render the frames.
		//
		frames = cls.framecount - cls.td_startframe - 1;
		time = Sys_DoubleTime() - cls.td_starttime;
		if (time <= 0)
			time = 1;
		Com_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames / time);
		if (demo_benchmarkdumps.integer)
			CL_Demo_DumpBenchmarkResult(frames, time);
	}

	// Go to the next demo in the demo playlist.
	if (demo_playlist_started)
	{
		CL_Demo_Playlist_f();
		mvd_demo_track_run = 0;
	}

	// Reset demoseeking and such.
	cls.demoseeking = DST_SEEKING_NONE;
	cls.demorewinding = false;

	TP_ExecTrigger("f_demoend");
}

//
// Returns the demo length.
//
float CL_GetDemoLength(void)
{
	return demo_time_length;
}

typedef enum demoprobe_parse_type_e
{
	READ_MVD_TIME	= 1,
	READ_QWD_TIME	= 2,
	TRY_READ_MVD	= 3
} demoprobe_parse_type_t;

//
// Validates a demo probe read.
//
#define DEMOPROBE_VALIDATE_READ(bytes_expected, bytes_read, message)		\
	if (bytes_expected != bytes_read)										\
	{																		\
		Com_DPrintf("CL_ProbeDemo: Unexpected end of demo. "message"\n");	\
		abort = true;														\
		break;																\
	}

//
// Seek the specified length and fail/abort if it fails.
//
#define DEMOPROBE_SEEK(file, length, message)								\
	if (VFS_SEEK(file, length, SEEK_CUR) == -1)								\
	{																		\
		Com_DPrintf("CL_ProbeDemo: Unexpected end of demo. "message"\n");	\
		abort = true;														\
		break;																\
	}

//
// Probe a demo in different ways.
//
qbool CL_ProbeDemo(vfsfile_t *demfile, demoprobe_parse_type_t probetype, float *demotime)
{
	#define PARSE_AS_MVD()			((probetype == READ_MVD_TIME) || (probetype == TRY_READ_MVD))
	#define REGARD_AS_MVD_COUNT		4		// Regard this to be an MVD when this count has been reached.

	vfserrno_t err;
	
	float qwd_time				= 0.0;		// QWD Time = Time since start of demo in seconds.
	byte mvd_time				= 0;		// MVD Time = Time in miliseconds since last time stamp.
	unsigned int total_mvd_time = 0;		// Total MVD time in miliseconds.
	
	byte command				= 0;		// Holds the command code.
	unsigned int multiple		= 0;		// Read dem_multiple data into this.
	unsigned int size			= 0;		// The size of the demo packet (follows dem_multiple, _single, _stats, _all and _read).

	int message_count			= 0;		// The total amount of demo messages.
	int len						= 0;		// Length of what's been read
	qbool is_mvd				= false;	// Is this an MVD. (Used when guessing if this is an MVD).
	int mvd_only_count			= 0;		// Try to figure out if this is a MVD by looking for commands only present in MVDs.
	qbool abort					= false;	// Something bad happened when reading the demo.

	while (!abort)
	{
		// Read the time.
		if (PARSE_AS_MVD())
		{
			// MVD time.
			len = VFS_READ(demfile, &mvd_time, 1, &err);
		}
		else
		{
			// QWD time.
			len = VFS_READ(demfile, &qwd_time, 4, &err);
			qwd_time = LittleFloat(qwd_time);
		}
		
		// Any well formed demo should only end at an expected time stamp.
		if ((len == 0) || (err == VFSERR_EOF))
		{
			Com_DPrintf("CL_ProbeDemo: End of file. All good!\n");
			break;
		}

		// Read the command.
		len = VFS_READ(demfile, &command, 1, &err);
		DEMOPROBE_VALIDATE_READ(1, len, "when reading command");

		size = 0;

		// The first 3 bits contains the command code.
		// The 5 other bits are:
		// dem_stats, 
		// dem_single	= Player number for the player affected.
		// dem_all		= Nothing, directed to all players.
		// dem_multiple = Nothing.
		switch (command & 0x7)
		{
			case dem_multiple :
			{
				// Only MVD.

				// Read a 32-bit number containing a bitmask for which the affected players are.
				// 32-bits, 32 players.
				len = VFS_READ(demfile, &multiple, 4, &err);

				if (err == VFSERR_EOF)
				{
					Com_Printf("Unexpected end of demo when reading multiple.\n");
					abort = true;
					break;
				}
			}
			case dem_single :		// Only MVD.
			case dem_all :			// Only MVD.
				mvd_only_count++;	// Count the number of MVD-only commands we find to determine if this is an MVD or not.
			case dem_stats : 
			case dem_read :
			{
				// Read the size of the packet, we'll need it to seek past it.
				len = VFS_READ(demfile, &size, 4, &err);
				DEMOPROBE_VALIDATE_READ(4, len, "when reading size");
				size = LittleLong(size);
				break;
			}
			case dem_set :
			{
				DEMOPROBE_SEEK(demfile, 4, "when reading incoming sequence number"); // 32-bit int.
				DEMOPROBE_SEEK(demfile, 4, "when reading outgoing sequence number"); // 32-bit int.
				break;
			}
			case dem_cmd :
			{
				// Only QWD.
				DEMOPROBE_SEEK(demfile, sizeof(usercmd_t), "when reading user movement cmd.");
				DEMOPROBE_SEEK(demfile, 12, "when reading user viewangles cmd."); // 3 * 32-bit floats.
				break;
			}
			default :
			{
				Com_DPrintf("CL_ProbeDemo: Unsupported command type %d!\n", (command & 0x7));
				abort = true;
				break;
			}
		}

		// We're just trying to find out if this is an MVD so break if it is.
		if ((probetype == TRY_READ_MVD) && (mvd_only_count >= REGARD_AS_MVD_COUNT))
		{
			is_mvd = true;
			break;
		}

		if (abort)
			break;

		// Read any specified data if needed.
		DEMOPROBE_SEEK(demfile, size, "when reading size bytes of message");

		// MVD time is saved as the time since last frame,
		// so we need to keep track of the total seperatly.
		if (probetype == READ_MVD_TIME)
		{
			total_mvd_time += mvd_time;
		}

		message_count++;
	}

	// Return to the start of the file.
	VFS_SEEK(demfile, 0, SEEK_SET);

	if (demotime)
	{
		if (probetype == READ_MVD_TIME)
		{
			// Convert mvd time to seconds.
			*demotime = total_mvd_time * 0.001;
		}
		else if (probetype == READ_QWD_TIME)
		{
			*demotime = qwd_time;
		}

		Com_DPrintf("CL_DemoProbe: Time: %f\n", *demotime);
	}

	// Is this a really short MVD, that doesn't contain our threshold of MVD only messages
	// but still enough for it to likely be one.
	if (!is_mvd 
		&& (probetype == TRY_READ_MVD)
		&& (message_count > 0) 
		&& (((float)mvd_only_count / message_count) >= 0.1))
	{
		is_mvd = true;
	}

	return is_mvd;
}

//
// Try to guess if this is an MVD by trying to parse it as one.
//
qbool CL_GetIsMVD(vfsfile_t *demfile)
{
	return CL_ProbeDemo(demfile, TRY_READ_MVD, NULL);
}

//
// Try to calculate the time of a demo.
//
float CL_CalculateDemoTime(vfsfile_t *demfile)
{
	float demotime = 0.0;

	CL_ProbeDemo(demfile, (cls.mvdplayback ? READ_MVD_TIME : READ_QWD_TIME), &demotime);

	return demotime;
}

//
// Returns true if the specified filename has a demo extension.
//
qbool CL_IsDemoExtension(const char *filename)
{
	char *ext = COM_FileExtension(filename);

	return (!strncasecmp(ext, "mvd", sizeof("mvd"))
		 || !strncasecmp(ext, "qwd", sizeof("qwd"))
		 || !strncasecmp(ext, "dem", sizeof("dem"))
		 || !strncasecmp(ext, "qwz", sizeof("qwz")));
}

//
// Resets various states for beginning playback.
//
static void CL_DemoPlaybackInit(void)
{	
	cls.demoplayback	= true;	

	// Set demoplayback vars depending on the demo type.
	// CL_GetIsMVD(playbackfile); 
	// TODO : Add a similar check for QWD also (or DEM), so that we can distinguish if it's a DEM or not playing also
	// TODO : Make a working check if a demo really is an mvd by its contents that also works on short demos.
	cls.mvdplayback		= !strcasecmp(COM_FileExtension(cls.demoname), "mvd"); 
	cls.nqdemoplayback	= !strcasecmp(COM_FileExtension(cls.demoname), "dem");

	 // Init some buffers for reading.
	CL_Demo_PB_Init(NULL, 0);

	// NetQuake demo support.
	if (cls.nqdemoplayback)
	{
		NQD_StartPlayback();
		demo_time_length = -1.0;
	}
	else
	{
		// Calculate the demo time.
		double start = Sys_DoubleTime();
		demo_time_length = CL_CalculateDemoTime(playbackfile);
		Com_DPrintf("Demo probe took %f seconds.\n", Sys_DoubleTime() - start);
	}

	// Setup the netchan and state.
	Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, 0);
	cls.state		= ca_demostart;
	cls.demotime	= 0;
	demostarttime	= -1.0;
	olddemotime		= 0;
	nextdemotime	= 0;
	cls.findtrack	= true;
	bufferingtime	= 0;

	// Used for knowing who messages is directed to in MVD's.
	cls.lastto		= 0;
	cls.lasttype	= 0;

	CL_ClearPredict();

	// Recording not allowed during mvdplayback.
	if (cls.mvdplayback && cls.demorecording)
	{
		CL_Stop_f();
	}
}

//
// Starts playback of a demo.
//
void CL_Play_f (void)
{
	#ifndef WITH_VFS_ARCHIVE_LOADING
	#ifdef WITH_ZIP
	char unpacked_path[MAX_OSPATH];
	#endif // WITH_ZIP
	#endif // WITH_VFS_ARCHIVE_LOADING

	char *real_name;
	char name[MAX_OSPATH], **s;
	static char *ext[] = {"qwd", "mvd", "dem", NULL};

	// Show usage.
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: %s <demoname>\n", Cmd_Argv(0));
		return;
	}

	// Save the name the user specified.
	real_name = Cmd_Argv(1);

	// Quick check for buffer overrun on COM_StripExtension below...
	if (strlen (real_name) > MAX_OSPATH - 4) {
		Com_Printf ("Path is too long (%d characters, max is %d)\n", strlen (real_name), MAX_OSPATH - 4);
		return;
	}

	// Disconnect any current game.
	Host_EndGame();

	TP_ExecTrigger("f_demostart");
	
	// VFS-FIXME: This will affect playing qwz inside a zip
	#ifndef WITH_VFS_ARCHIVE_LOADING 
	#ifdef WITH_ZIP
	//
	// Unpack the demo if it's zipped or gzipped. And get the path to the unpacked demo file.
	//
	if (CL_GetUnpackedDemoPath (Cmd_Argv(1), unpacked_path, sizeof(unpacked_path)))
	{
		real_name = unpacked_path;
	}
	#endif // WITH_ZIP
	#endif // WITH_VFS_ARCHIVE_LOADING

	strlcpy(name, real_name, sizeof(name));

	#ifdef WIN32
	//
	// Decompress QWZ demos to QWD before playing it (using an external app).
	//

	if (strlen(name) > 4 && !strcasecmp(COM_FileExtension(name), "qwz"))
	{
		PlayQWZDemo();

		// We failed to extract the QWZ demo.
		if (!playbackfile && !qwz_playback)
		{
			return;
		}

		playbackfile = FS_OpenVFS (tempqwd_name, "rb", FS_NONE_OS);
	}
	else
	#endif // WIN32

	#ifndef WITH_VFS_ARCHIVE_LOADING
	{
		//
		// Find the demo path, trying different extensions if needed.
		//

		// If they specified a valid extension, try that first
		for (s = ext; *s && !playbackfile; ++s)
			if (!strcasecmp(COM_FileExtension(name), *s)) 
				playbackfile = CL_Open_Demo_File(name, true, NULL);

		for (s = ext; *s && !playbackfile; s++)
		{
			// Strip the extension from the specified filename and append
			// the one we're currently checking for.
			COM_StripExtension(name, name, sizeof(name));
			strlcat(name, ".", sizeof(name));
			strlcat(name, *s, sizeof(name));

			playbackfile = CL_Open_Demo_File(name, true, NULL);
		}
	}
	#else // WITH_VFS_ARCHIVE_LOADING
	{
		char *file_ext = COM_FileExtension(Cmd_Argv(1));
		if (!playbackfile)
		{
			// Check the file extension is valid
			for (s = ext; *s; s++) 
			{
				if (strcmp(*s, file_ext) == 0)
					break;
			}
			if (*s != NULL)
			{
				strlcpy (name, real_name, sizeof(name));
				if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3))
				{
					playbackfile = FS_OpenVFS(va("%s/%s", com_basedir, name + 3), "rb", FS_NONE_OS);
				}
				else
				{
					// Search demo on quake file system, even in paks.
					playbackfile = FS_OpenVFS(name, "rb", FS_ANY);
				}

				// Look in the demo dir (user specified).
				if (!playbackfile)
				{
					playbackfile = FS_OpenVFS(va("%s/%s", CL_DemoDirectory(), name), "rb", FS_NONE_OS);
				}

				// Check the full system path (Run a demo anywhere on the file system).
				if (!playbackfile)
				{
					playbackfile = FS_OpenVFS(name, "rb", FS_NONE_OS);
				}

				if (playbackfile)
					strlcpy(name, Cmd_Argv(1), sizeof(name));
			}
		}
	}
	#endif // WITH_VFS_ARCHIVE_LOADING else

	// Read the file completely into memory
	if (playbackfile) 
	{
		size_t len;
		void *buf;
		vfsfile_t *mmap_file;

		len = VFS_GETLEN(playbackfile);
		buf = Q_malloc(len);

		VFS_READ(playbackfile, buf, len, NULL);
		if (!(mmap_file = FSMMAP_OpenVFS(buf, len))) 
		{
			// Couldn't create the memory file, just remove the buffer
			Q_free(buf);
		}
		else 
		{
			// Close the file on disk now that we have read the file into memory
			VFS_CLOSE(playbackfile);
			playbackfile = mmap_file;
		}
	}

	// Failed to open the demo from any path :(
	if (!playbackfile)
	{
		Com_Printf ("Error: Couldn't open %s\n", Cmd_Argv(1));
		return;
	}

	strlcpy(cls.demoname, name, sizeof(cls.demoname));

	// Reset multiview track slots.
	memset(mv_trackslots, -1, sizeof(mv_trackslots));
	nTrack1duel			= 0;
	nTrack2duel			= 0;
	mv_skinsforced		= false;

	// Reset stuff so demo rewinding works.
	cls.demoseeking		= DST_SEEKING_NONE;
	cls.demorewinding	= false;
	cls.demo_rewindtime = 0;

	CL_DemoPlaybackInit();

	Com_Printf("Playing demo from %s\n", COM_SkipPath(name));
}

static vfsfile_t* CL_Open_Demo_File(char* name, qbool searchpaks, char** fullPath)
{
	static char fullname[MAX_OSPATH];
	vfsfile_t *file = NULL;

	memset(fullname, 0, sizeof(fullname));
	if (fullPath != NULL)
		*fullPath = fullname;

	// Look for the file in the above directory if it has ../ prepended to the filename.
	if (!strncmp(name, "../", 3) || !strncmp(name, "..\\", 3))
	{
		snprintf(fullname, MAX_OSPATH, "%s/%s", com_basedir, name + 3);
		file = FS_OpenVFS(fullname, "rb", FS_NONE_OS);
	}
	else if (searchpaks)
	{
		// Search demo on quake file system, even in paks.
		strncpy(fullname, name, MAX_OSPATH);
		file = FS_OpenVFS(name, "rb", FS_ANY);
	}

	// Look in the demo dir (user specified).
	if (!file)
	{
		snprintf(fullname, MAX_OSPATH, "%s/%s", CL_DemoDirectory(), name);
		file = FS_OpenVFS(fullname, "rb", FS_NONE_OS);
	}

	// Check the full system path (Run a demo anywhere on the file system).
	if (!file)
	{
		strncpy(fullname, name, MAX_OSPATH);
		file = FS_OpenVFS(name, "rb", FS_NONE_OS);
	}

	return file;
}

//
// Renders a demo as quickly as possible.
//
void CL_TimeDemo_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_Play_f ();

	// We failed to start demoplayback.
	if (cls.state != ca_demostart)
		return;

	// cls.td_starttime will be grabbed at the second frame of the demo,
	// so all the loading time doesn't get counted.

	cls.timedemo = true;
	cls.td_starttime = 0;
	cls.td_startframe = cls.framecount;
	cls.td_lastframe = -1;		// Get a new message this frame.
}

void CL_QTVPlay (vfsfile_t *newf, void *buf, int buflen);

char qtvrequestbuffer[512 * 1024] = {0}; // mmm, demo list may be pretty long
int  qtvrequestsize = 0;

char qtvpassword[128] = {0};

vfsfile_t *qtvrequest = NULL;

//
// Closes a QTV request.
//
void QTV_CloseRequest(qbool warn)
{
	if (qtvrequest)
	{
		if (warn)
			Com_Printf("Closing qtv request file\n");

		VFS_CLOSE(qtvrequest);
		qtvrequest = NULL;
	}

	qtvrequestsize = 0;
}

//
// Polls a QTV proxy. (Called on each frame to see if we have a new QTV request available)
//
void CL_QTVPoll (void)
{
	char QTVSV[] = "QTVSV ";
	int	 QTVSVLEN = sizeof(QTVSV)-1;

	char hash[512] = {0};
	char challenge[128] = {0};
	char authmethod[128] = {0};
	vfserrno_t err;
	char *start, *end, *colon;
	int len, need, chunk_size;
	qbool streamavailable = false;
	qbool saidheader = false;
	float svversion = 0;
	int qtv_ezquake_ext = 0;

	// We're not playing any QTV stream.
	if (!qtvrequest)
		return;

	//
	// Calculate how much we need to read from the buffer.
	//
	need = sizeof(qtvrequestbuffer); // Read the entire buffer
	need -= 1;						// For null terminator.
	need -= qtvrequestsize;			// We probably already have something, so don't re-read something we already read.
	need = max(0, need);			// Don't cause a crash by trying to read a negative value.

	len = VFS_READ(qtvrequest, qtvrequestbuffer + qtvrequestsize, need, &err);

	// EOF, end of polling.
	if (!len && err == VFSERR_EOF)
	{
		QTV_CloseRequest(true);
		return;
	}

	// Increase how much we've read.
	qtvrequestsize += len;
	qtvrequestbuffer[qtvrequestsize] = '\0';

	// QTVSV not seen yet, abort.
	if (qtvrequestsize < QTVSVLEN)
		return;

	if (strncmp(qtvrequestbuffer, QTVSV, QTVSVLEN))
	{
		Com_Printf("Server is not a QTV server (or is incompatable)\n");
		QTV_CloseRequest(true);
		return;
	}

	// Make sure it's a complete chunk. "\n\n" specifies the end of a message.
	for (start = qtvrequestbuffer; *start; start++)
	{
		if (start[0] == '\n' && start[1] == '\n')
			break;
	}

	// We've reached the end and didn't find "\n\n" so the chunk is incomplete.
	if (!*start)
		return;

	svversion = atof(qtvrequestbuffer + QTVSVLEN);

	// server sent float version, but we compare only major version number here
	if ((int)svversion != (int)QTV_VERSION)
	{
		Com_Printf("QTV server doesn't support a compatible protocol version, returned %.2f, need %.2f\n", svversion, QTV_VERSION);
		QTV_CloseRequest(true);
		return;
	}

	start = qtvrequestbuffer;

	//
	// Loop through the request buffer line by line.
	//
	for (end = start; *end; )
	{
		// Found a new line.
		if (*end == '\n')
		{
			// Don't parse it again.
			*end = '\0';

			// Find the first colon in the string.
			colon = strchr(start, ':');

			// We found a request.
			if (colon)
			{
				// Remove the colon.
				*colon++ = '\0';

				while (*colon == ' ')
					colon++;

				//
				// Check the request type. Might be an error.
				//
				if (!strcmp(start, "PERROR"))
				{
					Com_Printf("QTV Error:\n%s\n", colon);
				}
				else if (!strcmp(start, "PRINT"))
				{
					Com_Printf("QTV:\n%s\n", colon);
				}
				else if (!strcmp(start, "TERROR"))
				{
					Com_Printf("QTV Error:\n%s\n", colon);
				}
				else if (!strcmp(start, "ADEMO"))
				{
					//
					// Print demos.
					//

					if (!saidheader)
					{
						saidheader = true;
						Com_Printf("Available Demos:\n");
					}

					Com_Printf("%s\n", colon);
				}
				else if (!strcmp(start, "ASOURCE"))
				{
					//
					// Print sources.
					//

					if (!saidheader)
					{
						saidheader = true;
						Com_Printf("Available Sources:\n");
					}

					Com_Printf("%s\n", colon);
				}
				else if (!strcmp(start, "AUTH"))
				{
					strlcpy(authmethod, colon, sizeof(authmethod));
				}
				else if (!strcmp(start, "CHALLENGE"))
				{
					strlcpy(challenge, colon, sizeof(challenge));
				}
				else if (!strcmp(start, "BEGIN"))
				{
					streamavailable = true;
				}
				else if (!strcmp(start, QTV_EZQUAKE_EXT))
				{
					qtv_ezquake_ext = atoi(colon);
				}
			}
			else
			{
				// What follows this is a stream.
				if (!strcmp(start, "BEGIN"))
				{
					streamavailable = true;
				}
				else if (!strncmp(start, "QTVSV ", 6))
				{
//					Com_Printf("QTVSV HEADER: %s\n", start);
				}
			}

			// From start to end, we have a line.
			start = end + 1;
		}

		end++;
	}

	// Get the size of the stream chunk.
	chunk_size = qtvrequestsize - (end - qtvrequestbuffer);

	if (chunk_size < 0)
	{
		Com_Printf("Error while parsing qtv request\n");
		QTV_CloseRequest(true);
		return;
	}

	// drop header
	qtvrequestsize = chunk_size;
	memmove(qtvrequestbuffer, end, qtvrequestsize);

//	Com_Printf("memmove: %d\n", qtvrequestsize);

	// We found a stream.
	if (streamavailable)
	{
		// Start playing the QTV stream.
		CL_QTVPlay(qtvrequest, qtvrequestbuffer, qtvrequestsize);
		cls.qtv_svversion = svversion;
		cls.qtv_ezquake_ext = qtv_ezquake_ext;
		qtvrequest = NULL;

		return;
	}

	// we need send auth now
	if (authmethod[0])
	{
		char connrequest[2048];

		if (!strcmp(authmethod, "PLAIN"))
		{
			snprintf(connrequest, sizeof(connrequest), 
					"%s" "AUTH: PLAIN\nPASSWORD: \"%s\"\n\n", QTV_CL_HEADER(QTV_VERSION, QTV_EZQUAKE_EXT_NUM), qtvpassword);

			VFS_WRITE(qtvrequest, connrequest, strlen(connrequest));

			return;
		}
		else if (!strcmp(authmethod, "CCITT"))
		{
			if (strlen(challenge)>=32)
			{
				unsigned short crcvalue;

				snprintf(hash, sizeof(hash), "%s%s", challenge, qtvpassword);
				crcvalue = CRC_Block((byte *)hash, strlen(hash));
				snprintf(hash, sizeof(hash), "0x%X", (unsigned int)CRC_Value(crcvalue));
				snprintf(connrequest, sizeof(connrequest), 
					"%s" "AUTH: CCITT\nPASSWORD: \"%s\"\n\n", QTV_CL_HEADER(QTV_VERSION, QTV_EZQUAKE_EXT_NUM), hash);

				VFS_WRITE(qtvrequest, connrequest, strlen(connrequest));

				return;
			}

			Com_Printf("Wrong challenge for AUTH: %s\n", authmethod);
		}
		else if (!strcmp(authmethod, "MD4"))
		{
			if (strlen(challenge)>=8)
			{
				unsigned int md4sum[4];

				snprintf(hash, sizeof(hash), "%s%s", challenge, qtvpassword);
				Com_BlockFullChecksum (hash, strlen(hash), (unsigned char*)md4sum);
				snprintf(hash, sizeof(hash), "%X%X%X%X", md4sum[0], md4sum[1], md4sum[2], md4sum[3]);
				snprintf(connrequest, sizeof(connrequest), 
					"%s" "AUTH: MD4\nPASSWORD: \"%s\"\n\n", QTV_CL_HEADER(QTV_VERSION, QTV_EZQUAKE_EXT_NUM), hash);

				VFS_WRITE(qtvrequest, connrequest, strlen(connrequest));

				return;
			}

			Com_Printf("Wrong challenge for AUTH: %s\n", authmethod);
		}
		else if (!strcmp(authmethod, "NONE"))
		{
			snprintf(connrequest, sizeof(connrequest),
					"%s" "AUTH: NONE\nPASSWORD: \n\n", QTV_CL_HEADER(QTV_VERSION, QTV_EZQUAKE_EXT_NUM));

			VFS_WRITE(qtvrequest, connrequest, strlen(connrequest));

			return;
		}
		else
		{
			Com_Printf("Unknown auth method %s\n", authmethod);
		}
	}

	QTV_CloseRequest(true);
}

/*
 * Queries a QTV proxy for a list of available stream sources or demos
 * This function is used for both for qtvlist and qtvdemolist
 */
void CL_QTVList_f (void)
{
	char *connrequest;
	vfsfile_t *newf;
	qbool qtvlist = !strcmp("qtv_query_sourcelist", Cmd_Argv(0));

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s hostname[:port] [password]\n", Cmd_Argv(0));
		return;
	}

	// Open the TCP connection to the QTV proxy.
	newf = FS_OpenTCP(Cmd_Argv(1));
	if (newf == NULL) {
		Com_Printf("Couldn't connect to proxy\n");
		return;
	}

	strlcpy(qtvpassword, Cmd_Argv(2), sizeof(qtvpassword));

	// Send the version of QTV the client supports.
	connrequest = QTV_CL_HEADER(QTV_VERSION, QTV_EZQUAKE_EXT_NUM);
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	// Get a source list from the server.
	connrequest = qtvlist ? "SOURCELIST\n" : "DEMOLIST\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	// Send our userinfo
	connrequest = "USERINFO: ";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	connrequest = cls.userinfo;
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	/* if we use pass, then send our supported auth methods */
	if (qtvpassword[0]) {
		connrequest = "AUTH: MD4\n" "AUTH: CCITT\n" "AUTH: PLAIN\n" "AUTH: NONE\n";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
	}

	/* "\n\n" will end the session */
	connrequest = "\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	/* Close any old request that might still be open */
	QTV_CloseRequest(true);

	/* Set the current connection to be the QTVRequest to be used later */
	qtvrequest = newf;
}

//
// Plays a QTV stream.
//
void CL_QTVPlay (vfsfile_t *newf, void *buf, int buflen)
{
	// End any current game.
	Host_EndGame();

	// Close the old playback file just in case, and
	// open the "network file" for QTV playback.
	if (playbackfile)
		VFS_CLOSE(playbackfile);
	playbackfile = newf;

	// Reset multiview track slots.
	memset(mv_trackslots, -1, sizeof(mv_trackslots));
	nTrack1duel = nTrack2duel = 0;
	mv_skinsforced = false;

	// We're now playing a demo.
	cls.demoplayback	= true;
	cls.mvdplayback		= QTV_PLAYBACK;
	cls.nqdemoplayback	= false;

	// Init playback buffers.
	CL_Demo_PB_Init(buf, buflen);

	// NetQuake demo support.
	if (cls.nqdemoplayback)
	{
		NQD_StartPlayback (); // Maybe some day QTV will stream NQ demos too...
	}

	// Setup demo playback for the netchan.
	cls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, 0);
	cls.demotime = 0;
	demostarttime = -1.0;
	olddemotime = nextdemotime = 0;
	cls.findtrack = true;

	cls.qtv_donotbuffer = true; // do not try buffering before "skins" not received

	bufferingtime = 0; // with eztv it correct, since eztv do not send data before we complete connection, so prebuffering is pointless

	// Used for knowing who messages is directed to in MVD's.
	cls.lastto = cls.lasttype = 0;

	CL_ClearPredict();

	// Recording not allowed during mvdplayback.
	if (cls.mvdplayback && cls.demorecording)
	{
		CL_Stop_f();
	}

	TP_ExecTrigger ("f_demostart");

	Com_Printf("Attempting to stream QTV data, buffer is %.1fs\n", (double)(QTVBUFFERTIME));
}

static char prev_qtv_connrequest[512]; /* FIXME: Stupid name, it might as well be ACTUAL streaming address */
static char prev_qtv_password[128];

char *CL_QTV_GetCurrentStream(void)
{
	if (cls.mvdplayback == QTV_PLAYBACK) {
		if (prev_qtv_connrequest[0]) {
			return &prev_qtv_connrequest[0];
		}
	}
	/* Not connected to QTV, no address then */
	return NULL;
}

//
// Reconnects to the previous QTV.
//
void CL_QTVReconnect_f (void)
{
	if (!prev_qtv_connrequest[0])
	{
		Com_Printf("No previous QTV proxy available to connect to.\n");
		return;
	}

	Cbuf_AddText(va("qtvplay %s %s\n", prev_qtv_connrequest, prev_qtv_password));
}

// checks if the argument is of the form http://quakeworld.fi:28000/watch.qtv?sid=8
// if so, issue a new qtvplay command with stream@hostname:port format
static qbool CL_QTVPlay_URL_format(void)
{
	const char *prefix = strstr(Cmd_Argv(1), "http://");
	const char *docpart = strstr(Cmd_Argv(1), "/watch.qtv?sid=");

	if (prefix && docpart && prefix == Cmd_Argv(1) && prefix < docpart) {
		int streamid = Q_atoi(docpart + strlen("/watch.qtv?sid="));
		int hostnamelen = docpart - prefix - strlen("http://");
		Cbuf_AddText(va("qtvplay %d@%.*s\n", streamid, hostnamelen, Cmd_Argv(1) + strlen("http://")));
		return true;
	}
	else {
		return false;
	}
}

//
// Start playback of a QTV stream.
//
void CL_QTVPlay_f (void)
{
	char *connrequest;
	vfsfile_t *newf;
	char stream_host[1024] = {0}, *stream, *host;

	// Show usage.
	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: qtvplay [stream@]hostname[:port] [password]\n");
		return;
	}

	if (CL_QTVPlay_URL_format())
		return;

	strlcpy(qtvpassword, Cmd_Argv(2), sizeof(qtvpassword));

	// The stream address.
	connrequest = Cmd_Argv(1);

	// We've succesfully connected to a QTV proxy, save the connrequest string so we can use it to reconnect.
	strlcpy(prev_qtv_connrequest, connrequest, sizeof(prev_qtv_connrequest));
	strlcpy(prev_qtv_password, qtvpassword, sizeof(prev_qtv_password));

	//
	// If a "#" is at the beginning of the given address it refers to a .qtv file.
	//
	if (*connrequest == '#')
	{
		#define QTV_FILE_STREAM     1
		#define QTV_FILE_JOIN       2
		#define QTV_FILE_OBSERVE    3
		#define QTV_FILE_CHALLENGE  4
		int match = 0;

		char buffer[1024];
		char *s;
		FILE *f;

		// Try to open the .qtv file.
		f = fopen(connrequest + 1, "rt");
		if (!f)
		{
			Com_Printf("qtvplay: can't open file %s\n", connrequest + 1);
			return;
		}

		//
		// Read the entire .qtv file and execute the approriate commands
		// Stream, join or observe.
		//
		while (!feof(f))
		{
			if (fgets(buffer, sizeof(buffer) - 1, f) == NULL) {
				Com_Printf("Error reading the QTV file.\n");
				break;
			}

			if (!strncmp(buffer, "Stream=", 7) || !strncmp(buffer, "Stream:", 7))
			{
				match = QTV_FILE_STREAM;
			}

			if (!strncmp(buffer, "Join=", 5) || !strncmp(buffer, "Join:", 5))
			{
				match = QTV_FILE_JOIN;
			}

			if (!strncmp(buffer, "Observe=", 8) || !strncmp(buffer, "Observe:", 8))
			{
				match = QTV_FILE_OBSERVE;
			}

			if (!strncmp(buffer, "Challenge=", 10) || !strncmp(buffer, "Challenge:", 10))
			{
				match = QTV_FILE_CHALLENGE;
			}

			// We found a match in the .qtv file.
			if (match)
			{
				// Strip new line chars.
				for (s = buffer + strlen(buffer)-1; s >= buffer; s--)
				{
					if (*s == '\r' || *s == '\n')
					{
						*s = 0;
					}
					else
					{
						break;
					}
				}

				// Skip the title part.
				s = buffer;
				while(*s && *s != ':' && *s != '=') {
					s++;
				}
				if (*s) {
					s++;
				}

				// Remove any leading white spaces.
				while(*s && *s <= ' ')
				{
					s++;
				}

				// Call the appropriate commands based on the match.
				switch (match)
				{
					case QTV_FILE_STREAM :
						Cbuf_AddText(va("qtvplay \"%s\"\n", s));
						break;
					case QTV_FILE_JOIN :
						Cbuf_AddText(va("join \"%s\"\n", s));
						break;
					case QTV_FILE_OBSERVE :
						Cbuf_AddText(va("observe \"%s\"\n", s));
						break;
					case QTV_FILE_CHALLENGE:
						// URL is passed in, it needs to be further processed by the qwurl command
						Cbuf_AddText(va("qwurl \"%s\"\n", s));
						break;
				}

				// Break the while-loop we found what we want.
				break;
			}
		}

		// Close the file.
		fclose(f);
		return;
	}

	// The argument is the host address for the QTV server.

	// Find the position of the last @ char in the connection string,
	// this is when the user specifies a specific stream # on the QTV proxy
	// that he wants to stream. Default is to stream the first one.
	// [stream@]hostname[:port]
	// (QTV proxies can be chained, so we must get the last @)
	// In other words split stream and host part

	strlcpy(stream_host, connrequest, sizeof(stream_host));

	connrequest = strchrrev(stream_host, '@');
	if (connrequest)
	{
		stream = stream_host;		// Stream part.
		connrequest[0] = 0;			// Truncate.
		host   = connrequest + 1;	// Host part.
	}
	else
	{
		stream = "";				// Use default stream, user not specifie stream part.
		host   = stream_host;		// Arg is just host.
	}

	// Open a TCP socket to the specified host.
	newf = FS_OpenTCP(host);

	// Failed to open the connection.
	if (!newf)
	{
		Com_Printf("Couldn't connect to proxy %s\n", host);
		return;
	}

	// Send a QTV request to the proxy.
	connrequest = QTV_CL_HEADER(QTV_VERSION, QTV_EZQUAKE_EXT_NUM);
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	// If the user specified a specific stream such as "5@hostname:port"
	// we need to send a SOURCE request.
	if (stream[0])
	{
		connrequest =	"SOURCE: ";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
		connrequest =	stream;
		VFS_WRITE(newf, connrequest, strlen(connrequest));
		connrequest =	"\n";
		VFS_WRITE(newf, connrequest, strlen(connrequest));
	}

	// Send our userinfo
	connrequest = "USERINFO: ";
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest = cls.userinfo;
	VFS_WRITE(newf, connrequest, strlen(connrequest));
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	// if we use pass, then send our supported auth methods
	if (qtvpassword[0])
	{
		connrequest =
						"AUTH: MD4\n"
						"AUTH: CCITT\n"
						"AUTH: PLAIN\n"
						"AUTH: NONE\n";

		VFS_WRITE(newf, connrequest, strlen(connrequest));
	}

	// Two \n\n tells the server we're done.
	connrequest =	"\n";
	VFS_WRITE(newf, connrequest, strlen(connrequest));

	// We're finished requesting, but not done yet so save the
	// socket for the actual streaming :)
	QTV_CloseRequest(false);
	qtvrequest = newf;
}


//=============================================================================
//								DEMO TOOLS
//=============================================================================

//
// Sets the playback speed of a demo.
//
void CL_Demo_SetSpeed_f (void)
{
	extern cvar_t cl_demospeed;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s [speed %%]\n", Cmd_Argv(0));
		return;
	}

	Cvar_SetValue(&cl_demospeed, atof(Cmd_Argv(1)) / 100.0);
}

//
// Cleans up after demo has been rewound to the correct point
//
void CL_Demo_Stop_Rewinding(void) 
{
	// Make sure we keep our tracked players after rewinding.
	memcpy(mv_trackslots, rewind_trackslots, sizeof(mv_trackslots));
	nTrack1duel			= rewind_duel_track1;
	nTrack2duel			= rewind_duel_track2;
	if (rewind_spec_track >= 0) 
	{
		Cam_Lock(rewind_spec_track);
	}
	else 
	{
		// Switch to free-floating camera and restore view
		Cam_Unlock();
		Cam_Pos_Set(rewind_pos[0], rewind_pos[1], rewind_pos[2]);
		Cam_Angles_Set(rewind_angle[0], rewind_angle[1], rewind_angle[2]);
	}
	cls.findtrack		= false;

	R_InitParticles();
	cls.demorewinding   = false;
}

// 
// Checks if demo needs to be rewound to previous point in time
//
void CL_Demo_Check_For_Rewind(float nextdemotime)
{
	// If we're seeking and our seek destination is in the past we need to rewind.
	if (cls.demoseeking && !cls.demorewinding && (cls.demotime < nextdemotime))
	{
		// Restart playback from the start of the file and then demo seek to the rewind spot.
		VFS_SEEK(playbackfile, 0, SEEK_SET);

		// We need to save track information.
		memcpy(rewind_trackslots, mv_trackslots, sizeof(rewind_trackslots));
		rewind_duel_track1 = nTrack1duel;
		rewind_duel_track2 = nTrack2duel;
		rewind_spec_track = WhoIsSpectated(); //spec_track;
		cls.findtrack = false;
		VectorCopy(cl.viewangles, rewind_angle);
		VectorCopy(cl.simorg, rewind_pos);

		// Restart the demo from scratch.
		CL_DemoPlaybackInit();

		cls.demopackettime  = 0.0;
		cls.demorewinding   = true;
	}
	
	if (cls.demorewinding)
	{
		// If we've reached active state, we can set the new demotime
		// to trigger the demo seek to the desired location.
		// Before we're active, cl.demotime will just get overwritten.
		if (cls.state >= ca_active)
		{
			cls.demotime = demostarttime + cls.demo_rewindtime;
			cls.demoseeking = DST_SEEKING_NORMAL;
			//cls.demorewinding = false;

			// We have now finished restarting the demo and will now seek
			// to the new demotime just like we do when seeking forward.
		}
	}
}

//
// Jumps to a specified time in a demo.
//
void CL_Demo_Jump_f (void)
{
    int seconds = 0, seen_col, relative = 0;
    char *text, *s;
	static char *usage_message = "Usage: %s [+|-][m:]<s> (seconds)\n";

	// Cannot jump without playing demo.
	if (!cls.demoplayback)
	{
		Com_Printf("Error: not playing a demo\n");
        return;
	}

	if (cls.mvdplayback == QTV_PLAYBACK)
	{
		Com_Printf("Error: cannot jump during QTV playback.\n");
		return;
	}

	// Must be active to jump.
	if (cls.state < ca_active)
	{
		Com_Printf("Error: demo must be active first\n");
		return;
	}

	// Show usage.
    if (Cmd_Argc() != 2)
	{
        Com_Printf(usage_message, Cmd_Argv(0));
        return;
    }

	// Get the jump string.
    text = Cmd_Argv(1);

	//
	// Parse which direction we're jumping in if
	// we're jumping relativly based on the current time.
	//
	if (text[0] == '-')
	{
		// Jumping backwards.
		text++;
		relative = -1;
	}
	else if (text[0] == '+')
	{
		// Jumping forward.
		text++;
		relative = 1;
	}
	else if (!isdigit(text[0]))
	{
		// Incorrect input, show usage message.
        Com_Printf(usage_message, Cmd_Argv(0));
        return;
	}

	// Find the number of colons (max 2 allowed) and make sure
	// we only have digits in the string.
	for (seen_col = 0, s = text; *s; s++)
	{
		if (*s == ':')
		{
			seen_col++;
		}
		else if (!isdigit(*s))
		{
			// Not a digit, show usage message.
			Com_Printf(usage_message, Cmd_Argv(0));
			return;
		}

		if (seen_col >= 2)
		{
			// More than two colons found, show usage message.
			Com_Printf(usage_message, Cmd_Argv(0));
			return;
		}
	}

	// If there's at least 1 colon we know everything
	// before it is minutes, so add it them to our jump time.
    if (strchr(text, ':'))
	{
        seconds += 60 * atoi(text);
        text = strchr(text, ':') + 1;
    }

	// The numbers after the first colon will be seconds.
    seconds += atoi(text);

	CL_Demo_Jump(seconds, relative, DST_SEEKING_NORMAL);
}

//
// Jumps to a specified time in a demo.
//
void CL_Demo_Jump_Mark_f (void)
{
	int seconds = 99999; // as far as possibile, we have NO idea about time, we search MARK

	// Cannot jump without playing demo.
	if (!cls.demoplayback)
	{
		Com_Printf("Error: not playing a demo\n");
        return;
	}

	// Must be active to jump.
	if (cls.state < ca_active)
	{
		Com_Printf("Error: demo must be active first\n");
		return;
	}

	CL_Demo_Jump(seconds, 0, DST_SEEKING_DEMOMARK);
}

static void CL_Demo_Jump_Status_Free (demoseekingstatus_condition_t *condition)
{
	if (condition == NULL)
		return;

	CL_Demo_Jump_Status_Free(condition->or);
	CL_Demo_Jump_Status_Free(condition->and);

	Q_free(condition);
}

static demoseekingstatus_condition_t *CL_Demo_Jump_Status_Condition_New (demoseekingstatus_matchtype_t type, int stat, int value)
{
	demoseekingstatus_condition_t *condition = Q_malloc(sizeof(demoseekingstatus_condition_t));

	condition->type = type;
	condition->stat = stat;
	condition->value = value;
	condition->or = NULL;
	condition->and = NULL;

	return condition;
}

static void CL_Demo_Jump_Status_Condition_Negate (demoseekingstatus_condition_t *condition)
{
	switch (condition->type) {
		case DEMOSEEKINGSTATUS_MATCH_EQUAL:
			condition->type = DEMOSEEKINGSTATUS_MATCH_NOT_EQUAL;
			break;
		case DEMOSEEKINGSTATUS_MATCH_NOT_EQUAL:
			condition->type = DEMOSEEKINGSTATUS_MATCH_EQUAL;
			break;
		case DEMOSEEKINGSTATUS_MATCH_LESS_THAN:
			condition->type = DEMOSEEKINGSTATUS_MATCH_GREATER_THAN;
			condition->value -= 1;
			break;
		case DEMOSEEKINGSTATUS_MATCH_GREATER_THAN:
			condition->type = DEMOSEEKINGSTATUS_MATCH_LESS_THAN;
			condition->value += 1;
			break;
		case DEMOSEEKINGSTATUS_MATCH_BIT_ON:
			condition->type = DEMOSEEKINGSTATUS_MATCH_BIT_OFF;
			break;
		case DEMOSEEKINGSTATUS_MATCH_BIT_OFF:
			condition->type = DEMOSEEKINGSTATUS_MATCH_BIT_ON;
			break;
		default:
			assert(false);
			break;
	}
}

static qbool CL_Demo_Jump_Status_Match (demoseekingstatus_condition_t *condition)
{
	if (condition->or && CL_Demo_Jump_Status_Match(condition->or))
		return true;

	switch (condition->type) {
		case DEMOSEEKINGSTATUS_MATCH_EQUAL:
			if (cl.stats[condition->stat] != condition->value)
				return false;
			break;
		case DEMOSEEKINGSTATUS_MATCH_NOT_EQUAL:
			if (cl.stats[condition->stat] == condition->value)
				return false;
			break;
		case DEMOSEEKINGSTATUS_MATCH_LESS_THAN:
			if (cl.stats[condition->stat] >= condition->value)
				return false;
			break;
		case DEMOSEEKINGSTATUS_MATCH_GREATER_THAN:
			if (cl.stats[condition->stat] <= condition->value)
				return false;
			break;
		case DEMOSEEKINGSTATUS_MATCH_BIT_ON:
			if (!(cl.stats[condition->stat] & condition->value))
				return false;
			break;
		case DEMOSEEKINGSTATUS_MATCH_BIT_OFF:
			if (cl.stats[condition->stat] & condition->value)
				return false;
			break;
		default:
			assert(false);
			return false;
	}

	if (condition->and != NULL) {
		return CL_Demo_Jump_Status_Match(condition->and);
	} else {
		return true;
	}
}

void CL_Demo_Jump_Status_Check (void)
{
	if (CL_Demo_Jump_Status_Match(cls.demoseekingstatus.conditions)) {
		if (cls.demoseekingstatus.non_matching_found) {
			CL_Demo_Jump_Status_Free(cls.demoseekingstatus.conditions);
			cls.demoseekingstatus.conditions = NULL;
			cls.demoseeking = DST_SEEKING_FOUND;
		}
	} else if (!cls.demoseekingstatus.non_matching_found) {
		cls.demoseekingstatus.non_matching_found = true;
	}
}

static int CL_Demo_Jump_Status_Parse_Weapon (const char *arg)
{
	if (!strcasecmp("axe", arg)) {
		return IT_AXE;
	} else if (!strcasecmp("sg", arg)) {
		return IT_SHOTGUN;
	} else if (!strcasecmp("ssg", arg)) {
		return IT_SUPER_SHOTGUN;
	} else if (!strcasecmp("ng", arg)) {
		return IT_NAILGUN;
	} else if (!strcasecmp("sng", arg)) {
		return IT_SUPER_NAILGUN;
	} else if (!strcasecmp("gl", arg)) {
		return IT_GRENADE_LAUNCHER;
	} else if (!strcasecmp("rl", arg)) {
		return IT_ROCKET_LAUNCHER;
	} else if (!strcasecmp("lg", arg)) {
		return IT_LIGHTNING;
	} else {
		return 0;
	}
}

static int CL_Demo_Jump_Status_Parse_Constraint (const char *arg, int *value)
{
	if (strlen(arg) < 2)
		return -1;

	*value = Q_atoi(arg+1);

	switch (arg[0]) {
		case '=':
			return DEMOSEEKINGSTATUS_MATCH_EQUAL;
		case '<':
			return DEMOSEEKINGSTATUS_MATCH_LESS_THAN;
		case '>':
			return DEMOSEEKINGSTATUS_MATCH_GREATER_THAN;
		default:
			return -1;
	}
}

//
// Jumps to a point in demo based on the status of player in POV
//
static void CL_Demo_Jump_Status_f (void)
{
	int i;
	qbool or = false;
	demoseekingstatus_condition_t *parent = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <conditions>\n", Cmd_Argv(0));
		Com_Printf("\n");
		Com_Printf("Skip forward in the demo until the conditions based on the player in POV are met.\n");
		Com_Printf("\n");
		Com_Printf("Valid conditions are:\n");
		Com_Printf("  Weapon, armor and powerup names (rl, ya, quad etc.), for items held\n");
		Com_Printf("  Weapon names with + in front (+rl), for the weapon in hand\n");
		Com_Printf("  Constraints on health, armor and ammo held with syntax id=value, id<value or id>value\n");
		Com_Printf("    The id can be h for health, a for armor, s for shells, n for nails, r for rockets or c for cells\n");
		Com_Printf("  All the conditions can be negated by inserting a ! character in front\n");
		Com_Printf("  Special value \"or\" can be used to connect two conditions requiring only one of them to be true\n");
		Com_Printf("\n");
		Com_Printf("Example: %s h<1 +rl or +lg\n", Cmd_Argv(0));
		Com_Printf("  Skip to the next position where player in the POV drops a RL or LG pack\n");
		return;
	}

	// Cannot jump without playing demo.
	if (!cls.demoplayback) {
		Com_Printf("Error: not playing a demo\n");
		return;
	}

	// Must be active to jump.
	if (cls.state < ca_active) {
		Com_Printf("Error: demo must be active first\n");
		return;
	}

	cls.demoseekingstatus.non_matching_found = false;
	CL_Demo_Jump_Status_Free(cls.demoseekingstatus.conditions);
	cls.demoseekingstatus.conditions = NULL;

	for (i = 1; i < Cmd_Argc(); i++) {
		demoseekingstatus_condition_t *condition = NULL;
		qbool neg = false;
		char *arg = Cmd_Argv(i);
		int weapon, value;
		int type;

		if (!strcasecmp("or", arg)) {
			if (cls.demoseekingstatus.conditions == NULL) {
				Com_Printf("Error: or can't be the first argument\n");
				return;
			}
			or = true;
			continue;
		}

		if (arg[0] == '!') {
			neg = true;
			arg++;
		}

		if ((weapon = CL_Demo_Jump_Status_Parse_Weapon(arg)) != 0) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, weapon);
		} else if (arg[0] == '+' && (weapon = CL_Demo_Jump_Status_Parse_Weapon(arg+1)) != 0) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_EQUAL, STAT_ACTIVEWEAPON, weapon);
		} else if (arg[0] == 'h' && (type = CL_Demo_Jump_Status_Parse_Constraint(arg+1, &value)) >= 0) {
			condition = CL_Demo_Jump_Status_Condition_New(type, STAT_HEALTH, value);
		} else if (arg[0] == 'a' && (type = CL_Demo_Jump_Status_Parse_Constraint(arg+1, &value)) >= 0) {
			condition = CL_Demo_Jump_Status_Condition_New(type, STAT_ARMOR, value);
		} else if (arg[0] == 's' && (type = CL_Demo_Jump_Status_Parse_Constraint(arg+1, &value)) >= 0) {
			condition = CL_Demo_Jump_Status_Condition_New(type, STAT_SHELLS, value);
		} else if (arg[0] == 'n' && (type = CL_Demo_Jump_Status_Parse_Constraint(arg+1, &value)) >= 0) {
			condition = CL_Demo_Jump_Status_Condition_New(type, STAT_NAILS, value);
		} else if (arg[0] == 'r' && (type = CL_Demo_Jump_Status_Parse_Constraint(arg+1, &value)) >= 0) {
			condition = CL_Demo_Jump_Status_Condition_New(type, STAT_ROCKETS, value);
		} else if (arg[0] == 'c' && (type = CL_Demo_Jump_Status_Parse_Constraint(arg+1, &value)) >= 0) {
			condition = CL_Demo_Jump_Status_Condition_New(type, STAT_CELLS, value);
		} else if (!strcasecmp("ga", arg)) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, IT_ARMOR1);
		} else if (!strcasecmp("ya", arg)) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, IT_ARMOR2);
		} else if (!strcasecmp("ra", arg)) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, IT_ARMOR3);
		} else if (!strcasecmp("quad", arg)) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, IT_QUAD);
		} else if (!strcasecmp("ring", arg)) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, IT_INVISIBILITY);
		} else if (!strcasecmp("pent", arg)) {
			condition = CL_Demo_Jump_Status_Condition_New(DEMOSEEKINGSTATUS_MATCH_BIT_ON, STAT_ITEMS, IT_INVULNERABILITY);
		} else {
			Com_Printf("Error: unknown condition: %s\n", Cmd_Argv(i));
			CL_Demo_Jump_Status_Free(cls.demoseekingstatus.conditions);
			cls.demoseekingstatus.conditions = NULL;
			return;
		}

		if (neg)
			CL_Demo_Jump_Status_Condition_Negate(condition);

		if (parent != NULL) {
			if (or) {
				parent->or = condition;
			} else {
				parent->and = condition;
			}
		} else {
			cls.demoseekingstatus.conditions = condition;
		}
		parent = condition;
		or = false;
	}

	CL_Demo_Jump(99999, 0, DST_SEEKING_STATUS);
}


//
// Jumps to a specified time in a demo. Time specified in seconds.
//
void CL_Demo_Jump(double seconds, int relative, demoseekingtype_t seeking)
{
	// Calculate the new demo time we want to jump to.
	double initialtime = (cls.nqdemoplayback ? cl.servertime : cls.demotime);
	double newdemotime = relative ? (initialtime + (relative * seconds)) : (demostarttime + seconds);

	// We need to rewind.
	if (newdemotime < initialtime)
	{
		cls.demo_rewindtime = newdemotime - demostarttime;
	}

	// Set the new demotime.	
	cls.demotime = newdemotime;

	cls.demoseeking = seeking;
	Con_ClearNotify ();
}

double Demo_GetSpeed(void)
{
	if (cls.mvdplayback == QTV_PLAYBACK) {
		return qtv_demospeed;
	}

	return bound(0, cl_demospeed.value, 20);
}

void Demo_AdjustSpeed(void)
{
	if (cls.mvdplayback == QTV_PLAYBACK)
	{
		if (qtv_adjustbuffer.integer)
		{
			extern	unsigned char pb_buf[];
			extern	int		pb_cnt;

			int				ms;
			double			demospeed, desired, current;

			ConsistantMVDDataEx(pb_buf, pb_cnt, &ms);

			desired = max(0.5, QTVBUFFERTIME); // well, we need some reserve for adjusting
			current = 0.001 * ms;

			// qqshka: this is linear version
			demospeed = current / desired;

			// if you unwilling constant speed change, then you can set range with qtv_adjustlowstart and qtv_adjusthighstart
			if (demospeed > bound(0, qtv_adjustlowstart.value, 1) && demospeed < max(1, qtv_adjusthighstart.value))
				demospeed = 1;

			// bound demospeed
			demospeed = bound(qtv_adjustminspeed.value, demospeed, qtv_adjustmaxspeed.value);

			qtv_demospeed = demospeed;
			return;
		}

		if (!qtv_allow_pause.value) {
			qtv_demospeed = 1;
			return;
		}
	}
	
	qtv_demospeed = bound(0, cl_demospeed.value, 20);
}

// 
void CL_QTVFixUser_f(void) {
	int uid, i;
	char newuserinfo[MAX_INFO_STRING] = { 0 };
	char* newName = NULL;
	char* newTeam = NULL;
	int topcolor = 0;
	int bottomcolor = 0;
	qbool isSpectator = (Cmd_Argc() >= 3 && !strcmp(Cmd_Argv(2), "spectator"));
	qbool isPlayer = (Cmd_Argc() >= 3 && !strcmp(Cmd_Argv(2), "player"));

	if (!cls.demoplayback || cls.mvdplayback != QTV_PLAYBACK) {
		Com_Printf("Only valid when viewing QTV streams.", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() <= 3 || !(isSpectator || isPlayer)) {
		Com_Printf("Usage: %s <userid> <spectator | player> <name> [team] [topcolor] [bottomcolor]\n", Cmd_Argv(0));
		Com_Printf("This allows you to directly set user info fields on bugged QTV streams.\n");
		Com_Printf("------ ----- ----\n");
		Com_Printf("userid frags name\n");
		Com_Printf("------ ----- ----\n");
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cl.players[i].name[0] || cl.players[i].userinfo[0] || cl.players[i].frags) {
				Com_Printf("%6i %4i %s\n", cl.players[i].userid, cl.players[i].frags, cl.players[i].name);
			}
		}
		return;
	}

	uid = atoi(Cmd_Argv(1));
	newName = Cmd_Argv(3);
	newTeam = Cmd_Argc() <= 4 ? "" : Cmd_Argv(4);
	topcolor = Cmd_Argc() <= 5 ? 0 : atoi(Cmd_Argv(5));
	bottomcolor = Cmd_Argc() <= 6 ? topcolor : atoi(Cmd_Argv(6));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] && !cl.players[i].userinfo[0] && !cl.players[i].frags)
			continue;

		if (cl.players[i].userid == uid) {
			player_info_t* player = &cl.players[i];

			strcpy(newuserinfo, "\\*client\\QTVBug");
			strlcat(newuserinfo, "\\name\\", MAX_INFO_STRING);
			strlcat(newuserinfo, newName, MAX_INFO_STRING);

			if (isSpectator) {
				strlcat(newuserinfo, "\\*spectator\\1", MAX_INFO_STRING);
			}

			if (*newTeam) {
				strlcat(newuserinfo, "\\team\\", MAX_INFO_STRING);
				strlcat(newuserinfo, newTeam, MAX_INFO_STRING);
			}

			strlcat(newuserinfo, "\\topcolor\\", MAX_INFO_STRING);
			strlcat(newuserinfo, va("%d", topcolor), MAX_INFO_STRING);
			strlcat(newuserinfo, "\\bottomcolor\\", MAX_INFO_STRING);
			strlcat(newuserinfo, va("%d", bottomcolor), MAX_INFO_STRING);

			// Our scoreboard will set spectators to -999 frags for sorting, so reset and wait for server update
			if (player->spectator && player->frags == -999 && isPlayer)
				player->frags = 0;

			memset(player->userinfo, 0, sizeof(player->userinfo));
			strlcpy(player->userinfo, newuserinfo, MAX_INFO_STRING);
			CL_ProcessUserInfo(i, player, NULL);
			return;
		}
	}
	Com_Printf("User not in server.\n");
}

//
// Inits the demo cache and adds demo commands.
//
void CL_Demo_Init(void)
{
	int parm, democache_size;
	byte *democache_buffer;

	//
	// Init the demo cache if the user specified to use one.
	//
	democache_available = false;
	if ((parm = COM_CheckParm("-democache")) && parm + 1 < COM_Argc())
	{
		democache_size = Q_atoi(COM_Argv(parm + 1)) * 1024;
		democache_size = max(democache_size, DEMOCACHE_MINSIZE);
		if ((democache_buffer = (byte *) malloc (democache_size)))
		{
			Com_Printf_State (PRINT_OK, "Democache initialized (%.1f MB)\n", (float) (democache_size) / (1024 * 1024));
			SZ_Init(&democache, democache_buffer, democache_size);
			democache_available = true;
		}
		else
		{
			Com_Printf_State (PRINT_FAIL, "Democache allocation failed\n");
		}
	}

	//
	// Add demo commands.
	//
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("recordqwd", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("stopqwd", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_Play_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
	Cmd_AddCommand ("easyrecord", CL_EasyRecord_f);

	Cmd_AddCommand("demo_setspeed", CL_Demo_SetSpeed_f);
	Cmd_AddCommand("demo_jump", CL_Demo_Jump_f);
	Cmd_AddCommand("demo_jump_mark", CL_Demo_Jump_Mark_f);
	Cmd_AddCommand("demo_jump_status", CL_Demo_Jump_Status_f);
	Cmd_AddCommand("demo_controls", DemoControls_f);

	//
	// mvd "recording"
	//
	Cmd_AddCommand("mvdrecord", CL_RecordMvd_f);
	Cmd_AddCommand("mvdstop", CL_StopMvd_f);

	//
	// QTV commands.
	//
	Cmd_AddCommand ("qtvplay", CL_QTVPlay_f);
	Cmd_AddCommand ("qtv_query_sourcelist", CL_QTVList_f);
	Cmd_AddCommand ("qtv_query_demolist", CL_QTVList_f);
	Cmd_AddCommand ("qtvreconnect", CL_QTVReconnect_f);
	Cmd_AddCommand ("qtv_fixuser", CL_QTVFixUser_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
#ifdef _WIN32
	Cvar_Register(&demo_format);
#endif
	Cvar_Register(&demo_dir);
	Cvar_Register(&demo_benchmarkdumps);
	Cvar_Register(&cl_startupdemo);
	Cvar_Register(&demo_jump_rewind);

	Cvar_ResetCurrentGroup();
}

qbool CL_Demo_SkipMessage (void)
{
	int tracknum = Cam_TrackNum();

	if (cls.demoseeking)
		return true;
	if (!cls.mvdplayback)
		return false;

	if (cls.lasttype == dem_multiple && ((tracknum == -1) || !(cls.lastto & (1 << tracknum))))
		return true;
	if (cls.lasttype == dem_single && ((tracknum == -1) || (cls.lastto != spec_track)))
		return true;

	return false;
}

qbool CL_Demo_IsPrimaryPointOfView (void)
{
	if (cls.mvdplayback && cl_multiview.value && CURRVIEW != 1)
		return false;

	return true;
}

