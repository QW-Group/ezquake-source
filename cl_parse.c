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

#include "quakedef.h"
#include "cdaudio.h"
#include "pmove.h"
#include "sbar.h"
#include "sound.h"
#include "teamplay.h"
#include "version.h"

#include "ignore.h"
#include "auth.h"
#include "fchecks.h"
#include "config_manager.h"
#include "utils.h"

void R_TranslatePlayerSkin (int playernum);
void R_PreMapLoad(char *mapname);

char *svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",		// <see code>
	"svc_time",			// [float] server time
	"svc_print",		// [string] null terminated string
	"svc_stufftext",	// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value

	"svc_serverdata",	// [long] version ...
	"svc_lightstyle",	// [byte] [string]
	"svc_updatename",	// [byte] [string]
	"svc_updatefrags",	// [byte] [short]
	"svc_clientdata",	// <shortbits + data>
	"svc_stopsound",	// <see code>
	"svc_updatecolors",	// [byte] [byte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",		// [byte] impact [byte] blood [vec3] from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",	// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",

	"svc_cdtrack",
	"svc_sellscreen",

	"svc_smallkick",
	"svc_bigkick",

	"svc_updateping",
	"svc_updateentertime",

	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_choke",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
 	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",

	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl",
	"svc_nails2",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL"
};

int	oldparsecountmod;
int	parsecountmod;
double	parsecounttime;

//=============================================================================

int packet_latency[NET_TIMINGS];

int CL_CalcNet (void) {
	extern cvar_t cl_oldPL;
	int a, i, lost, packetcount;
	frame_t	*frame;

	for (i = cls.netchan.outgoing_sequence-UPDATE_BACKUP + 1; i <= cls.netchan.outgoing_sequence; i++) {
		frame = &cl.frames[i & UPDATE_MASK];
		if (frame->receivedtime == -1)
			packet_latency[i & NET_TIMINGSMASK] = 9999;	// dropped
		else if (frame->receivedtime == -2)
			packet_latency[i & NET_TIMINGSMASK] = 10000;	// choked
		else if (frame->receivedtime == -3)
			packet_latency[i & NET_TIMINGSMASK] = -1;	// choked by c2spps
		else if (frame->invalid)
			packet_latency[i & NET_TIMINGSMASK] = 9998;	// invalid delta
		else
			packet_latency[i & NET_TIMINGSMASK] = (frame->receivedtime - frame->senttime) * 20;
	}

	lost = packetcount = 0;
	for (a = 0; a < NET_TIMINGS; a++)	{
		if (!cl_oldPL.value && a < UPDATE_BACKUP && (cls.realtime -
			cl.frames[(cls.netchan.outgoing_sequence-a)&UPDATE_MASK].senttime) < cls.latency)
			continue;

		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		if (packet_latency[i] == 9999)
			lost++;
		if (packet_latency[i] != -1)	// don't count packets choked by c2spps
			packetcount++;
	}
	return packetcount ? lost * 100 / packetcount : 100;
}

//=============================================================================

//Returns true if the file exists, otherwise it attempts to start a download from the server.
qboolean CL_CheckOrDownloadFile (char *filename) {
	FILE *f;

	if (strstr (filename, "..")) {
		Com_Printf ("Refusing to download a path with ..\n");
		return true;
	}

	FS_FOpenFile (filename, &f);
	if (f) {	// it exists, no need to download
		fclose (f);
		return true;
	}

	//ZOID - can't download when recording
	if (cls.demorecording) {
		Com_Printf ("Unable to download %s in record mode.\n", cls.downloadname);
		return true;
	}
	//ZOID - can't download when playback
	if (cls.demoplayback)
		return true;

	strcpy (cls.downloadname, filename);
	Com_Printf ("Downloading %s...\n", cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("download %s", cls.downloadname));

	cls.downloadnumber++;

	return false;
}

void Model_NextDownload (void) {
	char mapname[MAX_QPATH];
	int	i;

	if (cls.downloadnumber == 0) {
		Com_Printf ("Checking models...\n");
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_model;
	for ( ; cl.model_name[cls.downloadnumber][0]; cls.downloadnumber++)	{
		if (cl.model_name[cls.downloadnumber][0] == '*')
			continue;	// inline brush model
		if (!CL_CheckOrDownloadFile(cl.model_name[cls.downloadnumber]))
			return;		// started a download
	}

	
	if (!com_serveractive) {
		COM_StripExtension (COM_SkipPath(cl.model_name[1]), mapname);
		R_PreMapLoad(mapname);
	}

	for (i = 1; i < MAX_MODELS; i++) {
		if (!cl.model_name[i][0])
			break;

		cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);

		if (!cl.model_precache[i]) {
			Com_Printf("\nThe required model file '%s' could not be found or downloaded.\n\n", cl.model_name[i]);
			Host_EndGame();
			return;
		}
	}
	// all done
	cl.worldmodel = cl.model_precache[1];
	if (!cl.worldmodel)
		Host_Error ("Model_NextDownload: NULL worldmodel");
	R_NewMap ();
	TP_NewMap();
	MT_NewMap();
	Stats_NewMap();
	Hunk_Check();		// make sure nothing is hurt

	// done with modellist, request first of static signon messages
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("prespawn %i 0 %i", cl.servercount, cl.worldmodel->checksum2));
}

void Sound_NextDownload (void) {
	char *s;
	int i;

	if (cls.downloadnumber == 0) {
		Com_Printf ("Checking sounds...\n");
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_sound;
	for ( ; cl.sound_name[cls.downloadnumber][0]; cls.downloadnumber++)	{
		s = cl.sound_name[cls.downloadnumber];
		if (!CL_CheckOrDownloadFile(va("sound/%s",s)))
			return;		// started a download
	}

	for (i = 1; i < MAX_SOUNDS; i++) {
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}

	// done with sounds, request models now
	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	for (i = 0; i < cl_num_modelindices; i++)
		cl_modelindices[i] = -1;

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, 0));
}

void CL_RequestNextDownload (void) {
	switch (cls.downloadtype) {
	case dl_single:
		break;
	case dl_skin:
		Skin_NextDownload ();
		break;
	case dl_model:
		Model_NextDownload ();
		break;
	case dl_sound:
		Sound_NextDownload ();
		break;
	case dl_none:
	default:
		Com_DPrintf ("Unknown download type.\n");
	}
}

//A download message has been received from the server
void CL_ParseDownload (void) {
	int size, percent, r;
	byte name[1024];

	// read the data
	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (cls.demoplayback) {
		if (size > 0)
			msg_readcount += size;
		return; // not in demo playback
	}

	if (size == -1)	{
		Com_Printf ("File not found.\n");
		if (cls.download) {
			Com_Printf ("cls.download shouldn't have been set\n");
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download) {
		if (strncmp(cls.downloadtempname,"skins/",6))
			Q_snprintfz (name, sizeof(name), "%s/%s", cls.gamedir, cls.downloadtempname);
		else
			Q_snprintfz (name, sizeof(name), "qw/%s", cls.downloadtempname);

		COM_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download) {
			msg_readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	fwrite (net_message.data + msg_readcount, 1, size, cls.download);
	msg_readcount += size;

	if (percent != 100) {
// change display routines by zoid
		// request next block
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	} else {
		char oldn[MAX_OSPATH], newn[MAX_OSPATH];

		fclose (cls.download);

		// rename the temp file to its final name
		if (strcmp(cls.downloadtempname, cls.downloadname)) {
			if (strncmp(cls.downloadtempname,"skins/",6)) {
				sprintf (oldn, "%s/%s", cls.gamedir, cls.downloadtempname);
				sprintf (newn, "%s/%s", cls.gamedir, cls.downloadname);
			} else {
				sprintf (oldn, "qw/%s", cls.downloadtempname);
				sprintf (newn, "qw/%s", cls.downloadname);
			}
			r = rename (oldn, newn);
			if (r)
				Com_Printf ("failed to rename.\n");
		}

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}

static byte *upload_data;
static int upload_pos;
static int upload_size;
void CL_NextUpload(void) {
	static byte buffer[768];
	int r, percent, size;

	if (!upload_data)
		return;

	r = min(upload_size - upload_pos, sizeof(buffer));
	memcpy(buffer, upload_data + upload_pos, r);
	MSG_WriteByte (&cls.netchan.message, clc_upload);
	MSG_WriteShort (&cls.netchan.message, r);

	upload_pos += r;
	size = upload_size ? upload_size : 1;
	percent = upload_pos * 100 / size;
	MSG_WriteByte (&cls.netchan.message, percent);
	SZ_Write (&cls.netchan.message, buffer, r);

	Com_DPrintf ("UPLOAD: %6d: %d written\n", upload_pos - r, r);

	if (upload_pos != upload_size)
		return;

	Com_Printf ("Upload completed\n");

	free(upload_data);
	upload_data = 0;
	upload_pos = upload_size = 0;
}

qboolean CL_IsUploading(void) {
	return !!upload_data;
}

void CL_StopUpload(void) {
	if (upload_data) {
		free(upload_data);
		upload_data = NULL;
	}
}

int COM_FileOpenRead (char *path, FILE **hndl);

void CL_StartUpload (char *filename) {
	FILE *f;
	int size;

	if (cls.state < ca_onserver)
		return; // gotta be connected

	if (CL_IsUploading())
		CL_StopUpload();

	if ((size = COM_FileOpenRead (va("%s/%s", com_basedir, filename), &f)) == -1)
		return;

	Com_DPrintf ("Upload starting of %d...\n", size);

	upload_data = Q_Malloc (size);
	if (fread(upload_data, 1, size, f) != size) {
		fclose(f);
		CL_StopUpload();
		return;
	}

	fclose(f);
	upload_size = size;
	upload_pos = 0;

	CL_NextUpload();
} 
/*
=====================================================================
  SERVER CONNECTING MESSAGES
=====================================================================
*/

void CL_ParseServerData (void) {
	char *str, fn[MAX_OSPATH];
	FILE *f;
	qboolean cflag = false;
	int i, protover;
	extern cshift_t	cshift_empty;

	Com_DPrintf ("Serverdata packet received.\n");

	// wipe the clientState_t struct
	CL_ClearState ();
	memset (&cshift_empty, 0, sizeof(cshift_empty));	// Tonik

	// parse protocol version number
	// allow 2.2 and 2.29 demos to play
	protover = MSG_ReadLong ();
	if (protover != PROTOCOL_VERSION &&
		!(cls.demoplayback && (protover == 26 || protover == 27 || protover == 28)))
		Host_Error ("Server returned version %i, not %i\nYou probably need to upgrade.\nCheck http://www.fuhquake.net/", protover, PROTOCOL_VERSION);

	cl.protoversion = protover;
	cl.servercount = MSG_ReadLong ();

	// game directory
	str = MSG_ReadString ();

	cl.teamfortress = !Q_strcasecmp(str, "fortress");
	if (cl.teamfortress) {
		extern cvar_t	v_iyaw_cycle, v_iroll_cycle, v_ipitch_cycle,
			v_iyaw_level, v_iroll_level, v_ipitch_level, v_idlescale;
		cbuf_current = &cbuf_svc;	// hack
		Cvar_SetValue (&v_iyaw_cycle, 2);
		Cvar_SetValue (&v_iroll_cycle, 0.5);
		Cvar_SetValue (&v_ipitch_cycle, 1);
		Cvar_SetValue (&v_iyaw_level, 0.3);
		Cvar_SetValue (&v_iroll_level, 0.1);
		Cvar_SetValue (&v_ipitch_level, 0.3);
		Cvar_SetValue (&v_idlescale, 0);
	}

	if (Q_strcasecmp(cls.gamedirfile, str)) {
		// save current config
		CL_WriteConfiguration ();
		Q_strncpyz (cls.gamedirfile, str, sizeof(cls.gamedirfile));
		Q_snprintfz (cls.gamedir, sizeof(cls.gamedir),
			"%s/%s", com_basedir, cls.gamedirfile);
		cflag = true;
	}

	if (!com_serveractive)
		FS_SetGamedir (str);

	// run config.cfg and frontend.cfg in the gamedir if they exist
	
	
	
	
	if (cfg_legacy_exec.value && (cflag || cfg_legacy_exec.value >= 2)) {
		Q_snprintfz (fn, sizeof(fn), "%s/%s", cls.gamedir, "config.cfg");		
		Cbuf_AddText ("cl_warncmd 0\n");
		if ((f = fopen(fn, "r")) != NULL) {
			fclose(f);
			if (!strcmp(cls.gamedirfile, com_gamedirfile))
				Cbuf_AddText ("exec config.cfg\n");
			else
				Cbuf_AddText (va("exec ../%s/config.cfg\n", cls.gamedirfile));
		} else if (cfg_legacy_exec.value == 3 && strcmp(cls.gamedir, "qw")){
			
			Q_snprintfz (fn, sizeof(fn), "qw/%s", "config.cfg");
			if ((f = fopen(fn, "r")) != NULL) {
				fclose(f);
				Cbuf_AddText ("exec config.cfg\n");
			}
		}
		Q_snprintfz (fn, sizeof(fn), "%s/%s", cls.gamedir, "frontend.cfg");
		if ((f = fopen(fn, "r")) != NULL) {
			fclose(f);
			if (!strcmp(cls.gamedirfile, com_gamedirfile))
				Cbuf_AddText ("exec frontend.cfg\n");
			else
				Cbuf_AddText (va("exec ../%s/frontend.cfg\n", cls.gamedirfile));
		} else if (cfg_legacy_exec.value == 3 && strcmp(cls.gamedir, "qw")){
			
			Q_snprintfz (fn, sizeof(fn), "qw/%s", "frontend.cfg");
			if ((f = fopen(fn, "r")) != NULL) {
				fclose(f);
				Cbuf_AddText ("exec frontend.cfg\n");
			}
		}
		Cbuf_AddText ("cl_warncmd 1\n");
	}

	// parse player slot, high bit means spectator
	if (cls.mvdplayback) {	
		cls.netchan.last_received = nextdemotime = olddemotime = MSG_ReadFloat();
		cl.playernum = MAX_CLIENTS - 1;
		cl.spectator = true;
		for (i = 0; i < UPDATE_BACKUP; i++)
			cl.frames[i].playerstate[cl.playernum].pm_type = PM_SPECTATOR;
	} else {
		cl.playernum = MSG_ReadByte ();
		if (cl.playernum & 128) {
			cl.spectator = true;
			cl.playernum &= ~128;
		}
	}

	// get the full level name
	str = MSG_ReadString ();
	Q_strncpyz (cl.levelname, str, sizeof(cl.levelname));

	// get the movevars
	movevars.gravity			= MSG_ReadFloat();
	movevars.stopspeed          = MSG_ReadFloat();
	cl.maxspeed                 = MSG_ReadFloat();
	movevars.spectatormaxspeed  = MSG_ReadFloat();
	movevars.accelerate         = MSG_ReadFloat();
	movevars.airaccelerate      = MSG_ReadFloat();
	movevars.wateraccelerate    = MSG_ReadFloat();
	movevars.friction           = MSG_ReadFloat();
	movevars.waterfriction      = MSG_ReadFloat();
	cl.entgravity               = MSG_ReadFloat();

	// separate the printfs so the server message can have a color
	Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Com_Printf ("%c%s\n", 2, str);

	// ask for the sound list next
	memset(cl.sound_name, 0, sizeof(cl.sound_name));
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("soundlist %i %i", cl.servercount, 0));

	// now waiting for downloads, etc
	cls.state = ca_onserver;
}

void CL_ParseSoundlist (void) {
	int	numsounds, n;
	char *str;

	// precache sounds
	//	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));

	numsounds = MSG_ReadByte();

	while (1) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		numsounds++;
		if (numsounds == MAX_SOUNDS)
			Host_Error ("Server sent too many sound_precache");
		Q_strncpyz (cl.sound_name[numsounds], str, sizeof(cl.sound_name[numsounds]));
	}

	n = MSG_ReadByte();

	if (n) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("soundlist %i %i", cl.servercount, n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;
	Sound_NextDownload ();
}

void CL_ParseModellist (void) {
	int	i, nummodels, n;
	char *str;

	// precache models and note certain default indexes
	nummodels = MSG_ReadByte();

	while (1) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (++nummodels==MAX_MODELS)
			Host_Error ("Server sent too many model_precache");

		Q_strncpyz (cl.model_name[nummodels], str, sizeof(cl.model_name[nummodels]));

		for (i = 0; i < cl_num_modelindices; i++) {
			if (!strcmp(cl_modelnames[i], cl.model_name[nummodels])) {
				cl_modelindices[i] = nummodels;
				break;
			}
		}
	}

	if ((n = MSG_ReadByte())) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_model;
	Model_NextDownload ();
}

void CL_ParseBaseline (entity_state_t *es) {
	int	i;
	
	es->modelindex = MSG_ReadByte ();
	es->frame = MSG_ReadByte ();
	es->colormap = MSG_ReadByte();
	es->skinnum = MSG_ReadByte();

	for (i = 0; i < 3; i++) {
		es->origin[i] = MSG_ReadCoord ();
		es->angles[i] = MSG_ReadAngle ();
	}
}

//Static entities are non-interactive world objects like torches
void CL_ParseStatic (void) {
	entity_t *ent;
	entity_state_t es;

	CL_ParseBaseline (&es);

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl.num_statics];
	cl.num_statics++;

	// copy it to the current state
	
	ent->model = cl.model_precache[es.modelindex];	
	ent->frame = es.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = es.skinnum;

	VectorCopy (es.origin, ent->origin);
	VectorCopy (es.angles, ent->angles);
	
	R_AddEfrags (ent);
}

void CL_ParseStaticSound (void) {
	extern cvar_t cl_staticsounds;
	vec3_t org;
	int sound_num, vol, atten, i;
	
	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord ();
	sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	if (cl_staticsounds.value)
		S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}

/*
=====================================================================
ACTION MESSAGES
=====================================================================
*/

void CL_ParseStartSoundPacket(void) {
    vec3_t pos;
    int channel, ent, sound_num, volume, i;
	int tracknum;	
    float attenuation;  
	           
    channel = MSG_ReadShort(); 

	volume = (channel & SND_VOLUME) ? MSG_ReadByte () : DEFAULT_SOUND_PACKET_VOLUME;
	
	attenuation = (channel & SND_ATTENUATION) ? MSG_ReadByte () / 64.0 : DEFAULT_SOUND_PACKET_ATTENUATION;
	
	sound_num = MSG_ReadByte ();

	for (i = 0; i < 3; i++)
		pos[i] = MSG_ReadCoord ();
 
	ent = (channel >> 3) & 1023;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);


    if (cls.mvdplayback) {
	    tracknum = Cam_TrackNum();
	    if (cl.spectator && tracknum != -1 && ent == tracknum + 1)
		    ent = cl.playernum + 1;
    }

	
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
	if (ent == cl.playernum+1)
		TP_CheckPickupSound (cl.sound_name[sound_num], pos);
}       

//Server information pertaining to this client only, sent every frame
void CL_ParseClientdata (void) {
	int newparsecount;
	float latency;
	frame_t *frame;

	// calculate simulated time of message
	oldparsecountmod = parsecountmod;

	newparsecount = cls.netchan.incoming_acknowledged;

	cl.oldparsecount = (cls.mvdplayback) ? newparsecount - 1 : cl.parsecount;	
	cl.parsecount = newparsecount;
	parsecountmod = (cl.parsecount & UPDATE_MASK);
	frame = &cl.frames[parsecountmod];

	if (cls.mvdplayback) 
		frame->senttime = cls.realtime - cls.frametime;

	parsecounttime = cl.frames[parsecountmod].senttime;

	frame->receivedtime = cls.realtime;

	// calculate latency
	latency = frame->receivedtime - frame->senttime;

	if (latency >= 0 && latency <= 1) {
		// drift the average latency towards the observed latency
		if (latency < cls.latency)
			cls.latency = latency;
		else
			cls.latency += 0.001;	// drift up, so correction is needed
	}
}

void CL_NewTranslation (int slot) {
	player_info_t *player;
	int tracknum;

	if (cls.state < ca_connected)
		return;

	if (slot >= MAX_CLIENTS)
		Sys_Error ("CL_NewTranslation: slot >= MAX_CLIENTS");

	player = &cl.players[slot];
	if (!player->name[0] || player->spectator)
		return;

	player->topcolor = player->real_topcolor;
	player->bottomcolor = player->real_bottomcolor;


	if (cl.spectator && (tracknum = Cam_TrackNum()) != -1)
		skinforcing_team =  cl.players[tracknum].team;
	else if (!cl.spectator)
		skinforcing_team = cl.players[cl.playernum].team;


	if (!cl.teamfortress && !(cl.fpd & FPD_NO_FORCE_COLOR)) {
		qboolean teammate;

		teammate = (cl.teamplay && !strcmp(player->team, skinforcing_team)) ? true : false;
		if (cl_teamtopcolor >= 0 && teammate) {
			player->topcolor = cl_teamtopcolor;
			player->bottomcolor = cl_teambottomcolor;
		} else if (cl_enemytopcolor >= 0 && slot != cl.playernum && !teammate)	{
			player->topcolor = cl_enemytopcolor;
			player->bottomcolor = cl_enemybottomcolor;
		}
	}

	R_TranslatePlayerSkin(slot);
}

void CL_ProcessUserInfo (int slot, player_info_t *player, char *key) {
	qboolean update_skin;
	int mynum;

	Q_strncpyz (player->name, Info_ValueForKey (player->userinfo, "name"), sizeof(player->name));
	if (!player->name[0] && player->userid && strlen(player->userinfo) >= MAX_INFO_STRING - 17) {
		// somebody's trying to hide himself by overloading userinfo
		strcpy (player->name, " ");
	}
	player->real_topcolor = atoi(Info_ValueForKey (player->userinfo, "topcolor"));
	player->real_bottomcolor = atoi(Info_ValueForKey (player->userinfo, "bottomcolor"));
	strcpy (player->team, Info_ValueForKey (player->userinfo, "team"));

	player->spectator = (Info_ValueForKey (player->userinfo, "*spectator")[0]) ? true : false;

	if (slot == cl.playernum && player->name[0])
		cl.spectator = player->spectator;

	Sbar_Changed ();

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1)
		mynum = cl.playernum;

	update_skin = !key ||	(!player->spectator && ( !strcmp(key, "skin") || !strcmp(key, "topcolor") || 
													 !strcmp(key, "bottomcolor") || !strcmp(key, "team")
													)
							);

	if (slot == mynum && TP_NeedRefreshSkins() && strcmp(player->team, player->_team))
		TP_RefreshSkins();
	else if (update_skin)
		TP_RefreshSkin(slot);

	strcpy (player->_team, player->team);
}

void CL_PlayerEnterSlot(player_info_t *player) {
	extern player_state_t oldplayerstates[MAX_CLIENTS];

	player->ignored = player->validated = false;
	player->f_server[0] = 0;
	memset(&oldplayerstates[player - cl.players], 0, sizeof(player_state_t));
	Stats_EnterSlot(player - cl.players);
}

void CL_PlayerLeaveSlot(player_info_t *player) {
	return;
}

void CL_UpdateUserinfo (void) {
	qboolean was_empty_slot;
	int slot;
	player_info_t *player;

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_Error ("CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS");

	player = &cl.players[slot];

	was_empty_slot = player->name[0] ? false : true;

	player->userid = MSG_ReadLong ();
	Q_strncpyz (player->userinfo, MSG_ReadString(), sizeof(player->userinfo));

	CL_ProcessUserInfo (slot, player, NULL);	

	if (player->name[0] && was_empty_slot)
		CL_PlayerEnterSlot(player);
	else if (!player->name[0] && !was_empty_slot)
		CL_PlayerLeaveSlot(player);
}

void CL_SetInfo (void) {
	int	slot;
	player_info_t *player;
	char key[MAX_INFO_STRING], value[MAX_INFO_STRING];

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_Error ("CL_ParseServerMessage: svc_setinfo > MAX_CLIENTS");

	player = &cl.players[slot];

	Q_strncpyz (key, MSG_ReadString(), sizeof(key));
	Q_strncpyz (value, MSG_ReadString(), sizeof(value));

	if (!cl.teamfortress)	// don't allow cheating in TF
		Com_DPrintf ("SETINFO %s: %s=%s\n", player->name, key, value);

	Info_SetValueForStarKey (player->userinfo, key, value, MAX_INFO_STRING);

	CL_ProcessUserInfo (slot, player, key);
}

//Called by CL_FullServerinfo_f and CL_ParseServerInfoChange
void CL_ProcessServerInfo (void) {
	char *p, *s, *fbskins, *truelightning, *minlight;		
	int teamplay, fpd;
	qboolean skin_refresh;

	// game type (sbar code checks it) (GAME_DEATHMATCH default)
	cl.gametype = *(p = Info_ValueForKey(cl.serverinfo, "deathmatch")) ? (atoi(p) ? GAME_DEATHMATCH : GAME_COOP) : GAME_DEATHMATCH;

	// server side fps restriction
	cl.maxfps = Q_atof(Info_ValueForKey(cl.serverinfo, "maxfps"));


	if (cls.demoplayback) {
		cl.watervis = cl.allow_lumas = true;
		cl.fbskins = cl.truelightning = 1;
		fpd = 0;
	} else if (cl.spectator) {
		cl.watervis = !atoi(Info_ValueForKey(cl.serverinfo, "watervis")) ? 0 : 1;
		cl.allow_lumas = true;
		cl.fbskins = cl.truelightning = 1;
		fpd = atoi(Info_ValueForKey(cl.serverinfo, "fpd"));	
	} else {
		cl.watervis = !atoi(Info_ValueForKey(cl.serverinfo, "watervis")) ? 0 : 1;
		cl.allow_lumas = !strcmp(Info_ValueForKey(cl.serverinfo, "24bit_fbs"), "1") ? true : false;
		cl.fbskins = *(fbskins = Info_ValueForKey(cl.serverinfo, "fbskins")) ? bound(0, Q_atof(fbskins), 1) :
		cl.teamfortress ? 0 : 1;
		cl.truelightning = *(truelightning = Info_ValueForKey(cl.serverinfo, "truelightning")) ?
			bound(0, Q_atof(truelightning), 1) : 1;
		fpd = atoi(Info_ValueForKey(cl.serverinfo, "fpd"));
	}

	p = Info_ValueForKey(cl.serverinfo, "status");
	for (s = p; *s; s++)
		*s = tolower(*s);
	cl.standby = !strcmp(p, "standby") ? true : false;

	cl.minlight = (strlen(minlight = Info_ValueForKey(cl.serverinfo, "minlight")) ? bound(0, Q_atoi(minlight), 255) : 4);

	// ZQuake extension bits
	cl.z_ext = atoi(Info_ValueForKey(cl.serverinfo, "*z_ext"));

	// movement vars for prediction
	cl.bunnyspeedcap = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_bunnyspeedcap"));
	movevars.slidefix = Q_atoi(Info_ValueForKey(cl.serverinfo, "pm_slidefix"));
	movevars.ktjump = *(p = Info_ValueForKey(cl.serverinfo, "pm_ktjump")) ?
		Q_atof(p) : cl.teamfortress ? 0 : 0.5;

	// deathmatch and teamplay
	cl.deathmatch = atoi(Info_ValueForKey(cl.serverinfo, "deathmatch"));
	teamplay = atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));

	// update skins if needed
	skin_refresh = ( !teamplay != !cl.teamplay || ( (fpd ^ cl.fpd) & (FPD_NO_FORCE_COLOR|FPD_NO_FORCE_SKIN) ) );
	cl.teamplay = teamplay;
	cl.fpd = fpd;
	if (skin_refresh)
		TP_RefreshSkins();
}

void CL_ParseServerInfoChange (void) {
	char key[MAX_INFO_STRING], value[MAX_INFO_STRING];

	Q_strncpyz (key, MSG_ReadString(), sizeof(key));
	Q_strncpyz (value, MSG_ReadString(), sizeof(value));

	Com_DPrintf ("SERVERINFO: %s=%s\n", key, value);

	Info_SetValueForKey (cl.serverinfo, key, value, MAX_SERVERINFO_STRING);

	CL_ProcessServerInfo ();
}

//for CL_ParsePrint
static void FlushString (char *s, int level, qboolean team, int offset) {
	if (level == PRINT_CHAT)
		
		Com_Printf ("%s", TP_ParseWhiteText(s, team, offset));
		
	else
		Com_Printf ("%s", s);

	if (level >= 4)
		return;

	if (team)
		level = 4;

	TP_SearchForMsgTriggers (s + offset, level);
	Stats_ParsePrint(s, level);
}

void CL_ParsePrint (void) {
	qboolean suppress_talksound;	
	char *s, str[2048], *p, check_flood;		
	int len, level, flags = 0, offset = 0;
	extern cvar_t cl_chatsound;							

	extern qboolean TP_SuppressMessage(char *);


	level = MSG_ReadByte ();
	s = MSG_ReadString ();

	if (level == PRINT_CHAT) {
	
		if (TP_SuppressMessage(s))
			return;
	

		flags = TP_CategorizeMessage (s, &offset);
		FChecks_CheckRequest(s);
		Auth_CheckResponse (s, flags, offset);						

		if (Ignore_Message(s, flags, offset))						
			return;

		if (flags == 2 && !TP_FilterMessage(s + offset))
			return;

		check_flood = Ignore_Check_Flood(s, flags, offset);			
		if (check_flood == IGNORE_NO_ADD)							
			return;
		else if (check_flood == NO_IGNORE_ADD)					
			Ignore_Flood_Add(s);

		suppress_talksound = false;

		if (flags == 2)
			suppress_talksound = TP_CheckSoundTrigger (s + offset);

		if (!cl_chatsound.value ||		// no sound at all
			(cl_chatsound.value == 2 && flags != 2))	// only play sound in mm2
			suppress_talksound = true;

		if (!suppress_talksound)
			S_LocalSound ("misc/talk.wav");

		if (cl_nofake.value == 1 || (cl_nofake.value == 2 && flags != 2)) {
			for (p = s; *p; p++)
				if (*p == 0x0D || (*p == 0x0A && p[1]))
					*p = ' ';
		}
	}

	if (cl.sprint_buf[0] && (level != cl.sprint_level || s[0] == 1 || s[0] == 2)) {
		FlushString (cl.sprint_buf, cl.sprint_level, false, 0);
		cl.sprint_buf[0] = 0;
	}

	if (s[0] == 1 || s[0] == 2) {
		FlushString (s, level, (flags == 2), offset);
		return;
	}

	strncat (cl.sprint_buf, s, sizeof(cl.sprint_buf) - strlen(cl.sprint_buf) - 1);
	cl.sprint_buf[sizeof(cl.sprint_buf) - 1] = 0;
	cl.sprint_level = level;

	if ((p = strrchr(cl.sprint_buf, '\n'))) {
		len = p - cl.sprint_buf + 1;
		memcpy(str, cl.sprint_buf, len);
		str[len] = '\0';
		Q_strncpyz (cl.sprint_buf, p + 1, sizeof(cl.sprint_buf));
		FlushString (str, level, (flags == 2), offset);
	}
}

void CL_ParseStufftext (void) {
	char *s;

	s = MSG_ReadString ();

	Com_DPrintf ("stufftext: %s\n", s);

	
	if (!strncmp(s, "alias _cs", 9))
		Cbuf_AddTextEx (&cbuf_svc, "alias _cs \"wait;+attack;wait;wait;-attack;wait\"\n");
	else if (!strncmp(s, "alias _y", 8))
		Cbuf_AddTextEx (&cbuf_svc, "alias _y \"wait;-attack;wait;wait;+attack;wait;wait;-attack;wait\"\n");
	
	else if (!strcmp (s, "cmd snap") || (!strncmp (s, "r_skyname ", 10) && !strchr (s, '\n')))
		Cbuf_AddTextEx (&cbuf_svc, va("%s\n", s));
	else
		Cbuf_AddTextEx (&cbuf_svc, s);

	// Execute stuffed commands immediately when starting a demo
	if (cls.demoplayback && cls.state != ca_active)
		Cbuf_ExecuteEx (&cbuf_svc); // FIXME: execute cbuf_main too?
}

void CL_SetStat (int stat, int value) {
	int	j;

	if (stat < 0 || stat >= MAX_CL_STATS)
		Host_Error ("CL_SetStat: %i is invalid", stat);


	if (cls.mvdplayback) {
		cl.players[cls.lastto].stats[stat]=value;
		if ( Cam_TrackNum() != cls.lastto )
			return;
	}

	Sbar_Changed ();

	if (stat == STAT_ITEMS) {
	// set flash times
		Sbar_Changed ();
		for (j = 0; j < 32; j++)
			if ( (value & (1 << j)) && !(cl.stats[stat] & (1 << j)) )
				cl.item_gettime[j] = cl.time;
	}

	cl.stats[stat] = value;

	TP_StatChanged(stat, value);
}

void CL_MuzzleFlash (void) {
	vec3_t forward;
	dlight_t *dl;
	int i, j, num_ent;
	entity_state_t *ent;
	player_state_t *state;

	i = MSG_ReadShort ();

	if (!cl_muzzleflash.value)
		return;

	if (!cl.validsequence)
		return;

	if ((unsigned) (i - 1) >= MAX_CLIENTS) {
		// a monster firing
		num_ent = cl.frames[cl.validsequence & UPDATE_MASK].packet_entities.num_entities;
		for (j = 0; j < num_ent; j++) {
			ent = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities.entities[j];
			if (ent->number == i) {
				dl = CL_AllocDlight (-i);
				AngleVectors (ent->angles, forward, NULL, NULL);
				VectorMA (ent->origin, 18, forward, dl->origin);
				dl->radius = 200 + (rand() & 31);
				dl->minlight = 32;
				dl->die = cl.time + 0.1;
				dl->type = lt_muzzleflash;
				break;
			}
		}
		return;
	}
	if (cl_muzzleflash.value == 2 && i - 1 == cl.viewplayernum)
		return;

	dl = CL_AllocDlight (-i);
	state = &cl.frames[cls.mvdplayback ? oldparsecountmod : parsecountmod].playerstate[i - 1];	
	AngleVectors (state->viewangles, forward, NULL, NULL);
	VectorMA (state->origin, 18, forward, dl->origin);
	dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cl.time + 0.1;
	dl->type = lt_muzzleflash;
}

void CL_ParseQizmoVoice (void) {
	int i, seq, bits, num, unknown;

	// read the two-byte header
	seq = MSG_ReadByte ();
	bits = MSG_ReadByte ();

	seq |= (bits & 0x30) << 4;	// 10-bit block sequence number, strictly increasing
	num = bits >> 6;			// 2-bit sample number, bumped at the start of a new sample
	unknown = bits & 0x0f;		// mysterious 4 bits.  volume multiplier maybe?

	// 32 bytes of voice data follow
	for (i = 0; i < 32; i++)
		MSG_ReadByte ();
}

#define SHOWNET(x) {if (cl_shownet.value == 2) Com_Printf ("%3i:%s\n", msg_readcount - 1, x);}

void CL_ParseServerMessage (void) {
	int cmd, i, j;
	extern int mvd_fixangle;		
	int msg_svc_start;

	if (cl_shownet.value == 1)
		Com_Printf ("%i ", net_message.cursize);
	else if (cl_shownet.value == 2)
		Com_Printf ("------------------\n");

	cls.demomessage.cursize = 0;	

	CL_ParseClientdata ();
	CL_ClearProjectiles ();	

	// parse the message
	while (1) {
		if (msg_badread) {
			Host_Error ("CL_ParseServerMessage: Bad server message");
			break;
		}

		msg_svc_start = msg_readcount;
		cmd = MSG_ReadByte ();

		if (cmd == -1) {
			msg_readcount++;	// so the EOM showner has the right value
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cmd == svc_qizmovoice)
			SHOWNET("svc_qizmovoice")
		else if (cmd < sizeof(svc_strings) / sizeof(svc_strings[0]))
			SHOWNET(svc_strings[cmd]);

		// other commands
		switch (cmd) {
		default:
			Host_Error ("CL_ParseServerMessage: Illegible server message\n");
			break;

		case svc_nop:
			break;

		case svc_disconnect:
			if (cls.state == ca_connected) {
				Host_Error(	"Server disconnected\n"
							"Server version may not be compatible");
			} else {
				Com_DPrintf("Server disconnected\n");
				Host_EndGame();		// the server will be killed if it tries to kick the local player
				Host_Abort();
			}
			break;

		case svc_time:
			MSG_ReadFloat ();
			break;

		case svc_print:
			CL_ParsePrint ();
			break;

		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString ());
			break;

		case svc_stufftext:
			CL_ParseStufftext ();
			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverdata:
			Cbuf_ExecuteEx (&cbuf_svc);		// make sure any stuffed commands are done
			CL_ParseServerData ();
			vid.recalc_refdef = true;		// leave full screen intermission
			break;

		case svc_setangle:
			if (cls.mvdplayback) {	
				j = MSG_ReadByte();
				mvd_fixangle |= 1 << j;
				if (j != Cam_TrackNum()) {
					for (i = 0; i < 3; i++)
						MSG_ReadAngle();
				}
			}
			if (!cls.mvdplayback || (cls.mvdplayback && j == Cam_TrackNum())) {	
				for (i = 0; i < 3; i++)
					cl.viewangles[i] = MSG_ReadAngle ();
			}
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Host_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			Q_strncpyz (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[i].map));
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i >> 3, i & 7);
			break;

		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_CLIENTS");
			cl.players[i].frags = MSG_ReadShort ();
			break;

		case svc_updateping:
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_Error ("CL_ParseServerMessage: svc_updateping > MAX_CLIENTS");
			cl.players[i].ping = MSG_ReadShort ();
			break;

		case svc_updatepl:
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_Error ("CL_ParseServerMessage: svc_updatepl > MAX_CLIENTS");
			cl.players[i].pl = MSG_ReadByte ();
			break;

		case svc_updateentertime:
			// time is sent over as seconds ago
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
				Host_Error ("CL_ParseServerMessage: svc_updateentertime > MAX_CLIENTS");
			cl.players[i].entertime = (cls.demoplayback ? cls.demotime : cls.realtime) - MSG_ReadFloat ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			CL_ParseBaseline (&cl_entities[i].baseline);
			break;

		case svc_spawnstatic:
			CL_ParseStatic ();
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			j = MSG_ReadByte ();
			CL_SetStat (i, j);
			break;

		case svc_updatestatlong:
			i = MSG_ReadByte ();
			j = MSG_ReadLong ();
			CL_SetStat (i, j);
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cls.demoplayback ? cls.demotime : cls.realtime;
			vid.recalc_refdef = true;	// go to full screen
			for (i = 0; i < 3; i++)
				cl.simorg[i] = MSG_ReadCoord ();			
			for (i = 0; i < 3; i++)
				cl.simangles[i] = MSG_ReadAngle ();
			VectorClear (cl.simvel);
			TP_ExecTrigger ("f_mapend");
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cls.demoplayback ? cls.demotime : cls.realtime;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString ());			
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help");
			break;

		case svc_smallkick:
			cl.ideal_punchangle = -2;
			break;

		case svc_bigkick:
			cl.ideal_punchangle = -4;
			break;

		case svc_muzzleflash:
			CL_MuzzleFlash ();
			break;

		case svc_updateuserinfo:
			CL_UpdateUserinfo ();
			break;

		case svc_setinfo:
			CL_SetInfo ();
			break;

		case svc_serverinfo:
			CL_ParseServerInfoChange ();
			break;

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_playerinfo:
			CL_ParsePlayerinfo ();
			break;

		
		case svc_nails:
			CL_ParseProjectiles(false);
			break;
		case svc_nails2:
			if (!cls.mvdplayback)
				Host_Error("CL_ParseServerMessage: svc_nails2 without cls.mvdplayback");
			CL_ParseProjectiles(true);
			break;

		case svc_chokecount:		// some preceding packets were choked
			i = MSG_ReadByte ();
			for (j = cls.netchan.incoming_acknowledged - 1; i > 0 && j > cls.netchan.outgoing_sequence - UPDATE_BACKUP; j--) {
				if (cl.frames[j & UPDATE_MASK].receivedtime != -3) {
					cl.frames[j & UPDATE_MASK].receivedtime = -2;
					i--;
				}
			}
			break;

		case svc_modellist:
			CL_ParseModellist ();
			break;

		case svc_soundlist:
			CL_ParseSoundlist ();
			break;

		case svc_packetentities:
			CL_ParsePacketEntities (false);
			break;

		case svc_deltapacketentities:
			CL_ParsePacketEntities (true);
			break;

		case svc_maxspeed :
			cl.maxspeed = MSG_ReadFloat();
			break;

		case svc_entgravity :
			cl.entgravity = MSG_ReadFloat();
			break;

		case svc_setpause:
			if (MSG_ReadByte())
				cl.paused |= PAUSED_SERVER;
			else
				cl.paused &= ~PAUSED_SERVER;

			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

		case svc_qizmovoice:
			CL_ParseQizmoVoice ();
			break;
		}

		if (cls.demorecording) {
			if (!cls.demomessage.cursize) {
				SZ_Init(&cls.demomessage, cls.demomessage_data, sizeof(cls.demomessage_data));
				SZ_Write (&cls.demomessage, net_message.data, 8);
			}
			if (cmd == svc_deltapacketentities)
				CL_WriteDemoEntities();
			else
				SZ_Write(&cls.demomessage, net_message.data + msg_svc_start, msg_readcount - msg_svc_start);
		}
	}

	if (cls.demorecording) {
		
		if (cls.demomessage.cursize)
			CL_WriteDemoMessage (&cls.demomessage);
	}

	CL_SetSolidEntities ();
}
