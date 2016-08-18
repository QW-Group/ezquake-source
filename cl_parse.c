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

$Id: cl_parse.c,v 1.135 2007-10-28 19:56:44 qqshka Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "cdaudio.h"
#include "ignore.h"
#include "fchecks.h"
#include "config_manager.h"
#include "utils.h"
#include "localtime.h"
#include "sbar.h"
#include "textencoding.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"
#include "tp_triggers.h"
#include "pmove.h"
#include "stats_grid.h"
#include "qsound.h"
#include "menu.h"
#include "keys.h"
#include "hud.h"
#include "hud_common.h"
#include "mvd_utils.h"
#include "input.h"
#include "qtv.h"

void R_TranslatePlayerSkin (int playernum);

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

const int num_svc_strings = sizeof(svc_strings)/sizeof(svc_strings[0]);

int	parsecountmod;
double	parsecounttime;

//=========================================================
// Cl_Messages, just some simple network statistics/profiling
//=========================================================

#define NUMMSG 256 // for svc_xxx used one byte, so its in range...

typedef struct cl_message_s
{
	int msgs;
	int size;

	int svc; // well, its helpful after qsort
} cl_message_t;

static cl_message_t cl_messages[NUMMSG];

static void CL_Messages_f(void);
static void CL_InitialiseDemoMessageIfRequired(void);

void Cl_Messages_Init(void)
{
	int i;

	memset(cl_messages, 0, sizeof(cl_messages));

	for (i = 0; i < NUMMSG; i++)
		cl_messages[i].svc = i; // well, its helpful after qsort

	Cmd_AddCommand ("cl_messages", CL_Messages_f);
}

static int CL_Messages_qsort(const void *a, const void *b)
{
	cl_message_t *msg1 = (cl_message_t*)a;
	cl_message_t *msg2 = (cl_message_t*)b;

	if ( msg1->size < msg2->size )
		return -1;

	if ( msg1->size > msg2->size )
		return 1;

   return 0;
}

static void CL_Messages_f(void)
{
	cl_message_t messages[NUMMSG]; // local copy of cl_messages[] for qsorting

	int i, svc, total;
	char *svc_name;

	memcpy(messages, cl_messages, sizeof(messages)); // copy it
	qsort(messages, NUMMSG, sizeof(messages[0]), CL_Messages_qsort); // qsort it

	Com_Printf("Received msgs:\n");

	for (i = 0, total = 0; i < NUMMSG; i++)
	{
		if (messages[i].msgs < 1)
			continue;

		svc = messages[i].svc;

		if (svc < 0 || svc >= NUMMSG)
			Sys_Error("CL_Messages_f: svc < 0 || svc >= NUMMSG");

		svc_name = ( svc < num_svc_strings ? svc_strings[svc] : "unknown" );

		Com_Printf("%2d:%s: %d msgs: %0.2fk\n", svc, svc_name, messages[i].msgs, (float)(messages[i].size)/1024);

		total += messages[i].size;
	}

	Com_Printf("Total size: %d\n", total);
}

//=============================================================================

packet_info_t network_stats[NETWORK_STATS_SIZE];

int packet_latency[NET_TIMINGS];

int CL_CalcNet (void) 
{
	int a, i, j, lost, packetcount;
	frame_t	*frame;

	static cvar_t * netgraph_inframes = NULL;
	extern hud_t * hud_netgraph;

	static int last_calculated_outgoing;
	static int last_calculated_incoming;
	static int last_lost;

	if (last_calculated_incoming == cls.netchan.incoming_sequence  &&
	    last_calculated_outgoing == cls.netchan.outgoing_sequence)
		return last_lost;

	last_calculated_outgoing = cls.netchan.outgoing_sequence;
	last_calculated_incoming = cls.netchan.incoming_sequence;

	if (netgraph_inframes == NULL)
	    netgraph_inframes = HUD_FindVar(hud_netgraph, "inframes");

	for (i = cls.netchan.outgoing_sequence-UPDATE_BACKUP + 1; i <= cls.netchan.outgoing_sequence; i++) 
	{
		frame = &cl.frames[i & UPDATE_MASK];
	        j = i & NETWORK_STATS_MASK;
		if (frame->receivedtime == -1)
		{
			packet_latency[i & NET_TIMINGSMASK] = 9999;	// dropped
			network_stats[j].status = packet_dropped;
		}
		else if (frame->receivedtime == -2) 
		{
			packet_latency[i & NET_TIMINGSMASK] = 10000;	// choked
			network_stats[j].status = packet_choked;
		}
		else if (frame->receivedtime == -3)
		{
			packet_latency[i & NET_TIMINGSMASK] = -1;	// choked by c2spps
			network_stats[j].status = packet_netlimit;
		}
		else if (frame->invalid) 
		{
			packet_latency[i & NET_TIMINGSMASK] = 9998;	// invalid delta
			network_stats[j].status = packet_delta;
		}
		else 
		{
			double l;
			if (netgraph_inframes->value)      // [frames]
				l = 2*(frame->seq_when_received-i);
			else                                // [miliseconds]
				l = min((frame->receivedtime - frame->senttime)*1000, 1000);

			packet_latency[i&NET_TIMINGSMASK] = (int)l;
			network_stats[j].status = packet_ok;
		}
	}

	lost = packetcount = 0;
	for (a = 0; a < NET_TIMINGS; a++)	
	{
		// fix for packetloss on high ping
		if (a < UPDATE_BACKUP && (cls.realtime -
			cl.frames[(cls.netchan.outgoing_sequence-a)&UPDATE_MASK].senttime) < cls.latency)
			continue;

		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		if (packet_latency[i] == 9999)
			lost++;
		if (packet_latency[i] != -1)	// don't count packets choked by c2spps
			packetcount++;
	}
	last_lost = packetcount ? lost * 100 / packetcount : 100;
	return last_lost;
}

// More network statistics
int CL_CalcNetStatistics(
            /* in */
            float period,           // period of time
            packet_info_t *samples, // statistics table
            int num_samples,        // table size
            /* out */
            net_stat_result_t *res)
{
    int i, p, q;   // calc fom p to q

    float ping_min, ping_max, ping_avg, ping_dev, ping_sum, ping_dev_sum;
    float f_min, f_max, f_avg, f_dev, f_sum, f_dev_sum;
    int lost_lost, lost_rate, lost_delta, lost_netlimit;
    int size_sent, size_received;
    int samples_received, samples_sent, samples_delta;

    if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence  >  NETWORK_STATS_SIZE/2)
        return (res->samples = 0);

    // find boundaries
    q = cls.netchan.incoming_sequence - 1;
    p = q - 1;
    while (p > cls.netchan.outgoing_sequence - NETWORK_STATS_SIZE + 1  &&
           samples[q&NETWORK_STATS_MASK].senttime - samples[p&NETWORK_STATS_MASK].senttime < period)
	{
        p--;
	}

    // init values
    samples_sent = 0;
    samples_received = 0;
    samples_delta = 0;  // packets with delta compression applied

    ping_sum = 0;
    ping_min =  99999999;
    ping_max = -99999999;
    ping_dev_sum = 0;

    f_sum = 0;
    f_min =  99999999;
    f_max = -99999999;
    f_dev_sum = 0;

    lost_lost = 0;
    lost_rate = 0;
    lost_netlimit = 0;
    lost_delta = 0;

    size_sent = 0;
    size_received = 0;

    for (i=p; i < q; i++)
    {
        int a = i & NETWORK_STATS_MASK;

        if (samples[a].status == packet_netlimit)
        {
            // not even sent
            lost_netlimit++;
            continue;
        }

        // packet was sent
        samples_sent++;

        size_sent += samples[a].sentsize;

        switch (samples[a].status)
        {
			case packet_delta:
				lost_delta++;
				samples_delta++;
				break;
			case packet_choked:
				lost_rate++;
				break;
			case packet_dropped:
				lost_lost++;
				break;
			case packet_ok:
				// packet received
				{
					float ping;
					int frames;

					samples_received++;
					frames = samples[a].seq_diff;
					ping = 1000*(samples[a].receivedtime - samples[a].senttime);

					if (ping < ping_min)    ping_min = ping;
					if (ping > ping_max)    ping_max = ping;
					if (frames < f_min)     f_min = frames;
					if (frames > f_max)     f_max = frames;

					ping_sum += ping;
					f_sum += frames;

					size_received += samples[a].receivedsize;

					if (samples[a].delta)
						samples_delta++;
				}
			default:
				break;
        }

        // end of loop
    }

    if (samples_sent <= 0  ||  samples_received <= 0)
        return (res->samples = 0);

    ping_avg = ping_sum / samples_received;
    f_avg = f_sum / samples_received;

    // loop again to calc standard deviation
    for (i=p; i < q; i++)
    {
        int a = i & NETWORK_STATS_MASK;

        if (samples[a].status == packet_ok)
        {
            float ping;
            int frames;

            frames = samples[a].seq_diff;
            ping = 1000*(samples[a].receivedtime - samples[a].senttime);

            ping_dev_sum += (ping - ping_avg) * (ping - ping_avg);
            f_dev_sum += (frames - f_avg) * (frames - f_avg);
        }
    }

    ping_dev = sqrt(ping_dev_sum / samples_received);
    f_dev = sqrt(f_dev_sum / samples_received);

    // fill-in result struct
    res->ping_min = ping_min;
    res->ping_max = ping_max;
    res->ping_avg = ping_avg;
    res->ping_dev = ping_dev;

    res->ping_f_min = f_min;
    res->ping_f_max = f_max;
    res->ping_f_avg = f_avg;
    res->ping_f_dev = f_dev;

    res->lost_netlimit = lost_netlimit * 100.0 / (q-p);
    res->lost_lost     = lost_lost     * 100.0 / samples_sent;
    res->lost_rate     = lost_rate     * 100.0 / samples_sent;
    res->bandwidth_in  = lost_delta    * 100.0 / samples_sent;

    res->size_out = size_sent      / (float)samples_sent;
    res->size_in  = size_received  / (float)samples_received;

    res->bandwidth_out = size_sent      / period;
    res->bandwidth_in  = size_received  / period;

    res->delta = (samples_delta > 0) ? 1 : 0;
    res->samples = q-p;
    return res->samples;
}

//=============================================================================

// Returns true if the file exists, otherwise it attempts to start a download from the server.
qbool CL_CheckOrDownloadFile (char *filename) 
{
	vfsfile_t *f;
	char *tmp;

	if (strstr(filename, "..") || !strcmp(filename, "") || filename[0] == '/' || strchr(filename, '\\') || strchr(filename, ':') || strstr(filename, "//")) {
		Com_Printf("Warning: Invalid characters in filename \"%s\"\n", filename);
		return true;
	}

	if ((tmp = strrchr(filename, '.')) && (!strcasecmp(tmp, ".dll") || !strcasecmp(tmp, ".so"))) {
		Com_Printf("Warning: Non-allowed file \"%s\" skipped\n", filename);
		return true;
	}

	f = FS_OpenVFS(filename, "rb", FS_ANY);
	if (f) 
	{
		VFS_CLOSE(f);
		return true;
	}

	// Can't download when playback, except qtv which support download
	if (cls.demoplayback)
	{
		if (cls.mvdplayback == QTV_PLAYBACK)
		{
			if (!(cls.qtv_ezquake_ext & QTV_EZQUAKE_EXT_DOWNLOAD)) 
			{
				Com_Printf ("Unable to download %s, this QTV does't support download\n", filename);
				return true;
			}
		}
		else
			return true;
	}

	if (cls.state < ca_connected) 
	{
		Com_DPrintf ("Unable to download %s, not connected\n", filename);
		return true;
	}

	snprintf (cls.downloadname, sizeof(cls.downloadname), "%s/%s", cls.gamedir, filename);
	Com_Printf ("Downloading %s...\n", filename);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left

	cls.downloadmethod    = DL_QW; // by default its DL_QW, if server support DL_QWCHUNKED it will be changed.
	cls.downloadstarttime = Sys_DoubleTime();

	COM_StripExtension (cls.downloadname, cls.downloadtempname, sizeof(cls.downloadtempname));
	strlcat (cls.downloadtempname, ".tmp", sizeof(cls.downloadtempname));

	if (cls.mvdplayback == QTV_PLAYBACK) 
	{
		QTV_Cmd_Printf(QTV_EZQUAKE_EXT_DOWNLOAD, "download \"%s\"", filename);
	}
	else 
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("download \"%s\"", filename));
	}

	cls.downloadnumber++;

	return false;
}

void CL_FindModelNumbers (void) 
{
	int i, j;

	for (i = 0; i < cl_num_modelindices; i++)
		cl_modelindices[i] = -1;

	for (i = 0; i < MAX_MODELS; i++) 
	{
		for (j = 0; j < cl_num_modelindices; j++) 
		{
			if (!strcmp(cl_modelnames[j], cl.model_name[i]))
			{
				cl_modelindices[j] = i;
				break;
			}
		}
	}
}

void CL_ProxyEnter (void) 
{
	if (!strcmp(cl.levelname, "Qizmo menu") ||	// qizmo detection
		strstr(cl.serverinfo, "*QTV")) 			// fteqtv detection
	{
		// If we are connected only to the proxy frontend
		// we presume that the menu is on.
		M_EnterProxyMenu();

	} 
	else if (key_dest == key_menu && m_state == m_proxy) 
	{
		// This happens when we connect to a server using a proxy.
		M_LeaveMenus();
	}

	// If we used console to connect to a server via proxy,
	// cancel return to the proxy menu, because it's not there.
	if (key_dest_beforecon == key_menu)
		key_dest_beforecon = key_game;
}

static void CL_TransmitModelCrc (int index, char *info_key)
{
	if (index != -1) 
	{
		struct model_s *model = cl.model_precache[index];
		unsigned short crc = model->crc;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("setinfo %s %d", info_key, (int) crc));
		Info_SetValueForKey(cls.userinfo, info_key, va("%d", (int) crc), MAX_INFO_STRING);
	}
}

void CL_Prespawn (void)
{
	cl.worldmodel = cl.model_precache[1];
	if (!cl.worldmodel)
		Host_Error ("Model_NextDownload: NULL worldmodel");

	CL_FindModelNumbers ();
	R_NewMap (false);
	TP_NewMap();
	MT_NewMap();
	Stats_NewMap();
	CL_ProxyEnter();
	MVD_Stats_Cleanup();

	// Reset the status grid.
	StatsGrid_Remove(&stats_grid);
	StatsGrid_ResetHoldItems();
	HUD_NewMap();
	Hunk_Check(); // make sure nothing is hurt

	CL_TransmitModelCrc (cl_modelindices[mi_player], "pmodel");
	CL_TransmitModelCrc (cl_modelindices[mi_eyes], "emodel");

#if 0
//TEI: loading entitys from map, at clientside,
// will be usefull to locate more eyecandy and cameras
	if (cl.worldmodel->entities)
	{
		CL_PR_LoadProgs();
		CL_ED_LoadFromFile (cl.worldmodel->entities);
	}
#endif

	// done with modellist, request first of static signon messages, in case of qtv it different
	if (cls.mvdplayback == QTV_PLAYBACK)
	{
		QTV_Cmd_Printf(QTV_EZQUAKE_EXT_DOWNLOAD, "qtvspawn %i", cl.servercount);
	}
	else 
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("prespawn %i 0 %i", cl.servercount, cl.map_checksum2));
	}
}

/*
=================
VWepModel_NextDownload
=================
*/
void CL_ParseVWepPrecache (char *str);
void VWepModel_NextDownload (void)
{
	int		i;
	extern cvar_t cl_novweps;

	if (((!(cl.z_ext & Z_EXT_VWEP) || !cl.vw_model_name[0][0]) && !cls.mvdplayback)
	|| cl_novweps.value) 
	{
		// no vwep support, go straight to prespawn
		CL_Prespawn ();
		return;
	}

	if (cls.mvdplayback && cls.downloadnumber == 0)
	{
		CL_ParseVWepPrecache ("//vwep vwplayer w_axe w_shot w_shot2 w_nail w_nail2 w_rock w_rock2 w_light");
	}

	if (cls.downloadnumber == 0)
	{
		if (!com_serveractive || developer.value)
			Com_DPrintf ("Checking vwep models...\n");
		//cls.downloadnumber = 0;
	}

	cls.downloadtype = dl_vwep_model;
	for ( ; cls.downloadnumber < MAX_VWEP_MODELS; cls.downloadnumber++)
	{
		if (!cl.vw_model_name[cls.downloadnumber][0] ||
			cl.vw_model_name[cls.downloadnumber][0] == '-')
			continue;
		if (!CL_CheckOrDownloadFile(cl.vw_model_name[cls.downloadnumber]))
			return;		// started a download
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++)
	{
		if (!cl.vw_model_name[i][0])
			continue;

		if (strcmp(cl.vw_model_name[i], "-"))
			cl.vw_model_precache[i] = Mod_ForName (cl.vw_model_name[i], false);

		if (!cl.vw_model_precache[i])
		{
			// never mind
			// Com_Printf ("Warning: model %s could not be found\n", cl.vw_model_name[i]);
		}
	}

	if (!strcmp(cl.vw_model_name[0], "-") || cl.vw_model_precache[0])
		cl.vwep_enabled = true;
	else 
	{
		// if the vwep player model is required but not present,
		// don't enable vwep support
	}

	// all done
	CL_Prespawn ();
}

void Model_NextDownload (void) 
{
	int	i;
	char *s;
	char mapname[MAX_QPATH];

	if (cls.downloadnumber == 0) 
	{
		if (!com_serveractive || developer.value)
			Com_DPrintf ("Checking models...\n");
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_model;
	for ( ; cl.model_name[cls.downloadnumber][0]; cls.downloadnumber++)	
	{
		s = cl.model_name[cls.downloadnumber];
		if (s[0] == '*')
			continue; // inline brush model

		if (!CL_CheckOrDownloadFile(s))
			return;	// started a download
	}

	cl.clipmodels[1] = CM_LoadMap (cl.model_name[1], true, NULL, &cl.map_checksum2);
	COM_StripExtension (COM_SkipPath(cl.model_name[1]), mapname, sizeof(mapname));
	cl.map_checksum2 = Com_TranslateMapChecksum (mapname, cl.map_checksum2);

	for (i = 1; i < MAX_MODELS; i++) 
	{
		if (!cl.model_name[i][0])
			break;

		cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);

		if (!cl.model_precache[i]) 
		{
			Com_Printf("\n&cf22Couldn't load model:&r %s\n", cl.model_name[i]);
			Host_EndGame();
			return;
		}

		if (cl.model_name[i][0] == '*')
			cl.clipmodels[i] = CM_InlineModel(cl.model_name[i]);
	}

	// Done with normal models, request vwep models if necessary
	cls.downloadtype = dl_vwep_model;
	cls.downloadnumber = 0;
	VWepModel_NextDownload ();
}

void Sound_NextDownload (void) 
{
	char *s;
	int i;

	if (cls.downloadnumber == 0)
	{
		if (!com_serveractive || developer.value)
			Com_DPrintf ("Checking sounds...\n");
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_sound;
	for ( ; cl.sound_name[cls.downloadnumber][0]; cls.downloadnumber++)	
	{
		s = cl.sound_name[cls.downloadnumber];
		if (!CL_CheckOrDownloadFile(va("sound/%s",s)))
			return;		// started a download
	}

	for (i = 1; i < MAX_SOUNDS; i++) 
	{
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}

	// Done with sound downloads, go for models
	cls.downloadnumber = 0;
	cls.downloadtype = dl_model;
	Model_NextDownload ();

}

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS

//
// FTE's chunked download
//

extern void CL_RequestNextDownload (void);

#define MAXBLOCKS 1024	// Must be power of 2
#define DLBLOCKSIZE 1024

int chunked_download_number = 0; // Never reset, bumped up.

int downloadsize;
int receivedbytes;
int recievedblock[MAXBLOCKS];
int firstblock;
int blockcycle;

int CL_RequestADownloadChunk(void)
{
	int i;
	int b;

	if (cls.downloadmethod != DL_QWCHUNKS) // Paranoia!
		Host_Error("download not initiated\n");

	for (i = 0; i < MAXBLOCKS; i++)
	{
		blockcycle++;

		b = ((blockcycle) & (MAXBLOCKS-1)) + firstblock;

		if (!recievedblock[b&(MAXBLOCKS-1)]) // Don't ask for ones we've already got.
		{
			if (b >= (downloadsize+DLBLOCKSIZE-1)/DLBLOCKSIZE)	// Don't ask for blocks that are over the size of the file.
				continue;
			return b;
		}
	}

	return -1;
}

void CL_SendChunkDownloadReq(void)
{
	extern cvar_t cl_chunksperframe;
	int i, j, chunks;
	
	chunks = bound(1, cl_chunksperframe.integer, 5);

	for (j = 0; j < chunks; j++)
	{
		if (cls.downloadmethod != DL_QWCHUNKS)
			return;

		i = CL_RequestADownloadChunk();
		// i < 0 mean client complete download, let server know
		// qqshka: download percent optional, server does't really require it, that my extension, hope does't fuck up something

		if (i < 0)
		{
			if (strstr(Info_ValueForKey(cl.serverinfo, "*version"), "MVDSV"))
				CL_SendClientCommand(true, "nextdl %d %d %d", i, cls.downloadpercent, chunked_download_number);
			else
				CL_SendClientCommand(true, "stopdownload");

			cls.downloadpercent = 100;
			CL_FinishDownload(); // this also request next dl
		}
		else
		{
			CL_SendClientCommand(false, "nextdl %d %d %d", i, cls.downloadpercent, chunked_download_number);
		}
	}
}

void CL_ParseDownload (void);

void CL_Parse_OOB_ChunkedDownload(void)
{
	int j;

	for ( j = 0; j < sizeof("\\chunk")-1; j++ )
		MSG_ReadByte ();

	//
	// qqshka: well, this is evil.
	// In case of when one file completed download and next started
	// here may be some packets which travel via network,
	// so we got packets from different file, that mean we may assemble wrong data,
	// need somehow discard such packets, i have no idea how, so adding at least this check.
	//

	if (chunked_download_number != MSG_ReadLong ())
	{
		Com_DPrintf("Dropping OOB chunked message, out of sequence\n");
		return;
	}

	if (MSG_ReadByte() != svc_download)
	{
		Com_DPrintf("Something wrong in OOB message and chunked download\n");
		return;
	}

	CL_ParseDownload ();
}

void CL_ParseChunkedDownload(void)
{
	char *svname;
	int totalsize;
	int chunknum;
	char data[DLBLOCKSIZE];
	double tm;

	chunknum = MSG_ReadLong();
	if (chunknum < 0)
	{
		totalsize = MSG_ReadLong();
		svname    = MSG_ReadString();

		if (cls.download) 
		{ 
			// Ensure FILE is closed
			if (totalsize != -3) // -3 = dl stopped, so this known issue, do not warn
				Com_Printf ("cls.download shouldn't have been set\n");

			fclose (cls.download);
			cls.download = NULL;
			cls.downloadpercent = 0;
		}

		if (cls.demoplayback)
			return;

		if (totalsize < 0)
		{
			switch (totalsize)
			{
				case -3: Com_DPrintf("Server cancel downloading file %s\n", svname);			break;
				case -2: Com_Printf("Server permissions deny downloading file %s\n", svname);	break;
				default: Com_Printf("Couldn't find file %s on the server\n", svname);			break;
			}

			CL_FinishDownload(); // this also request next dl
			return;
		}

		if (cls.downloadmethod == DL_QWCHUNKS)
			Host_Error("Received second download - \"%s\"\n", svname);

// FIXME: damn, fixme!!!!!
//		if (strcasecmp(cls.downloadname, svname))
//			Host_Error("Server sent the wrong download - \"%s\" instead of \"%s\"\n", svname, cls.downloadname);

		// Start the new download
		FS_CreatePath (cls.downloadtempname);

		if ( !(cls.download = fopen (cls.downloadtempname, "wb")) ) 
		{
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_FinishDownload(); // This also requests next dl.
			return;
		}

		cls.downloadmethod  = DL_QWCHUNKS;
		cls.downloadpercent = 0;

		chunked_download_number++;

		downloadsize        = totalsize;

		firstblock    = 0;
		receivedbytes = 0;
		blockcycle    = -1;	//so it requests 0 first. :)
		memset(recievedblock, 0, sizeof(recievedblock));
		return;
	}

	MSG_ReadData(data, DLBLOCKSIZE);

	if (!cls.download) 
	{ 
		return;
	}

	if (cls.downloadmethod != DL_QWCHUNKS)
		Host_Error("cls.downloadmethod != DL_QWCHUNKS\n");

	if (cls.demoplayback)
	{	
		// Err, yeah, when playing demos we don't actually pay any attention to this.
		return;
	}

	if (chunknum < firstblock)
	{
		return;
	}

	if (chunknum - firstblock >= MAXBLOCKS)
	{
		return;
	}

	if (recievedblock[chunknum&(MAXBLOCKS-1)])
	{
		return;
	}

	receivedbytes += DLBLOCKSIZE;
	recievedblock[chunknum&(MAXBLOCKS-1)] = true;

	while(recievedblock[firstblock&(MAXBLOCKS-1)])
	{
		recievedblock[firstblock&(MAXBLOCKS-1)] = false;
		firstblock++;
	}

	fseek(cls.download, chunknum * DLBLOCKSIZE, SEEK_SET);
	if (downloadsize - chunknum * DLBLOCKSIZE < DLBLOCKSIZE)	//final block is actually meant to be smaller than we recieve.
		fwrite(data, 1, downloadsize - chunknum * DLBLOCKSIZE, cls.download);
	else
		fwrite(data, 1, DLBLOCKSIZE, cls.download);

	cls.downloadpercent = receivedbytes/(float)downloadsize*100;

	tm = Sys_DoubleTime() - cls.downloadstarttime; // how long we dl-ing
	cls.downloadrate = (tm ? receivedbytes / 1024 / tm : 0); // some average dl speed in KB/s
}

#endif // FTE_PEXT_CHUNKEDDOWNLOADS

void CL_RequestNextDownload (void) 
{
	switch (cls.downloadtype)
	{
		case dl_single:
			break;
		case dl_skin:
			Skin_NextDownload ();
			break;
		case dl_model:
			Model_NextDownload ();
			break;
		case dl_vwep_model:
			VWepModel_NextDownload ();
			break;
		case dl_sound:
			Sound_NextDownload ();
			break;
		case dl_none:
		default:
			Com_DPrintf ("Unknown download type.\n");
	}
}

void CL_FinishDownload(void)
{
	if (cls.download) {
		fclose (cls.download);

		if (cls.downloadpercent == 100) {
			Com_DPrintf("Download took %.1f seconds\n", Sys_DoubleTime() - cls.downloadstarttime);

			// rename the temp file to its final name
			if (strcmp(cls.downloadtempname, cls.downloadname))
				if (rename(cls.downloadtempname, cls.downloadname))
					Com_Printf ("Failed to rename %s to %s.\n",	cls.downloadtempname, cls.downloadname);
		} else {
			/* If download didn't complete, remove the unfinished leftover .tmp file ... */
			unlink(cls.downloadtempname);
		}
	}

	cls.download = NULL;
	cls.downloadpercent = 0;
	cls.downloadmethod = DL_NONE;

	// VFS-FIXME: D-Kure: Surely there is somewhere better for this in fs.c
	filesystemchanged = true;

	// get another file if needed

	if (cls.state != ca_disconnected)
		CL_RequestNextDownload ();
}

// A download message has been received from the server
void CL_ParseDownload (void)
{
	int size, percent;

	double current = Sys_DoubleTime();
	static double time = 0;
	static int s = 0;

	#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if (cls.fteprotocolextensions & FTE_PEXT_CHUNKEDDOWNLOADS)
	{
		CL_ParseChunkedDownload();
		return;
	}
	#endif // PFTE_PEXT_CHUNKEDDOWNLOADS

	if (cls.downloadmethod != DL_QW)
		Host_Error("cls.downloadmethod != DL_QW\n");

	// Read the data
	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	s += size;
	if (current - time > 1) 
	{
		cls.downloadrate = s / (1024 * (current - time));
		time = current;
		s = 0;
	}

	if (cls.demoplayback) 
	{
		qbool skip_download = true;

		// Skip download data in demo playback, except during qtving which support download.

		if (cls.mvdplayback == QTV_PLAYBACK)
		{
			if (cls.qtv_ezquake_ext & QTV_EZQUAKE_EXT_DOWNLOAD)
				skip_download = false;
		}

		if (skip_download)
		{
			if (size > 0)
				msg_readcount += size;
			return;
		}
	}

	if (size == -1)	
	{
		Com_Printf ("File not found.\n");
		if (cls.download) {
			Com_Printf ("cls.download shouldn't have been set\n");
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_FinishDownload(); // this also request next dl
		return;
	}

	// Open the file if not opened yet
	if (!cls.download) 
	{
		FS_CreatePath (cls.downloadtempname);

		if ( !(cls.download = fopen (cls.downloadtempname, "wb")) )
		{
			msg_readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_FinishDownload(); // this also request next dl
			return;
		}
	}

	fwrite (net_message.data + msg_readcount, 1, size, cls.download);
	msg_readcount += size;

	if (percent != 100) 
	{
		// Change display routines by zoid.
		// Request next block.
		cls.downloadpercent = percent;

		if (cls.mvdplayback == QTV_PLAYBACK) 
		{
			// nothing
		}
		else 
		{
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "nextdl");
		}
	} 
	else
	{
		cls.downloadpercent = 100;
		CL_FinishDownload(); // this also request next dl
	}
}

/*
=====================================================================

  UPLOAD FILE FUNCTIONS

=====================================================================
*/
void CL_NextUpload(void)
{
	static byte	buffer[FILE_TRANSFER_BUF_SIZE];
	int		r;
	int		percent;
	int		size;
	static int s = 0;
	double current = Sys_DoubleTime();
	static double	time;

	if ((!cls.is_file && !cls.mem_upload) || (cls.is_file && !cls.upload))
		return;

	r = min(cls.upload_size - cls.upload_pos, (int)sizeof(buffer));
	MSG_WriteByte (&cls.netchan.message, clc_upload);
	MSG_WriteShort (&cls.netchan.message, r);

	if (cls.is_file)
	{
		if ((int)fread(buffer, 1, r, cls.upload) != r)
		{
			Com_Printf("Error reading the upload file\n");
			CL_StopUpload();
			return;
		}
	} 
	else 
	{
		memcpy(buffer, cls.mem_upload + cls.upload_pos, r);
	}
	cls.upload_pos += r;
	size = cls.upload_size ? cls.upload_size : 1;
	percent = cls.upload_pos * 100 / size;
	cls.uploadpercent = percent;
	MSG_WriteByte (&cls.netchan.message, percent);
	SZ_Write (&cls.netchan.message, buffer, r);

	Com_DPrintf ("UPLOAD: %6d: %d written\n", cls.upload_pos - r, r);

	s += r;
	if (current - time > 1) 
	{
		cls.uploadrate = s/(1024*(current - time));
		time = current;
		s = 0;
	}
	
	if (cls.upload_pos != cls.upload_size)
		return;

	Com_Printf ("Upload completed\n");

	if (cls.is_file) 
	{
		fclose (cls.upload);
		cls.upload = NULL;
	} 
	else 
	{
		Q_free(cls.mem_upload);
		cls.mem_upload = 0;
	}
	cls.upload_pos = 0;
	cls.upload_size = 0;
}

void CL_StartUpload (byte *data, int size)
{
	if (cls.state < ca_onserver)
		return; // gotta be connected

	cls.is_file = false;

	// override
	if (cls.mem_upload)
		Q_free(cls.mem_upload);
	cls.mem_upload = (byte *) Q_malloc (size);
	memcpy(cls.mem_upload, data, size);
	cls.upload_size = size;
	cls.upload_pos = 0;
	Com_Printf ("Upload starting of %d...\n", cls.upload_size);

	CL_NextUpload();
}

static void ReplaceChar(char *s, char from, char to)
{
	if (s)
	{
		for ( ;*s ; ++s)
		{
			if (*s == from)
				*s = to;
		}
	}
}

void CL_StartFileUpload (void)
{
	char *name;
	int i;


	if (cls.state < ca_onserver) 
	{
		Com_Printf("must be connected\n");
		return;
	}

	name = Cmd_Argv (2);

	ReplaceChar(name, '\\', '/');

	if (
//		TODO: split name to pathname and filename
//		and check for 'bad symbols' only in pathname
		*name == '/' //no absolute
		|| !strncmp(name, "../", 3) // no leading ../
		|| strstr (name, "/../") // no /../
		|| ((i = strlen(name)) < 3 ? 0 : !strncmp(name + i - 3, "/..", 4)) // no /.. at end
		|| *name == '.' //relative is pointless
		|| ((i = strlen(name)) < 4 ? 0 : !strncasecmp(name+i-4,".log",4)) // no logs
#ifdef _WIN32
		// no leading X:
		|| ( name[1] == ':' && ((*name >= 'a' && *name <= 'z') ||
			(*name >= 'A' && *name <= 'Z')) )
#endif //_WIN32
	) 
	{
		Com_Printf ("File upload: bad path/name \"%s\"", name);
		return;
	}

	cls.is_file = true;

	// override
	if (cls.upload) 
	{
		fclose(cls.upload);
		cls.upload = NULL;
	}

	strlcpy(cls.uploadname, Cmd_Argv(2), sizeof(cls.uploadname));
	cls.upload = fopen(cls.uploadname, "rb"); // BINARY

	if (!cls.upload)
	{
		Com_Printf ("Bad file \"%s\"\n", cls.uploadname);
		return;
	}

	cls.upload_size = FS_FileLength(cls.upload);
	cls.upload_pos = 0;

	Com_Printf ("Upload starting: %s (%d bytes)...\n", cls.uploadname, cls.upload_size);

	CL_NextUpload();
}

qbool CL_IsUploading(void)
{
	if ((!cls.is_file && cls.mem_upload) || (cls.is_file && cls.upload))
		return true;
	return false;
}

void CL_StopUpload(void)
{
	if (cls.is_file) 
	{
		if (cls.upload) 
		{
			fclose(cls.upload);
			cls.upload = NULL;
		}
	} 
	else 
	{
		if (cls.mem_upload) 
		{
			Q_free(cls.mem_upload);
			cls.mem_upload = NULL;
		}
	}
}

/*
=====================================================================
  SERVER CONNECTING MESSAGES
=====================================================================
*/

void CL_ParseServerData (void) 
{
	char *str, fn[MAX_OSPATH];
	FILE *f;
	qbool cflag = false;
	int i, protover;

	Com_DPrintf("Serverdata packet received.\n");

	// wipe the clientState_t struct
	CL_ClearState();

	// parse protocol version number
	// allow 2.2 and 2.29 demos to play
#ifdef PROTOCOL_VERSION_FTE
	cls.fteprotocolextensions = 0;
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
	cls.fteprotocolextensions2 = 0;
#endif // PROTOCOL_VERSION_FTE2

	for(;;)
	{
		protover = MSG_ReadLong ();
#ifdef PROTOCOL_VERSION_FTE
		if (protover == PROTOCOL_VERSION_FTE)
		{
			cls.fteprotocolextensions = MSG_ReadLong();
			Com_DPrintf ("Using FTE extensions 0x%x\n", cls.fteprotocolextensions);
			continue;
		}
#endif

#ifdef PROTOCOL_VERSION_FTE2
		if (protover == PROTOCOL_VERSION_FTE2)
		{
			cls.fteprotocolextensions2 = MSG_ReadLong();
			Com_DPrintf ("Using FTE extensions2 0x%x\n", cls.fteprotocolextensions2);
			continue;
		}
#endif

		if (protover == PROTOCOL_VERSION) //this ends the version info
			break;
		if (cls.demoplayback && protover >= 24 && protover <= 28)	//older versions, maintain demo compatability.
			break;
		Host_Error ("Server returned version %i, not %i\nYou probably need to upgrade.\nCheck http://www.quakeworld.net/", protover, PROTOCOL_VERSION);
	}

#ifdef FTE_PEXT_FLOATCOORDS
	if (cls.fteprotocolextensions & FTE_PEXT_FLOATCOORDS)
	{
		if (!com_serveractive)
		{
			msg_coordsize = 4;
			msg_anglesize = 2;
		}
	}
	else
	{
		if (!com_serveractive)
		{
			msg_coordsize = 2;
			msg_anglesize = 1;
		}
	}
#endif

	cl.protoversion = protover;
	cl.servercount = MSG_ReadLong ();

	// game directory
	str = MSG_ReadString ();

	if (!strcmp(str, "")  || !strcmp(str, ".") || strstr(str, "..") || strstr(str, "//") || strchr(str, '\\') || strchr(str, ':')  || str[0] == '/') {
		Host_Error("Server reported invalid gamedir!\n");
	}

	cl.teamfortress = !strcasecmp(str, "fortress");
	if (cl.teamfortress) 
	{
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

	if (strcasecmp(cls.gamedirfile, str)) 
	{
		strlcpy (cls.gamedirfile, str, sizeof(cls.gamedirfile));
		snprintf (cls.gamedir, sizeof(cls.gamedir),
			"%s/%s", com_basedir, cls.gamedirfile);
		cflag = true;
	}

	if (!com_serveractive)
		FS_SetGamedir (str);

	if (cfg_legacy_exec.value && (cflag || cfg_legacy_exec.value >= 2)) 
	{
		snprintf (fn, sizeof(fn), "%s/%s", cls.gamedir, "config.cfg");
		Cbuf_AddText ("cl_warncmd 0\n");
		if ((f = fopen(fn, "r")) != NULL) 
		{
			fclose(f);
			if (!strcmp(cls.gamedirfile, com_gamedirfile))
				Cbuf_AddText ("exec config.cfg\n");
			else
				Cbuf_AddText (va("exec ../%s/config.cfg\n", cls.gamedirfile));
		} 
		else if (cfg_legacy_exec.value == 3 && strcmp(cls.gamedir, "qw"))
		{
			snprintf (fn, sizeof(fn), "qw/%s", "config.cfg");
			if ((f = fopen(fn, "r")) != NULL) 
			{
				fclose(f);
				Cbuf_AddText ("exec config.cfg\n");
			}
		}
		snprintf (fn, sizeof(fn), "%s/%s", cls.gamedir, "frontend.cfg");
		if ((f = fopen(fn, "r")) != NULL) 
		{
			fclose(f);
			if (!strcmp(cls.gamedirfile, com_gamedirfile))
				Cbuf_AddText ("exec frontend.cfg\n");
			else
				Cbuf_AddText (va("exec ../%s/frontend.cfg\n", cls.gamedirfile));
		} 
		else if (cfg_legacy_exec.value == 3 && strcmp(cls.gamedir, "qw"))
		{
			snprintf (fn, sizeof(fn), "qw/%s", "frontend.cfg");
			if ((f = fopen(fn, "r")) != NULL) 
			{
				fclose(f);
				Cbuf_AddText ("exec frontend.cfg\n");
			}
		}
		Cbuf_AddText ("cl_warncmd 1\n");
	}


	// parse player slot, high bit means spectator
	if (cls.mvdplayback)
	{
		cls.netchan.last_received = nextdemotime = olddemotime = MSG_ReadFloat();
		cl.playernum = MAX_CLIENTS - 1;
		cl.spectator = true;
		for (i = 0; i < UPDATE_BACKUP; i++)
			cl.frames[i].playerstate[cl.playernum].pm_type = PM_SPECTATOR;
	}
	else 
	{
		cl.playernum = MSG_ReadByte ();
		if (cl.playernum & 128) 
		{
			cl.spectator = true;
			cl.playernum &= ~128;
		}
	}

	// get the full level name
	str = MSG_ReadString ();
	strlcpy (cl.levelname, str, sizeof(cl.levelname));

	// get the movevars
	if (cl.protoversion >= 25) 
	{
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
	}
	else
	{
		// These seem to be the defaults for standard QW... 
		//   if there are mods with other defaults, we could 
		//   set accordingly to give best experience?
		movevars.gravity = 800.0f;
		movevars.stopspeed = 100.0f;
		cl.maxspeed = 320.0f;
		movevars.spectatormaxspeed = 500.0f;
		movevars.accelerate = 10.0f;
		movevars.airaccelerate = 0.7f;
		movevars.wateraccelerate = 10.0f;
		movevars.friction = 6.0f;
		movevars.waterfriction = 1.0f;
		cl.entgravity = 1.0f;
	}

	// separate the printfs so the server message can have a color
	if (!cls.demoseeking) {
		Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);
	}

	// ask for the sound list next
	memset(cl.sound_name, 0, sizeof(cl.sound_name));

	if (cls.mvdplayback == QTV_PLAYBACK) 
	{
		cls.qtv_donotbuffer = true; // do not try buffering before "skins" not received
		QTV_Cmd_Printf(QTV_EZQUAKE_EXT_DOWNLOAD, "qtvsoundlist %i %i", cl.servercount, 0);
	}
	else 
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("soundlist %i %i", cl.servercount, 0));
	}

	// now waiting for downloads, etc
	cls.state = ca_onserver;
}

void CL_ParseSoundlist (void)
{
	int	numsounds, n;
	char *str;

	// precache sounds
	// memset (cl.sound_precache, 0, sizeof(cl.sound_precache));

	if (cl.protoversion >= 26) 
	{
		numsounds = MSG_ReadByte();

		while (1) 
		{
			str = MSG_ReadString ();
			if (!str[0])
				break;
			numsounds++;
			if (numsounds == MAX_SOUNDS)
				Host_Error ("Server sent too many sound_precache");
			if (str[0] == '/')
				str++; // hexum -> fixup server error (submitted by empezar bug #1026106)
			strlcpy (cl.sound_name[numsounds], str, sizeof(cl.sound_name[numsounds]));
		}

		n = MSG_ReadByte();

		if (n) 
		{
			if (cls.mvdplayback == QTV_PLAYBACK) 
			{
				// none
			}
			else 
			{
				MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
				MSG_WriteString(&cls.netchan.message, va("soundlist %i %i", cl.servercount, n));
			}

			return;
		}
	}
	else 
	{
		// Taken from http://www.quakewiki.net/archives/demospecs/qwd/qwd.html#AEN562
		numsounds = 0;
		do {
			if (++numsounds > 255)
				Host_Error ("Server sent too many sound_precache");
			str = MSG_ReadString ();
			strlcpy (cl.sound_name[numsounds], str, sizeof(cl.sound_name[numsounds]));
		} while (*str);
	}

	// AFTER we got the soundlist, request the modellist (older FTE servers will send it right away anyway)
	// done with sounds, request models now
	memset (cl.model_precache, 0, sizeof(cl.model_precache));

	if (cls.mvdplayback == QTV_PLAYBACK)
	{
		QTV_Cmd_Printf(QTV_EZQUAKE_EXT_DOWNLOAD, "qtvmodellist %i %i", cl.servercount, 0);
	}
	else 
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, 0));
	}
}

void CL_ParseModellist (qbool extended)
{
	int	nummodels, n;
	char *str;

	if (cl.protoversion >= 26)
	{
		// Precache models and note certain default indexes
		nummodels = (extended) ? (unsigned) MSG_ReadShort () : MSG_ReadByte ();

		while (1) 
		{
			str = MSG_ReadString ();
			if (!str[0])
				break;

			#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_MODELDBL)
			nummodels++;
			if (nummodels >= MAX_MODELS) //Spike: tweeked this, we still complain if the server exceeds the standard limit without using extensions.
				Host_Error ("Server sent too many model_precache");
		
			if (nummodels >= 256 && !(cls.fteprotocolextensions & FTE_PEXT_MODELDBL))
			#else
			if (++nummodels == MAX_MODELS)
			#endif // PROTOCOL_VERSION_FTE
			{
				Host_Error ("Server sent too many model_precache");
			}

			if (str[0] == '/')
				str++; // hexum -> fixup server error (submitted by empezar bug #1026106)
			strlcpy (cl.model_name[nummodels], str, sizeof(cl.model_name[nummodels]));

			if (nummodels == 1)
				if (!com_serveractive) 
				{
					char mapname[MAX_QPATH];
					COM_StripExtension (COM_SkipPath(cl.model_name[1]), mapname, sizeof(mapname));
					Cvar_ForceSet (&host_mapname, mapname);
				}
		}

		if ((n = MSG_ReadByte())) 
		{
			if (cls.mvdplayback == QTV_PLAYBACK) 
			{
				// none
			}
			else 
			{
				MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			
				#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_MODELDBL)
				MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, (nummodels&0xff00)+n));
				#else
				MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, n));
				#endif // PROTOCOL_VERSION_FTE
			}

			return;
		}
	}
	else 
	{
		// Up to 2.1 (taken from http://www.quakewiki.net/archives/demospecs/qwd/qwd.html#AEN562)
		nummodels = 0;
		do 
		{
			if (++nummodels > 255)
				Host_Error ("Server sent too many model_precache\n");
			str = MSG_ReadString ();
			strlcpy (cl.model_name[nummodels], str, sizeof(cl.model_name[nummodels]));

			if (nummodels == 1)
				if (!com_serveractive) 
				{
					char mapname[MAX_QPATH];
					COM_StripExtension (COM_SkipPath(cl.model_name[1]), mapname, sizeof(mapname));
					Cvar_ForceSet (&host_mapname, mapname);
				}
		} while (*str);
	}
	// We now get here after having received both soundlist and modellist, but no
	// sounds have been downloaded yet, we must do that first, when that is finished
	// it will call for model download 
	// FIXME Implement a download queue later on and thus break this long chain of events
	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;
	Sound_NextDownload();
}

void CL_ParseBaseline (entity_state_t *es)
{
	int	i;

	es->modelindex = MSG_ReadByte ();
	es->frame = MSG_ReadByte ();
	es->colormap = MSG_ReadByte();
	es->skinnum = MSG_ReadByte();

	for (i = 0; i < 3; i++) 
	{
		es->origin[i] = MSG_ReadCoord ();
		es->angles[i] = MSG_ReadAngle ();
	}
}

// An easy way to keep compatability with other entity extensions
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_SPAWNSTATIC2)
extern void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits);

static void CL_ParseSpawnBaseline2 (void)
{
	entity_state_t nullst, es;

	if (!(cls.fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2)) 
	{
		Com_Printf ("illegible server message\nsvc_fte_spawnbaseline2 (%i) without FTE_PEXT_SPAWNSTATIC2\n", svc_fte_spawnbaseline2);
		Host_EndGame();
	}

	memset(&nullst, 0, sizeof (entity_state_t));
	memset(&es, 0, sizeof (entity_state_t));

	CL_ParseDelta(&nullst, &es, MSG_ReadShort());
	memcpy(&cl_entities[es.number].baseline, &es, sizeof(es));
}
#endif

// Static entities are non-interactive world objects like torches
void CL_ParseStatic (qbool extended)
{
	entity_t *ent;
	entity_state_t es;

	if (extended)
	{
		entity_state_t nullst;
		memset (&nullst, 0, sizeof(entity_state_t));

		CL_ParseDelta (&nullst, &es, MSG_ReadShort());
	} 
	else
	{
		CL_ParseBaseline (&es);
	}

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl.num_statics];
	cl.num_statics++;

	// Copy it to the current state
	ent->model = cl.model_precache[es.modelindex];
	ent->frame = es.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = es.skinnum;

	VectorCopy (es.origin, ent->origin);
	VectorCopy (es.angles, ent->angles);

	R_AddEfrags (ent);
}

// Clientside autogen of entitys,
// usefull for eyecandy and other stuff
// somewhat evile
// unfinished
void CL_GenStatic (vec3_t origin) 
{
	entity_t *ent;

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl.num_statics];
	cl.num_statics++;

	//TODO: load the correct model
	//ent->model = cl.model_precache[copy->v.modelindex];
	ent->model = Mod_ForName("progs/flame2.mdl",false);//cl.model_precache[1];

	VectorCopy (origin, ent->origin);

	R_AddEfrags (ent);
}

void CL_ParseStaticSound (void)
{
	extern cvar_t cl_staticsounds;
	static_sound_t ss;
	int i;

	for (i = 0; i < 3; i++)
		ss.org[i] = MSG_ReadCoord();

	ss.sound_num = MSG_ReadByte();
	ss.vol = MSG_ReadByte();
	ss.atten = MSG_ReadByte();

	if (cl.num_static_sounds < MAX_STATIC_SOUNDS) 
	{
		cl.static_sounds[cl.num_static_sounds] = ss;
		cl.num_static_sounds++;
	}

	if ((int) cl_staticsounds.value)
		S_StaticSound(cl.sound_precache[ss.sound_num], ss.org, ss.vol, ss.atten);
}

/*
=====================================================================
ACTION MESSAGES
=====================================================================
*/

void CL_ParseStartSoundPacket(void)
{
    vec3_t pos;
    int channel, ent, sound_num, volume, i;
	int tracknum;
    float attenuation;

    channel = MSG_ReadShort();

	volume = (channel & SND_VOLUME) ? MSG_ReadByte() : DEFAULT_SOUND_PACKET_VOLUME;

	attenuation = (channel & SND_ATTENUATION) ? MSG_ReadByte() / 64.0 : DEFAULT_SOUND_PACKET_ATTENUATION;

	sound_num = MSG_ReadByte();

	for (i = 0; i < 3; i++)
		pos[i] = MSG_ReadCoord();

	ent = (channel >> 3) & 1023;
	channel &= 7;

	if (ent > CL_MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	// MVD Playback
    if (cls.mvdplayback)
	{
	    tracknum = Cam_TrackNum();

	    if (cl.spectator && tracknum != -1 && ent == tracknum + 1 && cl_multiview.value < 2)
		{
		    ent = cl.playernum + 1;
		}
    }

	if (CL_Demo_SkipMessage())
		return;

    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);

	if (ent == cl.playernum+1)
		TP_CheckPickupSound (cl.sound_name[sound_num], pos);
}

// Server information pertaining to this client only, sent every frame
void CL_ParseClientdata (void) 
{
	int newparsecount, i;
	float latency;
	frame_t *frame;

	// calculate simulated time of message
	newparsecount = cls.netchan.incoming_acknowledged;

	cl.oldparsecount = (cls.mvdplayback) ? newparsecount - 1 : cl.parsecount;
	cl.parsecount = newparsecount;
	parsecountmod = (cl.parsecount & UPDATE_MASK);
	frame = &cl.frames[parsecountmod];

	frame->receivedtime = cls.realtime;
	if (cls.mvdplayback) {
		frame->senttime = cls.realtime - cls.frametime;
	}
	else if (cls.demoplayback) {
		frame->receivedtime = cls.demopackettime;
	}

	parsecounttime = cl.frames[parsecountmod].senttime;

	frame->seq_when_received = cls.netchan.outgoing_sequence;

	frame->receivedsize = net_message.cursize;

	// update network stats table
	i = cls.netchan.incoming_sequence & NETWORK_STATS_MASK;
	network_stats[i].receivedtime = cls.realtime;
	network_stats[i].receivedsize = net_message.cursize;
	network_stats[i].seq_diff = cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence;

	// calculate latency
	latency = frame->receivedtime - frame->senttime;

	if (latency >= 0 && latency <= 1) 
	{
		// drift the average latency towards the observed latency
		if (latency <= cls.latency)
			cls.latency = latency;
		else
			cls.latency += min(0.001, latency - cls.latency);	// drift up, so correction is needed
	}
}

void CL_NewTranslation (int slot)
{
	player_info_t *player;

	if (cls.state < ca_connected)
	{
		return;
	}

	if (slot >= MAX_CLIENTS)
	{
		Sys_Error ("CL_NewTranslation: slot >= MAX_CLIENTS");
	}

	player = &cl.players[slot];
	if (!player->name[0] || player->spectator)
	{
		return;
	}

	player->topcolor	= player->real_topcolor;
	player->bottomcolor = player->real_bottomcolor;

	skinforcing_team = TP_SkinForcingTeam();

	if (!cl.teamfortress && !(cl.fpd & FPD_NO_FORCE_COLOR))
	{
		qbool lockedTeams = TP_TeamLockSpecified();
		qbool teammate = false;

		// it's me or it's teamplay and he's my teammate
		if (cl.spectator && slot == spec_track && !lockedTeams) {
			teammate = true;
		}
		else if (!cl.spectator && slot == cl.playernum) {
			teammate = true;
		}
		else if (cl.teamplay && !strcmp(player->team, skinforcing_team)) {
			teammate = true;
		}

		if (teammate)
		{
			if (cl_teamtopcolor.integer != -1)
				player->topcolor = cl_teamtopcolor.value;
			if (cl_teambottomcolor.integer != -1)
				player->bottomcolor = cl_teambottomcolor.value;
		}
		else if (slot != cl.playernum)
		{
			if (cl_enemytopcolor.integer != -1)
				player->topcolor = cl_enemytopcolor.value;
			if (cl_enemybottomcolor.integer != -1)
				player->bottomcolor = cl_enemybottomcolor.value;
		}
	}

	R_TranslatePlayerSkin(slot);
}

void CL_ProcessUserInfo(int slot, player_info_t *player, char *key)
{
	extern cvar_t cl_name_as_skin;
	qbool update_skin;
	int mynum;

	strlcpy(player->name, Info_ValueForKey(player->userinfo, "name"), sizeof(player->name));

	if (!player->name[0] && player->userid && strlen(player->userinfo) >= MAX_INFO_STRING - 17) {
		// Somebody's trying to hide himself by overloading userinfo.
		strlcpy(player->name, " ", sizeof(player->name));
	}

	player->real_topcolor = atoi(Info_ValueForKey(player->userinfo, "topcolor"));
	player->real_bottomcolor = atoi(Info_ValueForKey(player->userinfo, "bottomcolor"));

	strlcpy(player->team, Info_ValueForKey(player->userinfo, "team"), sizeof(player->team));

	if (atoi(Info_ValueForKey(player->userinfo, "*spectator"))) {
		player->spectator = true;
	} else {
		player->spectator = false;
	}

	if (slot == cl.playernum && player->name[0]) {
		if (cl.spectator != player->spectator) {
			cl.spectator = player->spectator;
			TP_RefreshSkins();
		}
	}

	Sbar_Changed();

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1) {
		mynum = cl.playernum;
	}

	update_skin = !key || (!player->spectator && (!strcmp(key, "skin") || !strcmp(key, "topcolor") ||
				!strcmp(key, "bottomcolor") || !strcmp(key, "team") ||
				(!strcmp(key, "name") && cl_name_as_skin.value)));

	if (slot == mynum && TP_NeedRefreshSkins() && strcmp(player->team, player->_team)) {
		TP_RefreshSkins();
	} else if (update_skin) {
		TP_RefreshSkin(slot);
	}

	strlcpy(player->_team, player->team, sizeof (player->_team));
}

void CL_NotifyOnFull(void)
{
	if (!cl.spectator && !cls.demoplayback) {
		int limit = Q_atoi(Info_ValueForKey(cl.serverinfo, "maxclients"));
		int players = 0;
		int i;

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (*cl.players[i].name && !cl.players[i].spectator) {
				players++;
			}
		}

		if (players >= limit) {
			VID_NotifyActivity();
		}
	}
}

void CL_PlayerEnterSlot(player_info_t *player) 
{
	extern player_state_t oldplayerstates[MAX_CLIENTS];

	player->ignored = player->validated = false;
	player->f_server[0] = 0;
	memset(&oldplayerstates[player - cl.players], 0, sizeof(player_state_t));
	Stats_EnterSlot(player - cl.players);
	CL_NotifyOnFull();
}

void CL_PlayerLeaveSlot(player_info_t *player) 
{
	return;
}

void CL_UpdateUserinfo (void) 
{
	qbool was_empty_slot;
	int slot;
	player_info_t *player;

	slot = MSG_ReadByte();
	if (slot >= MAX_CLIENTS)
		Host_Error ("CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS");

	player = &cl.players[slot];

	was_empty_slot = player->name[0] ? false : true;

	player->userid = MSG_ReadLong();
	strlcpy (player->userinfo, MSG_ReadString(), sizeof(player->userinfo));

	CL_ProcessUserInfo(slot, player, NULL);

	if (player->name[0] && was_empty_slot) {
		CL_PlayerEnterSlot(player);
		MVD_Init_Info(slot);
	}
	else if (!player->name[0] && !was_empty_slot) {
		CL_PlayerLeaveSlot(player);
		MVD_Init_Info(slot);
	}
}

void CL_SetInfo (void) 
{
	int	slot;
	player_info_t *player;
	char key[MAX_INFO_STRING], value[MAX_INFO_STRING];

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_Error ("CL_ParseServerMessage: svc_setinfo > MAX_CLIENTS");

	player = &cl.players[slot];

	strlcpy(key, MSG_ReadString(), sizeof(key));
	strlcpy(value, MSG_ReadString(), sizeof(value));

	if (!cl.teamfortress)	// don't allow cheating in TF
		Com_DPrintf ("SETINFO %s: %s=%s\n", player->name, key, value);

	Info_SetValueForStarKey (player->userinfo, key, value, MAX_INFO_STRING);

	CL_ProcessUserInfo (slot, player, key);
}

/*
 * qqshka: Well, "cmd pext" slightly broken in MVDSV 0.30. I have to fix it somehow.
 */
static void CL_PEXT_Fix(void)
{
	char version_bugged[] = "MVDSV 0.30";
	char *version = Info_ValueForKey(cl.serverinfo, "*version");

	if (!strncmp(version, version_bugged, sizeof(version_bugged)-1))
	{
		// MVDSV 0.30 support maximum this FTE extensions.
#ifdef PROTOCOL_VERSION_FTE
		{
			unsigned int ext = 0;
#ifdef FTE_PEXT_ACCURATETIMINGS
			ext |= FTE_PEXT_ACCURATETIMINGS;
#endif
#ifdef FTE_PEXT_256PACKETENTITIES
			ext |= FTE_PEXT_256PACKETENTITIES;
#endif
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
			ext |= FTE_PEXT_CHUNKEDDOWNLOADS;
#endif
			cls.fteprotocolextensions &= ext;
		}
#endif // PROTOCOL_VERSION_FTE

		// MVDSV 0.30 support maximum this FTE extensions2.
#ifdef PROTOCOL_VERSION_FTE2
		{
			unsigned int ext = 0;
			cls.fteprotocolextensions2 &= ext;
		}
#endif // PROTOCOL_VERSION_FTE2

#ifdef FTE_PEXT_FLOATCOORDS
		if (!com_serveractive)
		{
			if (msg_coordsize != 2 || msg_anglesize != 1)
			{
				Com_DPrintf("Fixing FTE_PEXT_FLOATCOORDS!\n");

				msg_coordsize = 2;
				msg_anglesize = 1;
			}
		}
#endif
	}
}

// Called by CL_FullServerinfo_f and CL_ParseServerInfoChange
void CL_ProcessServerInfo (void) 
{
	char *p, *minlight;
	int new_teamplay, newfpd;
	qbool skin_refresh, standby, countdown;

	// fix broken PEXT.
	CL_PEXT_Fix(); // must be called once from CL_FullServerinfo_f() but should be ok here too.

	// game type (sbar code checks it) (GAME_DEATHMATCH default)
	cl.gametype = *(p = Info_ValueForKey(cl.serverinfo, "deathmatch")) ? (atoi(p) ? GAME_DEATHMATCH : GAME_COOP) : GAME_DEATHMATCH;

	// server side fps restriction
	cl.maxfps = Q_atof(Info_ValueForKey(cl.serverinfo, "maxfps"));

	newfpd = cls.demoplayback ? 0 : atoi(Info_ValueForKey(cl.serverinfo, "fpd"));

	p = Info_ValueForKey(cl.serverinfo, "status");
	standby = !strcasecmp(p, "standby");
	countdown = !strcasecmp(p, "countdown");

	if ((cl.standby || cl.countdown) && !(standby || countdown)) 
	{
		cl.gametime = 0;
		cl.gamestarttime = Sys_DoubleTime();
	}

	cl.standby = standby;
	cl.countdown = countdown;

	cl.minlight = (strlen(minlight = Info_ValueForKey(cl.serverinfo, "minlight")) ? bound(0, Q_atoi(minlight), 255) : 4);

	// Get the server's ZQuake extension bits
	cl.z_ext = atoi(Info_ValueForKey(cl.serverinfo, "*z_ext"));

	// Initialize cl.maxpitch & cl.minpitch
	p = (cl.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "maxpitch") : "";
	cl.maxpitch = *p ? Q_atof(p) : 80.0f;
	p = (cl.z_ext & Z_EXT_PITCHLIMITS) ? Info_ValueForKey (cl.serverinfo, "minpitch") : "";
	cl.minpitch = *p ? Q_atof(p) : -70.0f;

	// movement vars for prediction
	cl.bunnyspeedcap = Q_atof(Info_ValueForKey(cl.serverinfo, "pm_bunnyspeedcap"));
	movevars.slidefix = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_slidefix")) != 0);
	movevars.airstep = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_airstep")) != 0);
	movevars.pground = (Q_atof(Info_ValueForKey(cl.serverinfo, "pm_pground")) != 0)
		&& (cl.z_ext & Z_EXT_PF_ONGROUND) /* pground doesn't make sense without this */;
	movevars.ktjump = *(p = Info_ValueForKey(cl.serverinfo, "pm_ktjump")) ?
		Q_atof(p) : cl.teamfortress ? 0 : 1;

	// Deathmatch and teamplay.
	cl.deathmatch = atoi(Info_ValueForKey(cl.serverinfo, "deathmatch"));
	new_teamplay = atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));

	// Timelimit and fraglimit.
	cl.timelimit = atoi(Info_ValueForKey(cl.serverinfo, "timelimit"));
	cl.fraglimit = atoi(Info_ValueForKey(cl.serverinfo, "fraglimit"));

	// Update skins if needed.
	skin_refresh = ( !new_teamplay != !cl.teamplay || ( (newfpd ^ cl.fpd) & (FPD_NO_FORCE_COLOR|FPD_NO_FORCE_SKIN) ) );
	cl.teamplay = new_teamplay;
	cl.fpd = newfpd;
	if (skin_refresh)
		TP_RefreshSkins();
}

// Parse a string looking like this: //vwep vwplayer w_axe w_shot w_shot2
void CL_ParseVWepPrecache (char *str)
{
	int i, num;
	const char *p;

	Cmd_TokenizeString (str + 2 /* skip the // */);

	if (Cmd_Argc() < 2)
	{
		cl.vwep_enabled = 0;
		return;
	}

	if (cls.state == ca_active)
	{
		// vweps can be turned off on the fly, but not back on
		Com_DPrintf ("CL_ParseVWepPrecache: ca_active, ignoring\n");
		return;
	}

	num = min(Cmd_Argc()-1, MAX_VWEP_MODELS);

	for (i = 0; i < num; i++)
	{
		p = Cmd_Argv(i+1);

		if (!strcmp(p, "-")) 
		{
			// empty model
			strlcpy (cl.vw_model_name[i], "-", MAX_QPATH);
		}
		else 
		{
			if (strstr(p, "..") || p[0] == '/' || p[0] == '\\')
				Host_Error("CL_ParseVWepPrecache: illegal model name '%s'", p);

			if (strstr(p, "/"))
			{
				// A full path was specified.
				strlcpy(cl.vw_model_name[i], p, sizeof(cl.vw_model_name[0]));
			}
			else 
			{
				// Use default path.
				strlcpy(cl.vw_model_name[i], "progs/", sizeof(cl.vw_model_name[0]));
				strlcat(cl.vw_model_name[i], p, sizeof(cl.vw_model_name[0]));
			}

			// Use default extension if not specified.
			if (!strstr(p, "."))
				strlcat(cl.vw_model_name[i], ".mdl", sizeof(cl.vw_model_name[0]));
		}
	}

	Com_DPrintf ("VWEP precache: %i models\n", num);
}

void CL_ParseServerInfoChange (void) 
{
	char key[MAX_INFO_STRING], value[MAX_INFO_STRING];

	strlcpy (key, MSG_ReadString(), sizeof(key));
	strlcpy (value, MSG_ReadString(), sizeof(value));

	Com_DPrintf ("SERVERINFO: %s=%s\n", key, value);
	if (!cl.standby && !cl.countdown && !strncmp(key, "status", 6)) 
	{
		int timeleft = atoi(value) * 60;
		if (timeleft) 
		{
			if (cls.demoplayback) 
			{
				cl.gametime = (cl.timelimit * 60) - timeleft;
			}
			else 
			{
				cl.gamestarttime = Sys_DoubleTime() - (cl.timelimit * 60) + timeleft - cl.gamepausetime;
				Com_DPrintf("Clock sync, match started %i seconds ago\n", (cl.timelimit*60) + atoi(value)*60);
			}
		}
	}

	Info_SetValueForKey (cl.serverinfo, key, value, MAX_SERVERINFO_STRING);

	CL_ProcessServerInfo ();
}

// Converts quake color 4 to "&cf00", 13 to "&c00f", 12 to "&cff0" and so on
// those are marks used to make colored text in console
char *CL_Color2ConColor(int color)
{
	static char buf[6] = "&c000";
	const char hexc[] = "0123456789abcdefffffffffffffffff";
	// For some reason my palette doesn't give higher number then 127, those Fs are there for palettes that give higher than that
	int x = Sbar_ColorForMap(color);
	buf[2] = hexc[host_basepal[x * 3 + 0] / 8];
	buf[3] = hexc[host_basepal[x * 3 + 1] / 8];
	buf[4] = hexc[host_basepal[x * 3 + 2] / 8];
	return buf;
}

// Will add colors to nicks in "ParadokS rides JohnNy_cz's rocket"
// source - source frag message, dest - destination buffer, destlen - length of buffer
// cff - see the cfrags_format definition 
static wchar* CL_ColorizeFragMessage (const wchar *source, cfrags_format *cff)
{
	static wchar dest_buf[2048] = {0};
	wchar col1[6+1] = {0}, col2[6+1] = {0}, *dest = dest_buf, *col_off;
	int destlen = sizeof(dest_buf)/sizeof(dest_buf[0]), len;

	dest[0] = 0; // new string

	qwcslcpy(col1, str2wcs(CL_Color2ConColor(cff->p1col)), sizeof(col1)/sizeof(wchar));
	qwcslcpy(col2, str2wcs(CL_Color2ConColor(cff->p2col)), sizeof(col2)/sizeof(wchar));

	// before 1st nick
	qwcslcpy(dest, source, bound(0, cff->p1pos + 1, destlen));
	destlen -= (len = qwcslen(dest));
	dest += len;
	
	// color1
	len = qwcslen(col1) + 1;
	qwcslcpy(dest, col1, bound(0, len, destlen));
	destlen -= (len = qwcslen(dest));
	dest += len;
	// 1st nick
	qwcslcpy(dest, source + cff->p1pos, bound(0, cff->p1len + 1, destlen));
	destlen -= (len = qwcslen(dest));
	dest += len;
	// color off
	len = qwcslen(col_off = str2wcs("&cfff")) + 1;
	qwcslcpy(dest, col_off, bound(0, len, destlen));
	destlen -= (len = qwcslen(dest));
	dest += len;

	if (cff->p2len)
	{
		// middle part
		qwcslcpy(dest, source + cff->p1pos + cff->p1len, bound(0, cff->p2pos - cff->p1len - cff->p1pos + 1, destlen));
		destlen -= (len = qwcslen(dest));
		dest += len;
		// color2
		len = qwcslen(col2) + 1;
		qwcslcpy(dest, col2, bound(0, len, destlen));
		destlen -= (len = qwcslen(dest));
		dest += len;
		// 2nd nick
		qwcslcpy(dest, source + cff->p2pos, bound(0, cff->p2len + 1, destlen));
		destlen -= (len = qwcslen(dest));
		dest += len;
		// color off
		len = qwcslen(col_off = str2wcs("&cfff")) + 1;
		qwcslcpy(dest, col_off, bound(0, len, destlen));
		destlen -= (len = qwcslen(dest));
		dest += len;
		// the rest
		qwcslcpy(dest, source + cff->p2pos + cff->p2len, destlen);
	}
	else 
	{
		// the rest
		qwcslcpy(dest, source + cff->p1pos + cff->p1len, destlen);
	}

	return dest_buf;
}

extern cvar_t con_highlight, con_highlight_mark, name;
extern cvar_t cl_showFragsMessages;
extern cvar_t scr_coloredfrags;

// For CL_ParsePrint
static void FlushString (const wchar *s, int level, qbool team, int offset) 
{
	char *s0; // C-char copy of s

	char *mark;
	const wchar *text; // needed for con_highlight work
	char *f; 

	wchar zomfg[4096]; // FIXME

	cfrags_format cff = {0, 0, 0, 0, 0, 0, false}; // Stats_ParsePrint stuff

	// During gametime, use notify for team messages only
	if (con_mm2_only.integer && !team && !(cl.standby || cl.countdown))
		Print_flags[Print_current] |= PR_NONOTIFY;

	s0 = wcs2str (s);
	f = strstr (s0, name.string);
	CL_SearchForReTriggers (s0, 1<<level); // re_triggers, s0 not modified

	// Highlighting on && nickname found && it's not our own text (nickname not close to the beginning)
	if (con_highlight.value && f && ((f - s0) > 1)) 
	{
		switch ((int)(con_highlight.value)) 
		{
			case 1:
				mark = "";
				text = s;
				break;
			case 2:
				mark = con_highlight_mark.string;
				text = (level == PRINT_CHAT) ? (const wchar *) TP_ParseWhiteText (s, team, offset) : s;
				break;
			default:
			case 3:
				mark = con_highlight_mark.string;
				text = s;
				break;
		}
	}
	else 
	{
		mark = "";
		text = (level == PRINT_CHAT) ? (const wchar *) TP_ParseWhiteText(s, team, offset) : s;
	}

	// s0 is same after Stats_ParsePrint like before, but Stats_ParsePrint modify it during it's work
	// we can change this function a bit, so s0 can be const char*
	Stats_ParsePrint (s0, level, &cff);

	// Colorize player names here
	if (scr_coloredfrags.value && cff.p1len)
		text = CL_ColorizeFragMessage (text, &cff);

	/* FIXME
	 * disconnect: There should be something like Com_PrintfW...
	 * we have to *Print* message with one Con_Printf/Con_PrintfW call
	 * else re_triggers with remove option will be fucked
	 * so we have to use additional buffer
	 */
	memset (zomfg, 0, sizeof (zomfg));
	qwcslcpy (zomfg, str2wcs(mark), sizeof (zomfg) / sizeof (wchar));
	qwcslcpy (zomfg + qwcslen (zomfg), text, sizeof (zomfg) / sizeof (wchar) - qwcslen (zomfg));
	if (cl_showFragsMessages.value || !cff.isFragMsg) {
		if (cl_showFragsMessages.integer == 2 && cff.isFragMsg) {
			Print_flags[Print_current] |= PR_NONOTIFY;
		}
		Con_PrintW(zomfg); // FIXME logging
	}

	if (level >= 4)
		return;

	if (team)
		level = 4;

	TP_SearchForMsgTriggers ((const char *)wcs2str(s + offset), level);
}

#define  CHAT_MM1   1
#define  CHAT_MM2   2
#define  CHAT_SPEC  3
int SeparateChat(char *chat, int *out_type, char **out_msg)
{
    int server_cut = 31;    // maximum characters sent in nick by server

    int i;
    int classified = -1;
    int type = 0;
    char *msg=NULL;

	for (i=0; i <= MAX_CLIENTS; i++)
    {
        int client = -1;
        char buf[512];

        if (i == MAX_CLIENTS)
        {
            strlcpy (buf, "console: ", sizeof (buf));

			if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_MM1;
                msg = chat + strlen(buf);
            }
        }
        else
        {
            if (!cl.players[i].name[0])
                continue;

            snprintf(buf,  sizeof (buf), "%.*s: ", server_cut, Info_ValueForKey(cl.players[i].userinfo, "name"));

			if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_MM1;
                msg = chat + strlen(buf);
            }

            snprintf(buf,  sizeof (buf), "(%.*s): ", server_cut, Info_ValueForKey(cl.players[i].userinfo, "name"));

			if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_MM2;
                msg = chat + strlen(buf);
            }

            snprintf(buf,  sizeof (buf), "[SPEC] %.*s: ", server_cut, Info_ValueForKey(cl.players[i].userinfo, "name"));

			if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_SPEC;
                msg = chat + strlen(buf);
            }
        }

        if (client >= 0)
        {
            if (classified < 0)
                classified = client;
            else
                return -1;
        }
    }
    
    if (classified < 0)
        return -1;

    if (out_msg)
        *out_msg = msg;
    if (out_type)
        *out_type = type;

    return classified;
}

// QTV chat formed like #qtv2_id:qtv2_name: #qtv1_id:qtv1_name: #qtvClient_id:qtvClient_name: chat text
// above example was for case qtvClient -> qtv1 -> qtv2 -> server

// So, this fucntion intended for skipping leading proxies names and ids, and just return "qtvClient_name: chat text"

static char *SkipQTVLeadingProxies(char *s)
{
	char *last = NULL; // pointer to "qtvClient_name: chat text" at the end of parsing or NULL if we fail of some kind

	for ( ; s[0]; )
	{
		if (s[0] == '#' && isdigit(s[1])) // check do we have # and at least one digit
		{
			s++; // skip #
    
			while(isdigit(s[0])) // skip digits
				s++;
    
			if (s[0] == ':') // ok, seems it was qtv chat
			{
				char *name;

				s++; // skip :

				name = s; // remember name start

				// skip name
				// well, this code allow empty names... like "#id:: chat text"
				if (s[0] != ' ') // name must NOT start from space, unless someone do "crime"
				{
					while(s[0] && s[0] != ':')
						s++;

					if (s[0] == ':')
					{
						last = name; // yeah, its well formed qtv chat string

						if (!qtv_skipchained.integer)
							break; // user select to not skip chained proxies

						s++; // skip :

						if (s[0] == ' ')
							s++; // skip space

						continue; // probably we have chained qtvs, lets check it
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			else
			{
				break; // seems it wasn't chat
			}
		}

		break;
	}

	return last;
}

extern qbool TP_SuppressMessage (wchar *);
extern cvar_t cl_chatsound, msg_filter;
extern cvar_t ignore_qizmo_spec;

void CL_ProcessPrint (int level, char* s0)
{
	qbool suppress_talksound;
	wchar *s, str[2048], *p, check_flood;
	char *qtvtmp, qtvstr[2048];
	int flags = 0, offset = 0;
	size_t len;

	int client;
	int type;
	char *msg;

	char *chat_sound_file;
	float chat_sound_vol = 0.0;

	// { QTV: check do this string is QTV chat
	qtvtmp = SkipQTVLeadingProxies(s0);

	if (qtvtmp)
	{
		char name[1024] = {0}, *column;

		column = strchr(qtvtmp, ':');

		if (!column)
		{
			// this must not be the case, but...
			column = qtvtmp;
			name[0] = 0;
		}
		else
		{
			strlcpy(name, qtvtmp, bound(1, column - qtvtmp + 1, (int)sizeof(name)));
		}

		if      (!strncmp(s0, "#0:qtv_say_game:",      sizeof("#0:qtv_say_game:")-1))
			snprintf(qtvstr, sizeof(qtvstr), "%s%s%s\n", TP_ParseFunChars(qtv_gamechatprefix.string, false), name, column);
		else if (!strncmp(s0, "#0:qtv_say_team_game:", sizeof("#0:qtv_say_team_game:")-1))
			snprintf(qtvstr, sizeof(qtvstr), "%s(%s)%s\n", TP_ParseFunChars(qtv_gamechatprefix.string, false), name, column);
		else
			snprintf(qtvstr, sizeof(qtvstr), "%s%s%s\n", TP_ParseFunChars(qtv_chatprefix.string, false), name, column);

		s0 = qtvstr;
	}
	// }

	s = decode_string (s0);

	qwcslcpy (str, s, sizeof(str)/sizeof(str[0]));
	s = str; // so no memmory overwrite, because decode_string() uses static buffers

	if (level == PRINT_CHAT) 
	{
		if (TP_SuppressMessage (s)) {
			Com_DPrintf("Suppressed TP message: %s\n", s);
			return;
		}

		s0 = wcs2str (s); // TP_SuppressMessage may modify the source string, so s0 should be updated

		flags = TP_CategorizeMessage (s0, &offset);
		if (qtvtmp) {
			flags |= msgtype_qtv;
		}

		FChecks_CheckRequest (s0);

		s0 = wcs2str (s);

		if (Ignore_Message(s0, flags, offset)) {
			Com_DPrintf("Ignoring message: %s\n", s0);
			return;
		}

		if (flags == 0 && ignore_qizmo_spec.value && strlen (s0) > 0 && s0[0] == '[' && strstr(s0, "]: ") ) {
			Com_DPrintf("Ignoring qizmo message: %s\n", s0);
			return;
		}

		if (flags == 2 && !TP_FilterMessage (s + offset)) {
			Com_DPrintf("Filtered message: %s\n", s0);
			return;
		}

		check_flood = Ignore_Check_Flood(s0, flags, offset); // @CHECKME@
		if (check_flood == IGNORE_NO_ADD) {
			Com_DPrintf("Ignoring flood message: %s\n", s0);
			return;
		}
		else if (check_flood == NO_IGNORE_ADD)
			Ignore_Flood_Add(s0); // @CHECKME@

		suppress_talksound = false;

		if (flags == 2) 
		{
			suppress_talksound = TP_CheckSoundTrigger (s + offset);
			s0 = wcs2str (s); // TP_CheckSoundTrigger may modify the source string, so s0 should be updated
		}

		if (!cl_chatsound.value ||						// no sound at all
				(cl_chatsound.value == 2 && flags != 2))	// only play sound in mm2
			suppress_talksound = true;

		chat_sound_file = NULL;

		client = SeparateChat(s0, &type, &msg); // @CHECKME@

		if (client >= 0 && !suppress_talksound)
		{
			if (
					(
					 (type == CHAT_MM1 || type == CHAT_SPEC)
					 && (msg_filter.value == 1 || msg_filter.value == 3)
					)
					|| (
						(type == CHAT_MM2)
						&& (msg_filter.value == 2 || msg_filter.value == 3)
					   )
			   ) {
				return;
			}

			if (cl.players[client].spectator)
			{
				chat_sound_file = con_sound_spec_file.string;
				chat_sound_vol = con_sound_spec_volume.value;
			}
			else
			{
				switch (type)
				{
					case CHAT_MM1:
						chat_sound_file = con_sound_mm1_file.string;
						chat_sound_vol = con_sound_mm1_volume.value;
						break;
					case CHAT_MM2:
						chat_sound_file = con_sound_mm2_file.string;
						chat_sound_vol = con_sound_mm2_volume.value;
						break;
					case CHAT_SPEC:
						chat_sound_file = con_sound_spec_file.string;
						chat_sound_vol = con_sound_spec_volume.value;
						break;
					default:
						break;
				}
			}
		}
		if (chat_sound_file == NULL)
		{
			// assign "other" if not recognized
			chat_sound_file = con_sound_other_file.string;
			chat_sound_vol = con_sound_other_volume.value;
		}

		if (chat_sound_vol > 0) 
		{
			S_LocalSoundWithVol(chat_sound_file, chat_sound_vol);
		}

		if (level >= PRINT_HIGH && s[0]) // actually there are no need for level >= PRINT_HIGH check
			VID_NotifyActivity();

		if (s[0])  // KT sometimes sends empty strings
		{
			if (con_timestamps.value != 0) // @CHECKME@
			{
				SYSTEMTIME lt;
				char tmpbuf[16];
				GetLocalTime (&lt);
				if (con_timestamps.value == 1) {
					snprintf(tmpbuf,  sizeof (tmpbuf), "%2d:%02d ", lt.wHour, lt.wMinute);
					Com_Printf(tmpbuf);
				}
				else {
					snprintf(tmpbuf,  sizeof (tmpbuf), "%2d:%02d:%02d ", lt.wHour, lt.wMinute, lt.wSecond);
					Com_Printf(tmpbuf);
				}
			}
		}

		if (cl_nofake.value == 1 || (cl_nofake.value == 2 && flags != 2)) 
		{
			for (p = s; *p; p++)
			{
				if (*p == 0x0D || (*p == 0x0A && p[1]))
					*p = ' ';
			}
		}
	}

	if (cl.sprint_buf[0] && (level != cl.sprint_level || s[0] == 1 || s[0] == 2)) 
	{
		FlushString (cl.sprint_buf, cl.sprint_level, false, 0);
		cl.sprint_buf[0] = 0;
	}

	if (s[0] == 1 || s[0] == 2) 
	{
		FlushString (s, level, (flags == 2), offset);
		return;
	}

	// Emulate qwcslcat (which is not implemented)
	qwcslcpy (cl.sprint_buf + qwcslen(cl.sprint_buf), s, sizeof(cl.sprint_buf)/sizeof(wchar) - qwcslen(cl.sprint_buf));
	cl.sprint_level = level;

	if ((p = qwcsrchr(cl.sprint_buf, '\n'))) 
	{
		len = p - cl.sprint_buf + 1;
		memcpy(str, cl.sprint_buf, len * sizeof(wchar));
		str[len] = '\0';
		qwcslcpy (cl.sprint_buf, p + 1, sizeof(cl.sprint_buf)/sizeof(wchar));
		FlushString (str, level, (flags == 2), offset);
	}
}

void CL_ParsePrint (void)
{
	int level = MSG_ReadByte ();
	char* s0 = MSG_ReadString ();

	if (CL_Demo_SkipMessage())
		return;

	CL_ProcessPrint (level, s0);
}

void CL_ParseStufftext (void) 
{
	char *s = MSG_ReadString();

	// Always process demomarks, regardless of who inserted them
	if (!strcmp(s, "//demomark\n"))
	{
		// demo mark
		if (cls.demoseeking == DST_SEEKING_DEMOMARK) {
			extern cvar_t demo_jump_mark_offset;

			cls.demoseeking = DST_SEEKING_FOUND; // it will reset to the DST_SEEKING_NONE in the deep of the demo code

			return;
		}
	}

	// Any processing after this point will be ignored if not tracking the target player
	if (cls.state == ca_active && CL_Demo_SkipMessage())
		return;

	if (!strncmp(s, "//wps ", sizeof("//wps ") - 1))
	{
		if (developer.integer > 1)
			Com_DPrintf ("stufftext: %s\n", s);	
	}
	else
	{
		Com_DPrintf ("stufftext: %s\n", s);	
	}

	if (!strncmp(s, "alias _cs", 9))
		Cbuf_AddTextEx(&cbuf_svc, "alias _cs \"wait;+attack;wait;wait;-attack;wait\"\n");
	else if (!strncmp(s, "alias _y", 8))
		Cbuf_AddTextEx(&cbuf_svc, "alias _y \"wait;-attack;wait;wait;+attack;wait;wait;-attack;wait\"\n");
	else if (!strcmp(s, "cmd snap") || (!strncmp (s, "r_skyname ", 10) && !strchr (s, '\n')))
		Cbuf_AddTextEx(&cbuf_svc, va("%s\n", s));
	else if (!strncmp(s, "//tinfo ", sizeof("//tinfo ") - 1))
	{
		extern void Parse_TeamInfo(char *s);

		if (!cls.mvdplayback)
			Parse_TeamInfo( s + 2 );
	}
	else if (!strncmp(s, "//at ", sizeof("//at ") - 1))
	{
		// This is autotrack info from MVD demo/QTV stream, they are almost same.
		extern cvar_t demo_autotrack;
		extern cvar_t mvd_autotrack;

		if (demo_autotrack.integer)
		{
			Cbuf_AddTextEx (&cbuf_svc, va("track %s\n", s + sizeof("//at ")-1));
			if (mvd_autotrack.integer) 
			{
				// Do not let mvd_autotrack and demo_autotrack fight.
				Com_Printf("Server autotrack found, client autotrack turned off\n");
				Cvar_SetValue(&mvd_autotrack, 0);
			}
		}
	}
	else if (!strncmp(s, "//vwep ", 7) && s[strlen(s) - 1] == '\n') 
	{
		CL_ParseVWepPrecache(s);
		return;
	}
	else if (!strncmp(s, "//sn ", sizeof("//sn ") - 1))
	{
		// TODO : Don't require GL for this.
		extern void Parse_Shownick(char *s)	;
		Parse_Shownick( s + 2 );
	}
	else if (!strncmp(s, "//qul ", sizeof("//qul ") - 1))
	{
		// qtv user list
		Parse_QtvUserList( s + 2 );
	}
	else if (!strncmp(s, "//wps ", sizeof("//wps ") - 1))
	{
		// weapon stats
		extern void Parse_WeaponStats(char *s);
		Parse_WeaponStats( s + 2 );
	}
	else if (!strcmp(s, "cmd pext\n"))
	{
		// If someone requested protocol extensions we support - reply.

#ifdef PROTOCOL_VERSION_FTE
		extern unsigned int CL_SupportedFTEExtensions (void);
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_FTE2
		extern unsigned int CL_SupportedFTEExtensions2 (void);
#endif // PROTOCOL_VERSION_FTE2

		char tmp[128];
		char data[1024] = "cmd pext";
		int ext;

#ifdef PROTOCOL_VERSION_FTE
		ext = cls.fteprotocolextensions ? cls.fteprotocolextensions : CL_SupportedFTEExtensions();
		snprintf(tmp, sizeof(tmp), " 0x%x 0x%x", PROTOCOL_VERSION_FTE, ext);
		Com_Printf_State(PRINT_DBG, "PEXT: 0x%x is fte protocol ver and 0x%x is fteprotocolextensions\n", PROTOCOL_VERSION_FTE, ext);
		strlcat(data, tmp, sizeof(data));
#endif // PROTOCOL_VERSION_FTE 

#ifdef PROTOCOL_VERSION_FTE2
		ext = cls.fteprotocolextensions2 ? cls.fteprotocolextensions2 : CL_SupportedFTEExtensions2();
		snprintf(tmp, sizeof(tmp), " 0x%x 0x%x", PROTOCOL_VERSION_FTE2, ext);
		Com_Printf_State(PRINT_DBG, "PEXT: 0x%x is fte protocol ver and 0x%x is fteprotocolextensions2\n", PROTOCOL_VERSION_FTE2, ext);
		strlcat(data, tmp, sizeof(data));
#endif // PROTOCOL_VERSION_FTE2 

		strlcat(data, "\n", sizeof(data));
		Cbuf_AddTextEx(&cbuf_svc, data);
	}
	else
	{
		Cbuf_AddTextEx(&cbuf_svc, s);
	}

	// Execute stuffed commands immediately when starting a demo
	if (cls.demoplayback && cls.state != ca_active)
		Cbuf_ExecuteEx (&cbuf_svc); // FIXME: execute cbuf_main too?
}

void CL_SetStat (int stat, int value)
{
	int	j;

	if (stat < 0 || stat >= MAX_CL_STATS)
		Host_Error ("CL_SetStat: %i is invalid", stat);

	// Set the stat value for the current player we're parsing in the MVD.
	if (cls.mvdplayback)
	{
		cl.players[cls.lastto].stats[stat] = value;

		// If we're not tracking the active player,
		// then don't update sbar and such.
		if ( Cam_TrackNum() != cls.lastto )
			return;
	}

	Sbar_Changed();

	if (stat == STAT_ITEMS)
	{
		// Set flash times.
		for (j = 0; j < 32; j++)
			if ( (value & (1 << j)) && !(cl.stats[stat] & (1 << j)) )
				cl.item_gettime[j] = cl.time;
	}

	cl.stats[stat] = value;

#ifdef FTE_PEXT_ACCURATETIMINGS
	if (stat == STAT_TIME && (cls.fteprotocolextensions & FTE_PEXT_ACCURATETIMINGS))
	{
		cl.servertime_works = true;
		cl.servertime = cl.stats[STAT_TIME] * 0.001;
	}
	else
#endif
		if (stat == STAT_TIME && cl.z_ext & Z_EXT_SERVERTIME)
		{
			cl.servertime_works = true;
			cl.servertime = cl.stats[STAT_TIME] * 0.001;
		}

	if (cl.stats[STAT_MATCHSTARTTIME])
	{
		cl.gamestarttime = Sys_DoubleTime() - cl.servertime + ((double)cl.stats[STAT_MATCHSTARTTIME])/1000 - cl.gamepausetime;
	}

	TP_StatChanged(stat, value);
}

void CL_MuzzleFlash (void) 
{
	vec3_t forward, right, up, org, angles;
	model_t *mod;

	dlight_t *dl;
	int i, j, num_ent;
	entity_state_t *ent;
	player_state_t *state;
	vec3_t none = {0,0,0};

	i = MSG_ReadShort();

	if (CL_Demo_SkipMessage())
		return;

	if (!cl_muzzleflash.value)
		return;

	if (!cl.validsequence)
		return;

	if ((unsigned) (i - 1) >= MAX_CLIENTS) 
	{
		// A monster firing
		num_ent = cl.frames[cl.validsequence & UPDATE_MASK].packet_entities.num_entities;

		for (j = 0; j < num_ent; j++) 
		{
			ent = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities.entities[j];

			if (ent->number == i) 
			{
				mod = cl.model_precache[ent->modelindex];

				// Special muzzleflashes for some enemies.
				if (mod->modhint == MOD_SOLDIER)
				{
					AngleVectors(ent->angles, forward, right, up);
					VectorMA(ent->origin, 22, forward, org);
					VectorMA(org, 10, right, org);
					VectorMA(org, 12, up, org);

					if (amf_part_muzzleflash.value)
					{
						if (!ISPAUSED && amf_coronas.value)
							NewCorona(C_SMALLFLASH, org);
						DrawMuzzleflash(org, ent->angles, none);
					}
				}
				else if (mod->modhint == MOD_ENFORCER)
				{
					AngleVectors (ent->angles, forward, right, up);
					VectorMA(ent->origin, 22, forward, org);
					VectorMA(org, 10, right, org);
					VectorMA(org, 12, up, org);
					if (amf_part_muzzleflash.value)
					{
						if (amf_coronas.value)
							NewCorona(C_SMALLFLASH, org);
						DrawMuzzleflash(org, ent->angles, none);
					}
				}
				else if (mod->modhint == MOD_OGRE)
				{
					AngleVectors(ent->angles, forward, right, up);
					VectorMA(ent->origin, 22, forward, org);
					VectorMA(org, -8, right, org);
					VectorMA(org, 14, up, org);
					if (amf_part_muzzleflash.value)
					{
						if (amf_coronas.value)
							NewCorona(C_SMALLFLASH, org);
						DrawMuzzleflash(org, ent->angles, none);
					}
				}
				else
				{
					AngleVectors(ent->angles, forward, NULL, NULL);
					VectorMA(ent->origin, 18, forward, org);
				}

				dl = CL_AllocDlight(-i);
				dl->radius = 200 + (rand() & 31);
				dl->minlight = 32;
				dl->die = cl.time + 0.1;

				// Blue muzzleflashes for shamblers/teslas
				if (mod->modhint == MOD_SHAMBLER || mod->modhint == MOD_TESLA)
					dl->type = lt_blue;
				else
					dl->type = lt_muzzleflash;
				VectorCopy(org, dl->origin);

				break;
			}
		}
		return;
	}

	if (cl_muzzleflash.value == 2 && i - 1 == cl.viewplayernum)
		return;

	dl = CL_AllocDlight(-i);
	state = &cl.frames[cls.mvdplayback ? (cl.oldparsecount & UPDATE_MASK) : parsecountmod].playerstate[i - 1];

	if ((i - 1) == cl.viewplayernum)
	{
		VectorCopy(cl.simangles, angles);
		VectorCopy(cl.simorg, org);
	}
	else
	{
		VectorCopy(state->viewangles, angles);
		VectorCopy(state->origin, org);
	}

	AngleVectors(angles, forward, right, up);
	VectorMA(org, 22, forward, org);
	VectorMA(org, 10, right, org);
	VectorMA(org, 12, up, org);
	VectorCopy(org, dl->origin);

	dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cl.time + 0.1;
	dl->type = lt_muzzleflash;

	if (amf_part_muzzleflash.value)
	{
		if (((i - 1) != cl.viewplayernum) || (cameratype != C_NORMAL))
		{
			if (amf_coronas.value)
				NewCorona(C_SMALLFLASH, dl->origin);
			DrawMuzzleflash(org, angles, state->velocity);
		}
	}
}

void CL_ParseQizmoVoice (void) 
{
	/* hifi: removed unused warnings, kept null logic */
	int i;
	MSG_ReadByte();
	MSG_ReadByte();

	for (i = 0; i < 32; i++)
		MSG_ReadByte();
	/*
	   int i, seq, bits;

	// Read the two-byte header.
	seq = MSG_ReadByte();
	bits = MSG_ReadByte();

	seq |= (bits & 0x30) << 4;	// 10-bit block sequence number, strictly increasing
	num = bits >> 6;			// 2-bit sample number, bumped at the start of a new sample
	unknown = bits & 0x0f;		// mysterious 4 bits.  volume multiplier maybe?

	// 32 bytes of voice data follow
	for (i = 0; i < 32; i++)
	MSG_ReadByte();
	*/
}

#define SHOWNET(x) {if (cl_shownet.value == 2) Com_Printf ("%3i:%s\n", msg_readcount - 1, x);}

void CL_ParseServerMessage (void) 
{
	int cmd, i, j = 0;
	char *s;
	extern int mvd_fixangle;
	vec3_t newangles;
	int msg_svc_start;
	int oldread = 0;

	if (cl_shownet.value == 1) 
	{
		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("%i ", net_message.cursize);
	}
	else if (cl_shownet.value == 2) 
	{
		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("------------------\n");
	}

	cls.demomessage.cursize = 0;

	CL_ParseClientdata();
	CL_ClearProjectiles();

	// Parse the message.
	while (1) 
	{
		if (msg_badread) 
		{
			Host_Error ("CL_ParseServerMessage: Bad server message");
			break;
		}

		msg_svc_start = msg_readcount;
		cmd = MSG_ReadByte ();

		if (cmd == -1) 
		{
			msg_readcount++;	// so the EOM showner has the right value
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cmd == svc_qizmovoice)
			SHOWNET("svc_qizmovoice")
		else if (cmd < num_svc_strings)
			SHOWNET(svc_strings[cmd]);

		// Update msg no:
		if (cmd < NUMMSG)
			cl_messages[cmd].msgs++;

		oldread = msg_readcount;

		// Other commands
		switch (cmd) 
		{
			default:
				{	
					Host_Error ("CL_ParseServerMessage: Illegible server message (%d)\n", cmd);
					break;
				}
			case svc_nop:
				{
					break;
				}
			case svc_disconnect:
				{
					VID_NotifyActivity();

					if (cls.mvdplayback == QTV_PLAYBACK)
					{ 
						// Workaround, do not disconnect in case of QTV playback
						if (strcmp(s = MSG_ReadString(), "EndOfDemo"))
							Com_Printf("WARNING: Non-standard disconnect message from QTV '%s'\n", s);
						break;
					}

					if (cls.mvdplayback == true) // MVD playback, but not QTV stream.
					{
						extern	int		pb_cnt;

						// We still have some data, so lets try ignore disconnect since it probably multy map MVD.
						if (pb_cnt > 0)
						{
							if (strcmp(s = MSG_ReadString(), "EndOfDemo"))
								Com_Printf("WARNING: Non-standard disconnect message in MVD '%s'\n", s);

							Com_DPrintf("Ignoring Server disconnect\n");
							break;
						}
					}

					if (cls.state == ca_connected) 
					{
						Host_Error( "Server disconnected\n"
								"Server version may not be compatible");
					} 
					else if (cls.demoplayback && cls.state == ca_demostart) 
					{
						Com_DPrintf("Server disconnect found - hadn't started yet, ignoring.\n");
					}
					else 
					{
						Com_DPrintf("Server disconnected\n");
						Host_EndGame();	// The server will be killed if it tries to kick the local player.
						Host_Abort();
					}
					break;
				}
			case nq_svc_time:
				{
					MSG_ReadFloat();
					break;
				}
			case svc_print:
				{
					CL_ParsePrint();
					break;
				}
			case svc_centerprint:
				{
					// SCR_CenterPrint (MSG_ReadString ());
					// Centerprint re-triggers
					s = MSG_ReadString();

					if (CL_Demo_SkipMessage ())
						break;

					if (!cls.demoseeking)
					{
						if (!CL_SearchForReTriggers(s, RE_PRINT_CENTER))
							SCR_CenterPrint(s);
						Print_flags[Print_current] = 0;
					}
					break;
				}
			case svc_stufftext:
				{
					CL_ParseStufftext();
					break;
				}
			case svc_damage:
				{
					V_ParseDamage();
					break;
				}
			case svc_serverdata:
				{
					Cbuf_ExecuteEx(&cbuf_svc);		// make sure any stuffed commands are done
					CL_ParseServerData();
					vid.recalc_refdef = true;		// leave full screen intermission
					break;
				}
			case svc_setangle:
				{
					if (cls.mvdplayback)
						j = MSG_ReadByte();

					for (i = 0; i < 3; i++)
						newangles[i] = MSG_ReadAngle();

					if (CL_Demo_SkipMessage ())
						break;

					if (cls.mvdplayback) 
					{
						mvd_fixangle |= 1 << j;
						if (j == Cam_TrackNum())
							VectorCopy(newangles, cl.viewangles);
					} 
					else 
					{
#ifdef I_WANT_HAX
						// TODO: detect respawns
						if (!Cvar_Value("cl_teleports_keep_pitch"))
							cl.viewangles[0] = newangles[0];
						cl.viewangles[1] = newangles[1];
						cl.viewangles[2] = newangles[2];
#else
						VectorCopy (newangles, cl.viewangles);
#endif // I_WANT_HAX
					}
					break;
				}
			case svc_lightstyle:
				{
					i = MSG_ReadByte ();
					if (i >= MAX_LIGHTSTYLES)
						Host_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
					strlcpy(cl_lightstyle[i].map, MSG_ReadString(), sizeof(cl_lightstyle[i].map));
					cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
					break;
				}
			case svc_sound:
				{
					CL_ParseStartSoundPacket();
					break;
				}
			case svc_stopsound:
				{
					i = MSG_ReadShort();

					if (CL_Demo_SkipMessage ())
						break;

					S_StopSound(i >> 3, i & 7);
					break;
				}

			case svc_updatefrags:
				{
					Sbar_Changed();
					i = MSG_ReadByte();
					if (i >= MAX_CLIENTS)
						Host_Error("CL_ParseServerMessage: svc_updatefrags > MAX_CLIENTS");
					cl.players[i].frags = MSG_ReadShort();
					break;
				}
			case svc_updateping:
				{
					i = MSG_ReadByte();
					if (i >= MAX_CLIENTS)
						Host_Error("CL_ParseServerMessage: svc_updateping > MAX_CLIENTS");
					cl.players[i].ping = MSG_ReadShort();
					break;
				}
			case svc_updatepl:
				{
					i = MSG_ReadByte();
					if (i >= MAX_CLIENTS)
						Host_Error("CL_ParseServerMessage: svc_updatepl > MAX_CLIENTS");
					cl.players[i].pl = MSG_ReadByte();
					break;
				}
			case svc_updateentertime:
				{
					// Time is sent over as seconds ago.
					i = MSG_ReadByte();
					if (i >= MAX_CLIENTS)
						Host_Error ("CL_ParseServerMessage: svc_updateentertime > MAX_CLIENTS");
					cl.players[i].entertime = (cls.demoplayback ? cls.demotime : cls.realtime) - MSG_ReadFloat();
					break;
				}
			case svc_spawnbaseline:
				{
					i = MSG_ReadShort();
					CL_ParseBaseline(&cl_entities[i].baseline);
					break;
				}
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_SPAWNSTATIC2)
			case svc_fte_spawnbaseline2:
				{
					CL_ParseSpawnBaseline2();
					break;
				}
#endif // PROTOCOL_VERSION_FTE
			case svc_spawnstatic:
				{
					CL_ParseStatic(false);
					break;
				}
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_SPAWNSTATIC2)
			case svc_fte_spawnstatic2:
				{
					if (cls.fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2)
						CL_ParseStatic(true);
					else
						Host_Error("CL_ParseServerMessage: svc_fte_modellistshort without FTE_PEXT_SPAWNSTATIC2");
					break;
				}
#endif // PROTOCOL_VERSION_FTE
			case svc_temp_entity:
				{
					CL_ParseTEnt();
					break;
				}
			case svc_killedmonster:
				{
					cl.stats[STAT_MONSTERS]++;
					break;
				}
			case svc_foundsecret:
				{
					cl.stats[STAT_SECRETS]++;
					break;
				}
			case svc_updatestat:
				{
					i = MSG_ReadByte();
					j = MSG_ReadByte();

					CL_SetStat(i, j);
					break;
				}
			case svc_updatestatlong:
				{
					i = MSG_ReadByte();
					j = MSG_ReadLong();

					CL_SetStat(i, j);
					break;
				}
			case svc_spawnstaticsound:
				{
					CL_ParseStaticSound();
					break;
				}
			case svc_cdtrack:
				{
					cl.cdtrack = MSG_ReadByte ();
					CDAudio_Play((byte)cl.cdtrack, true);
					break;
				}
			case svc_intermission:
				{
					cl.intermission = 1;
					cl.completed_time = cls.demoplayback ? cls.demotime : cls.realtime;
					cl.solo_completed_time = cl.servertime;
					vid.recalc_refdef = true;	// go to full screen
					for (i = 0; i < 3; i++)
						cl.simorg[i] = MSG_ReadCoord();
					for (i = 0; i < 3; i++)
						cl.simangles[i] = MSG_ReadAngle();
					VectorClear(cl.simvel);
					TP_ExecTrigger ("f_mapend");
					break;
				}
			case svc_finale:
				{
					cl.intermission = 2;
					cl.completed_time = cls.demoplayback ? cls.demotime : cls.realtime;
					cl.solo_completed_time = cl.servertime;
					vid.recalc_refdef = true;	// go to full screen
					SCR_CenterPrint(MSG_ReadString ());
					break;
				}
			case svc_sellscreen:
				{
					Cmd_ExecuteString("help");
					break;
				}
			case svc_smallkick:
				{
					if (CL_Demo_SkipMessage ())
						break;

					cl.ideal_punchangle = -2;
					break;
				}
			case svc_bigkick:
				{
					if (CL_Demo_SkipMessage ())
						break;

					cl.ideal_punchangle = -4;
					break;
				}
			case svc_muzzleflash:
				{
					CL_MuzzleFlash();
					break;
				}
			case svc_updateuserinfo:
				{
					CL_UpdateUserinfo();
					break;
				}
			case svc_setinfo:
				{
					CL_SetInfo();
					break;
				}
			case svc_serverinfo:
				{
					CL_ParseServerInfoChange();
					break;
				}
			case svc_download:
				{
					CL_ParseDownload();
					break;
				}
			case svc_playerinfo:
				{
					if (cls.demorecording)
					{
						CL_InitialiseDemoMessageIfRequired();
						MSG_WriteByte(&cls.demomessage, svc_playerinfo);
					}
					CL_ParsePlayerinfo();
					break;
				}
			case svc_nails:
				{
					CL_ParseProjectiles(false);
					break;
				}
			case svc_nails2:
				{
					if (!cls.mvdplayback)
						Host_Error("CL_ParseServerMessage: svc_nails2 without cls.mvdplayback");
					CL_ParseProjectiles(true);
					break;
				}
			case svc_chokecount: // Some preceding packets were choked
				{
					i = MSG_ReadByte();
					for (j = cls.netchan.incoming_acknowledged - 1; i > 0 && j > cls.netchan.outgoing_sequence - UPDATE_BACKUP; j--) 
					{
						if (cl.frames[j & UPDATE_MASK].receivedtime != -3) 
						{
							cl.frames[j & UPDATE_MASK].receivedtime = -2;
							i--;
						}
					}
					break;
				}
			case svc_modellist:
				{
					CL_ParseModellist(false);
					break;
				}
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_MODELDBL)
			case svc_fte_modellistshort:
				{
					if (cls.fteprotocolextensions & FTE_PEXT_MODELDBL)
						CL_ParseModellist(true);
					else
						Host_Error("CL_ParseServerMessage: svc_fte_modellistshort without FTE_PEXT_MODELDBL");

					break;
				}
#endif  // PROTOCOL_VERSION_FTE
			case svc_soundlist:
				{
					CL_ParseSoundlist();
					break;
				}
			case svc_packetentities:
				{
					CL_ParsePacketEntities(false);
					break;
				}
			case svc_deltapacketentities:
				{
					CL_ParsePacketEntities(true);
					break;
				}
			case svc_maxspeed:
				{
					float newspeed = MSG_ReadFloat ();

					if (CL_Demo_SkipMessage ())
						break;

					cl.maxspeed = newspeed;
					break;
				}
			case svc_entgravity :
				{
					float newgravity = MSG_ReadFloat ();

					if (CL_Demo_SkipMessage ())
						break;

					cl.entgravity = newgravity;
					break;
				}
			case svc_setpause:
				{
					if (MSG_ReadByte())
						cl.paused |= PAUSED_SERVER;
					else
						cl.paused &= ~PAUSED_SERVER;

					if (ISPAUSED)
						CDAudio_Pause();
					else
						CDAudio_Resume();
					break;
				}
			case svc_qizmovoice:
				{
					CL_ParseQizmoVoice();
					break;
				}
		}

		// cl_messages, update size
		if (cmd < NUMMSG)
			cl_messages[cmd].size += msg_readcount - oldread;

		if (cls.demorecording)
		{
			// Init the demo message buffer if it hasn't been done.
			CL_InitialiseDemoMessageIfRequired();

			// Write the change in entities to the demo being recorded
			// or the net message we just received.
			if (cmd == svc_deltapacketentities)
				CL_WriteDemoEntities();
			else if (cmd == svc_download) {
				// there's no point in writing it to the demo
			}
			else if (cmd == svc_serverdata)
				CL_WriteServerdata(&cls.demomessage);
			else if (cmd != svc_playerinfo)		// We write svc_playerinfo out as we read it in
				SZ_Write(&cls.demomessage, net_message.data + msg_svc_start, msg_readcount - msg_svc_start);
		}
	}

	if (cls.demorecording)
	{
		// Write the gathered changes to the demo file.
		if (cls.demomessage.cursize)
			CL_WriteDemoMessage (&cls.demomessage);
	}

	CL_SetSolidEntities ();
}

static void CL_InitialiseDemoMessageIfRequired(void)
{
	if (!cls.demomessage.cursize)
	{
		SZ_Init(&cls.demomessage, cls.demomessage_data, sizeof(cls.demomessage_data));
		SZ_Write(&cls.demomessage, net_message.data, 8);
	}
}
